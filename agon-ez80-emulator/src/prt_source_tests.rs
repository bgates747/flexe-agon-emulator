//! PRT source regressions through the machine's real I/O and GPIO paths.
//! Functional clock/input tests, not wall-time or silicon-latency calibration.
use super::*;
use std::sync::atomic::{AtomicBool, AtomicI32};

struct Disconnected;
impl uart::SerialLink for Disconnected {
    fn send(&mut self, _: u8) {}
    fn recv(&mut self) -> Option<u8> { None }
    fn read_clear_to_send(&mut self) -> bool { true }
}
fn fixture() -> AgonMachine {
    let (tx, _) = std::sync::mpsc::channel();
    AgonMachine::new(AgonMachineConfig {
        uart0_link: Box::new(Disconnected), uart1_link: Box::new(Disconnected),
        soft_reset: Arc::new(AtomicBool::new(false)),
        emulator_shutdown: Arc::new(AtomicBool::new(false)),
        exit_status: Arc::new(AtomicI32::new(0)), paused: Arc::new(AtomicBool::new(false)),
        clockspeed_hz: 18_432_000, ram_init: RamInit::Zero,
        mos_bin: std::path::PathBuf::new(), gpios: Arc::new(gpio::GpioSet::new()),
        tx_gpio_vga_frame: tx, interrupt_precision: 16, external_ram_size: 512*1024,
    })
}
fn tick(m: &mut AgonMachine, clocks: i32) {
    m.cycle_counter.set(clocks);
    m.apply_elapsed_cycles();
}
fn program(m: &mut AgonMachine, timer: usize, reload: u16, ctl: u8) {
    let port = 0x80 + 3 * timer as u16;
    m.port_out(port + 1, reload as u8);
    m.port_out(port + 2, (reload >> 8) as u8);
    m.port_out(port, ctl);
    tick(m, 1); // existing reload-on-next-tick behavior, no fabricated elapsed I/O
}
fn count(m: &mut AgonMachine, timer: usize) -> u16 {
    let port = 0x81 + 3 * timer as u16;
    let low = m.port_in(port);
    u16::from(low) | (u16::from(m.port_in(port + 1)) << 8)
}
fn pulses(m: &AgonMachine, pin: u8, number: usize) {
    m.gpios.b.set_input_pin(pin, false);
    for _ in 0..number {
        m.gpios.b.set_input_pin(pin, true);
        m.gpios.b.set_input_pin(pin, false);
    }
}
#[test]
fn source_register_reset_roundtrip_and_shared_fields() {
    let mut m = fixture();
    assert_eq!(m.port_in(0x92), 0);
    for value in 0..=255u8 {
        m.io_unhandled.set(None);
        m.port_out(0x92, value);
        assert_eq!(m.port_in(0x92), value, "all eight source bits are writable");
        assert_eq!(m.io_unhandled.get(), None);
    }
    m.port_out(0x92, 0xa5);
    let saved = m.port_in(0x92);
    m.port_out(0x92, (saved & !0x0c) | 0x0c);
    assert_eq!(m.port_in(0x92), 0xad);
    let current = m.port_in(0x92);
    m.port_out(0x92, (current & !0x0c) | (saved & 0x0c));
    assert_eq!(m.port_in(0x92), saved);
    assert_eq!(fixture().port_in(0x92), 0);
}
#[test]
fn gpio_source_holds_without_edges() {
    let mut m = fixture();
    m.port_out(0x92, 0xaa);
    for timer in 0..6 { program(&mut m, timer, 1000, 0x1f); }
    let before: Vec<_> = (0..6).map(|n| count(&mut m, n)).collect();
    tick(&mut m, 4096);
    for timer in 0..4 { assert_eq!(count(&mut m, timer), before[timer]); }
    for timer in 4..6 { assert_eq!(count(&mut m, timer), before[timer] - 16); }
}
#[test]
fn gpio_events_use_selected_pin_and_ignore_divider() {
    for source in [2u8, 3] {
        for divider in 0..4u8 {
            let mut m = fixture();
            m.port_out(0x92, source * 0x55);
            for timer in 0..4 { program(&mut m, timer, 100, 0x13 | (divider << 2)); }
            pulses(&m, 0, 3);
            tick(&mut m, 1);
            assert_eq!([count(&mut m, 0), count(&mut m, 1), count(&mut m, 2), count(&mut m, 3)], [97,100,97,100]);
            pulses(&m, 1, 5);
            tick(&mut m, 1);
            assert_eq!([count(&mut m, 0), count(&mut m, 1), count(&mut m, 2), count(&mut m, 3)], [97,95,97,95]);
        }
    }
}
#[test]
fn gpio_pulses_are_counted_once_between_batches() {
    let mut m = fixture();
    m.port_out(0x92, 0x0c);
    program(&mut m, 1, 100, 0x1f);
    pulses(&m, 1, 7); // ends low: level-only polling would lose all seven
    tick(&mut m, 16);
    assert_eq!(count(&mut m, 1), 93);
    tick(&mut m, 1024);
    assert_eq!(count(&mut m, 1), 93, "do not replay consumed edges");
    m.gpios.b.set_input_pin(1, true);
    tick(&mut m, 16);
    m.gpios.b.set_input_pin(1, true);
    tick(&mut m, 16);
    assert_eq!(count(&mut m, 1), 92, "a held high input is one rising edge");
    m.gpios.b.set_input_pin(1, false);
    tick(&mut m, 16);
    assert_eq!(count(&mut m, 1), 92, "falling edge is not a count");
}
#[test]
fn gpio_output_and_disabled_history_are_not_inputs() {
    let mut m = fixture();
    m.port_out(0x92, 0x0c);
    pulses(&m, 1, 9); // before enable; must not replay when starting
    program(&mut m, 1, 100, 0x1f);
    assert_eq!(count(&mut m, 1), 100);
    pulses(&m, 1, 3);
    tick(&mut m, 1);
    assert_eq!(count(&mut m, 1), 97);
    m.port_out(0x9b, 0xfd); // PB1 output
    pulses(&m, 1, 6);
    tick(&mut m, 1);
    assert_eq!(count(&mut m, 1), 97);
    m.port_out(0x9b, 0xff); // input again
    m.port_out(0x83, 0); // disabled, preserving current count
    pulses(&m, 1, 4);
    m.port_out(0x83, 0x1d); // resume without reload
    tick(&mut m, 1);
    assert_eq!(count(&mut m, 1), 97);
}
#[test]
fn restore_system_clock_without_resetting_counter() {
    let mut m = fixture();
    m.port_out(0x92, 0x08);
    program(&mut m, 1, 100, 0x1f);
    pulses(&m, 1, 3);
    tick(&mut m, 1);
    assert_eq!(count(&mut m, 1), 97);
    m.port_out(0x92, 0);
    tick(&mut m, 256);
    assert_eq!(count(&mut m, 1), 96);
    pulses(&m, 1, 7);
    tick(&mut m, 256);
    assert_eq!(count(&mut m, 1), 95, "system source ignores GPIO");
    m.port_out(0x92, 0x08);
    tick(&mut m, 256);
    assert_eq!(count(&mut m, 1), 95, "source change does not replay old pin edges");
}
#[test]
fn rtc_source_uses_nominal_32768_input_not_cpu_divider() {
    for divider in 0..4u8 {
        let mut m = fixture();
        m.port_out(0x92, 0x55);
        for timer in 0..4 { program(&mut m, timer, 1000, 0x13 | (divider << 2)); }
        // At18.432MHz,1125 clocks are exactly two32768Hz periods.
        // Four reload ticks above remain below the first oscillator edge.
        tick(&mut m, 1121);
        for timer in 0..4 { assert_eq!(count(&mut m, timer), 998); }
        pulses(&m, 0, 7); pulses(&m, 1, 7);
        tick(&mut m, 1);
        for timer in 0..4 { assert_eq!(count(&mut m, timer), 998); }
        for _ in 0..10 { tick(&mut m, 1125); }
        for timer in 0..4 { assert_eq!(count(&mut m, timer), 978); }
    }
}
#[test]
fn gpio_reload_zero_wrap_and_irq_clear() {
    let mut m = fixture();
    m.port_out(0x92, 0x0c);
    program(&mut m, 1, 0, 0x5f);
    for _ in 0..512 { pulses(&m, 1, 128); tick(&mut m, 1024); }
    assert_eq!(count(&mut m, 1), 0);
    tick(&mut m, 8); // existing interrupt-latch latency is in system clocks
    assert_eq!(m.port_in(0x83) & 0x80, 0x80);
    assert_eq!(m.port_in(0x83) & 0x80, 0);
    pulses(&m, 1, 1);
    tick(&mut m, 8);
    assert_eq!(count(&mut m, 1), 65535);
    m.port_out(0x83, 0);
    pulses(&m, 1, 2);
    tick(&mut m, 1024);
    assert_eq!(count(&mut m, 1), 65535);
}
#[test]
fn default_source_counter_latch_and_independent_timers() {
    let mut m = fixture();
    for timer in 0..6 { program(&mut m, timer, 0x1201, 0x1f); }
    tick(&mut m, 256);
    for timer in 0..6 { assert_eq!(count(&mut m, timer), 0x1200); }
    let low = m.port_in(0x84);
    tick(&mut m, 256);
    assert_eq!(low, 0);
    assert_eq!(m.port_in(0x85), 0x12, "high byte is latched by the low read");
    assert_eq!(count(&mut m, 1), 0x11ff);
}
