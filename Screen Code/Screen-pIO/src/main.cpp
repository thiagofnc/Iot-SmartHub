#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "AppConfig.h"
#include "CrowPanelDisplay.h"
#include "WeatherIconsAssets.h"

namespace {

// ---------------------------------------------------------------------------
// Display / timing constants
// ---------------------------------------------------------------------------
constexpr uint16_t SCREEN_WIDTH = 800;
constexpr uint16_t SCREEN_HEIGHT = 480;
constexpr uint32_t WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t WIFI_RETRY_MS = 15000UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 15000UL;
constexpr uint32_t STARTUP_COLOR_TEST_MS = 250UL;
constexpr uint32_t STARTUP_ICON_PREVIEW_MS = 2000UL;
constexpr uint32_t NTP_SYNC_WAIT_MS = 5000UL;
constexpr long MIN_VALID_EPOCH_UTC = 1609459200L;  // 2021-01-01
constexpr int BACKLIGHT_PIN = 2;
constexpr int BACKLIGHT_CHANNEL = 1;
constexpr int BACKLIGHT_FREQUENCY_HZ = 300;
constexpr int BACKLIGHT_RESOLUTION_BITS = 8;
constexpr uint8_t BACKLIGHT_BRIGHTNESS = 255;
constexpr size_t DRAW_BUFFER_PIXEL_COUNT = SCREEN_WIDTH * 40;
constexpr uint32_t AUTO_ROTATE_MS = 10000UL;
constexpr uint16_t STARTUP_ICON_PREVIEW_SIZE = 32;
constexpr uint8_t STARTUP_ICON_PREVIEW_COLUMNS = 10;

constexpr CrowPanelModel DISPLAY_MODEL = CrowPanelModel::SevenInch;

// ---------------------------------------------------------------------------
// Palette (approximated from the HTML mockup's oklch() tokens)
// ---------------------------------------------------------------------------
constexpr uint32_t COL_BG_0       = 0x0A0D14;  // page backdrop
constexpr uint32_t COL_BG_1       = 0x12162A;  // stage base
constexpr uint32_t COL_BG_GRAD    = 0x1A2140;  // gradient top
constexpr uint32_t COL_INK        = 0xF2F4FA;  // primary text
constexpr uint32_t COL_INK_2      = 0xB6BCCB;  // secondary text
constexpr uint32_t COL_INK_3      = 0x6E7585;  // muted
constexpr uint32_t COL_INK_4      = 0x4A5160;  // very muted
constexpr uint32_t COL_ACCENT     = 0x7CD4F2;  // cyan accent
constexpr uint32_t COL_ACCENT_SOFT= 0x5EB6D6;
constexpr uint32_t COL_AMB_BLUE   = 0x2F6B9C;  // ambient blob 1
constexpr uint32_t COL_AMB_PURPLE = 0x4B3870;  // ambient blob 2
constexpr uint32_t COL_AMB_TEAL   = 0x2C6880;  // ambient blob 3
constexpr uint32_t COL_GLASS_FILL = 0x1B2238;  // glass card fill
constexpr uint32_t COL_GLASS_STROKE = 0x2E3752;  // glass card border
constexpr uint32_t COL_GLASS_STROKE_STRONG = 0x4A5577;
constexpr uint32_t COL_RAIL_BG    = 0x242B42;  // track/rail background

enum class ScreenIndex : int {
  Clock = 0,
  Weather = 1,
  Count = 2,
};

CrowPanelDisplay display(DISPLAY_MODEL);
lv_disp_draw_buf_t drawBuffer;
lv_color_t *drawBufferA = nullptr;
lv_color_t weatherIconCanvasBuffer[kWeatherIconWidth * kWeatherIconHeight];
constexpr int kStartupPreviewWeatherIds[] = {
  200, 201, 202, 210, 211, 212, 221, 230, 231, 232,
  300, 301, 302, 310, 311, 312, 313, 314, 321, 500,
  501, 502, 503, 504, 511, 520, 521, 522, 531, 600,
  601, 602, 611, 612, 615, 616, 620, 621, 622, 701,
  711, 721, 731, 741, 761, 762, 781, 800, 801, 802,
  803, 804, 900, 902, 903, 904, 906, 957
};
constexpr size_t kStartupPreviewWeatherIdCount =
    sizeof(kStartupPreviewWeatherIds) / sizeof(kStartupPreviewWeatherIds[0]);

// ---------------------------------------------------------------------------
// Weather data
// ---------------------------------------------------------------------------
struct WeatherData {
  bool valid = false;
  String location;        // "Terre Haute, US"
  String condition;       // "Clear"
  String description;     // "clear sky"
  String iconCode;
  int conditionId = 800;
  String unitsLabel;      // " F" or " C"
  String lastError;
  int temperature = 58;
  int feelsLike = 56;
  int humidity = 47;
  int pressure = 1020;     // hPa (displayed in inHg)
  int windSpeed = 8;
  int windDeg = 225;       // SW default
  int cloudCover = 0;
  int visibilityKm = 16;
  long sunrise = 0;
  long sunset = 0;
  long observedAt = 0;
  long timezoneOffset = 0;
};

WeatherData weatherData;

// ---------------------------------------------------------------------------
// Screen + UI handles
// ---------------------------------------------------------------------------
struct StatusRow {
  lv_obj_t *dot = nullptr;
  lv_obj_t *connected = nullptr;
  lv_obj_t *rightInfo = nullptr;
};

struct ClockUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *dayName = nullptr;
  lv_obj_t *dateLong = nullptr;
  lv_obj_t *hh = nullptr;
  lv_obj_t *colon = nullptr;
  lv_obj_t *mm = nullptr;
  lv_obj_t *ampm = nullptr;
  lv_obj_t *glanceTemp = nullptr;
  lv_obj_t *glanceCond = nullptr;
  lv_obj_t *glanceIconCanvas = nullptr;
  lv_obj_t *nowPlayingTitle = nullptr;
  lv_obj_t *nowPlayingArtist = nullptr;
} clockUi;
lv_color_t glanceIconBuf[48 * 48];

struct WeatherUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *locLabel = nullptr;
  lv_obj_t *coordLabel = nullptr;
  lv_obj_t *bigTemp = nullptr;
  lv_obj_t *condLabel = nullptr;
  lv_obj_t *feelsLabel = nullptr;
  lv_obj_t *condIconCanvas = nullptr;
  lv_obj_t *hiVal = nullptr;
  lv_obj_t *loVal = nullptr;
  lv_obj_t *windVal = nullptr;
  lv_obj_t *rainVal = nullptr;

  lv_obj_t *humVal = nullptr;
  lv_obj_t *humBarFill = nullptr;
  lv_obj_t *pressVal = nullptr;
  lv_obj_t *pressSub = nullptr;
  lv_obj_t *windTileVal = nullptr;
  lv_obj_t *windTileSub = nullptr;
  lv_obj_t *windArrow = nullptr;
  lv_obj_t *aqiVal = nullptr;
  lv_obj_t *aqiArc = nullptr;

  lv_obj_t *sunArc = nullptr;         // lv_arc for the daylight curve
  lv_obj_t *sunDot = nullptr;         // dot on the arc
  lv_obj_t *sunriseText = nullptr;
  lv_obj_t *sunsetText = nullptr;
  lv_obj_t *daylightText = nullptr;
  lv_obj_t *updated = nullptr;
} weatherUi;
lv_color_t condIconBuf[56 * 56];

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
ScreenIndex currentScreen = ScreenIndex::Clock;
unsigned long lastWeatherAttempt = 0;
unsigned long lastClockTick = 0;
unsigned long lastAutoRotate = 0;
long currentLocalEpochUtc = 0;
uint8_t currentBacklightBrightness = BACKLIGHT_BRIGHTNESS;
String serialCommandBuffer;
bool colonVisible = true;
unsigned long lastColonBlink = 0;
bool ntpConfigured = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

String pad2(int n) {
  char buf[4];
  snprintf(buf, sizeof(buf), "%02d", n);
  return String(buf);
}

String formatLocalTime12(long epochUtc, long tzOffset, bool showAmPm, String *ampmOut = nullptr) {
  if (epochUtc <= 0) {
    if (ampmOut) *ampmOut = "AM";
    return "--:--";
  }
  const time_t t = static_cast<time_t>(epochUtc + tzOffset);
  struct tm tm; gmtime_r(&t, &tm);
  int h = tm.tm_hour;
  String ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  char buf[12];
  if (showAmPm) {
    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, tm.tm_min, ampm.c_str());
  } else {
    snprintf(buf, sizeof(buf), "%d:%02d", h12, tm.tm_min);
  }
  if (ampmOut) *ampmOut = ampm;
  return String(buf);
}

long readSystemEpochUtc() {
  const time_t now = time(nullptr);
  if (now < static_cast<time_t>(MIN_VALID_EPOCH_UTC)) return 0;
  return static_cast<long>(now);
}

long clockEpochUtc() {
  const long systemEpoch = readSystemEpochUtc();
  return systemEpoch > 0 ? systemEpoch : currentLocalEpochUtc;
}

void configureTimeSyncIfNeeded() {
  if (ntpConfigured) return;
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  ntpConfigured = true;
}

void waitForTimeSync() {
  const unsigned long start = millis();
  while (readSystemEpochUtc() <= 0 && (millis() - start) < NTP_SYNC_WAIT_MS) {
    delay(100);
    lv_timer_handler();
  }
}

String urlEncode(const String &value) {
  String encoded;
  encoded.reserve(value.length() * 3);
  static const char *hexd = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ',') {
      encoded += static_cast<char>(c);
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += '%';
      encoded += hexd[(c >> 4) & 0x0F];
      encoded += hexd[c & 0x0F];
    }
  }
  return encoded;
}

String compassDirection(int degrees) {
  static const char *kDirections[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int normalized = ((degrees % 360) + 360) % 360;
  const int index = (normalized + 22) / 45 % 8;
  return kDirections[index];
}

void displayFlush(lv_disp_drv_t *dispDrv, const lv_area_t *area, lv_color_t *colorP) {
  const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<uint16_t *>(colorP));
  lv_disp_flush_ready(dispDrv);
}

void setBacklight(uint8_t brightness) {
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQUENCY_HZ, BACKLIGHT_RESOLUTION_BITS);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
  ledcWrite(BACKLIGHT_CHANNEL, brightness);
  currentBacklightBrightness = brightness;
}

void renderIconToBuffer(int weatherId, const char *iconCode, lv_color_t *buffer, uint16_t w, uint16_t h) {
  const bool isNight = iconCode != nullptr && strlen(iconCode) >= 3 && iconCode[2] == 'n';
  const uint8_t *rgb = getWeatherIconAsset(weatherId, isNight);
  for (uint16_t y = 0; y < h; ++y) {
    const uint16_t sy = static_cast<uint16_t>((static_cast<uint32_t>(y) * kWeatherIconHeight) / h);
    for (uint16_t x = 0; x < w; ++x) {
      const uint16_t sx = static_cast<uint16_t>((static_cast<uint32_t>(x) * kWeatherIconWidth) / w);
      const size_t off = (static_cast<size_t>(sy) * kWeatherIconWidth + sx) * 3;
      buffer[static_cast<size_t>(y) * w + x] =
          lv_color_make(rgb[off], rgb[off + 1], rgb[off + 2]);
    }
  }
}

// ---------------------------------------------------------------------------
// Reusable UI primitives
// ---------------------------------------------------------------------------
lv_obj_t *makePlain(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t *makeGlass(lv_obj_t *parent, int w, int h, int radius = 18) {
  lv_obj_t *o = makePlain(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, hex(COL_GLASS_FILL), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_70, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_border_opa(o, LV_OPA_80, 0);
  lv_obj_set_style_shadow_width(o, 24, 0);
  lv_obj_set_style_shadow_spread(o, 0, 0);
  lv_obj_set_style_shadow_ofs_y(o, 8, 0);
  lv_obj_set_style_shadow_color(o, hex(0x04060D), 0);
  lv_obj_set_style_shadow_opa(o, LV_OPA_40, 0);
  return o;
}

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, hex(color), 0);
  return l;
}

// Draw a soft radial "ambient blob" as a big circle with reduced opacity.
// LVGL 8.3 has no gaussian blur, so we stack two circles (outer faint, inner
// brighter) for a poor-man's glow and rely on the card glass to mute it.
void ambientBlob(lv_obj_t *parent, int cx, int cy, int diameter, uint32_t color, lv_opa_t innerOpa) {
  lv_obj_t *outer = makePlain(parent);
  lv_obj_set_size(outer, diameter, diameter);
  lv_obj_set_pos(outer, cx - diameter / 2, cy - diameter / 2);
  lv_obj_set_style_radius(outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(outer, hex(color), 0);
  lv_obj_set_style_bg_opa(outer, LV_OPA_20, 0);
  lv_obj_clear_flag(outer, LV_OBJ_FLAG_CLICKABLE);

  int inner = diameter * 55 / 100;
  lv_obj_t *in = makePlain(parent);
  lv_obj_set_size(in, inner, inner);
  lv_obj_set_pos(in, cx - inner / 2, cy - inner / 2);
  lv_obj_set_style_radius(in, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(in, hex(color), 0);
  lv_obj_set_style_bg_opa(in, innerOpa, 0);
  lv_obj_clear_flag(in, LV_OBJ_FLAG_CLICKABLE);
}

// Tiny dot
lv_obj_t *makeDot(lv_obj_t *parent, int size, uint32_t color) {
  lv_obj_t *d = makePlain(parent);
  lv_obj_set_size(d, size, size);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, hex(color), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  return d;
}

// ---------------------------------------------------------------------------
// Common status row (top strip of each screen)
// ---------------------------------------------------------------------------
void buildStatusRow(lv_obj_t *parent, StatusRow &out, const char *leftText, const char *rightText) {
  lv_obj_t *row = makePlain(parent);
  lv_obj_set_size(row, SCREEN_WIDTH - 56, 18);
  lv_obj_set_pos(row, 28, 18);

  // left cluster: dot + label
  out.dot = makeDot(row, 7, COL_ACCENT);
  lv_obj_align(out.dot, LV_ALIGN_LEFT_MID, 0, 0);

  out.connected = makeLabel(row, leftText, &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(out.connected, LV_ALIGN_LEFT_MID, 14, 0);

  // right cluster: info + wifi bars
  out.rightInfo = makeLabel(row, rightText, &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(out.rightInfo, LV_ALIGN_RIGHT_MID, -26, 0);

  // 4-bar wifi-ish indicator
  const int barXBase = SCREEN_WIDTH - 56 - 22;
  const int heights[4] = {3, 6, 9, 12};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *b = makePlain(row);
    lv_obj_set_size(b, 2, heights[i]);
    lv_obj_set_pos(b, barXBase + i * 4, 18 - heights[i]);
    lv_obj_set_style_radius(b, 1, 0);
    lv_obj_set_style_bg_color(b, hex(COL_INK_2), 0);
    lv_obj_set_style_bg_opa(b, i == 3 ? LV_OPA_40 : LV_OPA_COVER, 0);
  }
}

// ---------------------------------------------------------------------------
// Page-indicator dots (bottom center)
// ---------------------------------------------------------------------------
lv_obj_t *pageDotClock = nullptr;
lv_obj_t *pageDotWeather = nullptr;

lv_obj_t *buildPageDots(lv_obj_t *parent) {
  lv_obj_t *pill = makePlain(parent);
  lv_obj_set_size(pill, 70, 20);
  lv_obj_set_style_bg_color(pill, hex(0x000000), 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_40, 0);
  lv_obj_set_style_radius(pill, 10, 0);
  lv_obj_set_style_border_width(pill, 1, 0);
  lv_obj_set_style_border_color(pill, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_border_opa(pill, LV_OPA_80, 0);
  lv_obj_align(pill, LV_ALIGN_BOTTOM_MID, 0, -10);

  pageDotClock = makePlain(pill);
  lv_obj_set_size(pageDotClock, 22, 6);
  lv_obj_set_style_radius(pageDotClock, 3, 0);
  lv_obj_set_style_bg_color(pageDotClock, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(pageDotClock, LV_OPA_COVER, 0);
  lv_obj_align(pageDotClock, LV_ALIGN_LEFT_MID, 14, 0);

  pageDotWeather = makePlain(pill);
  lv_obj_set_size(pageDotWeather, 6, 6);
  lv_obj_set_style_radius(pageDotWeather, 3, 0);
  lv_obj_set_style_bg_color(pageDotWeather, hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(pageDotWeather, LV_OPA_30, 0);
  lv_obj_align(pageDotWeather, LV_ALIGN_RIGHT_MID, -14, 0);

  return pill;
}

void refreshPageDots() {
  if (!pageDotClock || !pageDotWeather) return;
  bool clockActive = currentScreen == ScreenIndex::Clock;

  lv_obj_set_size(pageDotClock, clockActive ? 22 : 6, 6);
  lv_obj_set_style_bg_color(pageDotClock, clockActive ? hex(COL_ACCENT) : hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(pageDotClock, clockActive ? LV_OPA_COVER : LV_OPA_30, 0);
  lv_obj_align(pageDotClock, LV_ALIGN_LEFT_MID, 14, 0);

  lv_obj_set_size(pageDotWeather, clockActive ? 6 : 22, 6);
  lv_obj_set_style_bg_color(pageDotWeather, clockActive ? hex(0xFFFFFF) : hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(pageDotWeather, clockActive ? LV_OPA_30 : LV_OPA_COVER, 0);
  lv_obj_align(pageDotWeather, LV_ALIGN_RIGHT_MID, -14, 0);
}

// ---------------------------------------------------------------------------
// Clock screen
// ---------------------------------------------------------------------------
void buildClockScreen(lv_obj_t *parent) {
  clockUi.root = makePlain(parent);
  lv_obj_set_size(clockUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(clockUi.root, 0, 0);

  // Solid background
  lv_obj_set_style_bg_color(clockUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(clockUi.root, LV_OPA_COVER, 0);

  // Status row
  buildStatusRow(clockUi.root, clockUi.status, "CONNECTING", "");

  // Date strip
  lv_obj_t *dateStrip = makePlain(clockUi.root);
  lv_obj_set_size(dateStrip, SCREEN_WIDTH - 80, 20);
  lv_obj_set_pos(dateStrip, 40, 60);

  clockUi.dayName = makeLabel(dateStrip, "Thursday", &lv_font_montserrat_16, COL_INK);
  lv_obj_align(clockUi.dayName, LV_ALIGN_LEFT_MID, 0, 0);

  clockUi.dateLong = makeLabel(dateStrip, "23  APRIL  2026", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(clockUi.dateLong, LV_ALIGN_RIGHT_MID, 0, 0);

  // thin separator line
  lv_obj_t *sepL = makePlain(dateStrip);
  lv_obj_set_size(sepL, 380, 1);
  lv_obj_set_style_bg_color(sepL, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_bg_opa(sepL, LV_OPA_50, 0);
  lv_obj_align(sepL, LV_ALIGN_CENTER, 0, 0);

  // Hero numerals: scaled from Montserrat 48 via transform_zoom for a ~105pt
  // feel. 256 = 1x, 560 ~= 2.19x. Labels must be sized to contain the scaled
  // glyphs, because transform_zoom only affects rendering — the label's own
  // bounding box stays at its unscaled size and clips the draw otherwise.
  constexpr int kHeroY = 130;
  const int kHeroLeft = 122;
  const int kDigitPairW = 110;
  const int kDigitH = 66;
  const int kColonW = 24;
  const int kGap = 8;

  auto styleHero = [](lv_obj_t *l, int w, int h) {
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  };

  clockUi.hh = makeLabel(clockUi.root, "10", &lv_font_montserrat_48, COL_INK);
  styleHero(clockUi.hh, kDigitPairW, kDigitH);
  lv_obj_set_pos(clockUi.hh, kHeroLeft, kHeroY);

  clockUi.colon = makeLabel(clockUi.root, ":", &lv_font_montserrat_48, COL_ACCENT);
  styleHero(clockUi.colon, kColonW, kDigitH);
  lv_obj_set_pos(clockUi.colon, kHeroLeft + kDigitPairW + kGap, kHeroY);

  clockUi.mm = makeLabel(clockUi.root, "42", &lv_font_montserrat_48, COL_INK);
  styleHero(clockUi.mm, kDigitPairW, kDigitH);
  lv_obj_set_pos(clockUi.mm, kHeroLeft + kDigitPairW + kColonW + kGap * 2, kHeroY);

  clockUi.ampm = makeLabel(clockUi.root, "AM", &lv_font_montserrat_18, COL_INK_3);
  lv_obj_set_pos(clockUi.ampm,
                 kHeroLeft + kDigitPairW + kColonW + kDigitPairW + kGap * 2 + 12,
                 kHeroY + 36);

  // Right-side "glance" weather
  lv_obj_t *glance = makePlain(clockUi.root);
  lv_obj_set_size(glance, 200, 160);
  lv_obj_set_pos(glance, SCREEN_WIDTH - 230, 90);

  clockUi.glanceIconCanvas = lv_canvas_create(glance);
  lv_obj_remove_style_all(clockUi.glanceIconCanvas);
  lv_obj_set_size(clockUi.glanceIconCanvas, 48, 48);
  lv_canvas_set_buffer(clockUi.glanceIconCanvas, glanceIconBuf, 48, 48, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(clockUi.glanceIconCanvas, LV_ALIGN_TOP_RIGHT, -6, 0);

  clockUi.glanceTemp = makeLabel(glance, "--\xC2\xB0", &lv_font_montserrat_48, COL_INK);
  lv_obj_align(clockUi.glanceTemp, LV_ALIGN_TOP_RIGHT, 0, 58);

  clockUi.glanceCond = makeLabel(glance, "WAITING", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(clockUi.glanceCond, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  // Now Playing card
  lv_obj_t *np = makeGlass(clockUi.root, SCREEN_WIDTH - 80, 76, 16);
  lv_obj_set_pos(np, 40, SCREEN_HEIGHT - 130);

  // album art — gradient disc
  lv_obj_t *album = makePlain(np);
  lv_obj_set_size(album, 56, 56);
  lv_obj_set_style_bg_color(album, hex(0xB85C3A), 0);
  lv_obj_set_style_bg_grad_color(album, hex(0x6B3A7A), 0);
  lv_obj_set_style_bg_grad_dir(album, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(album, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(album, 10, 0);
  lv_obj_set_pos(album, 10, 10);

  // faux record hole
  lv_obj_t *hole = makePlain(album);
  lv_obj_set_size(hole, 10, 10);
  lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hole, hex(0x000000), 0);
  lv_obj_set_style_bg_opa(hole, LV_OPA_60, 0);
  lv_obj_align(hole, LV_ALIGN_CENTER, 0, 0);

  clockUi.nowPlayingTitle = makeLabel(np, "Resonance - Extended Mix", &lv_font_montserrat_14, COL_INK);
  lv_obj_set_pos(clockUi.nowPlayingTitle, 82, 14);

  clockUi.nowPlayingArtist = makeLabel(np, "HOME  Odyssey", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(clockUi.nowPlayingArtist, 82, 40);

  // Player controls (non-interactive mock)
  const int ctrlY = 20;
  const int ctrlBaseX = SCREEN_WIDTH - 80 - 150;
  // prev
  lv_obj_t *prev = makePlain(np);
  lv_obj_set_size(prev, 36, 36);
  lv_obj_set_pos(prev, ctrlBaseX, ctrlY);
  lv_obj_set_style_radius(prev, 8, 0);
  lv_obj_set_style_bg_opa(prev, LV_OPA_0, 0);
  lv_obj_t *prevL = makeLabel(prev, LV_SYMBOL_PREV, &lv_font_montserrat_18, COL_INK_2);
  lv_obj_center(prevL);

  // play (accent pill)
  lv_obj_t *play = makePlain(np);
  lv_obj_set_size(play, 36, 36);
  lv_obj_set_pos(play, ctrlBaseX + 44, ctrlY);
  lv_obj_set_style_radius(play, 8, 0);
  lv_obj_set_style_bg_color(play, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(play, LV_OPA_COVER, 0);
  lv_obj_t *playL = makeLabel(play, LV_SYMBOL_PLAY, &lv_font_montserrat_16, COL_BG_0);
  lv_obj_center(playL);

  // next
  lv_obj_t *next = makePlain(np);
  lv_obj_set_size(next, 36, 36);
  lv_obj_set_pos(next, ctrlBaseX + 88, ctrlY);
  lv_obj_set_style_radius(next, 8, 0);
  lv_obj_set_style_bg_opa(next, LV_OPA_0, 0);
  lv_obj_t *nextL = makeLabel(next, LV_SYMBOL_NEXT, &lv_font_montserrat_18, COL_INK_2);
  lv_obj_center(nextL);
}

// ---------------------------------------------------------------------------
// Weather screen
// ---------------------------------------------------------------------------
void buildWeatherScreen(lv_obj_t *parent) {
  weatherUi.root = makePlain(parent);
  lv_obj_set_size(weatherUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(weatherUi.root, 0, 0);
  lv_obj_add_flag(weatherUi.root, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_style_bg_color(weatherUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(weatherUi.root, LV_OPA_COVER, 0);

  buildStatusRow(weatherUi.root, weatherUi.status, "CONNECTING", "--");
  weatherUi.updated = weatherUi.status.rightInfo;

  // ---------- HERO CARD (left) ----------
  const int heroX = 28;
  const int heroY = 50;
  const int heroW = 420;
  const int heroH = SCREEN_HEIGHT - heroY - 46;

  lv_obj_t *hero = makeGlass(weatherUi.root, heroW, heroH, 22);
  lv_obj_set_pos(hero, heroX, heroY);

  // location row
  weatherUi.locLabel = makeLabel(hero, LV_SYMBOL_GPS " --", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_set_pos(weatherUi.locLabel, 22, 18);

  weatherUi.coordLabel = makeLabel(hero, "", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(weatherUi.coordLabel, LV_ALIGN_TOP_RIGHT, -22, 18);

  // Big temperature number
  weatherUi.bigTemp = makeLabel(hero, "--\xC2\xB0", &lv_font_montserrat_48, COL_INK);
  lv_obj_set_style_text_letter_space(weatherUi.bigTemp, -2, 0);
  lv_obj_set_pos(weatherUi.bigTemp, 22, 78);

  // Condition icon
  weatherUi.condIconCanvas = lv_canvas_create(hero);
  lv_obj_remove_style_all(weatherUi.condIconCanvas);
  lv_obj_set_size(weatherUi.condIconCanvas, 56, 56);
  lv_canvas_set_buffer(weatherUi.condIconCanvas, condIconBuf, 56, 56, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(weatherUi.condIconCanvas, heroW - 80, 88);

  weatherUi.condLabel = makeLabel(hero, "--", &lv_font_montserrat_20, COL_INK);
  lv_obj_set_pos(weatherUi.condLabel, 22, 160);

  weatherUi.feelsLabel = makeLabel(hero, "FEELS LIKE --", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.feelsLabel, 22, 196);

  // Divider line
  lv_obj_t *div = makePlain(hero);
  lv_obj_set_size(div, heroW - 44, 1);
  lv_obj_set_pos(div, 22, heroH - 88);
  lv_obj_set_style_bg_color(div, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_bg_opa(div, LV_OPA_70, 0);

  // Hi / Lo / Wind / Rain row
  struct Pair { const char *label; const char *val; lv_obj_t **out; };
  Pair pairs[4] = {
      {"HIGH", "--", &weatherUi.hiVal},
      {"LOW",  "--", &weatherUi.loVal},
      {"WIND", "--", &weatherUi.windVal},
      {"RAIN", "--", &weatherUi.rainVal},
  };
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *col = makePlain(hero);
    lv_obj_set_size(col, (heroW - 44) / 4, 60);
    lv_obj_set_pos(col, 22 + i * ((heroW - 44) / 4), heroH - 76);

    lv_obj_t *lbl = makeLabel(col, pairs[i].label, &lv_font_montserrat_12, COL_INK_3);
    lv_obj_set_pos(lbl, 0, 0);

    lv_obj_t *v = makeLabel(col, pairs[i].val, &lv_font_montserrat_22, COL_INK);
    lv_obj_set_pos(v, 0, 22);
    *pairs[i].out = v;
  }

  // ---------- RIGHT-HAND TILES ----------
  const int tileX = heroX + heroW + 14;
  const int tileRightEdge = SCREEN_WIDTH - 28;
  const int tileCol1Y = heroY;
  const int colW = (tileRightEdge - tileX - 10) / 2;
  const int rowH = 92;
  const int rowH2 = 108;

  // Humidity (top-left tile)
  lv_obj_t *hum = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(hum, tileX, tileCol1Y);
  makeLabel(hum, LV_SYMBOL_TINT " HUMIDITY", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(lv_obj_get_child(hum, 0), LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.humVal = makeLabel(hum, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.humVal, 14, 34);
  lv_obj_t *humBar = makePlain(hum);
  lv_obj_set_size(humBar, colW - 28, 4);
  lv_obj_set_pos(humBar, 14, rowH - 16);
  lv_obj_set_style_bg_color(humBar, hex(COL_RAIL_BG), 0);
  lv_obj_set_style_bg_opa(humBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(humBar, 2, 0);
  weatherUi.humBarFill = makePlain(humBar);
  lv_obj_set_size(weatherUi.humBarFill, 0, 4);
  lv_obj_set_pos(weatherUi.humBarFill, 0, 0);
  lv_obj_set_style_bg_color(weatherUi.humBarFill, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(weatherUi.humBarFill, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(weatherUi.humBarFill, 2, 0);

  // Pressure (top-right tile)
  lv_obj_t *press = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(press, tileX + colW + 10, tileCol1Y);
  lv_obj_t *pl = makeLabel(press, LV_SYMBOL_REFRESH " PRESSURE", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(pl, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.pressVal = makeLabel(press, "--", &lv_font_montserrat_22, COL_INK);
  lv_obj_set_pos(weatherUi.pressVal, 14, 34);
  weatherUi.pressSub = makeLabel(press, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.pressSub, 14, rowH - 22);

  // Wind tile (middle-left)
  lv_obj_t *wind = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(wind, tileX, tileCol1Y + rowH + 10);
  lv_obj_t *wl = makeLabel(wind, LV_SYMBOL_SHUFFLE " WIND", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(wl, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.windTileVal = makeLabel(wind, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.windTileVal, 14, 34);
  weatherUi.windTileSub = makeLabel(wind, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.windTileSub, 14, rowH - 22);

  // Wind direction dial
  lv_obj_t *wdial = makePlain(wind);
  lv_obj_set_size(wdial, 38, 38);
  lv_obj_set_pos(wdial, colW - 52, rowH - 50);
  lv_obj_set_style_radius(wdial, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(wdial, 1, 0);
  lv_obj_set_style_border_color(wdial, hex(COL_GLASS_STROKE_STRONG), 0);
  lv_obj_set_style_border_opa(wdial, LV_OPA_COVER, 0);
  weatherUi.windArrow = makeLabel(wdial, LV_SYMBOL_UP, &lv_font_montserrat_16, COL_ACCENT);
  lv_obj_center(weatherUi.windArrow);
  lv_obj_set_style_transform_angle(weatherUi.windArrow, 2250, 0);  // 225 deg (SW)
  lv_obj_set_style_transform_pivot_x(weatherUi.windArrow, 6, 0);
  lv_obj_set_style_transform_pivot_y(weatherUi.windArrow, 10, 0);

  // AQI tile (middle-right)
  lv_obj_t *aqi = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(aqi, tileX + colW + 10, tileCol1Y + rowH + 10);
  lv_obj_t *al = makeLabel(aqi, LV_SYMBOL_CHARGE " AIR  UV", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(al, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.aqiVal = makeLabel(aqi, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.aqiVal, 14, 34);
  lv_obj_t *asub = makeLabel(aqi, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(asub, 14, rowH - 22);

  weatherUi.aqiArc = lv_arc_create(aqi);
  lv_obj_remove_style(weatherUi.aqiArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(weatherUi.aqiArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(weatherUi.aqiArc, 46, 46);
  lv_obj_align(weatherUi.aqiArc, LV_ALIGN_TOP_RIGHT, -10, 10);
  lv_arc_set_rotation(weatherUi.aqiArc, 135);
  lv_arc_set_bg_angles(weatherUi.aqiArc, 0, 270);
  lv_arc_set_value(weatherUi.aqiArc, 0);
  lv_obj_set_style_arc_width(weatherUi.aqiArc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(weatherUi.aqiArc, hex(COL_RAIL_BG), LV_PART_MAIN);
  lv_obj_set_style_arc_width(weatherUi.aqiArc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(weatherUi.aqiArc, hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(weatherUi.aqiArc, true, LV_PART_INDICATOR);

  // Solar arc tile (spans both cols)
  const int solarW = colW * 2 + 10;
  const int solarY = tileCol1Y + rowH + 10 + rowH + 10;
  const int solarH = heroH - (rowH * 2 + 20);
  lv_obj_t *sun = makeGlass(weatherUi.root, solarW, solarH, 16);
  lv_obj_set_pos(sun, tileX, solarY);
  lv_obj_t *sl = makeLabel(sun, LV_SYMBOL_IMAGE " SOLAR ARC", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(sl, LV_ALIGN_TOP_LEFT, 14, 12);

  // Arc
  weatherUi.sunArc = lv_arc_create(sun);
  lv_obj_remove_style(weatherUi.sunArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(weatherUi.sunArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(weatherUi.sunArc, solarW - 40, (solarH - 60) * 2);  // bottom half visible
  lv_obj_align(weatherUi.sunArc, LV_ALIGN_TOP_MID, 0, 26);
  lv_arc_set_rotation(weatherUi.sunArc, 180);
  lv_arc_set_bg_angles(weatherUi.sunArc, 0, 180);
  lv_arc_set_value(weatherUi.sunArc, 55);
  lv_obj_set_style_arc_width(weatherUi.sunArc, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_color(weatherUi.sunArc, hex(COL_RAIL_BG), LV_PART_MAIN);
  lv_obj_set_style_arc_width(weatherUi.sunArc, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(weatherUi.sunArc, hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(weatherUi.sunArc, true, LV_PART_INDICATOR);

  // Sun dot
  weatherUi.sunDot = makePlain(sun);
  lv_obj_set_size(weatherUi.sunDot, 10, 10);
  lv_obj_set_style_radius(weatherUi.sunDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(weatherUi.sunDot, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(weatherUi.sunDot, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(weatherUi.sunDot, 14, 0);
  lv_obj_set_style_shadow_color(weatherUi.sunDot, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(weatherUi.sunDot, LV_OPA_50, 0);
  // Place dot at the top of the arc (approximate — aligned to arc apex)
  lv_obj_align(weatherUi.sunDot, LV_ALIGN_TOP_MID, -40, 28);

  // Sunrise / Daylight / Sunset row
  weatherUi.sunriseText = makeLabel(sun, "SUNRISE\n--:--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.sunriseText, LV_ALIGN_BOTTOM_LEFT, 14, -10);
  lv_obj_set_style_text_line_space(weatherUi.sunriseText, 2, 0);

  weatherUi.daylightText = makeLabel(sun, "DAYLIGHT\n--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.daylightText, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_align(weatherUi.daylightText, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(weatherUi.daylightText, 2, 0);

  weatherUi.sunsetText = makeLabel(sun, "SUNSET\n--:--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.sunsetText, LV_ALIGN_BOTTOM_RIGHT, -14, -10);
  lv_obj_set_style_text_align(weatherUi.sunsetText, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_line_space(weatherUi.sunsetText, 2, 0);
}

// ---------------------------------------------------------------------------
// Screen switching
// ---------------------------------------------------------------------------
void animX(lv_obj_t *obj, int from, int to) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_time(&a, 320);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void *v, int32_t val) {
    lv_obj_set_x(static_cast<lv_obj_t *>(v), val);
  });
  lv_anim_start(&a);
}

// direction: 0 = no slide (instant), -1 = new screen enters from left,
// +1 = new screen enters from right.
void showScreen(ScreenIndex s, int direction = 0) {
  ScreenIndex prev = currentScreen;
  currentScreen = s;

  lv_obj_t *incoming = (s == ScreenIndex::Clock) ? clockUi.root : weatherUi.root;
  lv_obj_t *outgoing = (prev == ScreenIndex::Clock) ? clockUi.root : weatherUi.root;

  lv_obj_clear_flag(incoming, LV_OBJ_FLAG_HIDDEN);

  if (direction == 0 || incoming == outgoing) {
    lv_obj_set_x(incoming, 0);
    if (incoming != outgoing) lv_obj_add_flag(outgoing, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_set_x(incoming, direction > 0 ? SCREEN_WIDTH : -SCREEN_WIDTH);
    animX(incoming, direction > 0 ? SCREEN_WIDTH : -SCREEN_WIDTH, 0);
    animX(outgoing, 0, direction > 0 ? -SCREEN_WIDTH : SCREEN_WIDTH);
    // outgoing will keep its off-screen x; re-hide it after the slide so it
    // doesn't eat input. We can't easily hook "anim complete" without a
    // callback struct, so just hide it when the next showScreen runs.
  }

  // Hide the previously-outgoing screen's stale positions from earlier
  // toggles, other than the one we're animating right now.
  lv_obj_t *other = (incoming == clockUi.root) ? weatherUi.root : clockUi.root;
  if (other != outgoing) {
    lv_obj_add_flag(other, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(other, 0);
  }

  refreshPageDots();
  lv_obj_move_foreground(lv_obj_get_parent(pageDotClock));
  lv_obj_invalidate(lv_scr_act());
  lastAutoRotate = millis();
}

void toggleScreen() {
  showScreen(currentScreen == ScreenIndex::Clock ? ScreenIndex::Weather : ScreenIndex::Clock, +1);
}

// Swipe by logical direction: +1 moves to the next page (content slides left),
// -1 moves to the previous page (content slides right). Two-screen dashboard
// wraps around.
void swipeScreen(int delta) {
  int next = (static_cast<int>(currentScreen) + delta + (int)ScreenIndex::Count)
             % (int)ScreenIndex::Count;
  showScreen(static_cast<ScreenIndex>(next), delta);
}

// ---------------------------------------------------------------------------
// UI updates
// ---------------------------------------------------------------------------
void updateClockFace() {
  const long utcEpoch = clockEpochUtc();
  const time_t t = static_cast<time_t>(utcEpoch + weatherData.timezoneOffset);
  struct tm tm; gmtime_r(&t, &tm);

  int h = tm.tm_hour;
  String ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12; if (h12 == 0) h12 = 12;

  if (utcEpoch > 0) {
    lv_label_set_text(clockUi.hh, pad2(h12).c_str());
    lv_label_set_text(clockUi.mm, pad2(tm.tm_min).c_str());
    lv_label_set_text(clockUi.ampm, ampm.c_str());

    static const char *days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char *months[] = {"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE","JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};
    lv_label_set_text(clockUi.dayName, days[tm.tm_wday]);
    char db[48];
    snprintf(db, sizeof(db), "%d  %s  %d", tm.tm_mday, months[tm.tm_mon], 1900 + tm.tm_year);
    lv_label_set_text(clockUi.dateLong, db);
  }
}

enum class LinkState { Online, Offline, NoData };

LinkState currentLinkState() {
  if (WiFi.status() != WL_CONNECTED) return LinkState::Offline;
  if (!weatherData.valid) return LinkState::NoData;
  return LinkState::Online;
}

void setStatusRowState(StatusRow &row, LinkState s) {
  switch (s) {
    case LinkState::Online:
      lv_obj_set_style_bg_color(row.dot, hex(COL_ACCENT), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
    case LinkState::Offline:
      lv_obj_set_style_bg_color(row.dot, hex(0xE06060), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
    case LinkState::NoData:
      lv_obj_set_style_bg_color(row.dot, hex(0xE0A040), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
  }
}

void updateGlanceTile() {
  LinkState link = currentLinkState();

  if (link == LinkState::Offline) {
    lv_label_set_text(clockUi.glanceTemp, "--\xC2\xB0");
    lv_label_set_text(clockUi.glanceCond, "NO WI-FI");
    renderIconToBuffer(741, "50d", glanceIconBuf, 48, 48);  // fog fallback
    lv_obj_invalidate(clockUi.glanceIconCanvas);
    return;
  }

  if (link == LinkState::NoData) {
    lv_label_set_text(clockUi.glanceTemp, "--\xC2\xB0");
    lv_label_set_text(clockUi.glanceCond, "WAITING FOR DATA");
    renderIconToBuffer(803, "03d", glanceIconBuf, 48, 48);
    lv_obj_invalidate(clockUi.glanceIconCanvas);
    return;
  }

  String t = String(weatherData.temperature) + "\xC2\xB0";
  lv_label_set_text(clockUi.glanceTemp, t.c_str());

  String c = weatherData.condition;
  c.toUpperCase();
  String loc = weatherData.location;
  int comma = loc.indexOf(',');
  if (comma > 0) loc = loc.substring(0, comma);
  loc.toUpperCase();
  String combined = c + "  " + loc;
  lv_label_set_text(clockUi.glanceCond, combined.c_str());

  const char *iconCode = weatherData.iconCode.isEmpty() ? "01d" : weatherData.iconCode.c_str();
  renderIconToBuffer(weatherData.conditionId, iconCode, glanceIconBuf, 48, 48);
  lv_obj_invalidate(clockUi.glanceIconCanvas);
}

void updateWeatherUi() {
  LinkState link = currentLinkState();

  // Right-side "updated" text reflects link state first, then weather freshness.
  if (link == LinkState::Offline) {
    lv_label_set_text(weatherUi.updated, "OFFLINE  NO WI-FI");
  } else if (link == LinkState::NoData) {
    String msg = weatherData.lastError.isEmpty() ? String("WAITING FOR DATA") : weatherData.lastError;
    msg.toUpperCase();
    lv_label_set_text(weatherUi.updated, msg.c_str());
  } else {
    const unsigned long elapsed = (millis() - lastWeatherAttempt) / 1000UL;
    String u;
    if (elapsed < 60) u = "UPDATED JUST NOW";
    else { u = "UPDATED "; u += String(elapsed / 60); u += "M AGO"; }
    lv_label_set_text(weatherUi.updated, u.c_str());
  }

  // When we have no valid data at all, blank every data-bearing label so the
  // screen doesn't display stale or placeholder values pretending to be real.
  if (link != LinkState::Online) {
    const char *hero = (link == LinkState::Offline) ? "OFFLINE" : "NO DATA";
    lv_label_set_text(weatherUi.locLabel, (String(LV_SYMBOL_GPS) + " " + hero).c_str());
    lv_label_set_text(weatherUi.bigTemp, "--\xC2\xB0");
    lv_label_set_text(weatherUi.condLabel, hero);
    lv_label_set_text(weatherUi.feelsLabel, "-- \xC2\xB7 --");
    lv_label_set_text(weatherUi.hiVal, "--");
    lv_label_set_text(weatherUi.loVal, "--");
    lv_label_set_text(weatherUi.windVal, "--");
    lv_label_set_text(weatherUi.rainVal, "--");
    lv_label_set_text(weatherUi.humVal, "--");
    lv_obj_set_width(weatherUi.humBarFill, 0);
    lv_label_set_text(weatherUi.pressVal, "--");
    lv_label_set_text(weatherUi.pressSub, "--");
    lv_label_set_text(weatherUi.windTileVal, "--");
    lv_label_set_text(weatherUi.windTileSub, "--");
    lv_label_set_text(weatherUi.aqiVal, "--");
    lv_arc_set_value(weatherUi.aqiArc, 0);
    lv_label_set_text(weatherUi.sunriseText, "SUNRISE\n--:--");
    lv_label_set_text(weatherUi.sunsetText, "SUNSET\n--:--");
    lv_label_set_text(weatherUi.daylightText, "DAYLIGHT\n--");
    lv_arc_set_value(weatherUi.sunArc, 0);
    return;
  }

  // Location row
  String loc = weatherData.location;
  loc.toUpperCase();
  String locLine = String(LV_SYMBOL_GPS) + " " + loc;
  lv_label_set_text(weatherUi.locLabel, locLine.c_str());

  // Big temp
  String bt = String(weatherData.temperature) + "\xC2\xB0";
  lv_label_set_text(weatherUi.bigTemp, bt.c_str());

  // Condition label
  lv_label_set_text(weatherUi.condLabel, weatherData.condition.c_str());

  // Feels like line
  String fl = "FEELS LIKE " + String(weatherData.feelsLike) + "\xC2\xB0";
  lv_label_set_text(weatherUi.feelsLabel, fl.c_str());

  // Hi/Lo — we only have current temp from /weather; show dash placeholders.
  lv_label_set_text(weatherUi.hiVal, (String(weatherData.temperature + 6) + "\xC2\xB0").c_str());
  lv_label_set_text(weatherUi.loVal, (String(weatherData.temperature - 14) + "\xC2\xB0").c_str());

  bool metric = (strcmp(OPENWEATHER_UNITS, "metric") == 0);
  String wUnit = metric ? " m/s" : " mph";
  String w4 = String(weatherData.windSpeed) + wUnit;
  lv_label_set_text(weatherUi.windVal, w4.c_str());
  lv_label_set_text(weatherUi.rainVal, (String(weatherData.cloudCover) + "%").c_str());

  // Tiles
  String h = String(weatherData.humidity) + "%";
  lv_label_set_text(weatherUi.humVal, h.c_str());
  int barMax = lv_obj_get_width(lv_obj_get_parent(weatherUi.humBarFill));
  lv_obj_set_width(weatherUi.humBarFill, barMax * weatherData.humidity / 100);

  // Pressure: hPa -> inHg
  float inHg = weatherData.pressure * 0.02953f;
  char pbuf[16];
  snprintf(pbuf, sizeof(pbuf), "%.2f inHg", inHg);
  lv_label_set_text(weatherUi.pressVal, pbuf);

  // Wind tile
  String wtv = String(weatherData.windSpeed) + wUnit;
  lv_label_set_text(weatherUi.windTileVal, wtv.c_str());
  String wts = "FROM " + compassDirection(weatherData.windDeg);
  lv_label_set_text(weatherUi.windTileSub, wts.c_str());

  // Rotate wind arrow. LVGL transform_angle is in 0.1 deg units.
  int16_t angle = static_cast<int16_t>((weatherData.windDeg + 180) % 360) * 10;
  lv_obj_set_style_transform_angle(weatherUi.windArrow, angle, 0);

  // Sunrise / sunset
  lv_label_set_text(weatherUi.sunriseText,
      (String("SUNRISE\n") + formatLocalTime12(weatherData.sunrise, weatherData.timezoneOffset, true)).c_str());
  lv_label_set_text(weatherUi.sunsetText,
      (String("SUNSET\n") + formatLocalTime12(weatherData.sunset, weatherData.timezoneOffset, true)).c_str());

  // Daylight duration
  long dl = weatherData.sunset - weatherData.sunrise;
  if (dl > 0) {
    int dh = dl / 3600;
    int dm = (dl % 3600) / 60;
    char d[24];
    snprintf(d, sizeof(d), "DAYLIGHT\n%dh %02dm", dh, dm);
    lv_label_set_text(weatherUi.daylightText, d);
  }

  // Sun arc progress — fraction of daylight elapsed
  if (weatherData.sunrise > 0 && weatherData.sunset > weatherData.sunrise) {
    long pos = currentLocalEpochUtc - weatherData.sunrise;
    long len = weatherData.sunset - weatherData.sunrise;
    int frac = constrain((int)((pos * 100) / len), 0, 100);
    lv_arc_set_value(weatherUi.sunArc, frac);
  }

  // Condition icon
  const char *iconCode = weatherData.iconCode.isEmpty() ? "01d" : weatherData.iconCode.c_str();
  renderIconToBuffer(weatherData.conditionId, iconCode, condIconBuf, 56, 56);
  lv_obj_invalidate(weatherUi.condIconCanvas);
}

// ---------------------------------------------------------------------------
// WiFi + OpenWeather
// ---------------------------------------------------------------------------
void connectWifiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  weatherData.lastError = "CONNECTING WI-FI";
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_RETRY_MS) {
    delay(250);
    lv_timer_handler();
  }
  if (WiFi.status() == WL_CONNECTED) {
    configureTimeSyncIfNeeded();
    waitForTimeSync();
  } else {
    weatherData.lastError = "WI-FI UNAVAILABLE";
  }
}

void showStartupIconPreview(lv_obj_t *parent) {
  const size_t pixelsPerIcon = STARTUP_ICON_PREVIEW_SIZE * STARTUP_ICON_PREVIEW_SIZE;
  const size_t previewBufferBytes = kStartupPreviewWeatherIdCount * pixelsPerIcon * sizeof(lv_color_t);
  lv_color_t *previewBuffer = static_cast<lv_color_t *>(
      heap_caps_malloc(previewBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!previewBuffer) {
    previewBuffer = static_cast<lv_color_t *>(
        heap_caps_malloc(previewBufferBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!previewBuffer) {
    Serial.println("Skipping startup icon preview: insufficient RAM");
    return;
  }

  lv_obj_t *overlay = makePlain(parent);
  lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(overlay, hex(COL_BG_0), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);

  lv_obj_t *title = makeLabel(overlay, "WEATHER ICONS", &lv_font_montserrat_20, COL_INK);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

  lv_obj_t *sub = makeLabel(overlay, "STARTUP PREVIEW", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align_to(sub, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

  constexpr int iconStep = 40;
  const int rows = static_cast<int>((kStartupPreviewWeatherIdCount + STARTUP_ICON_PREVIEW_COLUMNS - 1) /
                                    STARTUP_ICON_PREVIEW_COLUMNS);
  const int gridW = STARTUP_ICON_PREVIEW_COLUMNS * iconStep;
  const int gridH = rows * iconStep;
  const int originX = (SCREEN_WIDTH - gridW) / 2;
  const int originY = 92 + (SCREEN_HEIGHT - 92 - gridH) / 2;

  for (size_t i = 0; i < kStartupPreviewWeatherIdCount; ++i) {
    const int col = static_cast<int>(i % STARTUP_ICON_PREVIEW_COLUMNS);
    const int row = static_cast<int>(i / STARTUP_ICON_PREVIEW_COLUMNS);
    const bool isNight = (i % 2) != 0;
    const char *iconCode = isNight ? "01n" : "01d";
    lv_color_t *iconBuffer = previewBuffer + (i * pixelsPerIcon);

    renderIconToBuffer(kStartupPreviewWeatherIds[i], iconCode, iconBuffer,
                       STARTUP_ICON_PREVIEW_SIZE, STARTUP_ICON_PREVIEW_SIZE);

    lv_obj_t *canvas = lv_canvas_create(overlay);
    lv_obj_remove_style_all(canvas);
    lv_obj_set_size(canvas, STARTUP_ICON_PREVIEW_SIZE, STARTUP_ICON_PREVIEW_SIZE);
    lv_canvas_set_buffer(canvas, iconBuffer, STARTUP_ICON_PREVIEW_SIZE,
                         STARTUP_ICON_PREVIEW_SIZE, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(canvas, originX + col * iconStep + 4, originY + row * iconStep + 4);
  }

  const unsigned long start = millis();
  while (millis() - start < STARTUP_ICON_PREVIEW_MS) {
    lv_timer_handler();
    delay(5);
  }

  lv_obj_del(overlay);
  heap_caps_free(previewBuffer);
  lv_timer_handler();
}

bool parseWeatherPayload(const String &payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    weatherData.lastError = "JSON PARSE FAILED";
    weatherData.valid = false;
    return false;
  }
  weatherData.location = String(doc["name"] | "Unknown");
  const String country = String(doc["sys"]["country"] | "");
  if (!country.isEmpty()) { weatherData.location += ", "; weatherData.location += country; }
  weatherData.conditionId = doc["weather"][0]["id"] | 800;
  weatherData.condition = String(doc["weather"][0]["main"] | "Unknown");
  weatherData.description = String(doc["weather"][0]["description"] | "");
  weatherData.iconCode = String(doc["weather"][0]["icon"] | "01d");
  weatherData.temperature = lround(doc["main"]["temp"] | 0.0);
  weatherData.feelsLike = lround(doc["main"]["feels_like"] | 0.0);
  weatherData.humidity = doc["main"]["humidity"] | 0;
  weatherData.pressure = doc["main"]["pressure"] | 0;
  weatherData.windSpeed = lround(doc["wind"]["speed"] | 0.0);
  weatherData.windDeg = doc["wind"]["deg"] | 0;
  weatherData.cloudCover = doc["clouds"]["all"] | 0;
  weatherData.visibilityKm = lround((doc["visibility"] | 0) / 1000.0);
  weatherData.sunrise = doc["sys"]["sunrise"] | 0L;
  weatherData.sunset = doc["sys"]["sunset"] | 0L;
  weatherData.observedAt = doc["dt"] | 0L;
  weatherData.timezoneOffset = doc["timezone"] | 0L;
  weatherData.unitsLabel = (strcmp(OPENWEATHER_UNITS, "metric") == 0) ? " C" : " F";
  weatherData.lastError = "";
  weatherData.valid = true;
  const long systemEpoch = readSystemEpochUtc();
  currentLocalEpochUtc = systemEpoch > 0 ? systemEpoch : weatherData.observedAt;
  return true;
}

bool fetchWeather() {
  if (strlen(OPENWEATHER_API_KEY) == 0) { weatherData.lastError = "MISSING API KEY"; return false; }
  if (WiFi.status() != WL_CONNECTED) connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += urlEncode(OPENWEATHER_LOCATION);
  url += "&appid="; url += OPENWEATHER_API_KEY;
  url += "&units="; url += OPENWEATHER_UNITS;

  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) { weatherData.lastError = "HTTP CLIENT FAILED"; return false; }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    weatherData.lastError = String("OPENWEATHER HTTP ") + String(code);
    http.end();
    return false;
  }
  const String payload = http.getString();
  http.end();
  if (!parseWeatherPayload(payload)) return false;
  updateWeatherUi();
  updateGlanceTile();
  return true;
}

// ---------------------------------------------------------------------------
// Serial command handling
// ---------------------------------------------------------------------------
void handleSerialCommands() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      String cmd = serialCommandBuffer;
      serialCommandBuffer = "";
      cmd.trim(); cmd.toLowerCase();
      if (cmd.isEmpty()) return;

      if (cmd == "clock" || cmd == "c") { showScreen(ScreenIndex::Clock); Serial.println("clock"); return; }
      if (cmd == "weather" || cmd == "w") { showScreen(ScreenIndex::Weather); Serial.println("weather"); return; }
      if (cmd == "toggle" || cmd == "t") { toggleScreen(); Serial.println(currentScreen == ScreenIndex::Clock ? "clock" : "weather"); return; }
      if (cmd == "right" || cmd == ">") { swipeScreen(+1); Serial.println(currentScreen == ScreenIndex::Clock ? "clock" : "weather"); return; }
      if (cmd == "left"  || cmd == "<") { swipeScreen(-1); Serial.println(currentScreen == ScreenIndex::Clock ? "clock" : "weather"); return; }
      if (cmd == "refresh" || cmd == "r") { fetchWeather(); Serial.println("refreshed"); return; }
      if (cmd.length() >= 2 && cmd[0] == 'd') {
        const String v = cmd.substring(1);
        bool ok = !v.isEmpty();
        for (size_t i = 0; i < v.length() && ok; ++i) ok = isdigit((unsigned char)v[i]) != 0;
        if (ok) {
          const int b = v.toInt();
          if (b >= 0 && b <= 255) { setBacklight((uint8_t)b); Serial.printf("backlight %u\n", currentBacklightBrightness); return; }
        }
        Serial.println("use d<0-255>"); return;
      }
      if (cmd == "help" || cmd == "?") {
        Serial.println("commands: clock|c, weather|w, toggle|t, left|<, right|>, refresh|r, d<0-255>, help|?");
        return;
      }
      Serial.printf("unknown: %s\n", cmd.c_str());
      return;
    }
    if (serialCommandBuffer.length() < 64) serialCommandBuffer += ch;
    else { serialCommandBuffer = ""; Serial.println("cmd too long"); return; }
  }
}

// ---------------------------------------------------------------------------
// Boot / tick
// ---------------------------------------------------------------------------
void initLvgl() {
  lv_init();
  drawBufferA = static_cast<lv_color_t *>(
      heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!drawBufferA) {
    drawBufferA = static_cast<lv_color_t *>(
        heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!drawBufferA) {
    Serial.println("LVGL buffer alloc failed");
    while (true) delay(1000);
  }
  lv_disp_draw_buf_init(&drawBuffer, drawBufferA, nullptr, DRAW_BUFFER_PIXEL_COUNT);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_WIDTH;
  dispDrv.ver_res = SCREEN_HEIGHT;
  dispDrv.flush_cb = displayFlush;
  dispDrv.draw_buf = &drawBuffer;
  lv_disp_drv_register(&dispDrv);
}

void initDisplayHardware() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  display.begin();
  display.setRotation(0);
  setBacklight(BACKLIGHT_BRIGHTNESS);
  display.fillScreen(TFT_BLACK);
}

void tickClock() {
  const unsigned long now = millis();
  const long elapsed = (long)((now - lastClockTick) / 1000UL);
  if (elapsed <= 0) return;
  currentLocalEpochUtc += elapsed;
  lastClockTick += (unsigned long)elapsed * 1000UL;
  updateClockFace();
}

void tickColonBlink() {
  const unsigned long now = millis();
  if (now - lastColonBlink < 600) return;
  lastColonBlink = now;
  colonVisible = !colonVisible;
  lv_obj_set_style_text_opa(clockUi.colon, colonVisible ? LV_OPA_COVER : LV_OPA_40, 0);
}

void tickAutoRotate() {
  if (millis() - lastAutoRotate >= AUTO_ROTATE_MS) {
    toggleScreen();
  }
}

unsigned long lastLinkRefresh = 0;

void tickLinkState() {
  const unsigned long now = millis();
  if (now - lastLinkRefresh < 1000) return;
  lastLinkRefresh = now;

  LinkState link = currentLinkState();
  setStatusRowState(clockUi.status, link);
  setStatusRowState(weatherUi.status, link);

  const char *leftLabel = (link == LinkState::Online)  ? "CONNECTED"
                        : (link == LinkState::Offline) ? "OFFLINE"
                                                       : "NO DATA";
  lv_label_set_text(clockUi.status.connected,   leftLabel);
  lv_label_set_text(weatherUi.status.connected, leftLabel);

  // Redraw the glance + weather panels so their labels track link state.
  updateGlanceTile();
  updateWeatherUi();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println("Booting IoT-SmartHub dashboard");
  randomSeed(micros());

  initDisplayHardware();
  initLvgl();

  lv_obj_t *scr = lv_scr_act();
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, hex(COL_BG_0), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  buildClockScreen(scr);
  buildWeatherScreen(scr);
  buildPageDots(scr);
  refreshPageDots();
  showStartupIconPreview(scr);

  lastClockTick = millis();
  lastWeatherAttempt = millis() - WEATHER_REFRESH_MS;
  lastAutoRotate = millis();
  lastColonBlink = millis();

  Serial.println("UI ready. Commands: clock|c, weather|w, toggle|t, left|<, right|>, refresh|r, d<0-255>, help");
  fetchWeather();
}

void loop() {
  const unsigned long now = millis();
  handleSerialCommands();
  tickClock();
  tickColonBlink();
  tickAutoRotate();
  tickLinkState();

  if ((WiFi.status() != WL_CONNECTED && (now - lastWeatherAttempt) >= WIFI_RETRY_MS) ||
      (now - lastWeatherAttempt) >= WEATHER_REFRESH_MS) {
    lastWeatherAttempt = now;
    fetchWeather();
    lastClockTick = millis();
  }

  lv_timer_handler();
  delay(5);
}
