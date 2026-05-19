# Solo Sailing Timer (M5Stack Tough)

A touchscreen countdown timer for solo sailing, built around an exposed
M5Stack Tough and an IP-rated junction box for the high-current wiring.

## Hardware

Recommended parts:

- M5Stack Tough
- M5Stack Unit 2Relay
- IP-rated junction box for relay, fuses, and terminals
- 12V active siren for the countdown alarm
- 12V horn for fog signals
- 12V supply
- 6-core cable between junction box and Tough
- Fuses for electronics, siren, and horn branches
- Cable glands, ferrules, and strain relief

The Tough is the user interface and warning speaker. The relay, siren, horn,
fuses, and power distribution live in the junction box.

## Wiring

The 12V supply enters the junction box first.

Cable from junction box to M5Stack Tough:

- `+12V`: fused Tough power input
- `GND`: Tough power ground
- `Grove GND`: Unit 2Relay control ground
- `Grove 5V`: Unit 2Relay logic power from the Tough Grove port
- `Grove SIG1`: relay channel signal
- `Grove SIG2`: relay channel signal

`GND` and `Grove GND` are electrically common, but keeping both conductors in
the cable makes the power and Grove pinout explicit.

Unit 2Relay connection:

- Tough Port A `G32` -> Unit 2Relay CH2 -> fog horn
- Tough Port A `G33` -> Unit 2Relay CH1 -> alarm siren

Do not feed 12V into Grove `5V`. The Grove `5V` conductor is supplied by the
Tough Grove port only.

Load wiring:

```text
12V supply
  -> electronics fuse -> Tough power pair
  -> siren fuse      -> relay CH1 COM
  -> horn fuse       -> relay CH2 COM

relay CH1 NO -> siren +
relay CH2 NO -> horn +
siren - / horn - -> 12V GND
```

If the horn current or inrush is too high for the Unit 2Relay, use CH2 to
drive an external automotive or marine relay coil, and let that relay switch
the horn power.

## UI / Behavior

The Tough has no normal programmable front-panel app buttons. The firmware uses
the touchscreen only.

The visual style is inspired by Raymarine i70-style instruments: black face,
white readout panes, large numeric readouts, and restrained dark controls.

The app has two operating modes: **Timer** and **Signals**. Switch between them
by swiping horizontally:

- Swipe left from Timer: Signals
- Swipe right from Signals: Timer
- The header shows page dots and a side chevron to make the swipe direction visible.
- The small `*` button in the header opens settings for the current mode.

Timer screen:

- Shows disarmed, armed, warning, or alarm state.
- Shows interval or remaining time.
- Hold the main white readout area to start the countdown.
- Any touch while armed/warning restarts the countdown.
- Any touch in alarm stops the siren and starts a new countdown.
- Hold the main white readout area to return to disarmed.

Timer settings:

- Open from the Timer screen header.
- `Interval`: 1..30 minutes
- `Warning`: 0..60 seconds
- **Done** returns to Timer.

Signals settings:

- Open from the Signals screen header.
- `Underway interval`: 30s, 60s, or 120s between automatic signal groups
- Applies to `Sailing` and `Power`
- `Stopped` remains 120s
- **Done** returns to Signals.
- Settings are stored in ESP32 preferences and survive restart.

Signals screen:

- Touch and hold the horn image for 0.25s without moving to sound the horn manually.
- Hold the stacked horn image to sound one five-short-blast warning pattern.
- Under **Fog / Auto**, hold a mode button to start automatic fog signals.
- Hold **Off** to stop automatic fog signals.
- Hold progress is shown in the header as a full-width `HOLD` bar so it stays visible under your finger.
- The active mode button is highlighted.
- Hold another mode to switch.
- Modes:
  - `Sailing`: one prolonged blast + two short blasts every configured underway interval
  - `Power`: one prolonged blast every configured underway interval
  - `Stopped`: two prolonged blasts every 120s

The five-blast warning pattern overrides any automatic or manual horn output
already in progress. Fog controls remain available while the countdown alarm is
active. The skipper is responsible for choosing the legally correct fog signal
for the situation.

Output indicators:

- Lower-right `A`: alarm siren relay is active.
- Lower-right `H`: horn relay is active.
- Filled/bright means the output should currently be heard; dark outline means off.

Warning + alarm:

- Warning phase: built-in Tough speaker beeps increase in frequency as time
  runs out.
- Alarm phase: siren pulses faster for ~30s, then stays on continuously up to
  5 minutes total.
- After 5 minutes the siren relay turns off automatically.

## Build

Install the M5Stack board package and libraries:

```bash
make deps
```

Compile:

```bash
make build
```

Upload:

```bash
make upload PORT=/dev/ttyUSB0
```

Monitor:

```bash
make monitor PORT=/dev/ttyUSB0
```

## Notes

- Default build target is `m5stack:esp32:m5stack_tough`.
- The code uses M5Unified/M5GFX for display, touch, and speaker support.
- Relay outputs default to active-high. If your relay board is active-low, set
  `RELAY_ACTIVE_LOW = true` in `arduino-project/arduino-project.ino`.
