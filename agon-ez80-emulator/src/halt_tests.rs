//! Functional HALT/IRQ regressions; no SDL, firmware image or wall-clock timing.
use super::*;
use ez80::Reg16;
use std::sync::atomic::{AtomicBool, AtomicI32};

struct DisconnectedSerial;
impl uart::SerialLink for DisconnectedSerial {
    fn send(&mut self, _: u8) {}
    fn recv(&mut self) -> Option<u8> { None }
    fn read_clear_to_send(&mut self) -> bool { true }
}
fn fixture(precision: i32) -> (AgonMachine, Cpu) {
    let (tx, _) = std::sync::mpsc::channel();
    let mut m = AgonMachine::new(AgonMachineConfig {
        uart0_link: Box::new(DisconnectedSerial), uart1_link: Box::new(DisconnectedSerial),
        soft_reset: Arc::new(AtomicBool::new(false)),
        emulator_shutdown: Arc::new(AtomicBool::new(false)),
        exit_status: Arc::new(AtomicI32::new(0)), paused: Arc::new(AtomicBool::new(false)),
        clockspeed_hz: 18_432_000, ram_init: RamInit::Zero,
        mos_bin: std::path::PathBuf::new(), gpios: Arc::new(gpio::GpioSet::new()),
        tx_gpio_vga_frame: tx, interrupt_precision: precision, external_ram_size: 512*1024,
    });
    m.enable_hostfs = false;
    m.cs0_lbr = 4; m.cs0_ubr = 11;
    m.mem_rom[0x1000] = 0x76; // HALT, then NOP
    m.mem_rom[0x1001] = 0;
    m.mem_rom[0x0a] = 0; m.mem_rom[0x0b] = 0x20; // PRT0 vector -> 0x2000
    m.mem_rom[0x2000..0x2003].copy_from_slice(&[0x3c, 0xed, 0x4d]); // INC A; RETI
    let mut cpu = Cpu::new_ez80();
    cpu.state.reg.adl = true;
    cpu.state.reg.set24(Reg16::SP, 0x060000);
    cpu.state.reg.set_a(0);
    m.mem_rom[0x0fff] = 0xfb; // EI through the public instruction path
    cpu.state.set_pc(0x0fff);
    m.execute_instruction(&mut cpu);
    cpu.state.set_pc(0x1000);
    m.cycle_counter.set(0);
    (m, cpu)
}
fn dispatch(m: &mut AgonMachine, cpu: &mut Cpu) {
    m.execute_instruction(cpu);
    if m.cycle_counter.get() >= m.interrupt_precision {
        m.apply_elapsed_cycles();
        m.do_interrupts(cpu);
    }
}
#[test]
fn halt_parked_clock_progresses_without_fetch_or_retirement() {
    let (mut m, mut cpu) = fixture(1);
    m.execute_instruction(&mut cpu);
    assert!(cpu.is_halted());
    assert_eq!(cpu.state.pc(), 0x1001);
    let retired = cpu.state.instructions_executed;
    m.cycle_counter.set(0);
    for _ in 0..16 { m.execute_instruction(&mut cpu); }
    assert_eq!(m.cycle_counter.get(), 16);
    assert_eq!(cpu.state.pc(), 0x1001);
    assert_eq!(cpu.state.instructions_executed, retired);
}
#[test]
fn halt_accepted_irq_executes_handler_and_returns_after_halt() {
    let (mut m, mut cpu) = fixture(1);
    m.execute_instruction(&mut cpu);
    m.prt_timers[0].write_ctl(0xc0); // already pending IRQ, timer stopped
    m.do_interrupts(&mut cpu);
    assert!(!cpu.state.halted, "accepted IRQ must release HALT");
    assert_eq!(cpu.state.pc(), 0x2000);
    m.execute_instruction(&mut cpu);
    assert_eq!(cpu.state.reg.a(), 1);
    m.execute_instruction(&mut cpu);
    assert_eq!(cpu.state.pc(), 0x1001);
    m.execute_instruction(&mut cpu);
    assert_eq!(cpu.state.pc(), 0x1002);
}
#[test]
fn halt_masked_irq_does_not_enter_handler() {
    let (mut m, mut cpu) = fixture(1);
    m.mem_rom[0x0fff] = 0xf3; // DI through the public instruction path
    cpu.state.set_pc(0x0fff);
    m.execute_instruction(&mut cpu);
    cpu.state.set_pc(0x1000);
    m.execute_instruction(&mut cpu);
    m.prt_timers[0].write_ctl(0xc0);
    for _ in 0..16 { dispatch(&mut m, &mut cpu); }
    assert!(cpu.state.halted);
    assert_eq!(cpu.state.pc(), 0x1001);
    assert_eq!(cpu.state.reg.a(), 0);
}
#[test]
fn halt_future_timer_wakes_at_precise_and_coarse_boundaries() {
    for precision in [1, 64] {
        let (mut m, mut cpu) = fixture(precision);
        m.execute_instruction(&mut cpu);
        m.cycle_counter.set(0);
        m.prt_timers[0].write_reload_low(4);
        m.prt_timers[0].write_reload_high(0);
        m.prt_timers[0].write_ctl(0x43); // enabled, reload, IRQ, divide4
        for _ in 0..256 {
            dispatch(&mut m, &mut cpu);
            if !cpu.state.halted { break; }
        }
        assert!(!cpu.state.halted, "future timer must wake at precision {precision}");
        assert!(m.total_cycles_elapsed > 0);
        assert_eq!(cpu.state.pc(), 0x2000);
    }
}
