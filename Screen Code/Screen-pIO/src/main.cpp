#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <TJpg_Decoder.h>
#include <time.h>

#include "AppConfig.h"
#include "CrowPanelDisplay.h"
#include "WeatherIconsAssets.h"

// Custom Montserrat font used for the hero clock digits. Generated with
// lv_font_conv from Montserrat-Bold at 180px (digits 0-9 + colon only).
// The C source lives at src/montserrat_hero_180.c.
extern "C" const lv_font_t montserrat_hero_180;

namespace {

// ---------------------------------------------------------------------------
// Display / timing constants
// ---------------------------------------------------------------------------
constexpr uint16_t SCREEN_WIDTH = 800;
constexpr uint16_t SCREEN_HEIGHT = 480;
constexpr uint16_t PORTRAIT_WIDTH = SCREEN_HEIGHT;
constexpr uint16_t PORTRAIT_HEIGHT = SCREEN_WIDTH;
constexpr uint32_t WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t SPOTIFY_PLAYING_REFRESH_MS = 5000UL;
constexpr uint32_t SPOTIFY_PAUSED_REFRESH_MS = 5000UL;
constexpr uint32_t SPOTIFY_IDLE_REFRESH_MS = 5000UL;
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
constexpr size_t DRAW_BUFFER_PIXEL_COUNT = SCREEN_WIDTH * 80;
constexpr uint32_t AUTO_ROTATE_MS = 0UL;
constexpr uint16_t STARTUP_ICON_PREVIEW_SIZE = 32;
constexpr uint8_t STARTUP_ICON_PREVIEW_COLUMNS = 10;

// microSD slot on the CrowPanel 5"/7" ESP32-S3 boards (SPI mode).
constexpr int SD_PIN_CS   = 10;
constexpr int SD_PIN_MOSI = 11;
constexpr int SD_PIN_SCK  = 12;
constexpr int SD_PIN_MISO = 13;

constexpr size_t SD_IMAGE_PIXEL_COUNT = SCREEN_WIDTH * SCREEN_HEIGHT;
constexpr size_t SD_IMAGE_BYTES = SD_IMAGE_PIXEL_COUNT * 2;  // RGB565

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
  Portrait = 2,
  Battery = 3,
  NearbyDevices = 4,
  WorldCup = 5,
  BatteryPortrait = 6,
  NearbyDevicesPortrait = 7,
  WorldCupPortrait = 8,
  Music = 9,    // Reached via "up" from Clock; not part of the swipe carousel.
  MusicPortrait = 10,
  Count = 6,    // Count of screens that participate in swipe rotation.
};

CrowPanelDisplay display(DISPLAY_MODEL);
lv_disp_draw_buf_t drawBuffer;
lv_color_t *drawBufferA = nullptr;
lv_color_t *drawBufferB = nullptr;
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

// SD-backed full-screen image viewer state.
bool sdReady = false;
lv_color_t *sdImageBuffer = nullptr;
lv_obj_t *sdImageOverlay = nullptr;
lv_obj_t *sdImageCanvas = nullptr;

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
  float latitude = 0.0f;
  float longitude = 0.0f;
  int temperature = 58;
  int feelsLike = 56;
  int humidity = 47;
  int pressure = 1020;     // hPa (displayed in inHg)
  int windSpeed = 8;
  int windDeg = 225;       // SW default
  int cloudCover = 0;
  int visibilityKm = 16;
  int airQualityIndex = 0;
  int pm25 = 0;
  int pm10 = 0;
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

struct PortraitClockUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *dayName = nullptr;
  lv_obj_t *dateLong = nullptr;
  lv_obj_t *hh = nullptr;
  lv_obj_t *colon = nullptr;
  lv_obj_t *mm = nullptr;
  lv_obj_t *ampm = nullptr;
  lv_obj_t *weatherIconCanvas = nullptr;
  lv_obj_t *weatherTemp = nullptr;
  lv_obj_t *weatherCond = nullptr;
  lv_obj_t *weatherLoc = nullptr;
  lv_obj_t *nowPlayingTitle = nullptr;
  lv_obj_t *nowPlayingArtist = nullptr;
} portraitUi;
lv_color_t portraitIconBuf[56 * 56];

// ---------------------------------------------------------------------------
#include "features/app_state.inc"
#include "features/ui_common.inc"
#include "features/clock_screen.inc"
#include "features/weather_screen.inc"
#include "features/portrait_screen.inc"
#include "features/battery_screen.inc"
#include "features/music_player.inc"
#include "features/nearby_screen.inc"
#include "features/world_cup_screen.inc"

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

lv_obj_t *rootForScreen(ScreenIndex s) {
  switch (s) {
    case ScreenIndex::Clock: return clockUi.root;
    case ScreenIndex::Weather: return weatherUi.root;
    case ScreenIndex::Portrait: return portraitUi.root;
    case ScreenIndex::Battery: return batteryUi.root;
    case ScreenIndex::NearbyDevices: return nearbyUi.root;
    case ScreenIndex::WorldCup: return wcUi.root;
    case ScreenIndex::BatteryPortrait: return batteryPortraitUi.root;
    case ScreenIndex::NearbyDevicesPortrait: return nearbyPortraitUi.root;
    case ScreenIndex::WorldCupPortrait: return wcPortraitUi.root;
    case ScreenIndex::Music: return musicUi.root;
    case ScreenIndex::MusicPortrait: return musicPortraitUi.root;
    default: return clockUi.root;
  }
}

const char *screenName(ScreenIndex s) {
  switch (s) {
    case ScreenIndex::Clock: return "clock";
    case ScreenIndex::Weather: return "weather";
    case ScreenIndex::Portrait: return "port";
    case ScreenIndex::Battery: return "battery";
    case ScreenIndex::NearbyDevices: return "nearby";
    case ScreenIndex::WorldCup: return "wc";
    case ScreenIndex::BatteryPortrait: return "battery-port";
    case ScreenIndex::NearbyDevicesPortrait: return "nearby-port";
    case ScreenIndex::WorldCupPortrait: return "wc-port";
    case ScreenIndex::Music: return "music";
    case ScreenIndex::MusicPortrait: return "music-portrait";
    default: return "clock";
  }
}

bool isPortraitScreen(ScreenIndex s) {
  return s == ScreenIndex::Portrait || s == ScreenIndex::BatteryPortrait ||
         s == ScreenIndex::NearbyDevicesPortrait ||
         s == ScreenIndex::WorldCupPortrait ||
         s == ScreenIndex::MusicPortrait;
}

void applyScreenOrientation(ScreenIndex s) {
  const bool portrait = isPortraitScreen(s);
  display.setRotation(portrait ? 1 : 0);
  lv_disp_set_rotation(lv_disp_get_default(), portrait ? LV_DISP_ROT_90 : LV_DISP_ROT_NONE);
}

// direction: 0 = no slide (instant), -1 = new screen enters from left,
// +1 = new screen enters from right.
void showScreen(ScreenIndex s, int direction = 0) {
  ScreenIndex prev = currentScreen;
  currentScreen = s;

  if (isPortraitScreen(prev) != isPortraitScreen(s)) direction = 0;
  applyScreenOrientation(s);
  if (isMusicScreen(s)) {
    ensureMusicBackground();
    ensureMusicCover();
  }

  lv_obj_t *incoming = rootForScreen(s);
  lv_obj_t *outgoing = rootForScreen(prev);

  lv_obj_clear_flag(incoming, LV_OBJ_FLAG_HIDDEN);

  const lv_coord_t screenSpan = lv_disp_get_hor_res(nullptr);
  if (direction == 0 || incoming == outgoing) {
    lv_obj_set_x(incoming, 0);
    if (incoming != outgoing) lv_obj_add_flag(outgoing, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_set_x(incoming, direction > 0 ? screenSpan : -screenSpan);
    animX(incoming, direction > 0 ? screenSpan : -screenSpan, 0);
    animX(outgoing, 0, direction > 0 ? -screenSpan : screenSpan);
    // outgoing will keep its off-screen x; re-hide it after the slide so it
    // doesn't eat input. We can't easily hook "anim complete" without a
    // callback struct, so just hide it when the next showScreen runs.
  }

  // Hide the previously-outgoing screen's stale positions from earlier
  // toggles, other than the one we're animating right now.
  lv_obj_t *roots[] = {clockUi.root, weatherUi.root, portraitUi.root,
                       batteryUi.root, batteryPortraitUi.root,
                       nearbyUi.root, nearbyPortraitUi.root,
                       wcUi.root, wcPortraitUi.root,
                       musicUi.root, musicPortraitUi.root};
  for (lv_obj_t *other : roots) {
    if (other && other != incoming && other != outgoing) {
      lv_obj_add_flag(other, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(other, 0);
    }
  }

  lv_obj_invalidate(lv_scr_act());
  lastAutoRotate = millis();
}

void toggleScreen() {
  if (currentScreen == ScreenIndex::Clock) showScreen(ScreenIndex::Weather, +1);
  else showScreen(ScreenIndex::Clock, +1);
}

// Swipe by logical direction: +1 moves to the next page (content slides left),
// -1 moves to the previous page (content slides right). The carousel wraps.
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
  const long localEpoch = utcEpoch + weatherData.timezoneOffset;
  const long localMinute = localEpoch / 60L;
  if (utcEpoch > 0 && localMinute == lastRenderedClockMinute) return;

  const time_t t = static_cast<time_t>(utcEpoch + weatherData.timezoneOffset);
  struct tm tm; gmtime_r(&t, &tm);

  int h = tm.tm_hour;
  String ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12; if (h12 == 0) h12 = 12;

  if (utcEpoch > 0) {
    lastRenderedClockMinute = localMinute;
    lv_label_set_text(clockUi.hh, pad2(h12).c_str());
    lv_label_set_text(clockUi.mm, pad2(tm.tm_min).c_str());
    lv_label_set_text(clockUi.ampm, ampm.c_str());
    lv_label_set_text(portraitUi.hh, pad2(h12).c_str());
    lv_label_set_text(portraitUi.mm, pad2(tm.tm_min).c_str());
    lv_label_set_text(portraitUi.ampm, ampm.c_str());

    static const char *days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char *months[] = {"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE","JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};
    lv_label_set_text(clockUi.dayName, days[tm.tm_wday]);
    lv_label_set_text(portraitUi.dayName, days[tm.tm_wday]);
    char db[48];
    snprintf(db, sizeof(db), "%d  %s  %d", tm.tm_mday, months[tm.tm_mon], 1900 + tm.tm_year);
    lv_label_set_text(clockUi.dateLong, db);
    lv_label_set_text(portraitUi.dateLong, db);
  }
}

enum class LinkState { Online, Offline, NoData };
enum class WifiIndicatorState { Connected, Connecting, Offline };

LinkState currentLinkState() {
  if (WiFi.status() != WL_CONNECTED) return LinkState::Offline;
  if (!weatherData.valid) return LinkState::NoData;
  return LinkState::Online;
}

WifiIndicatorState currentWifiIndicatorState() {
  if (WiFi.status() == WL_CONNECTED) return WifiIndicatorState::Connected;
  if (wifiConnectInProgress) return WifiIndicatorState::Connecting;
  return WifiIndicatorState::Offline;
}

const char *wifiIndicatorLabel(WifiIndicatorState s) {
  switch (s) {
    case WifiIndicatorState::Connected: return "Connected";
    case WifiIndicatorState::Connecting: return "Connecting";
    case WifiIndicatorState::Offline: return "Offline";
  }
  return "Offline";
}

void setStatusRowWifiState(StatusRow &row, WifiIndicatorState s) {
  if (!row.dot) return;
  switch (s) {
    case WifiIndicatorState::Connected:
      lv_obj_set_style_bg_color(row.dot, hex(COL_ACCENT), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
    case WifiIndicatorState::Connecting:
      lv_obj_set_style_bg_color(row.dot, hex(0xE0A040), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
    case WifiIndicatorState::Offline:
      lv_obj_set_style_bg_color(row.dot, hex(0xE06060), 0);
      lv_obj_set_style_bg_opa(row.dot, LV_OPA_COVER, 0);
      break;
  }
}

void updateWifiStatusIndicator(WifiIndicatorState wifi) {
  setStatusRowWifiState(clockUi.status, wifi);
  setStatusRowWifiState(weatherUi.status, wifi);
  setStatusRowWifiState(portraitUi.status, wifi);
  setStatusRowWifiState(batteryUi.status, wifi);
  setStatusRowWifiState(batteryPortraitUi.status, wifi);
  setStatusRowWifiState(nearbyUi.status, wifi);
  setStatusRowWifiState(nearbyPortraitUi.status, wifi);
  setStatusRowWifiState(wcUi.status, wifi);
  setStatusRowWifiState(wcPortraitUi.status, wifi);
  setStatusRowWifiState(musicUi.status, wifi);
  setStatusRowWifiState(musicPortraitUi.status, wifi);

  const char *label = wifiIndicatorLabel(wifi);
  if (clockUi.status.connected) lv_label_set_text(clockUi.status.connected, label);
  if (weatherUi.status.connected) lv_label_set_text(weatherUi.status.connected, label);
  if (portraitUi.status.connected) lv_label_set_text(portraitUi.status.connected, label);
  if (batteryUi.status.connected) lv_label_set_text(batteryUi.status.connected, label);
  if (batteryPortraitUi.status.connected) lv_label_set_text(batteryPortraitUi.status.connected, label);
  if (nearbyUi.status.connected) lv_label_set_text(nearbyUi.status.connected, label);
  if (nearbyPortraitUi.status.connected) lv_label_set_text(nearbyPortraitUi.status.connected, label);
  if (wcUi.status.connected) lv_label_set_text(wcUi.status.connected, label);
  if (wcPortraitUi.status.connected) lv_label_set_text(wcPortraitUi.status.connected, label);
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

void updatePortraitClockUi() {
  LinkState link = currentLinkState();

  if (link == LinkState::Offline) {
    if (portraitUi.status.rightInfo) lv_label_set_text(portraitUi.status.rightInfo, "--\xC2\xB0  \xE2\x80\xA2  41%");
    lv_label_set_text(portraitUi.weatherTemp, "--\xC2\xB0");
    lv_label_set_text(portraitUi.weatherCond, "NO WI-FI");
    lv_label_set_text(portraitUi.weatherLoc, "OFFLINE");
    renderIconToBuffer(741, "50d", portraitIconBuf, 24, 24);
    lv_obj_invalidate(portraitUi.weatherIconCanvas);
    return;
  }

  if (link == LinkState::NoData) {
    if (portraitUi.status.rightInfo) lv_label_set_text(portraitUi.status.rightInfo, "--\xC2\xB0  \xE2\x80\xA2  41%");
    lv_label_set_text(portraitUi.weatherTemp, "--\xC2\xB0");
    lv_label_set_text(portraitUi.weatherCond, "WAITING FOR DATA");
    lv_label_set_text(portraitUi.weatherLoc, "NO DATA");
    renderIconToBuffer(803, "03d", portraitIconBuf, 24, 24);
    lv_obj_invalidate(portraitUi.weatherIconCanvas);
    return;
  }

  String tempText = String(weatherData.temperature) + "\xC2\xB0";
  if (portraitUi.status.rightInfo) lv_label_set_text(portraitUi.status.rightInfo, (tempText + "  \xE2\x80\xA2  41%").c_str());
  lv_label_set_text(portraitUi.weatherTemp, tempText.c_str());

  String cond = weatherData.condition;
  cond.toUpperCase();
  lv_label_set_text(portraitUi.weatherCond, cond.c_str());

  String loc = weatherData.location;
  int comma = loc.indexOf(',');
  if (comma > 0) loc = loc.substring(0, comma);
  loc.toUpperCase();
  lv_label_set_text(portraitUi.weatherLoc, loc.c_str());

  const char *iconCode = weatherData.iconCode.isEmpty() ? "01d" : weatherData.iconCode.c_str();
  renderIconToBuffer(weatherData.conditionId, iconCode, portraitIconBuf, 24, 24);
  lv_obj_invalidate(portraitUi.weatherIconCanvas);
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
    lv_label_set_text(weatherUi.coordLabel, "");
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
    lv_label_set_text(weatherUi.aqiSub, "--");
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
  char coordBuf[32];
  snprintf(coordBuf, sizeof(coordBuf), "%.2f, %.2f", weatherData.latitude, weatherData.longitude);
  lv_label_set_text(weatherUi.coordLabel, coordBuf);

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

  if (weatherData.airQualityIndex > 0) {
    String aqiValText = String(weatherData.airQualityIndex);
    lv_label_set_text(weatherUi.aqiVal, aqiValText.c_str());
  } else {
    lv_label_set_text(weatherUi.aqiVal, "--");
  }
  if (weatherData.airQualityIndex > 0) {
    char aqiSubBuf[32];
    snprintf(aqiSubBuf, sizeof(aqiSubBuf), "%s  PM %.0f/%.0f",
             airQualityLabel(weatherData.airQualityIndex),
             static_cast<double>(weatherData.pm25),
             static_cast<double>(weatherData.pm10));
    lv_label_set_text(weatherUi.aqiSub, aqiSubBuf);
    lv_arc_set_value(weatherUi.aqiArc, weatherData.airQualityIndex * 20);
  } else {
    lv_label_set_text(weatherUi.aqiSub, "--");
    lv_arc_set_value(weatherUi.aqiArc, 0);
  }

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
  renderIconToBuffer(weatherData.conditionId, iconCode, condIconBuf, 144, 144);
  lv_obj_invalidate(weatherUi.condIconCanvas);
}

// ---------------------------------------------------------------------------
#include "features/weather_network.inc"

#include "features/spotify_api.inc"

#include "features/world_cup_api.inc"

#include "features/sd_image_viewer.inc"

#include "features/serial_commands.inc"

#include "features/screen_connectivity.inc"

// Boot / tick
// ---------------------------------------------------------------------------
void initLvgl() {
  lv_init();
  condIconBuf = static_cast<lv_color_t *>(
      heap_caps_malloc(144 * 144 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!condIconBuf) {
    condIconBuf = static_cast<lv_color_t *>(
        heap_caps_malloc(144 * 144 * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!condIconBuf) {
    Serial.println("Weather icon buffer alloc failed");
    while (true) delay(1000);
  }

  drawBufferA = static_cast<lv_color_t *>(
      heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  drawBufferB = static_cast<lv_color_t *>(
      heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (!drawBufferA || !drawBufferB) {
    if (drawBufferA) { heap_caps_free(drawBufferA); drawBufferA = nullptr; }
    if (drawBufferB) { heap_caps_free(drawBufferB); drawBufferB = nullptr; }
    drawBufferA = static_cast<lv_color_t *>(
        heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!drawBufferA) {
    Serial.println("LVGL buffer alloc failed");
    while (true) delay(1000);
  }
  lv_disp_draw_buf_init(&drawBuffer, drawBufferA, drawBufferB, DRAW_BUFFER_PIXEL_COUNT);

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
  lv_obj_t *activeColon = nullptr;
  if (currentScreen == ScreenIndex::Clock) activeColon = clockUi.colon;
  else if (currentScreen == ScreenIndex::Portrait) activeColon = portraitUi.colon;

  if (activeColon) {
    // Toggle visibility instead of swapping text. The custom hero font only
    // contains 0-9 and ':', so asking it to render a space would fall back
    // to LVGL's placeholder outline rectangle.
    if (colonVisible) lv_obj_clear_flag(activeColon, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(activeColon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(activeColon);
  }
}

void tickAutoRotate() {
  if (AUTO_ROTATE_MS > 0 && millis() - lastAutoRotate >= AUTO_ROTATE_MS) {
    toggleScreen();
  }
}

void tickMusic() {
  const unsigned long now = millis();

  // Advance playback once per second when playing.
  if (spotify.valid) {
    if (isMusicScreen(currentScreen)) updateMusicProgress();
  } else if (musicIsPlaying) {
    if (now - musicLastTickMs >= 1000UL) {
      const unsigned long stepSec = (now - musicLastTickMs) / 1000UL;
      musicLastTickMs += stepSec * 1000UL;
      musicElapsedSec += (int)stepSec;
      const int duration = musicCurrentTrack().durationSec;
      if (musicElapsedSec >= duration) {
        musicTrackIndex = (musicTrackIndex + 1) % kMusicTrackCount;
        musicElapsedSec = 0;
        if (isMusicScreen(currentScreen)) updateMusicTrackUi();
      }
      if (isMusicScreen(currentScreen)) updateMusicProgress();
    }
  } else {
    // Keep the baseline current so the next "play" doesn't instantly jump.
    musicLastTickMs = now;
  }

  // Animate the colorful soundbars. Only touch LVGL objects when the Music
  // screen is the active surface — no point repainting while hidden.
  if (!isMusicScreen(currentScreen)) return;
  MusicUi &ui = activeMusicUi();
  if (!ui.soundbars[0]) return;
  if (now - musicLastBarAnimMs < 80UL) return;
  musicLastBarAnimMs = now;

  const int baseY = ui.soundbarBaseY;
  const int maxH = ui.soundbarHeight;
  const int minH = 10;

  if (!musicCurrentlyPlaying()) {
    updateMusicSoundbarVisibility();
    return;
  }
  updateMusicSoundbarVisibility();

  for (int i = 0; i < kMusicSoundbarCount; ++i) {
    int h = minH;
    if (musicCurrentlyPlaying()) {
      // Pseudo-random oscillation using a per-bar phase advanced each tick.
      // Three detuned sinusoids summed → a lively bouncing pattern without
      // needing real audio data.
      musicSoundbarPhase[i] += (uint8_t)(17 + (i * 5));
      const uint8_t p = musicSoundbarPhase[i];
      const int s1 = (int)lv_trigo_sin((int16_t)(p * 360 / 256));
      const int s2 = (int)lv_trigo_sin((int16_t)((p * 2 + i * 40) * 360 / 256));
      const int s3 = (int)lv_trigo_sin((int16_t)((p / 2 + i * 90) * 360 / 256));
      // lv_trigo_sin returns a value in [-32767, 32767]. Normalize to [0, 1000].
      const int mix = (s1 + s2 + s3) / 3;                 // [-32767, 32767]
      const int norm = (mix + 32768) * 1000 / 65536;      // [0, 1000]
      h = minH + (maxH - minH) * norm / 1000;
    }
    lv_obj_set_height(ui.soundbars[i], h);
    lv_obj_set_y(ui.soundbars[i], baseY - h);
  }
}

unsigned long lastLinkRefresh = 0;
unsigned long lastWcUpdatedLabelTick = 0;

void tickWorldCup() {
  if (wcUiDirty) applyWcUiUpdate();

  const unsigned long now = millis();
  if (footballDataConfigured() && !wcPollInFlight) {
    // Hold off the first fetch for 30s post-boot so weather + spotify can
    // finish their initial SSL handshakes and the heap can settle. After
    // that, refresh once per minute.
    const bool firstFetchDue =
        (wcMatch.lastFetchMs == 0) && (now >= 30UL * 1000UL);
    const unsigned long sinceFetch = now - wcMatch.lastFetchMs;
    const bool periodicDue =
        (wcMatch.lastFetchMs != 0) && (sinceFetch >= 60UL * 1000UL);
    if (firstFetchDue || periodicDue) {
      // mbedtls needs ~32KB of contiguous DMA-capable heap for the SSL
      // handshake. Skip this round if we're below a safe floor — we'll
      // try again on the next tick once whatever is holding the heap
      // (usually a concurrent Spotify poll) finishes.
      const size_t largest =
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (largest < 50U * 1024U) {
        static unsigned long lastSkipLog = 0;
        if (now - lastSkipLog > 10000UL) {
          Serial.printf("[wc] skip — internal largest block %u (need 50k)\n",
                        (unsigned)largest);
          lastSkipLog = now;
        }
        // Retry sooner than the full 60s — bump lastFetchMs forward so the
        // next attempt fires in ~5s.
        if (wcMatch.lastFetchMs != 0) wcMatch.lastFetchMs = now - 55UL * 1000UL;
      } else {
        startWcPollTask();
      }
    }
  }

  // Tick the "updated Nm ago" label every 30s so it stays current even when
  // we're not refetching.
  if (wcMatch.valid && now - lastWcUpdatedLabelTick >= 30000UL) {
    lastWcUpdatedLabelTick = now;
    updateWorldCupUi();
  }
}

// Schedule a weather refresh on a worker task. The fetch + SSL handshake
// blocks for several seconds, so it can't live on core 1 next to LVGL.
// Mirrors the spotify/wc pattern: write into weatherData on the worker,
// flip weatherUiDirty, apply on the next main-loop tick.
void tickWeather() {
  if (weatherUiDirty) applyWeatherUiUpdate();
  if (weatherPollInFlight) return;

  const unsigned long now = millis();
  const bool disconnected = (WiFi.status() != WL_CONNECTED);
  const unsigned long interval = disconnected ? WIFI_RETRY_MS : WEATHER_REFRESH_MS;
  if (now - lastWeatherAttempt >= interval) {
    lastWeatherAttempt = now;
    startWeatherPollTask();
  }
}

void tickLinkState() {
  const unsigned long now = millis();
  if (now - lastLinkRefresh < 1000) return;
  lastLinkRefresh = now;

  LinkState link = currentLinkState();
  WifiIndicatorState wifi = currentWifiIndicatorState();
  if (static_cast<int>(link) == lastRenderedLinkState &&
      static_cast<int>(wifi) == lastRenderedWifiStatus) {
    return;
  }
  lastRenderedLinkState = static_cast<int>(link);
  lastRenderedWifiStatus = static_cast<int>(wifi);

  updateWifiStatusIndicator(wifi);

  // Music screen keeps its "NOW PLAYING · SPOTIFY" label unchanged — the
  // accent dot + right-hand info already cover link state for that screen.

  // Redraw the glance + weather panels so their labels track link state.
  updateGlanceTile();
  updateWeatherUi();
  updatePortraitClockUi();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial1.begin(MAIN_UART_BAUD, SERIAL_8N1, MAIN_UART_RX_PIN, MAIN_UART_TX_PIN);
  delay(150);
  Serial.println();
  Serial.println("Booting IoT-SmartHub dashboard");
  Serial.printf("Main UART on RX=%d TX=%d @ %u baud\n",
                MAIN_UART_RX_PIN, MAIN_UART_TX_PIN,
                static_cast<unsigned>(MAIN_UART_BAUD));
  randomSeed(micros());

  initDisplayHardware();
  initLvgl();
  sdReady = initSdCard();

  lv_obj_t *scr = lv_scr_act();
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, hex(COL_BG_0), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  buildClockScreen(scr);
  buildWeatherScreen(scr);
  buildPortraitClockScreen(scr);
  buildBatteryScreen(scr);
  buildBatteryPortraitScreen(scr);
  buildNearbyScreen(scr);
  buildNearbyPortraitScreen(scr);
  buildWorldCupScreen(scr);
  buildWorldCupPortraitScreen(scr);
  buildMusicScreen(scr);
  buildMusicPortraitScreen(scr);
  showScreen(ScreenIndex::Clock, 0);
  updateWifiStatusIndicator(WifiIndicatorState::Connecting);
  lastRenderedWifiStatus = static_cast<int>(WifiIndicatorState::Connecting);
  showStartupIconPreview(scr);

  updateMusicTrackUi();
  updateMusicPlayIcon();
  updateMusicProgress();
  musicLastTickMs = millis();
  musicLastBarAnimMs = millis();

  lastClockTick = millis();
  lastWeatherAttempt = millis();
  lastAutoRotate = millis();
  lastColonBlink = millis();

  updateClockFace();
  updateGlanceTile();
  updateWeatherUi();
  updatePortraitClockUi();
  updateBatteryUi();

  Serial.println("UI ready. Commands: clock|c, weather|w, port, battery|b, battery-port|bp, nearby|n, nearby-port|np, wc, wc-port|wcp, music|m, rotate, toggle|t, up|^, down|v, left|<, right|>, refresh|r, spotify|sp, dev<number> <0-100>, d<0-255>, show <name>, hide, help");
  beginScreenConnectivity();
  startWeatherPollTask();
  startSpotifyPollTask();
}

void loop() {
  handleSerialCommands();
  tickClock();
  tickColonBlink();
  tickAutoRotate();
  tickMusic();
  tickSpotify();
  tickWorldCup();
  tickWeather();
  tickLinkState();
  tickScreenConnectivity();

  lv_timer_handler();
  delay(5);
}
