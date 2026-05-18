#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>

#include "aa_timer_font.h"

// M5Stack Tough + Unit 2Relay on Port A.
// Port A pins on Tough are G32/G33. Unit 2Relay uses both signal lines.
static const uint8_t FOG_RELAY_PIN = 32;    // Unit 2Relay CH2: fog horn
static const uint8_t ALARM_RELAY_PIN = 33;  // Unit 2Relay CH1: alarm siren

static const bool RELAY_ACTIVE_LOW = false;

// Config limits
static const uint8_t INTERVAL_MIN_MIN = 1;
static const uint8_t INTERVAL_MIN_MAX = 30;
static const uint8_t WARN_SEC_MIN = 0;
static const uint8_t WARN_SEC_MAX = 60;
static const uint8_t SIGNAL_INTERVAL_INDEX_MAX = 2;

// Defaults
static const uint8_t DEFAULT_INTERVAL_MIN = 10;
static const uint8_t DEFAULT_WARN_SEC = 20;
static const uint8_t DEFAULT_SIGNAL_INTERVAL_INDEX = 2;

// Timing
static const unsigned long UI_PERIOD_MS = 100;
static const unsigned long HOLD_ACTION_MS = 1200;
static const unsigned long HOLD_INDICATOR_DELAY_MS = 250;
static const int16_t SWIPE_MIN_X_PX = 45;
static const int16_t SWIPE_MAX_Y_PX = 90;

// Beep/Alarm settings
static const uint16_t WARN_SPEAKER_FREQ_HZ = 2400;
static const unsigned long WARN_PERIOD_START_MS = 700;
static const unsigned long WARN_PERIOD_END_MS = 120;
static const uint8_t WARN_DUTY_START_PCT = 35;
static const uint8_t WARN_DUTY_END_PCT = 92;
static const unsigned long ALARM_RAMP_MS = 30000;
static const unsigned long ALARM_MAX_MS = 300000;

// Fog signal settings. COLREG-style intervals use at most 2 minutes between groups.
static const unsigned long SIGNAL_INTERVAL_OPTIONS_MS[] = {30000, 60000, 120000};
static const unsigned long FOG_PROLONGED_MS = 5000;
static const unsigned long FOG_SHORT_MS = 1000;
static const unsigned long FOG_GAP_MS = 2000;

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// i70-inspired instrument palette. Neutral colors are defined as RGB triples so
// they are auditable; the panel white is deliberately warm to avoid a cyan cast
// on the Tough LCD/backlight.
static const uint16_t COLOR_BG = RGB565(0, 0, 0);
static const uint16_t COLOR_WHITE = RGB565(255, 255, 255);
static const uint16_t COLOR_PANEL = RGB565(255, 252, 244);
static const uint16_t COLOR_BUTTON = RGB565(32, 32, 32);
static const uint16_t COLOR_BUTTON_DIM = RGB565(16, 16, 16);
static const uint16_t COLOR_BUTTON_HILITE = RGB565(80, 80, 80);
static const uint16_t COLOR_BUTTON_MID = RGB565(40, 40, 40);
static const uint16_t COLOR_BUTTON_SHADOW = RGB565(8, 8, 8);
static const uint16_t COLOR_TEXT = COLOR_WHITE;
static const uint16_t COLOR_TEXT_LIGHT = RGB565(190, 190, 190);
static const uint16_t COLOR_TEXT_DARK = COLOR_BG;
static const uint16_t COLOR_TEXT_MUTED = RGB565(110, 110, 110);
static const uint16_t COLOR_HORN_IDLE = RGB565(132, 132, 132);
static const uint16_t COLOR_LINE = RGB565(64, 64, 64);
static const uint16_t COLOR_RED = RGB565(255, 0, 0);
static const uint16_t COLOR_AMBER = RGB565(255, 165, 0);

struct Config {
  uint8_t interval_min;
  uint8_t warn_sec;
  uint8_t fog_pattern;
  uint8_t signal_interval_index;
};

enum TimerState {
  STATE_DISARMED = 0,
  STATE_ARMED,
  STATE_WARNING,
  STATE_ALARM
};

enum Screen {
  SCREEN_TIMER = 0,
  SCREEN_SIGNALS,
  SCREEN_TIMER_SETTINGS,
  SCREEN_SIGNALS_SETTINGS
};

enum HoldAction {
  HOLD_NONE = 0,
  HOLD_ARM,
  HOLD_DISARM,
  HOLD_SIGNAL_OFF,
  HOLD_SIGNAL_SAILING,
  HOLD_SIGNAL_POWER,
  HOLD_SIGNAL_STOPPED
};

enum FogPattern {
  FOG_SAILING = 0,
  FOG_POWER_MAKING_WAY,
  FOG_STOPPED,
  FOG_PATTERN_COUNT
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

Preferences prefs;
M5Canvas ui(&M5.Display);
Config config;
TimerState state = STATE_DISARMED;
Screen screen = SCREEN_TIMER;

unsigned long interval_ms = 0;
unsigned long remaining_ms = 0;
unsigned long last_tick_ms = 0;
unsigned long alarm_start_ms = 0;

bool speaker_on = false;
unsigned long speaker_cycle_start_ms = 0;
unsigned long speaker_off_ms = 0;

bool alarm_relay_on = false;
bool fog_relay_on = false;
unsigned long alarm_cycle_start_ms = 0;
unsigned long alarm_off_ms = 0;

bool fog_auto_enabled = false;
bool fog_group_active = false;
uint8_t fog_step = 0;
unsigned long fog_next_ms = 0;
bool manual_horn_armed = false;

HoldAction hold_action = HOLD_NONE;
unsigned long hold_start_ms = 0;
bool touch_started_on_hold_control = false;

unsigned long last_ui_ms = 0;
bool force_redraw = true;

static const Rect HEADER_GEAR = {252, 4, 64, 28};
static const Rect HEADER_DONE = {246, 4, 70, 28};

static const Rect TIMER_PRIMARY_FULL = {0, 190, 320, 50};
static const Rect TIMER_PRIMARY = {0, 190, 160, 50};
static const Rect TIMER_SECONDARY = {160, 190, 160, 50};

static const Rect SETTINGS_INTERVAL_MINUS = {18, 84, 54, 42};
static const Rect SETTINGS_INTERVAL_PLUS = {226, 84, 54, 42};
static const Rect SETTINGS_WARN_MINUS = {18, 156, 54, 42};
static const Rect SETTINGS_WARN_PLUS = {226, 156, 54, 42};
static const Rect SETTINGS_SIGNAL_30 = {18, 128, 88, 54};
static const Rect SETTINGS_SIGNAL_60 = {116, 128, 88, 54};
static const Rect SETTINGS_SIGNAL_120 = {214, 128, 88, 54};

static const Rect SIGNALS_MANUAL = {0, 190, 320, 50};
static const Rect SIGNALS_HORN_IMAGE = {0, 36, 106, 154};
static const Rect SIGNALS_OFF = {116, 46, 88, 60};
static const Rect SIGNALS_SAILING = {216, 46, 88, 60};
static const Rect SIGNALS_POWER = {116, 120, 88, 60};
static const Rect SIGNALS_STOPPED = {216, 120, 88, 60};
static const Rect OUTPUT_ALARM_ICON = {200, 8, 22, 18};
static const Rect OUTPUT_HORN_ICON = {226, 8, 22, 18};

uint8_t clampU8(uint8_t value, uint8_t min_value, uint8_t max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

void saveConfig() {
  prefs.putUChar("interval", config.interval_min);
  prefs.putUChar("warn", config.warn_sec);
  prefs.putUChar("fog", config.fog_pattern);
  prefs.putUChar("sigint", config.signal_interval_index);
}

void loadConfig() {
  config.interval_min = prefs.getUChar("interval", DEFAULT_INTERVAL_MIN);
  config.warn_sec = prefs.getUChar("warn", DEFAULT_WARN_SEC);
  config.fog_pattern = prefs.getUChar("fog", FOG_SAILING);
  config.signal_interval_index = prefs.getUChar("sigint", DEFAULT_SIGNAL_INTERVAL_INDEX);
  config.interval_min = clampU8(config.interval_min, INTERVAL_MIN_MIN, INTERVAL_MIN_MAX);
  config.warn_sec = clampU8(config.warn_sec, WARN_SEC_MIN, WARN_SEC_MAX);
  config.signal_interval_index = clampU8(config.signal_interval_index, 0, SIGNAL_INTERVAL_INDEX_MAX);
  if (config.fog_pattern >= FOG_PATTERN_COUNT) {
    config.fog_pattern = FOG_SAILING;
  }
}

void setRelayPin(uint8_t pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

void setAlarmRelay(bool on) {
  alarm_relay_on = on;
  setRelayPin(ALARM_RELAY_PIN, on);
}

void setFogRelay(bool on) {
  fog_relay_on = on;
  setRelayPin(FOG_RELAY_PIN, on);
}

void stopSpeaker() {
  if (speaker_on) {
    M5.Speaker.stop();
    speaker_on = false;
  }
}

void stopAlarmOutput() {
  stopSpeaker();
  setAlarmRelay(false);
}

void stopFogOutput() {
  setFogRelay(false);
  manual_horn_armed = false;
  fog_group_active = false;
  fog_step = 0;
}

void stopAllOutputs() {
  stopAlarmOutput();
  stopFogOutput();
}

void formatTime(unsigned long ms, char *buf, size_t len) {
  unsigned long total_sec = (ms + 999UL) / 1000UL;
  unsigned int minutes = total_sec / 60UL;
  unsigned int seconds = total_sec % 60UL;
  snprintf(buf, len, "%02u:%02u", minutes, seconds);
}

void enterDisarmed() {
  state = STATE_DISARMED;
  stopAlarmOutput();
  force_redraw = true;
}

void enterArmed(unsigned long now) {
  interval_ms = (unsigned long)config.interval_min * 60UL * 1000UL;
  remaining_ms = interval_ms;
  last_tick_ms = now;
  state = STATE_ARMED;
  stopAlarmOutput();
  force_redraw = true;
}

void enterAlarm(unsigned long now) {
  state = STATE_ALARM;
  alarm_start_ms = now;
  alarm_cycle_start_ms = now;
  alarm_off_ms = now;
  stopSpeaker();
  setAlarmRelay(true);
  force_redraw = true;
}

void resetCountdown(unsigned long now) {
  interval_ms = (unsigned long)config.interval_min * 60UL * 1000UL;
  remaining_ms = interval_ms;
  last_tick_ms = now;
  state = STATE_ARMED;
  stopAlarmOutput();
  force_redraw = true;
}

void updateCountdown(unsigned long now) {
  if (now <= last_tick_ms) {
    return;
  }

  unsigned long delta = now - last_tick_ms;
  last_tick_ms = now;

  if (delta >= remaining_ms) {
    remaining_ms = 0;
    enterAlarm(now);
    return;
  }

  remaining_ms -= delta;
  if (config.warn_sec > 0 && remaining_ms <= (unsigned long)config.warn_sec * 1000UL) {
    state = STATE_WARNING;
  } else {
    state = STATE_ARMED;
  }
}

void updateWarningSpeaker(unsigned long now) {
  if (config.warn_sec == 0) {
    stopSpeaker();
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

  if (speaker_on && now >= speaker_off_ms) {
    M5.Speaker.stop();
    speaker_on = false;
  }

  if (now - speaker_cycle_start_ms >= period) {
    speaker_cycle_start_ms = now;
    speaker_off_ms = now + on_time;
    M5.Speaker.tone(WARN_SPEAKER_FREQ_HZ, on_time);
    speaker_on = true;
  }
}

void updateAlarmRelay(unsigned long now) {
  unsigned long elapsed = now - alarm_start_ms;
  if (elapsed >= ALARM_MAX_MS) {
    if (alarm_relay_on) {
      setAlarmRelay(false);
      force_redraw = true;
    }
    return;
  }

  if (elapsed >= ALARM_RAMP_MS) {
    if (!alarm_relay_on) {
      setAlarmRelay(true);
      force_redraw = true;
    }
    return;
  }

  unsigned long period = 1000UL - (elapsed * 750UL) / ALARM_RAMP_MS;
  unsigned long on_time = period / 2UL;

  if (now - alarm_cycle_start_ms >= period) {
    alarm_cycle_start_ms = now;
    alarm_off_ms = now + on_time;
    setAlarmRelay(true);
  }

  if (alarm_relay_on && now >= alarm_off_ms) {
    setAlarmRelay(false);
  }
}

const char *fogPatternName(uint8_t pattern) {
  switch (pattern) {
    case FOG_SAILING:
      return "Sailing";
    case FOG_POWER_MAKING_WAY:
      return "Power";
    case FOG_STOPPED:
      return "Stopped";
    default:
      return "Sailing";
  }
}

unsigned long signalGroupIntervalMs() {
  if (config.fog_pattern == FOG_STOPPED) {
    return SIGNAL_INTERVAL_OPTIONS_MS[SIGNAL_INTERVAL_INDEX_MAX];
  }
  return SIGNAL_INTERVAL_OPTIONS_MS[config.signal_interval_index];
}

uint8_t fogPatternStepCount(uint8_t pattern) {
  switch (pattern) {
    case FOG_SAILING:
      return 5;  // prolonged, gap, short, gap, short
    case FOG_POWER_MAKING_WAY:
      return 1;  // prolonged
    case FOG_STOPPED:
      return 3;  // prolonged, gap, prolonged
    default:
      return 1;
  }
}

bool fogStepIsBlast(uint8_t pattern, uint8_t step) {
  if (pattern == FOG_POWER_MAKING_WAY) {
    return step == 0;
  }
  if (pattern == FOG_STOPPED) {
    return step == 0 || step == 2;
  }
  return step == 0 || step == 2 || step == 4;
}

unsigned long fogStepDuration(uint8_t pattern, uint8_t step) {
  if (!fogStepIsBlast(pattern, step)) {
    return FOG_GAP_MS;
  }
  if (pattern == FOG_SAILING && (step == 2 || step == 4)) {
    return FOG_SHORT_MS;
  }
  return FOG_PROLONGED_MS;
}

void startFogGroup(unsigned long now) {
  fog_group_active = true;
  fog_step = 0;
  setFogRelay(fogStepIsBlast(config.fog_pattern, fog_step));
  fog_next_ms = now + fogStepDuration(config.fog_pattern, fog_step);
  force_redraw = true;
}

void updateFog(unsigned long now, bool touch_holding_manual) {
  if (state == STATE_ALARM) {
    stopFogOutput();
    return;
  }

  if (touch_holding_manual) {
    if (!fog_relay_on) {
      setFogRelay(true);
      force_redraw = true;
    }
    fog_group_active = false;
    return;
  }

  if (!fog_auto_enabled) {
    if (fog_relay_on) {
      setFogRelay(false);
      force_redraw = true;
    }
    fog_group_active = false;
    return;
  }

  if (!fog_group_active && now >= fog_next_ms) {
    startFogGroup(now);
    return;
  }

  if (!fog_group_active || now < fog_next_ms) {
    return;
  }

  fog_step++;
  if (fog_step >= fogPatternStepCount(config.fog_pattern)) {
    fog_group_active = false;
    setFogRelay(false);
    fog_next_ms = now + signalGroupIntervalMs();
    force_redraw = true;
    return;
  }

  setFogRelay(fogStepIsBlast(config.fog_pattern, fog_step));
  fog_next_ms = now + fogStepDuration(config.fog_pattern, fog_step);
  force_redraw = true;
}

bool rectContains(const Rect &r, int16_t x, int16_t y) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void useSmallFont() {
  ui.setFont(&fonts::Font2);
  ui.setTextSize(1);
}

void useMediumFont() {
  ui.setFont(&fonts::Font4);
  ui.setTextSize(1);
}

void useHeaderFont() {
  ui.setFont(&fonts::Font2);
  ui.setTextSize(1);
}

void useLargeNumberFont() {
  ui.setFont(&fonts::Font7);
  ui.setTextSize(1);
}

void useTimeFont() {
  ui.setFont(&fonts::Font4);
  ui.setTextSize(2);
}

uint16_t blendBlackOver(uint16_t bg, uint8_t alpha4) {
  if (alpha4 == 0) {
    return bg;
  }
  if (alpha4 >= 15) {
    return COLOR_TEXT_DARK;
  }

  uint8_t keep = 15 - alpha4;
  uint8_t r = ((bg >> 11) & 0x1F) * keep / 15;
  uint8_t g = ((bg >> 5) & 0x3F) * keep / 15;
  uint8_t b = (bg & 0x1F) * keep / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void drawAATimerGlyph(const AATimerGlyph *glyph, int16_t x, int16_t y) {
  if (glyph == nullptr) {
    return;
  }

  uint16_t pixel_index = 0;
  for (uint8_t row = 0; row < glyph->height; row++) {
    for (uint8_t col = 0; col < glyph->width; col++, pixel_index++) {
      uint8_t packed = pgm_read_byte(glyph->data + pixel_index / 2);
      uint8_t alpha4 = (pixel_index & 1) == 0 ? packed >> 4 : packed & 0x0F;
      if (alpha4 != 0) {
        ui.drawPixel(x + col, y + row, blendBlackOver(COLOR_PANEL, alpha4));
      }
    }
  }
}

int16_t aaTimerTextWidth(const char *text) {
  int16_t width = 0;
  while (*text != '\0') {
    const AATimerGlyph *glyph = aaTimerGlyph(*text++);
    if (glyph != nullptr) {
      width += glyph->advance;
    }
  }
  return width;
}

void drawAATimerTextCentered(const char *text, int16_t center_x, int16_t top_y) {
  int16_t x = center_x - aaTimerTextWidth(text) / 2;
  while (*text != '\0') {
    const AATimerGlyph *glyph = aaTimerGlyph(*text++);
    if (glyph != nullptr) {
      drawAATimerGlyph(glyph, x, top_y);
      x += glyph->advance;
    }
  }
}

void drawButton(const Rect &r, const char *label, uint32_t fill, uint32_t text, uint32_t outline = COLOR_LINE) {
  useSmallFont();
  uint32_t body = fill == COLOR_BUTTON ? COLOR_BUTTON_MID : fill;
  uint32_t top = fill == COLOR_BUTTON ? COLOR_BUTTON_HILITE : fill;
  uint32_t lower = fill == COLOR_BUTTON ? COLOR_BUTTON : fill;
  uint32_t shadow = fill == COLOR_BUTTON ? COLOR_BUTTON_SHADOW : COLOR_BG;

  ui.fillRoundRect(r.x, r.y, r.w, r.h, 9, shadow);
  ui.fillRoundRect(r.x + 2, r.y + 1, r.w - 4, r.h - 3, 8, body);
  ui.fillRoundRect(r.x + 3, r.y + 2, r.w - 6, 12, 6, top);
  ui.fillRect(r.x + 3, r.y + r.h / 2, r.w - 6, r.h / 2 - 4, lower);
  ui.drawRoundRect(r.x, r.y, r.w, r.h, 9, outline);
  ui.drawFastHLine(r.x + 8, r.y + 2, r.w - 16, COLOR_TEXT_MUTED);
  ui.drawFastHLine(r.x + 8, r.y + r.h - 3, r.w - 16, COLOR_BG);
  ui.drawFastVLine(r.x + 1, r.y + 8, r.h - 16, COLOR_LINE);
  ui.drawFastVLine(r.x + r.w - 2, r.y + 8, r.h - 16, COLOR_BG);
  ui.setTextColor(text);
  ui.setTextDatum(middle_center);
  ui.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
  ui.setTextDatum(top_left);
}

void drawBottomButton(const Rect &r, const char *label, bool left_round, bool right_round, uint32_t fill = COLOR_BUTTON) {
  useSmallFont();
  uint32_t body = fill == COLOR_BUTTON ? COLOR_BUTTON_MID : fill;
  uint32_t top = fill == COLOR_BUTTON ? COLOR_TEXT_MUTED : COLOR_BUTTON_HILITE;
  uint32_t lower = fill == COLOR_BUTTON ? COLOR_BUTTON_SHADOW : body;
  int16_t radius = 15;

  if (left_round && right_round) {
    ui.fillRoundRect(r.x, r.y, r.w, r.h, radius, body);
  } else if (left_round) {
    ui.fillRoundRect(r.x, r.y, radius * 2, r.h, radius, body);
    ui.fillRect(r.x + radius, r.y, r.w - radius, r.h, body);
  } else if (right_round) {
    ui.fillRoundRect(r.x + r.w - radius * 2, r.y, radius * 2, r.h, radius, body);
    ui.fillRect(r.x, r.y, r.w - radius, r.h, body);
  } else {
    ui.fillRect(r.x, r.y, r.w, r.h, body);
  }

  int16_t inset_left = left_round ? radius : 0;
  int16_t inset_right = right_round ? radius : 0;
  ui.drawFastHLine(r.x + inset_left, r.y + 2, r.w - inset_left - inset_right, top);
  ui.drawFastHLine(r.x + inset_left, r.y + r.h - 2, r.w - inset_left - inset_right, lower);
  if (!left_round) {
    ui.drawFastVLine(r.x, r.y + 3, r.h - 6, COLOR_BG);
  }
  if (!right_round) {
    ui.drawFastVLine(r.x + r.w - 1, r.y + 3, r.h - 6, COLOR_BG);
  }
  ui.setTextColor(COLOR_TEXT);
  ui.setTextDatum(middle_center);
  ui.drawString(label, r.x + r.w / 2, r.y + r.h / 2 + 2);
  ui.setTextDatum(top_left);
}

void drawPageDots(bool timer_active) {
  ui.fillCircle(145, 18, 4, timer_active ? COLOR_TEXT : COLOR_LINE);
  ui.drawCircle(145, 18, 4, COLOR_LINE);
  ui.fillCircle(161, 18, 4, timer_active ? COLOR_LINE : COLOR_TEXT);
  ui.drawCircle(161, 18, 4, COLOR_LINE);
}

void drawModeHeader(const char *title, bool timer_active) {
  ui.fillRect(0, 0, 320, 36, COLOR_BG);
  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.setTextDatum(middle_left);
  useHeaderFont();
  ui.drawString(title, 8, 18);
  ui.setTextDatum(top_left);
  drawPageDots(timer_active);
  drawButton(HEADER_GEAR, "Config", COLOR_BUTTON_DIM, COLOR_TEXT, COLOR_LINE);
}

void drawSettingsHeader(const char *title) {
  ui.fillRect(0, 0, 320, 36, COLOR_BG);
  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.setTextDatum(middle_left);
  useMediumFont();
  ui.drawString(title, 8, 18);
  ui.setTextDatum(top_left);
  drawButton(HEADER_DONE, "Done", COLOR_BUTTON, COLOR_TEXT, COLOR_LINE);
}

void drawTimerScreen(unsigned long now) {
  drawModeHeader("Timer", true);

  char time_buf[8] = {0};
  formatTime(state == STATE_DISARMED ? (unsigned long)config.interval_min * 60UL * 1000UL : remaining_ms, time_buf, sizeof(time_buf));

  ui.fillRect(0, 36, 320, 154, COLOR_PANEL);
  ui.drawFastHLine(0, 36, 320, COLOR_LINE);
  ui.drawFastHLine(0, 189, 320, COLOR_LINE);
  ui.setTextColor(COLOR_TEXT, COLOR_PANEL);
  ui.setTextDatum(top_center);
  useMediumFont();
  ui.setTextColor(COLOR_TEXT_DARK, COLOR_PANEL);
  ui.drawString(state == STATE_ALARM ? "ALARM" : state == STATE_WARNING ? "WARNING" : state == STATE_ARMED ? "ARMED" : "DISARMED", 160, 46);
  if (state == STATE_ALARM) {
    useTimeFont();
    ui.drawString("TIME UP", 160, 78);
  } else {
    drawAATimerTextCentered(time_buf, 160, 74);
  }
  useSmallFont();
  ui.setTextColor(COLOR_TEXT_MUTED, COLOR_PANEL);
  ui.drawString(String("Warn ") + config.warn_sec + "s", 160, 160);
  ui.setTextDatum(top_left);

  if (state == STATE_DISARMED) {
    drawBottomButton(TIMER_PRIMARY_FULL, "Hold to Arm", true, true);
  } else if (state == STATE_ALARM) {
    drawBottomButton(TIMER_PRIMARY, "Ack", true, false);
    drawBottomButton(TIMER_SECONDARY, "Hold Disarm", false, true);
  } else {
    drawBottomButton(TIMER_PRIMARY, "Reset", true, false);
    drawBottomButton(TIMER_SECONDARY, "Hold Disarm", false, true);
  }
}

void drawTimerSettingsScreen() {
  drawSettingsHeader("Timer Settings");

  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.setTextDatum(top_center);
  useMediumFont();
  ui.drawString("Interval", 150, 78);
  useMediumFont();
  ui.drawString(String(config.interval_min) + " min", 150, 104);

  useMediumFont();
  ui.drawString("Warning", 150, 150);
  useMediumFont();
  ui.drawString(String(config.warn_sec) + " sec", 150, 176);
  ui.setTextDatum(top_left);

  drawButton(SETTINGS_INTERVAL_MINUS, "-", COLOR_BUTTON, COLOR_TEXT);
  drawButton(SETTINGS_INTERVAL_PLUS, "+", COLOR_BUTTON, COLOR_TEXT);
  drawButton(SETTINGS_WARN_MINUS, "-", COLOR_BUTTON, COLOR_TEXT);
  drawButton(SETTINGS_WARN_PLUS, "+", COLOR_BUTTON, COLOR_TEXT);
}

uint32_t signalIntervalButtonColor(uint8_t index) {
  return config.signal_interval_index == index ? COLOR_AMBER : COLOR_BUTTON;
}

void drawSignalSettingsScreen() {
  drawSettingsHeader("Signals Settings");

  ui.setTextColor(COLOR_TEXT, COLOR_BG);
  ui.setTextDatum(top_center);
  useMediumFont();
  ui.drawString("Underway interval", 160, 90);
  useSmallFont();
  ui.drawString("Sailing + Power", 160, 114);
  ui.setTextDatum(top_left);

  drawButton(SETTINGS_SIGNAL_30, "30s", signalIntervalButtonColor(0), COLOR_TEXT, config.signal_interval_index == 0 ? COLOR_TEXT : COLOR_LINE);
  drawButton(SETTINGS_SIGNAL_60, "60s", signalIntervalButtonColor(1), COLOR_TEXT, config.signal_interval_index == 1 ? COLOR_TEXT : COLOR_LINE);
  drawButton(SETTINGS_SIGNAL_120, "120s", signalIntervalButtonColor(2), COLOR_TEXT, config.signal_interval_index == 2 ? COLOR_TEXT : COLOR_LINE);
}

uint32_t signalButtonColor(uint8_t pattern) {
  return fog_auto_enabled && config.fog_pattern == pattern ? COLOR_AMBER : COLOR_BUTTON;
}

uint32_t signalOffButtonColor() {
  return fog_auto_enabled ? COLOR_BUTTON : COLOR_AMBER;
}

void drawHornImage(bool active) {
  uint16_t color = active ? COLOR_TEXT_DARK : COLOR_HORN_IDLE;
  uint16_t bg = COLOR_PANEL;
  int16_t x = SIGNALS_HORN_IMAGE.x;
  int16_t y = SIGNALS_HORN_IMAGE.y;

  ui.fillRoundRect(x + 12, y + 54, 16, 48, 5, color);
  ui.fillRoundRect(x + 25, y + 64, 18, 28, 4, color);
  ui.fillRect(x + 38, y + 70, 12, 16, color);
  ui.fillTriangle(x + 48, y + 58, x + 84, y + 34, x + 84, y + 112, color);
  ui.fillTriangle(x + 48, y + 86, x + 84, y + 34, x + 84, y + 112, color);
  ui.fillRoundRect(x + 78, y + 30, 14, 86, 6, color);
  ui.fillRoundRect(x + 83, y + 40, 5, 66, 3, bg);
  ui.fillCircle(x + 20, y + 66, 3, bg);
  ui.fillCircle(x + 20, y + 90, 3, bg);
  ui.drawLine(x + 93, y + 52, x + 102, y + 44, color);
  ui.drawLine(x + 95, y + 73, x + 105, y + 73, color);
  ui.drawLine(x + 93, y + 94, x + 102, y + 102, color);
  ui.drawRect(x + 11, y + 53, 18, 50, COLOR_TEXT_DARK);
  ui.drawLine(x + 48, y + 58, x + 84, y + 34, COLOR_TEXT_DARK);
  ui.drawLine(x + 48, y + 86, x + 84, y + 112, COLOR_TEXT_DARK);
  ui.drawRoundRect(x + 78, y + 30, 14, 86, 6, COLOR_TEXT_DARK);
}

void drawSignalAutoButton(const Rect &r, const char *label, bool selected) {
  uint32_t fill = COLOR_PANEL;
  uint32_t text = COLOR_TEXT_DARK;

  ui.fillRect(r.x, r.y, r.w, r.h, fill);
  ui.drawRect(r.x, r.y, r.w, r.h, COLOR_TEXT_DARK);
  if (selected) {
    ui.drawRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, COLOR_TEXT_DARK);
    ui.fillRect(r.x + 5, r.y + 5, 9, 9, COLOR_TEXT_DARK);
  }
  ui.setTextColor(text, fill);
  ui.setTextDatum(middle_center);
  useSmallFont();
  ui.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
  ui.setTextDatum(top_left);
}

void drawOutputIcon(const Rect &r, const char *label, bool active, uint32_t active_color) {
  uint32_t fill = active ? active_color : COLOR_BUTTON_DIM;
  uint32_t outline = active ? COLOR_TEXT : COLOR_LINE;
  uint32_t text = active ? COLOR_TEXT_DARK : COLOR_TEXT_MUTED;

  ui.fillRect(r.x, r.y, r.w, r.h, fill);
  ui.drawRect(r.x, r.y, r.w, r.h, outline);
  useSmallFont();
  ui.setTextColor(text, fill);
  ui.setTextDatum(middle_center);
  ui.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
  ui.setTextDatum(top_left);
}

void drawOutputIndicators() {
  drawOutputIcon(OUTPUT_ALARM_ICON, "A", alarm_relay_on, COLOR_RED);
  drawOutputIcon(OUTPUT_HORN_ICON, "H", fog_relay_on, COLOR_AMBER);
}

void drawSignalsScreen(unsigned long now) {
  drawModeHeader("Signals", false);

  (void)now;
  ui.fillRect(0, 36, 320, 154, COLOR_PANEL);
  ui.drawFastHLine(0, 36, 320, COLOR_TEXT_DARK);
  drawHornImage(fog_relay_on);
  drawSignalAutoButton(SIGNALS_OFF, "Auto Off", !fog_auto_enabled);
  drawSignalAutoButton(SIGNALS_SAILING, "Sailing", fog_auto_enabled && config.fog_pattern == FOG_SAILING);
  drawSignalAutoButton(SIGNALS_POWER, "Powered", fog_auto_enabled && config.fog_pattern == FOG_POWER_MAKING_WAY);
  drawSignalAutoButton(SIGNALS_STOPPED, "Stopped", fog_auto_enabled && config.fog_pattern == FOG_STOPPED);
  ui.drawFastHLine(0, 189, 320, COLOR_TEXT_DARK);

  drawBottomButton(SIGNALS_MANUAL, fog_relay_on ? "Horn On" : "Horn", true, true, fog_relay_on ? COLOR_AMBER : COLOR_BUTTON);

}

void drawHoldProgress(unsigned long now) {
  if (hold_action == HOLD_NONE) {
    return;
  }

  unsigned long held_ms = now - hold_start_ms;
  if (held_ms < HOLD_INDICATOR_DELAY_MS) {
    return;
  }

  uint16_t progress = held_ms >= HOLD_ACTION_MS ? 320 : (uint16_t)((held_ms * 320UL) / HOLD_ACTION_MS);

  ui.fillRect(0, 0, 320, 34, COLOR_BUTTON_DIM);
  ui.fillRect(0, 0, progress, 34, COLOR_AMBER);
  ui.drawRect(0, 0, 320, 34, COLOR_TEXT);
  useMediumFont();
  ui.setTextColor(progress > 150 ? COLOR_TEXT_DARK : COLOR_TEXT, progress > 150 ? COLOR_AMBER : COLOR_BUTTON_DIM);
  ui.setTextDatum(middle_center);
  ui.drawString("HOLD", 160, 17);
  ui.setTextDatum(top_left);
}

void drawUi(unsigned long now) {
  if (!force_redraw && now - last_ui_ms < UI_PERIOD_MS) {
    return;
  }
  last_ui_ms = now;
  force_redraw = false;

  ui.fillScreen(COLOR_BG);

  switch (screen) {
    case SCREEN_TIMER:
      drawTimerScreen(now);
      break;
    case SCREEN_SIGNALS:
      drawSignalsScreen(now);
      break;
    case SCREEN_TIMER_SETTINGS:
      drawTimerSettingsScreen();
      break;
    case SCREEN_SIGNALS_SETTINGS:
      drawSignalSettingsScreen();
      break;
  }

  drawOutputIndicators();
  drawHoldProgress(now);
  ui.pushSprite(0, 0);
}

void startHold(HoldAction action, unsigned long now) {
  hold_action = action;
  hold_start_ms = now;
  force_redraw = true;
}

void clearHold() {
  if (hold_action != HOLD_NONE || manual_horn_armed) {
    hold_action = HOLD_NONE;
    manual_horn_armed = false;
    force_redraw = true;
  }
}

void switchScreen(Screen next_screen) {
  if (screen != next_screen) {
    screen = next_screen;
    force_redraw = true;
  }
}

bool switchScreenBySwipe(int16_t distance_x, int16_t distance_y) {
  if (abs(distance_x) < SWIPE_MIN_X_PX || abs(distance_y) > SWIPE_MAX_Y_PX) {
    return false;
  }

  if (screen == SCREEN_TIMER && distance_x < 0) {
    switchScreen(SCREEN_SIGNALS);
    return true;
  } else if (screen == SCREEN_SIGNALS && distance_x > 0) {
    switchScreen(SCREEN_TIMER);
    return true;
  }
  return false;
}

void selectFogPattern(uint8_t pattern, unsigned long now) {
  if (fog_auto_enabled && config.fog_pattern == pattern) {
    fog_auto_enabled = false;
    fog_group_active = false;
    setFogRelay(false);
    force_redraw = true;
    return;
  }

  config.fog_pattern = pattern;
  saveConfig();
  fog_auto_enabled = true;
  fog_group_active = false;
  setFogRelay(false);
  fog_next_ms = now;
  force_redraw = true;
}

void stopFogAuto() {
  fog_auto_enabled = false;
  fog_group_active = false;
  setFogRelay(false);
  force_redraw = true;
}

void applyHoldAction(unsigned long now) {
  switch (hold_action) {
    case HOLD_ARM:
      enterArmed(now);
      break;
    case HOLD_DISARM:
      enterDisarmed();
      break;
    case HOLD_SIGNAL_OFF:
      stopFogAuto();
      break;
    case HOLD_SIGNAL_SAILING:
      selectFogPattern(FOG_SAILING, now);
      break;
    case HOLD_SIGNAL_POWER:
      selectFogPattern(FOG_POWER_MAKING_WAY, now);
      break;
    case HOLD_SIGNAL_STOPPED:
      selectFogPattern(FOG_STOPPED, now);
      break;
    case HOLD_NONE:
      break;
  }
  hold_action = HOLD_NONE;
}

void handleTap(int16_t x, int16_t y, unsigned long now) {
  if (screen == SCREEN_TIMER) {
    if (rectContains(HEADER_GEAR, x, y)) {
      switchScreen(SCREEN_TIMER_SETTINGS);
      return;
    }
    if (state == STATE_ALARM && rectContains(TIMER_PRIMARY, x, y)) {
      enterArmed(now);
      return;
    }
    if ((state == STATE_ARMED || state == STATE_WARNING) && rectContains(TIMER_PRIMARY, x, y)) {
      resetCountdown(now);
      return;
    }
  }

  if (screen == SCREEN_SIGNALS) {
    if (rectContains(HEADER_GEAR, x, y)) {
      switchScreen(SCREEN_SIGNALS_SETTINGS);
      return;
    }
    return;
  }

  if (screen == SCREEN_TIMER_SETTINGS) {
    if (rectContains(HEADER_DONE, x, y)) {
      switchScreen(SCREEN_TIMER);
    } else if (rectContains(SETTINGS_INTERVAL_MINUS, x, y) && config.interval_min > INTERVAL_MIN_MIN) {
      config.interval_min--;
      saveConfig();
      force_redraw = true;
    } else if (rectContains(SETTINGS_INTERVAL_PLUS, x, y) && config.interval_min < INTERVAL_MIN_MAX) {
      config.interval_min++;
      saveConfig();
      force_redraw = true;
    } else if (rectContains(SETTINGS_WARN_MINUS, x, y) && config.warn_sec > WARN_SEC_MIN) {
      config.warn_sec--;
      saveConfig();
      force_redraw = true;
    } else if (rectContains(SETTINGS_WARN_PLUS, x, y) && config.warn_sec < WARN_SEC_MAX) {
      config.warn_sec++;
      saveConfig();
      force_redraw = true;
    }
    return;
  }

  if (screen == SCREEN_SIGNALS_SETTINGS) {
    if (rectContains(HEADER_DONE, x, y)) {
      switchScreen(SCREEN_SIGNALS);
    } else if (rectContains(SETTINGS_SIGNAL_30, x, y)) {
      config.signal_interval_index = 0;
      saveConfig();
      fog_next_ms = fog_auto_enabled && !fog_group_active ? now : fog_next_ms;
      force_redraw = true;
    } else if (rectContains(SETTINGS_SIGNAL_60, x, y)) {
      config.signal_interval_index = 1;
      saveConfig();
      fog_next_ms = fog_auto_enabled && !fog_group_active ? now : fog_next_ms;
      force_redraw = true;
    } else if (rectContains(SETTINGS_SIGNAL_120, x, y)) {
      config.signal_interval_index = 2;
      saveConfig();
      fog_next_ms = fog_auto_enabled && !fog_group_active ? now : fog_next_ms;
      force_redraw = true;
    }
    return;
  }
}

void handleTouch(unsigned long now) {
  auto touch = M5.Touch.getDetail();

  if (touch.wasPressed()) {
    int16_t x = touch.x;
    int16_t y = touch.y;
    touch_started_on_hold_control = false;

    if (state == STATE_WARNING) {
      resetCountdown(now);
      return;
    }
    if (state == STATE_ALARM) {
      enterArmed(now);
      return;
    }

    if (screen == SCREEN_TIMER) {
      if (state == STATE_DISARMED && rectContains(TIMER_PRIMARY_FULL, x, y)) {
        touch_started_on_hold_control = true;
        startHold(HOLD_ARM, now);
        return;
      }
      if (rectContains(TIMER_SECONDARY, x, y) && state != STATE_DISARMED) {
        touch_started_on_hold_control = true;
        startHold(HOLD_DISARM, now);
        return;
      }
    }

    if (screen == SCREEN_SIGNALS && rectContains(SIGNALS_MANUAL, x, y)) {
      touch_started_on_hold_control = true;
      manual_horn_armed = true;
      hold_start_ms = now;
      setFogRelay(true);
      fog_group_active = false;
      force_redraw = true;
      return;
    }

    if (screen == SCREEN_SIGNALS) {
      if (rectContains(SIGNALS_OFF, x, y)) {
        touch_started_on_hold_control = true;
        startHold(HOLD_SIGNAL_OFF, now);
        return;
      }
      if (rectContains(SIGNALS_SAILING, x, y)) {
        touch_started_on_hold_control = true;
        startHold(HOLD_SIGNAL_SAILING, now);
        return;
      }
      if (rectContains(SIGNALS_POWER, x, y)) {
        touch_started_on_hold_control = true;
        startHold(HOLD_SIGNAL_POWER, now);
        return;
      }
      if (rectContains(SIGNALS_STOPPED, x, y)) {
        touch_started_on_hold_control = true;
        startHold(HOLD_SIGNAL_STOPPED, now);
        return;
      }
    }
  }

  bool handled_swipe = false;
  if (touch.wasReleased()) {
    handled_swipe = switchScreenBySwipe(touch.distanceX(), touch.distanceY());
    touch_started_on_hold_control = false;
    clearHold();
  }

  if (touch.wasClicked() && !handled_swipe) {
    handleTap(touch.x, touch.y, now);
  }

  if (hold_action != HOLD_NONE && touch.isPressed()) {
    bool still_inside = false;
    if (hold_action == HOLD_ARM) {
      still_inside = rectContains(TIMER_PRIMARY_FULL, touch.x, touch.y);
    } else if (hold_action == HOLD_DISARM) {
      still_inside = rectContains(TIMER_SECONDARY, touch.x, touch.y);
    } else if (hold_action == HOLD_SIGNAL_OFF) {
      still_inside = rectContains(SIGNALS_OFF, touch.x, touch.y);
    } else if (hold_action == HOLD_SIGNAL_SAILING) {
      still_inside = rectContains(SIGNALS_SAILING, touch.x, touch.y);
    } else if (hold_action == HOLD_SIGNAL_POWER) {
      still_inside = rectContains(SIGNALS_POWER, touch.x, touch.y);
    } else if (hold_action == HOLD_SIGNAL_STOPPED) {
      still_inside = rectContains(SIGNALS_STOPPED, touch.x, touch.y);
    }

    if (!still_inside) {
      clearHold();
    } else if (now - hold_start_ms >= HOLD_ACTION_MS) {
      applyHoldAction(now);
    } else {
      force_redraw = true;
    }
  }

  if (manual_horn_armed && touch.isPressed()) {
    if (!rectContains(SIGNALS_MANUAL, touch.x, touch.y)) {
      manual_horn_armed = false;
      setFogRelay(false);
      force_redraw = true;
    }
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(180);
  ui.setColorDepth(16);
  ui.createSprite(M5.Display.width(), M5.Display.height());
  M5.Speaker.setVolume(180);

  pinMode(ALARM_RELAY_PIN, OUTPUT);
  pinMode(FOG_RELAY_PIN, OUTPUT);
  setAlarmRelay(false);
  setFogRelay(false);

  prefs.begin("alarmsolo", false);
  loadConfig();

  enterDisarmed();
}

void loop() {
  unsigned long now = millis();
  M5.update();

  handleTouch(now);

  switch (state) {
    case STATE_DISARMED:
      stopSpeaker();
      break;
    case STATE_ARMED:
    case STATE_WARNING:
      updateCountdown(now);
      if (state == STATE_WARNING) {
        updateWarningSpeaker(now);
      } else {
        stopSpeaker();
      }
      break;
    case STATE_ALARM:
      updateAlarmRelay(now);
      break;
  }

  bool manual_horn_active = manual_horn_armed && M5.Touch.getDetail().isPressed();
  updateFog(now, manual_horn_active);
  drawUi(now);
}
