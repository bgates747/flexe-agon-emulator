// Flexe Agon Emulator eZ80 UART functional model and regressions.
// GPL-3.0, matching the Fab frontend. Functional 8N1 model, not silicon timing.
use std::collections::VecDeque;
pub trait SerialLink {
    fn send(&mut self, byte: u8);
    fn recv(&mut self) -> Option<u8>;
    fn read_clear_to_send(&mut self) -> bool;
}
pub struct Uart {
    link: Box<dyn SerialLink>,
    rx_fifo: VecDeque<u8>, tx_fifo: VecDeque<u8>,
    shift: Option<(u8, i32)>, receive_cooldown: i32, rx_idle: i32,
    cts: bool, delta: u8, errors: u8, tx_pending: bool,
    pub ier: u8, fctl: u8, pub lctl: u8, pub brg_div: u16, pub spr: u8,
    trace: Option<std::path::PathBuf>, tx_irqs: u64, modem_irqs: u64,
    rx_irqs: u64, tx_drops: u64, rx_overruns: u64, cts_edges: u64,
}
impl Uart {
    pub fn new(mut link: Box<dyn SerialLink>) -> Self {
        let cts=link.read_clear_to_send();
        Self {link,rx_fifo:VecDeque::new(),tx_fifo:VecDeque::new(),shift:None,
            receive_cooldown:0,rx_idle:0,cts,delta:0,errors:0,tx_pending:false,
            ier:0,fctl:0,lctl:0,brg_div:2,spr:0,trace:None,tx_irqs:0,
            modem_irqs:0,rx_irqs:0,tx_drops:0,rx_overruns:0,cts_edges:0}
    }
    pub fn with_trace(mut self)->Self {
        self.trace=std::env::var_os("MOS_UART_STATS").map(std::path::PathBuf::from);self
    }
    fn char_cycles(&self)->i32 { i32::from(self.brg_div.max(1))*160 }
    fn capacity(&self)->usize {if self.fctl&1!=0 {16} else {1}}
    fn threshold(&self)->usize {
        if self.fctl&1==0 {1} else {[1,4,8,14][(self.fctl>>6) as usize]}
    }
    fn sample_cts(&mut self) {
        let now=self.link.read_clear_to_send();
        if now!=self.cts {self.cts=now;self.delta|=1;self.cts_edges+=1;}
    }
    // GPIO level reads must not acknowledge the modem delta register.
    pub fn clear_to_send(&mut self)->bool {self.sample_cts();self.cts}
    fn start_shift(&mut self) {
        if self.shift.is_none() {
            if let Some(byte)=self.tx_fifo.pop_front() {
                self.shift=Some((byte,self.char_cycles()));
                if self.tx_fifo.is_empty(){self.tx_pending=true;}
            }
        }
    }
    pub fn apply_ticks(&mut self,cycles:i32) {
        assert!(cycles>=0);
        self.sample_cts();
        self.start_shift();
        let mut budget=cycles;
        while let Some((byte,left))=self.shift {
            if budget<left {self.shift=Some((byte,left-budget));break;}
            budget-=left;self.link.send(byte);self.shift=None;self.start_shift();
        }
        self.rx_idle=self.rx_idle.saturating_add(cycles);
        self.receive_cooldown-=cycles;
        while self.receive_cooldown<=0 {
            match self.link.recv() {
                Some(byte)=>{
                    if self.rx_fifo.len()<self.capacity(){self.rx_fifo.push_back(byte)}
                    else {self.errors|=2;self.rx_overruns+=1;}
                    self.rx_idle=0;
                    self.receive_cooldown+=self.char_cycles();
                }
                None=>{self.receive_cooldown=0;break;}
            }
        }
    }
    pub fn send_byte(&mut self,value:u8) {
        if self.tx_fifo.len()<self.capacity() {
            self.tx_fifo.push_back(value);self.tx_pending=false;
        } else {self.tx_drops+=1;} // Full holding FIFO discards the write.
    }
    pub fn receive_byte(&mut self)->u8 {
        self.rx_idle=0;
        self.rx_fifo.pop_front().unwrap_or(0)
    }
    pub fn rx_byte_ready(&self)->bool {!self.rx_fifo.is_empty()}
    pub fn read_lsr(&mut self)->u8 {
        let status=u8::from(self.rx_byte_ready())|self.errors|
            if self.tx_fifo.is_empty(){0x20}else{0}|
            if self.tx_fifo.is_empty()&&self.shift.is_none(){0x40}else{0};
        self.errors=0;status
    }
    pub fn write_fctl(&mut self,value:u8) {
        if (value^self.fctl)&1!=0 || value&2!=0 {self.rx_fifo.clear();self.errors=0;self.rx_idle=0;}
        if (value^self.fctl)&1!=0 || value&4!=0 {
            self.tx_fifo.clear();self.tx_pending=true;
        }
        self.fctl=value&0xc1;
    }
    pub fn write_ier(&mut self,value:u8) {
        if value&2!=0 && self.ier&2==0 && self.tx_fifo.is_empty(){self.tx_pending=true;}
        self.ier=value;
        if value&0x0a==0 {
            if let Some(path)=&self.trace {
                let row=format!("{{\"tx_irqs\":{},\"modem_irqs\":{},\"rx_irqs\":{},\"tx_drops\":{},\"rx_overruns\":{},\"cts_edges\":{}}}\n",
                    self.tx_irqs,self.modem_irqs,self.rx_irqs,self.tx_drops,self.rx_overruns,self.cts_edges);
                std::fs::write(path,row).expect("UART stats write failed");
            }
        }
    }
    fn cause(&mut self)->u8 {
        self.sample_cts();
        if self.ier&4!=0 && self.errors&0x1e!=0 {6}
        else if self.ier&1!=0 && self.rx_fifo.len()>=self.threshold(){4}
        else if self.ier&1!=0 && !self.rx_fifo.is_empty() && self.rx_idle>=4*self.char_cycles(){12}
        else if self.ier&2!=0 && self.tx_pending {2}
        else if self.ier&8!=0 && self.delta!=0 {0}
        else {1}
    }
    pub fn interrupt_pending(&mut self)->bool {self.cause()&1==0}
    pub fn read_iir(&mut self)->u8 {
        let cause=self.cause();
        match cause {
            2=>{self.tx_pending=false;self.tx_irqs+=1;},
            0=>self.modem_irqs+=1,
            4|12=>self.rx_irqs+=1,
            _=>{},
        }
        cause | if self.fctl&1!=0 {0xc0}else{0}
    }
    pub fn read_modem_status_register(&mut self)->u8 {
        self.sample_cts();let value=if self.cts{0x10}else{0}|self.delta;
        self.delta=0;value
    }
    pub fn is_access_brg_registers(&self)->bool {self.lctl&0x80!=0}
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{rc::Rc,cell::RefCell};
    #[derive(Default)]struct Wire {cts:bool,rx:VecDeque<u8>,tx:Vec<u8>}
    struct Link(Rc<RefCell<Wire>>);
    impl SerialLink for Link {
        fn send(&mut self,b:u8){self.0.borrow_mut().tx.push(b)}
        fn recv(&mut self)->Option<u8>{self.0.borrow_mut().rx.pop_front()}
        fn read_clear_to_send(&mut self)->bool{self.0.borrow().cts}
    }
    fn rig()->(Uart,Rc<RefCell<Wire>>) {
        let w=Rc::new(RefCell::new(Wire{cts:true,..Wire::default()}));
        let mut u=Uart::new(Box::new(Link(w.clone())));u.brg_div=1;u.write_fctl(7);(u,w)
    }
    #[test]fn no_pending_and_iir_does_not_clear_ier(){
        let(mut u,_)=rig();assert_eq!(u.read_iir(),0xc1);
        u.write_ier(2);assert!(u.interrupt_pending());assert_eq!(u.read_iir(),0xc2);
        assert_eq!(u.ier,2);assert!(!u.interrupt_pending());
        u.send_byte(7);u.apply_ticks(1);assert_eq!(u.read_iir(),0xc2);assert_eq!(u.ier,2);
    }
    #[test]fn fifo_and_shift_have_distinct_empty_states(){
        let(mut u,w)=rig();for b in 0..16{u.send_byte(b)}
        u.send_byte(99);assert_eq!(u.read_lsr()&0x60,0);u.apply_ticks(1);
        u.send_byte(16);u.apply_ticks(159);assert_eq!(w.borrow().tx,vec![0]);
        u.apply_ticks(15*160);assert_eq!(u.read_lsr()&0x60,0x20);
        u.apply_ticks(159);assert_eq!(u.read_lsr()&0x40,0);
        u.apply_ticks(1);assert_eq!(u.read_lsr()&0x60,0x60);
        assert_eq!(w.borrow().tx,(0..17).collect::<Vec<u8>>());
    }
    #[test]fn modem_edges_acknowledge_only_on_msr(){
        let(mut u,w)=rig();u.write_ier(8);w.borrow_mut().cts=false;
        assert!(!u.clear_to_send());assert!(u.interrupt_pending());assert_eq!(u.read_iir(),0xc0);
        assert!(u.interrupt_pending());assert_eq!(u.read_modem_status_register(),1);
        assert!(!u.interrupt_pending());w.borrow_mut().cts=true;u.apply_ticks(1);
        assert_eq!(u.read_modem_status_register(),0x11);assert_eq!(u.read_modem_status_register(),0x10);
    }
    #[test]fn cts_does_not_gate_already_accepted_bytes(){
        let(mut u,w)=rig();u.send_byte(42);w.borrow_mut().cts=false;
        u.apply_ticks(160);assert_eq!(w.borrow().tx,vec![42]);
    }
    #[test]fn priority_and_rx_overrun_latching(){
        let(mut u,w)=rig();w.borrow_mut().rx.extend(0..17);
        u.apply_ticks(17*160);u.write_ier(15);w.borrow_mut().cts=false;
        assert_eq!(u.read_iir(),0xc6);assert_ne!(u.read_lsr()&2,0);
        assert_eq!(u.read_lsr()&2,0);assert_eq!(u.read_iir(),0xc4);
        for b in 0..16 {assert_eq!(u.receive_byte(),b)}
        assert_eq!(u.read_iir(),0xc2);assert_eq!(u.read_iir(),0xc0);
        u.read_modem_status_register();assert_eq!(u.read_iir(),0xc1);
    }
    #[test]fn trigger_timeout_and_fifo_reset(){
        let(mut u,w)=rig();u.write_fctl(0xc1);u.write_ier(1);w.borrow_mut().rx.push_back(7);
        u.apply_ticks(1);assert_eq!(u.read_iir(),0xc1);u.apply_ticks(4*160);
        assert_eq!(u.read_iir(),0xcc);assert_eq!(u.receive_byte(),7);assert_eq!(u.read_iir(),0xc1);
        u.send_byte(8);u.apply_ticks(1);u.send_byte(9);u.write_fctl(5);
        u.apply_ticks(160);assert_eq!(w.borrow().tx,vec![8]);
        w.borrow_mut().rx.push_back(10);u.apply_ticks(160);u.write_fctl(3);
        assert_eq!(u.read_lsr()&1,0);
    }
    #[test]fn single_holding_register_without_fifo(){
        let(mut u,w)=rig();u.write_fctl(0);u.send_byte(1);u.send_byte(2);
        u.apply_ticks(1);u.send_byte(3);u.apply_ticks(319);assert_eq!(w.borrow().tx,vec![1,3]);
    }
}
