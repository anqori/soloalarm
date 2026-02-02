# Solo Sailing Timer (Arduino Uno R3)

A minimal-hardware countdown timer for solo sailing:
- Rotary knob for interval setting
- One button for reset
- One toggle switch to arm/disarm
- Quiet warning speaker
- Loud 12V siren via relay at time=0

## Hardware (no soldering)

Recommended pluggable parts:
- Arduino Uno R3
- Grove Base Shield V2 (part 103030000)
- Grove OLED 0.96" (SSD1306/SSD1308, I2C)
- Grove Button (momentary)
- Grove Switch (toggle)
- Grove Speaker (warning)
- Grove Rotary Sensor (interval knob)
- Grove Relay 2-Channel (part 103020132) for 12V siren
- 12V active siren (loud alarm)
- 12V -> 5V buck module (LM2596 type with screw terminals)
- Grove 4-pin cables

## Wiring (pin map)

Power:
- 12V input -> buck module -> 5V
- Buck 5V -> Arduino 5V pin
- Buck GND -> Arduino GND
- All grounds shared (Arduino, relay module, speaker, siren)
- Set the Base Shield power switch to 5V

Grove ports (Base Shield V2):
- D2/D3 port: Grove Button (uses D2)
- D4/D5 port: Grove Switch (uses D4)
- D6/D7 port: Grove Speaker (uses D6, PWM)
- D8/D9 port: Grove Relay 2CH (uses D8; D9 kept off)
- A0/A1 port: Grove Rotary Sensor (uses A0)
- I2C port: Grove OLED

Siren (12V load):
- 12V + -> relay COM
- relay NO -> siren +
- siren - -> 12V GND

If your relay module is active-low, set `RELAY_ACTIVE_LOW = true` in
`arduino-project/arduino-project.ino`.
If your button logic is inverted, toggle `BUTTON_ACTIVE_LOW` in
`arduino-project/arduino-project.ino`.

## UI / Behavior

- **Disarmed**: shows interval + warn time.
- **Set interval**: turn the rotary knob (maps to 1..30 minutes).
- **Set warn time**: hold the button at boot for ~2 seconds.
  - Seconds scroll 0..60 with acceleration.
  - Release to save.
- **Arm**: flip the toggle switch ON.
- **Reset while armed**: tap button to reset countdown to full interval.

Warning + alarm:
- Warning phase: quiet speaker beeps increase in frequency as time runs out.
- Alarm phase: 12V siren pulses faster for ~30s, then stays on continuously up to 5 minutes total.
- In alarm, tap button to acknowledge and start a new full countdown.

## Build

Install required libraries:

```
make deps
```

Compile:

```
make build
```

Upload (example port):

```
make upload PORT=/dev/ttyACM0
```

Monitor (example):

```
make monitor PORT=/dev/ttyACM0 BAUD=115200
```

## Notes

- The warning speaker is intentionally quiet. The siren is only used at time=0.
- Use a buck converter for 12V -> 5V to keep the Arduino cool.
- The timer stores warn time in EEPROM so settings persist.
- Interval comes from the rotary sensor position.
- Grove Relay 2CH uses digital SIG1/SIG2 (not I2C). Channel 2 is held OFF.
- Warning loudness can be tuned in `arduino-project/arduino-project.ino` via
  `WARN_SPEAKER_FREQ_HZ`, `WARN_PERIOD_*`, and `WARN_DUTY_*`.
- Default build target is `MiniCore:avr:328` with `variant=modelPB` for ATmega328PB boards (e.g., ARD ONE C-MC).
- For ATmega328P boards, override with `make build BOARD_OPTIONS=variant=modelP` (and same for `make upload`).
