#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Hardware pins (Arduino Uno R3 + Grove Base Shield V2)
static const uint8_t BUTTON_PIN = 2;     // Grove Button on D2/D3 port (SIG1)
static const uint8_t ARM_PIN = 4;        // Grove Switch on D4/D5 port (SIG1)
static const uint8_t RELAY_PIN = 8;      // Grove Relay 2CH on D8/D9 port (SIG1)
static const uint8_t RELAY2_PIN = 9;     // Keep channel 2 OFF
static const uint8_t SPEAKER_PIN = 6;    // Grove Speaker on D6/D7 port (SIG1, PWM)
static const uint8_t ROTARY_PIN = A0;    // Grove Rotary Sensor on A0/A1 port (SIG1)

// Button polarity: Grove Button modules are typically active HIGH.
static const bool BUTTON_ACTIVE_LOW = false;

// Relay polarity: set true if your module is active LOW
static const bool RELAY_ACTIVE_LOW = false;

// OLED display (SSD1306 128x64 I2C)
static const uint8_t OLED_ADDR = 0x3C;
static const int OLED_RESET = -1;
static const uint8_t OLED_WIDTH = 128;
static const uint8_t OLED_HEIGHT = 64;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Config limits
static const uint8_t INTERVAL_MIN_MIN = 1;
static const uint8_t INTERVAL_MIN_MAX = 30;
static const uint8_t WARN_SEC_MIN = 0;
static const uint8_t WARN_SEC_MAX = 60;

// Defaults
static const uint8_t DEFAULT_INTERVAL_MIN = 10;
static const uint8_t DEFAULT_WARN_SEC = 20;

// UI timing
static const unsigned long DEBOUNCE_MS = 30;
static const unsigned long BOOT_HOLD_WARN_MS = 2000;
static const unsigned long DISPLAY_PERIOD_MS = 200;

// Beep/Alarm settings
static const uint16_t WARN_SPEAKER_FREQ_HZ = 2400;
static const unsigned long WARN_PERIOD_START_MS = 700;
static const unsigned long WARN_PERIOD_END_MS = 120;
static const uint8_t WARN_DUTY_START_PCT = 35;
static const uint8_t WARN_DUTY_END_PCT = 92;
static const unsigned long ALARM_RAMP_MS = 30000;
static const unsigned long ALARM_MAX_MS = 300000;

struct Config {
  uint8_t magic;
  uint8_t interval_min;
  uint8_t warn_sec;
  uint8_t checksum;
};

static const uint8_t CONFIG_MAGIC = 0xA5;

enum TimerState {
  STATE_DISARMED = 0,
  STATE_SET_WARN,
  STATE_ARMED,
  STATE_WARNING,
  STATE_ALARM
};

struct ButtonState {
  bool stable = false;
  bool last_read = false;
  unsigned long last_change_ms = 0;
  bool pressed = false;
  bool released = false;
  unsigned long down_since_ms = 0;
};

Config config;
TimerState state = STATE_DISARMED;
ButtonState button;

bool display_ok = false;
unsigned long last_display_ms = 0;

unsigned long interval_ms = 0;
unsigned long remaining_ms = 0;
unsigned long last_tick_ms = 0;
unsigned long alarm_start_ms = 0;

unsigned long adjust_hold_start_ms = 0;
unsigned long next_adjust_ms = 0;

bool speaker_on = false;
unsigned long speaker_cycle_start_ms = 0;
unsigned long speaker_off_ms = 0;

bool relay_on = false;
unsigned long relay_cycle_start_ms = 0;
unsigned long relay_off_ms = 0;

uint16_t rotary_filtered = 0;
bool rotary_initialized = false;
uint8_t interval_min_runtime = DEFAULT_INTERVAL_MIN;

uint8_t checksumFor(const Config &cfg) {
  return (uint8_t)(cfg.magic ^ cfg.interval_min ^ cfg.warn_sec ^ 0x5A);
}

void saveConfig() {
  config.magic = CONFIG_MAGIC;
  config.checksum = checksumFor(config);
  EEPROM.put(0, config);
}

void loadConfig() {
  EEPROM.get(0, config);
  bool valid = config.magic == CONFIG_MAGIC && config.checksum == checksumFor(config);
  if (!valid || config.interval_min < INTERVAL_MIN_MIN || config.interval_min > INTERVAL_MIN_MAX ||
      config.warn_sec < WARN_SEC_MIN || config.warn_sec > WARN_SEC_MAX) {
    config.interval_min = DEFAULT_INTERVAL_MIN;
    config.warn_sec = DEFAULT_WARN_SEC;
    saveConfig();
  }
}

void setRelayPin(uint8_t pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

void setRelay(bool on) {
  relay_on = on;
  setRelayPin(RELAY_PIN, on);
}

void stopOutputs() {
  if (speaker_on) {
    noTone(SPEAKER_PIN);
    speaker_on = false;
  }
  setRelay(false);
  setRelayPin(RELAY2_PIN, false);
}

bool isArmSwitchOn() {
  return digitalRead(ARM_PIN) == LOW;
}

bool isButtonPressed() {
  bool pin_low = (digitalRead(BUTTON_PIN) == LOW);
  return BUTTON_ACTIVE_LOW ? pin_low : !pin_low;
}

void updateButton(unsigned long now) {
  button.pressed = false;
  button.released = false;

  bool raw = isButtonPressed();
  if (raw != button.last_read) {
    button.last_read = raw;
    button.last_change_ms = now;
  }

  if ((now - button.last_change_ms) > DEBOUNCE_MS && raw != button.stable) {
    button.stable = raw;
    if (button.stable) {
      button.pressed = true;
      button.down_since_ms = now;
    } else {
      button.released = true;
    }
  }
}

void startAdjust() {
  adjust_hold_start_ms = millis();
  next_adjust_ms = adjust_hold_start_ms;
}

unsigned long adjustStepIntervalMs(unsigned long held_ms) {
  if (held_ms < 2000) {
    return 600;
  }
  if (held_ms < 5000) {
    return 300;
  }
  return 120;
}

void updateAdjustWarn(unsigned long now) {
  if (now < next_adjust_ms) {
    return;
  }
  uint8_t next = config.warn_sec + 1;
  if (next > WARN_SEC_MAX) {
    next = WARN_SEC_MIN;
  }
  config.warn_sec = next;
  unsigned long held_ms = now - adjust_hold_start_ms;
  next_adjust_ms = now + adjustStepIntervalMs(held_ms);
}

uint8_t clampIntervalMinutes(uint8_t value) {
  if (value < INTERVAL_MIN_MIN) {
    return INTERVAL_MIN_MIN;
  }
  if (value > INTERVAL_MIN_MAX) {
    return INTERVAL_MIN_MAX;
  }
  return value;
}

uint8_t mapRotaryToMinutes(uint16_t reading) {
  uint16_t span = INTERVAL_MIN_MAX - INTERVAL_MIN_MIN;
  uint16_t scaled = (reading * span) / 1023U;
  return clampIntervalMinutes((uint8_t)(INTERVAL_MIN_MIN + scaled));
}

void updateIntervalFromRotary() {
  uint16_t reading = (uint16_t)analogRead(ROTARY_PIN);
  if (!rotary_initialized) {
    rotary_filtered = reading;
    rotary_initialized = true;
  } else {
    rotary_filtered = (uint16_t)((rotary_filtered * 7U + reading) / 8U);
  }
  interval_min_runtime = mapRotaryToMinutes(rotary_filtered);
  config.interval_min = interval_min_runtime;
}

void enterDisarmed() {
  state = STATE_DISARMED;
  stopOutputs();
}

void enterArmed(unsigned long now) {
  interval_ms = (unsigned long)interval_min_runtime * 60UL * 1000UL;
  remaining_ms = interval_ms;
  last_tick_ms = now;
  state = (config.warn_sec > 0) ? STATE_ARMED : STATE_ARMED;
  stopOutputs();
}

void enterAlarm(unsigned long now) {
  state = STATE_ALARM;
  alarm_start_ms = now;
  relay_cycle_start_ms = now;
  relay_off_ms = now;
  setRelay(true);
}

void resetCountdown(unsigned long now) {
  interval_ms = (unsigned long)interval_min_runtime * 60UL * 1000UL;
  remaining_ms = interval_ms;
  last_tick_ms = now;
}

void updateCountdown(unsigned long now) {
  unsigned long delta = now - last_tick_ms;
  if (delta == 0) {
    return;
  }
  last_tick_ms = now;

  if (delta >= remaining_ms) {
    remaining_ms = 0;
  } else {
    remaining_ms -= delta;
  }

  if (remaining_ms == 0) {
    enterAlarm(now);
  } else if (config.warn_sec > 0 && remaining_ms <= (unsigned long)config.warn_sec * 1000UL) {
    state = STATE_WARNING;
  } else {
    state = STATE_ARMED;
  }
}

void updateWarningSpeaker(unsigned long now) {
  if (config.warn_sec == 0) {
    if (speaker_on) {
      noTone(SPEAKER_PIN);
      speaker_on = false;
    }
    return;
  }

  unsigned long warn_window_ms = (unsigned long)config.warn_sec * 1000UL;
  unsigned long elapsed = warn_window_ms - remaining_ms;
  if (elapsed > warn_window_ms) {
    elapsed = warn_window_ms;
  }

  unsigned long period_span = WARN_PERIOD_START_MS - WARN_PERIOD_END_MS;
  unsigned long period = WARN_PERIOD_START_MS - (elapsed * period_span) / warn_window_ms;
  uint8_t duty = WARN_DUTY_START_PCT + (uint8_t)((elapsed * (WARN_DUTY_END_PCT - WARN_DUTY_START_PCT)) / warn_window_ms);
  unsigned long on_time = (period * duty) / 100UL;
  if (on_time >= period && period > 5) {
    on_time = period - 5;
  }

  if (now - speaker_cycle_start_ms >= period) {
    speaker_cycle_start_ms = now;
    speaker_off_ms = now + on_time;
    tone(SPEAKER_PIN, WARN_SPEAKER_FREQ_HZ);
    speaker_on = true;
  }

  if (speaker_on && now >= speaker_off_ms) {
    noTone(SPEAKER_PIN);
    speaker_on = false;
  }
}

void updateAlarmRelay(unsigned long now) {
  unsigned long elapsed = now - alarm_start_ms;
  if (elapsed >= ALARM_MAX_MS) {
    if (relay_on) {
      setRelay(false);
    }
    return;
  }

  if (elapsed >= ALARM_RAMP_MS) {
    if (!relay_on) {
      setRelay(true);
    }
    return;
  }

  unsigned long period = 1200 - (elapsed * 800UL) / ALARM_RAMP_MS; // 1200 -> 400 ms
  unsigned long on_time = period / 2;

  if (now - relay_cycle_start_ms >= period) {
    relay_cycle_start_ms = now;
    relay_off_ms = now + on_time;
    setRelay(true);
  }

  if (relay_on && now >= relay_off_ms) {
    setRelay(false);
  }
}

void formatTime(unsigned long ms, char *out, size_t out_len) {
  unsigned long total_seconds = ms / 1000UL;
  unsigned long minutes = total_seconds / 60UL;
  unsigned long seconds = total_seconds % 60UL;
  if (minutes > 99) {
    minutes = 99;
  }
  snprintf(out, out_len, "%02lu:%02lu", minutes, seconds);
}

void drawDisplay(unsigned long now) {
  if (!display_ok) {
    return;
  }
  if (now - last_display_ms < DISPLAY_PERIOD_MS) {
    return;
  }
  last_display_ms = now;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  char time_buf[8] = {0};

  switch (state) {
    case STATE_DISARMED:
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("DISARMED");
      display.setCursor(0, 16);
      display.print("Interval:");
      formatTime((unsigned long)interval_min_runtime * 60UL * 1000UL, time_buf, sizeof(time_buf));
      display.setTextSize(2);
      display.setCursor(0, 28);
      display.print(time_buf);
      display.setTextSize(1);
      display.setCursor(0, 52);
      display.print("Warn: ");
      display.print(config.warn_sec);
      display.print("s");
      break;
    case STATE_SET_WARN:
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("SET WARN");
      display.setTextSize(3);
      display.setCursor(0, 24);
      display.print(config.warn_sec);
      display.setTextSize(1);
      display.setCursor(60, 44);
      display.print("sec");
      display.setCursor(0, 56);
      display.print("Release to save");
      break;
    case STATE_ARMED:
    case STATE_WARNING:
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(state == STATE_WARNING ? "WARNING" : "ARMED");
      formatTime(remaining_ms, time_buf, sizeof(time_buf));
      display.setTextSize(3);
      display.setCursor(0, 24);
      display.print(time_buf);
      display.setTextSize(1);
      display.setCursor(0, 56);
      display.print("Tap to reset");
      break;
    case STATE_ALARM:
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("ALARM");
      display.setTextSize(2);
      display.setCursor(0, 24);
      display.print("TIME UP");
      display.setTextSize(1);
      display.setCursor(0, 48);
      display.print("Tap: restart");
      display.setCursor(0, 56);
      display.print("Switch: disarm");
      break;
  }

  display.display();
}

void setup() {
  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  pinMode(ARM_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  setRelay(false);
  setRelayPin(RELAY2_PIN, false);

  loadConfig();
  interval_min_runtime = clampIntervalMinutes(config.interval_min);

  display_ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (display_ok) {
    display.clearDisplay();
    display.display();
  }

  bool held_for_boot_window = isButtonPressed();
  unsigned long boot_start = millis();
  while (held_for_boot_window && (millis() - boot_start < BOOT_HOLD_WARN_MS)) {
    if (!isButtonPressed()) {
      held_for_boot_window = false;
      break;
    }
    delay(10);
  }

  if (held_for_boot_window) {
    state = STATE_SET_WARN;
    startAdjust();
  } else {
    enterDisarmed();
  }
}

void loop() {
  unsigned long now = millis();
  updateButton(now);

  bool arm_on = isArmSwitchOn();

  if (!arm_on) {
    if (state != STATE_DISARMED && state != STATE_SET_WARN) {
      enterDisarmed();
    }
  }

  switch (state) {
    case STATE_DISARMED: {
      updateIntervalFromRotary();
      if (arm_on) {
        enterArmed(now);
        break;
      }
      break;
    }
    case STATE_SET_WARN: {
      if (button.released) {
        saveConfig();
        enterDisarmed();
      } else if (button.stable) {
        updateAdjustWarn(now);
      }
      break;
    }
    case STATE_ARMED:
    case STATE_WARNING: {
      if (!arm_on) {
        enterDisarmed();
        break;
      }
      if (button.pressed) {
        resetCountdown(now);
      }
      updateCountdown(now);
      if (state == STATE_WARNING) {
        updateWarningSpeaker(now);
      } else {
        if (speaker_on) {
          noTone(SPEAKER_PIN);
          speaker_on = false;
        }
      }
      break;
    }
    case STATE_ALARM: {
      if (!arm_on) {
        enterDisarmed();
        break;
      }
      if (button.pressed) {
        enterArmed(now);
        break;
      }
      updateAlarmRelay(now);
      break;
    }
  }

  drawDisplay(now);
}
