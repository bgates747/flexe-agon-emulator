# Keyboard behaviour

Host key presses and releases enter a modeled PS/2 set-2 keyboard connected to
ESP32 RTC GPIO pads. The adapter executes the keyboard ULP instructions loaded
by the actual VDP firmware; that firmware owns decoding, keyboard layout,
modifiers and the key-state packets returned to MOS.

Both press and release transitions are forwarded, including held-key state.
Host repeat events are suppressed; firmware-issued typematic settings control
device repeat. Losing window focus releases held keys. Function keys follow
the same scancode path rather than separate ASCII shortcuts.

This is a functional hardware-oriented path. The ULP interpreter implements
the instruction subset required by the keyboard firmware and fails closed on
unsupported instructions. Serialized CPU scheduling and modeled pad timing
are not a claim of electrical or cycle-accurate hardware fidelity. Different
VDP firmware/layouts and broader key combinations need their own qualification.
Use the guest firmware's layout settings; a host layout does not replace them.
