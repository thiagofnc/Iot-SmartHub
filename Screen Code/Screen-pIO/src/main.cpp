#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <string.h>
#include <time.h>

#include "AppConfig.h"
#include "CrowPanelDisplay.h"
#include "OpenWeatherIconAssets.h"

namespace {

constexpr uint16_t SCREEN_WIDTH = 800;
constexpr uint16_t SCREEN_HEIGHT = 480;
constexpr uint32_t WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t WIFI_RETRY_MS = 15000UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 15000UL;
constexpr uint32_t STARTUP_COLOR_TEST_MS = 400UL;
constexpr int BACKLIGHT_PIN = 2;
constexpr int BACKLIGHT_CHANNEL = 1;
constexpr int BACKLIGHT_FREQUENCY_HZ = 300;
constexpr int BACKLIGHT_RESOLUTION_BITS = 8;
constexpr uint8_t BACKLIGHT_BRIGHTNESS = 255;
constexpr size_t DRAW_BUFFER_PIXEL_COUNT = SCREEN_WIDTH * 40;
constexpr uint16_t STARTUP_GALLERY_ICON_SIZE = 32;
constexpr uint32_t STARTUP_GALLERY_MS = 5000UL;
constexpr uint16_t LIFE_CELL_SIZE = 4;
constexpr uint16_t LIFE_COLS = SCREEN_WIDTH / LIFE_CELL_SIZE;
constexpr uint16_t LIFE_ROWS = SCREEN_HEIGHT / LIFE_CELL_SIZE;
constexpr uint32_t LIFE_STEP_MS = 90UL;
constexpr uint8_t LIFE_SEED_DENSITY_PERCENT = 35;
constexpr uint16_t LIFE_RESEED_GENERATIONS = 128;

// Elecrow's 5-inch and 7-inch 800x480 HMI boards use different RGB mappings.
// If your panel is the 7-inch model, change this to CrowPanelModel::SevenInch.
constexpr CrowPanelModel DISPLAY_MODEL = CrowPanelModel::SevenInch;

enum class ScreenMode {
  Weather,
  Clock,
  Life,
};

CrowPanelDisplay display(DISPLAY_MODEL);
lv_disp_draw_buf_t drawBuffer;
lv_color_t *drawBufferA = nullptr;
lv_color_t weatherIconCanvasBuffer[kOpenWeatherIconWidth * kOpenWeatherIconHeight];
constexpr const char *kStartupGalleryCodes[] = {
    "01d", "01n", "02d", "02n", "03d", "03n", "04d", "04n", "09d",
    "09n", "10d", "10n", "11d", "11n", "13d", "13n", "50d", "50n",
};
constexpr size_t kStartupGalleryCount = sizeof(kStartupGalleryCodes) / sizeof(kStartupGalleryCodes[0]);
lv_color_t startupGalleryBuffers[kStartupGalleryCount][STARTUP_GALLERY_ICON_SIZE * STARTUP_GALLERY_ICON_SIZE];

struct WeatherData {
  bool valid = false;
  String location;
  String condition;
  String description;
  String iconCode;
  String unitsLabel;
  String lastError;
  int temperature = 0;
  int feelsLike = 0;
  int humidity = 0;
  int pressure = 0;
  int windSpeed = 0;
  int windDeg = 0;
  int cloudCover = 0;
  int visibilityKm = 0;
  long sunrise = 0;
  long sunset = 0;
  long observedAt = 0;
  long timezoneOffset = 0;
};

struct DashboardUi {
  lv_obj_t *dashboardRoot = nullptr;
  lv_obj_t *hero = nullptr;
  lv_obj_t *iconCard = nullptr;
  lv_obj_t *iconTitle = nullptr;
  lv_obj_t *iconCanvas = nullptr;
  lv_obj_t *status = nullptr;
  lv_obj_t *clock = nullptr;
  lv_obj_t *location = nullptr;
  lv_obj_t *condition = nullptr;
  lv_obj_t *description = nullptr;
  lv_obj_t *temperature = nullptr;
  lv_obj_t *feelsLike = nullptr;
  lv_obj_t *humidity = nullptr;
  lv_obj_t *wind = nullptr;
  lv_obj_t *pressure = nullptr;
  lv_obj_t *sunrise = nullptr;
  lv_obj_t *sunset = nullptr;
  lv_obj_t *visibility = nullptr;
} ui;

struct ClockUi {
  lv_obj_t *clockRoot = nullptr;
  lv_obj_t *time = nullptr;
  lv_obj_t *date = nullptr;
  lv_obj_t *location = nullptr;
  lv_obj_t *footer = nullptr;
} clockUi;

WeatherData weatherData;
ScreenMode currentScreenMode = ScreenMode::Weather;
unsigned long lastWeatherAttempt = 0;
unsigned long lastClockTick = 0;
long currentLocalEpochUtc = 0;
unsigned long lastLifeStep = 0;
uint32_t lifeGeneration = 0;
uint8_t *lifeCurrent = nullptr;
uint8_t *lifeNext = nullptr;
uint16_t *lifeFrameBuffer = nullptr;

String formatLocalTime(long epochUtc, long timezoneOffsetSeconds) {
  if (epochUtc <= 0) {
    return "--:--";
  }

  const time_t localEpoch = static_cast<time_t>(epochUtc + timezoneOffsetSeconds);
  struct tm timeInfo;
  gmtime_r(&localEpoch, &timeInfo);

  char buffer[24];
  strftime(buffer, sizeof(buffer), "%I:%M %p", &timeInfo);
  return String(buffer);
}

String formatLocalDate(long epochUtc, long timezoneOffsetSeconds) {
  if (epochUtc <= 0) {
    return "Waiting for time";
  }

  const time_t localEpoch = static_cast<time_t>(epochUtc + timezoneOffsetSeconds);
  struct tm timeInfo;
  gmtime_r(&localEpoch, &timeInfo);

  char buffer[48];
  strftime(buffer, sizeof(buffer), "%A, %B %d", &timeInfo);
  return String(buffer);
}

String formatObservedLine() {
  if (!weatherData.valid) {
    return "Waiting for weather data";
  }

  String line = "Observed ";
  line += formatLocalTime(weatherData.observedAt, weatherData.timezoneOffset);
  line += "  |  Refresh ";
  const unsigned long elapsed = millis() - lastWeatherAttempt;
  const unsigned long remaining = (elapsed >= WEATHER_REFRESH_MS) ? 0 : (WEATHER_REFRESH_MS - elapsed);
  line += String((remaining / 1000UL) / 60UL);
  line += "m";
  return line;
}

String urlEncode(const String &value) {
  String encoded;
  encoded.reserve(value.length() * 3);

  static const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~' || c == ',') {
      encoded += static_cast<char>(c);
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
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

String toLowerCopy(const String &value) {
  String lower = value;
  lower.toLowerCase();
  return lower;
}

void displayFlush(lv_disp_drv_t *dispDrv, const lv_area_t *area, lv_color_t *colorP) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  display.pushImage(area->x1, area->y1, width, height, reinterpret_cast<uint16_t *>(colorP));
  lv_disp_flush_ready(dispDrv);
}

void setBacklight(uint8_t brightness) {
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQUENCY_HZ, BACKLIGHT_RESOLUTION_BITS);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
  ledcWrite(BACKLIGHT_CHANNEL, brightness);
}

void renderIconToBuffer(const char *iconCode, lv_color_t *buffer, uint16_t width, uint16_t height) {
  const uint8_t *rgbData = getOpenWeatherIconAsset(iconCode);

  for (uint16_t y = 0; y < height; ++y) {
    const uint16_t srcY = static_cast<uint16_t>((static_cast<uint32_t>(y) * kOpenWeatherIconHeight) / height);
    for (uint16_t x = 0; x < width; ++x) {
      const uint16_t srcX = static_cast<uint16_t>((static_cast<uint32_t>(x) * kOpenWeatherIconWidth) / width);
      const size_t srcOffset = (static_cast<size_t>(srcY) * kOpenWeatherIconWidth + srcX) * 3;
      buffer[static_cast<size_t>(y) * width + x] =
          lv_color_make(rgbData[srcOffset], rgbData[srcOffset + 1], rgbData[srcOffset + 2]);
    }
  }
}

lv_obj_t *createStyledBlock(lv_obj_t *parent, lv_color_t bgColor, lv_opa_t bgOpacity, int radius) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_style_bg_color(obj, bgColor, 0);
  lv_obj_set_style_bg_opa(obj, bgOpacity, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  return obj;
}

uint16_t wrapLifeX(int value) {
  if (value < 0) {
    return LIFE_COLS - 1;
  }
  if (value >= LIFE_COLS) {
    return 0;
  }
  return static_cast<uint16_t>(value);
}

uint16_t wrapLifeY(int value) {
  if (value < 0) {
    return LIFE_ROWS - 1;
  }
  if (value >= LIFE_ROWS) {
    return 0;
  }
  return static_cast<uint16_t>(value);
}

size_t lifeIndex(uint16_t row, uint16_t col) {
  return static_cast<size_t>(row) * LIFE_COLS + col;
}

void initLifeBuffers() {
  const size_t cellCount = static_cast<size_t>(LIFE_ROWS) * LIFE_COLS;
  lifeCurrent = static_cast<uint8_t *>(heap_caps_malloc(cellCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lifeNext = static_cast<uint8_t *>(heap_caps_malloc(cellCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lifeFrameBuffer = static_cast<uint16_t *>(
      heap_caps_malloc(static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (lifeCurrent == nullptr || lifeNext == nullptr || lifeFrameBuffer == nullptr) {
    Serial.println("Failed to allocate Game of Life buffers");
    while (true) {
      delay(1000);
    }
  }

  memset(lifeCurrent, 0, cellCount);
  memset(lifeNext, 0, cellCount);
  memset(lifeFrameBuffer, 0, static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * sizeof(uint16_t));
}

uint8_t countLifeNeighbors(uint16_t row, uint16_t col) {
  uint8_t neighbors = 0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      neighbors += lifeCurrent[lifeIndex(wrapLifeY(static_cast<int>(row) + dy),
                                         wrapLifeX(static_cast<int>(col) + dx))]
                       ? 1
                       : 0;
    }
  }
  return neighbors;
}

uint16_t rainbowColor565(uint8_t phase) {
  const uint8_t segment = phase / 43;
  const uint8_t offset = (phase % 43) * 6;

  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  switch (segment) {
    case 0:
      red = 255;
      green = offset;
      blue = 0;
      break;
    case 1:
      red = static_cast<uint8_t>(255 - offset);
      green = 255;
      blue = 0;
      break;
    case 2:
      red = 0;
      green = 255;
      blue = offset;
      break;
    case 3:
      red = 0;
      green = static_cast<uint8_t>(255 - offset);
      blue = 255;
      break;
    case 4:
      red = offset;
      green = 0;
      blue = 255;
      break;
    default:
      red = 255;
      green = 0;
      blue = static_cast<uint8_t>(255 - offset);
      break;
  }

  return display.color565(red, green, blue);
}

void seedLifeBoard() {
  for (uint16_t row = 0; row < LIFE_ROWS; ++row) {
    for (uint16_t col = 0; col < LIFE_COLS; ++col) {
      lifeCurrent[lifeIndex(row, col)] = (random(100) < LIFE_SEED_DENSITY_PERCENT) ? 1 : 0;
      lifeNext[lifeIndex(row, col)] = 0;
    }
  }
  lifeGeneration = 0;
}

void renderLifeBoard() {
  for (uint16_t row = 0; row < LIFE_ROWS; ++row) {
    const uint16_t yStart = row * LIFE_CELL_SIZE;
    for (uint16_t col = 0; col < LIFE_COLS; ++col) {
      uint16_t color = TFT_BLACK;
      if (lifeCurrent[lifeIndex(row, col)]) {
        const uint8_t hue =
            static_cast<uint8_t>((col * 7 + row * 11 + static_cast<uint16_t>(lifeGeneration * 5)) & 0xFF);
        color = rainbowColor565(hue);
      }
      const uint16_t xStart = col * LIFE_CELL_SIZE;
      for (uint16_t py = 0; py < LIFE_CELL_SIZE; ++py) {
        uint16_t *line = lifeFrameBuffer + static_cast<size_t>(yStart + py) * SCREEN_WIDTH + xStart;
        for (uint16_t px = 0; px < LIFE_CELL_SIZE; ++px) {
          line[px] = color;
        }
      }
    }
  }
  display.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, lifeFrameBuffer);
}

void stepLifeBoard() {
  uint16_t aliveCount = 0;
  for (uint16_t row = 0; row < LIFE_ROWS; ++row) {
    for (uint16_t col = 0; col < LIFE_COLS; ++col) {
      const uint8_t neighbors = countLifeNeighbors(row, col);
      const bool alive = lifeCurrent[lifeIndex(row, col)] > 0;
      const bool nextAlive = (alive && (neighbors == 2 || neighbors == 3)) || (!alive && neighbors == 3);
      lifeNext[lifeIndex(row, col)] = nextAlive ? 1 : 0;
      aliveCount += nextAlive ? 1 : 0;
    }
  }

  const size_t cellCount = static_cast<size_t>(LIFE_ROWS) * LIFE_COLS;
  memcpy(lifeCurrent, lifeNext, cellCount);
  memset(lifeNext, 0, cellCount);
  ++lifeGeneration;

  if (aliveCount == 0 || (lifeGeneration % LIFE_RESEED_GENERATIONS) == 0) {
    seedLifeBoard();
  }
}

void enterWeatherMode() {
  currentScreenMode = ScreenMode::Weather;
  lv_obj_clear_flag(ui.dashboardRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(clockUi.clockRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(lv_scr_act());
  lv_timer_handler();
}

void enterClockMode() {
  currentScreenMode = ScreenMode::Clock;
  lv_obj_add_flag(ui.dashboardRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(clockUi.clockRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(lv_scr_act());
  lv_timer_handler();
}

void enterLifeMode() {
  currentScreenMode = ScreenMode::Life;
  lv_obj_add_flag(ui.dashboardRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(clockUi.clockRoot, LV_OBJ_FLAG_HIDDEN);
  seedLifeBoard();
  renderLifeBoard();
  lastLifeStep = millis();
}

void toggleScreenMode() {
  if (currentScreenMode == ScreenMode::Weather) {
    enterClockMode();
  } else if (currentScreenMode == ScreenMode::Clock) {
    enterLifeMode();
  } else {
    enterWeatherMode();
  }
}

void updateClockScreenUi() {
  lv_label_set_text(clockUi.time, formatLocalTime(currentLocalEpochUtc, weatherData.timezoneOffset).c_str());
  lv_label_set_text(clockUi.date, formatLocalDate(currentLocalEpochUtc, weatherData.timezoneOffset).c_str());

  if (weatherData.valid) {
    lv_label_set_text(clockUi.location, weatherData.location.c_str());
    String footer = weatherData.condition + "  |  " + String(weatherData.temperature) + weatherData.unitsLabel;
    lv_label_set_text(clockUi.footer, footer.c_str());
  } else {
    lv_label_set_text(clockUi.location, "Clock");
    lv_label_set_text(clockUi.footer, "Waiting for weather data");
  }
}

void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "weather" || command == "w") {
    enterWeatherMode();
    Serial.println("Screen mode: weather");
    return;
  }

  if (command == "life" || command == "l") {
    enterLifeMode();
    Serial.println("Screen mode: life");
    return;
  }

  if (command == "clock" || command == "c") {
    enterClockMode();
    Serial.println("Screen mode: clock");
    return;
  }

  if (command == "toggle" || command == "t") {
    toggleScreenMode();
    Serial.printf("Screen mode: %s\n",
                  currentScreenMode == ScreenMode::Weather ? "weather"
                  : currentScreenMode == ScreenMode::Clock   ? "clock"
                                                             : "life");
    return;
  }

  if (command == "help" || command == "?") {
    Serial.println("Commands: weather|w, clock|c, life|l, toggle|t, help|?");
    return;
  }

  if (!command.isEmpty()) {
    Serial.printf("Unknown command: %s\n", command.c_str());
  }
}

void updateWeatherIcon() {
  const char *iconCode = weatherData.iconCode.isEmpty() ? "03d" : weatherData.iconCode.c_str();
  renderIconToBuffer(iconCode, weatherIconCanvasBuffer, kOpenWeatherIconWidth, kOpenWeatherIconHeight);
  lv_obj_invalidate(ui.iconCanvas);
}

void createMetricCard(lv_obj_t *parent, lv_obj_t **titleOut, lv_obj_t **valueOut) {
  lv_obj_t *card = createStyledBlock(parent, lv_color_hex(0x13263A), LV_OPA_90, 22);
  lv_obj_set_style_pad_all(card, 16, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x244A70), 0);
  lv_obj_set_style_shadow_width(card, 16, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x05101B), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
  lv_obj_set_size(card, 196, 102);
  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card, 8, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_obj_set_style_text_color(title, lv_color_hex(0x88A8C7), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

  lv_obj_t *value = lv_label_create(card);
  lv_obj_set_style_text_color(value, lv_color_hex(0xF8FBFF), 0);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);

  *titleOut = title;
  *valueOut = value;
}

void showStartupIconGallery() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x061320), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x061320), 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_obj_set_style_text_color(title, lv_color_hex(0xF4F8FC), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_label_set_text(title, "OpenWeather Icon Set");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

  lv_obj_t *subtitle = lv_label_create(screen);
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0x89A9C6), 0);
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
  lv_label_set_text(subtitle, "Boot preview before dashboard");
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 54);

  lv_obj_t *grid = createStyledBlock(screen, lv_color_hex(0x0B1B2B), LV_OPA_80, 24);
  lv_obj_set_size(grid, 748, 356);
  lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_set_style_pad_all(grid, 16, 0);
  lv_obj_set_style_pad_row(grid, 10, 0);
  lv_obj_set_style_pad_column(grid, 12, 0);
  lv_obj_set_style_border_width(grid, 1, 0);
  lv_obj_set_style_border_color(grid, lv_color_hex(0x1C4269), 0);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  for (size_t i = 0; i < kStartupGalleryCount; ++i) {
    lv_obj_t *cell = createStyledBlock(grid, lv_color_hex(0x102538), LV_OPA_90, 18);
    lv_obj_set_size(cell, 108, 100);
    lv_obj_set_style_pad_all(cell, 10, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(0x244A70), 0);
    lv_obj_set_layout(cell, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, 6, 0);

    renderIconToBuffer(kStartupGalleryCodes[i], startupGalleryBuffers[i], STARTUP_GALLERY_ICON_SIZE, STARTUP_GALLERY_ICON_SIZE);

    lv_obj_t *canvas = lv_canvas_create(cell);
    lv_obj_remove_style_all(canvas);
    lv_obj_set_size(canvas, STARTUP_GALLERY_ICON_SIZE, STARTUP_GALLERY_ICON_SIZE);
    lv_canvas_set_buffer(canvas, startupGalleryBuffers[i], STARTUP_GALLERY_ICON_SIZE, STARTUP_GALLERY_ICON_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *code = lv_label_create(cell);
    lv_obj_set_style_text_color(code, lv_color_hex(0xDCEAF6), 0);
    lv_obj_set_style_text_font(code, &lv_font_montserrat_18, 0);
    lv_label_set_text(code, kStartupGalleryCodes[i]);
  }

  lv_timer_handler();
  const unsigned long start = millis();
  while ((millis() - start) < STARTUP_GALLERY_MS) {
    lv_timer_handler();
    delay(10);
  }

  lv_obj_clean(screen);
}

void createDashboardUi() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x061320), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x061320), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

  ui.dashboardRoot = createStyledBlock(screen, lv_color_hex(0x000000), LV_OPA_TRANSP, 0);
  lv_obj_set_size(ui.dashboardRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(ui.dashboardRoot, 0, 0);
  lv_obj_clear_flag(ui.dashboardRoot, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header = createStyledBlock(ui.dashboardRoot, lv_color_hex(0x0B1B2B), LV_OPA_80, 28);
  lv_obj_set_size(header, 760, 96);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 18);
  lv_obj_set_style_pad_all(header, 20, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_color(header, lv_color_hex(0x1C4269), 0);
  lv_obj_set_style_shadow_width(header, 18, 0);
  lv_obj_set_style_shadow_color(header, lv_color_hex(0x050D16), 0);
  lv_obj_set_style_shadow_opa(header, LV_OPA_40, 0);

  ui.location = lv_label_create(header);
  lv_obj_set_style_text_color(ui.location, lv_color_hex(0xF4F8FC), 0);
  lv_obj_set_style_text_font(ui.location, &lv_font_montserrat_34, 0);
  lv_label_set_text(ui.location, "OpenWeather Dashboard");
  lv_obj_align(ui.location, LV_ALIGN_TOP_LEFT, 0, 0);

  ui.status = lv_label_create(header);
  lv_obj_set_style_text_color(ui.status, lv_color_hex(0x89A9C6), 0);
  lv_obj_set_style_text_font(ui.status, &lv_font_montserrat_18, 0);
  lv_label_set_text(ui.status, "Waiting for Wi-Fi");
  lv_obj_align(ui.status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  ui.clock = lv_label_create(header);
  lv_obj_set_style_text_color(ui.clock, lv_color_hex(0x9BD1FF), 0);
  lv_obj_set_style_text_font(ui.clock, &lv_font_montserrat_28, 0);
  lv_label_set_text(ui.clock, "--:--");
  lv_obj_align(ui.clock, LV_ALIGN_RIGHT_MID, 0, 0);

  ui.hero = createStyledBlock(ui.dashboardRoot, lv_color_hex(0x0A1A2A), LV_OPA_90, 30);
  lv_obj_set_size(ui.hero, 332, 330);
  lv_obj_align(ui.hero, LV_ALIGN_TOP_LEFT, 20, 128);
  lv_obj_set_style_pad_all(ui.hero, 22, 0);
  lv_obj_set_style_pad_row(ui.hero, 8, 0);
  lv_obj_set_style_border_width(ui.hero, 1, 0);
  lv_obj_set_style_border_color(ui.hero, lv_color_hex(0x1E466B), 0);
  lv_obj_set_style_shadow_width(ui.hero, 18, 0);
  lv_obj_set_style_shadow_color(ui.hero, lv_color_hex(0x06111C), 0);
  lv_obj_set_style_shadow_opa(ui.hero, LV_OPA_40, 0);
  lv_obj_set_layout(ui.hero, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ui.hero, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ui.hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  ui.iconCard = createStyledBlock(ui.hero, lv_color_hex(0x12314A), LV_OPA_90, 26);
  lv_obj_set_size(ui.iconCard, 288, 144);
  lv_obj_set_style_bg_grad_color(ui.iconCard, lv_color_hex(0x12314A), 0);
  lv_obj_set_style_bg_grad_dir(ui.iconCard, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(ui.iconCard, 1, 0);
  lv_obj_set_style_border_color(ui.iconCard, lv_color_hex(0x2B638E), 0);
  lv_obj_set_layout(ui.iconCard, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ui.iconCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ui.iconCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(ui.iconCard, 18, 0);
  lv_obj_set_style_pad_row(ui.iconCard, 8, 0);

  ui.iconTitle = lv_label_create(ui.iconCard);
  lv_obj_set_style_text_color(ui.iconTitle, lv_color_hex(0x8CC5F0), 0);
  lv_obj_set_style_text_font(ui.iconTitle, &lv_font_montserrat_18, 0);
  lv_label_set_text(ui.iconTitle, "Current Conditions");

  ui.iconCanvas = lv_canvas_create(ui.iconCard);
  lv_obj_remove_style_all(ui.iconCanvas);
  lv_obj_set_size(ui.iconCanvas, kOpenWeatherIconWidth, kOpenWeatherIconHeight);
  lv_canvas_set_buffer(ui.iconCanvas, weatherIconCanvasBuffer, kOpenWeatherIconWidth, kOpenWeatherIconHeight,
                       LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(ui.iconCanvas, lv_color_hex(0x12314A), LV_OPA_COVER);

  ui.condition = lv_label_create(ui.hero);
  lv_obj_set_style_text_color(ui.condition, lv_color_hex(0x8FD4FF), 0);
  lv_obj_set_style_text_font(ui.condition, &lv_font_montserrat_22, 0);
  lv_obj_set_width(ui.condition, 286);
  lv_label_set_text(ui.condition, "Condition");

  ui.temperature = lv_label_create(ui.hero);
  lv_obj_set_style_text_color(ui.temperature, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(ui.temperature, &lv_font_montserrat_48, 0);
  lv_obj_set_width(ui.temperature, 286);
  lv_label_set_text(ui.temperature, "--");

  ui.feelsLike = lv_label_create(ui.hero);
  lv_obj_set_style_text_color(ui.feelsLike, lv_color_hex(0x89A9C6), 0);
  lv_obj_set_style_text_font(ui.feelsLike, &lv_font_montserrat_18, 0);
  lv_obj_set_width(ui.feelsLike, 286);
  lv_label_set_text(ui.feelsLike, "Feels like --");

  ui.description = lv_label_create(ui.hero);
  lv_obj_set_style_text_color(ui.description, lv_color_hex(0xD0E4F7), 0);
  lv_obj_set_style_text_font(ui.description, &lv_font_montserrat_18, 0);
  lv_label_set_long_mode(ui.description, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ui.description, 286);
  lv_label_set_text(ui.description, "Waiting for weather response");

  lv_obj_t *grid = createStyledBlock(ui.dashboardRoot, lv_color_hex(0x000000), LV_OPA_TRANSP, 0);
  lv_obj_set_size(grid, 408, 328);
  lv_obj_align(grid, LV_ALIGN_TOP_RIGHT, -20, 128);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_row(grid, 14, 0);
  lv_obj_set_style_pad_column(grid, 16, 0);

  lv_obj_t *title = nullptr;
  createMetricCard(grid, &title, &ui.humidity);
  lv_label_set_text(title, "Humidity");
  lv_label_set_text(ui.humidity, "--");

  createMetricCard(grid, &title, &ui.wind);
  lv_label_set_text(title, "Wind");
  lv_label_set_text(ui.wind, "--");

  createMetricCard(grid, &title, &ui.pressure);
  lv_label_set_text(title, "Pressure");
  lv_label_set_text(ui.pressure, "--");

  createMetricCard(grid, &title, &ui.visibility);
  lv_label_set_text(title, "Visibility");
  lv_label_set_text(ui.visibility, "--");

  createMetricCard(grid, &title, &ui.sunrise);
  lv_label_set_text(title, "Sunrise");
  lv_label_set_text(ui.sunrise, "--");

  createMetricCard(grid, &title, &ui.sunset);
  lv_label_set_text(title, "Sunset");
  lv_label_set_text(ui.sunset, "--");

  updateWeatherIcon();
}

void createClockUi() {
  lv_obj_t *screen = lv_scr_act();

  clockUi.clockRoot = createStyledBlock(screen, lv_color_hex(0x07131D), LV_OPA_COVER, 0);
  lv_obj_set_size(clockUi.clockRoot, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(clockUi.clockRoot, 0, 0);
  lv_obj_clear_flag(clockUi.clockRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(clockUi.clockRoot, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *panel = createStyledBlock(clockUi.clockRoot, lv_color_hex(0x0B1D2D), LV_OPA_90, 36);
  lv_obj_set_size(panel, 728, 392);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x1E466B), 0);
  lv_obj_set_style_shadow_width(panel, 22, 0);
  lv_obj_set_style_shadow_color(panel, lv_color_hex(0x041019), 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_40, 0);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(panel, 28, 0);
  lv_obj_set_style_pad_row(panel, 14, 0);

  clockUi.location = lv_label_create(panel);
  lv_obj_set_style_text_color(clockUi.location, lv_color_hex(0x8CC5F0), 0);
  lv_obj_set_style_text_font(clockUi.location, &lv_font_montserrat_22, 0);
  lv_label_set_text(clockUi.location, "Clock");

  clockUi.time = lv_label_create(panel);
  lv_obj_set_style_text_color(clockUi.time, lv_color_hex(0xF7FBFF), 0);
  lv_obj_set_style_text_font(clockUi.time, &lv_font_montserrat_48, 0);
  lv_label_set_text(clockUi.time, "--:--");

  clockUi.date = lv_label_create(panel);
  lv_obj_set_style_text_color(clockUi.date, lv_color_hex(0xD3E4F5), 0);
  lv_obj_set_style_text_font(clockUi.date, &lv_font_montserrat_28, 0);
  lv_label_set_text(clockUi.date, "Waiting for time");

  clockUi.footer = lv_label_create(panel);
  lv_obj_set_style_text_color(clockUi.footer, lv_color_hex(0x89A9C6), 0);
  lv_obj_set_style_text_font(clockUi.footer, &lv_font_montserrat_18, 0);
  lv_label_set_text(clockUi.footer, "Waiting for weather data");
}

void updateDashboardUi() {
  if (!weatherData.valid) {
    lv_label_set_text(ui.status, weatherData.lastError.c_str());
    return;
  }

  lv_label_set_text(ui.location, weatherData.location.c_str());
  lv_label_set_text(ui.status, formatObservedLine().c_str());
  lv_label_set_text(ui.clock, formatLocalTime(weatherData.observedAt, weatherData.timezoneOffset).c_str());
  lv_label_set_text(ui.condition, weatherData.condition.c_str());
  lv_label_set_text(ui.description, weatherData.description.c_str());
  updateWeatherIcon();

  String temperatureText = String(weatherData.temperature) + String(weatherData.unitsLabel);
  lv_label_set_text(ui.temperature, temperatureText.c_str());

  String feelsLikeText = "Feels like " + String(weatherData.feelsLike) + weatherData.unitsLabel;
  lv_label_set_text(ui.feelsLike, feelsLikeText.c_str());

  String humidityText = String(weatherData.humidity) + "%";
  lv_label_set_text(ui.humidity, humidityText.c_str());

  String windText = String(weatherData.windSpeed);
  windText += (strcmp(OPENWEATHER_UNITS, "metric") == 0) ? " m/s " : " mph ";
  windText += compassDirection(weatherData.windDeg);
  lv_label_set_text(ui.wind, windText.c_str());

  String pressureText = String(weatherData.pressure) + " hPa";
  lv_label_set_text(ui.pressure, pressureText.c_str());

  String visibilityText = String(weatherData.visibilityKm) + " km";
  lv_label_set_text(ui.visibility, visibilityText.c_str());

  lv_label_set_text(ui.sunrise, formatLocalTime(weatherData.sunrise, weatherData.timezoneOffset).c_str());
  lv_label_set_text(ui.sunset, formatLocalTime(weatherData.sunset, weatherData.timezoneOffset).c_str());
  updateClockScreenUi();
}

void setStatusOnly(const String &message) {
  weatherData.lastError = message;
  weatherData.valid = false;
  lv_label_set_text(ui.status, message.c_str());
}

void connectWifiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  setStatusOnly("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_RETRY_MS) {
    delay(250);
    lv_timer_handler();
  }

  if (WiFi.status() == WL_CONNECTED) {
    lv_label_set_text(ui.status, "Wi-Fi connected, requesting weather");
  } else {
    setStatusOnly("Wi-Fi unavailable");
  }
}

bool parseWeatherPayload(const String &payload) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    weatherData.lastError = "JSON parse failed";
    weatherData.valid = false;
    return false;
  }

  weatherData.location = String(doc["name"] | "Unknown");
  const String country = String(doc["sys"]["country"] | "");
  if (!country.isEmpty()) {
    weatherData.location += ", ";
    weatherData.location += country;
  }

  weatherData.condition = String(doc["weather"][0]["main"] | "Unknown");
  weatherData.description = String(doc["weather"][0]["description"] | "No description");
  weatherData.iconCode = String(doc["weather"][0]["icon"] | "03d");
  weatherData.description.toLowerCase();
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
  currentLocalEpochUtc = weatherData.observedAt;
  return true;
}

bool fetchWeather() {
  if (strlen(OPENWEATHER_API_KEY) == 0 || strcmp(OPENWEATHER_API_KEY, "YOUR_OPENWEATHER_API_KEY") == 0) {
    setStatusOnly("Set your OpenWeather API key");
    return false;
  }

  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
    setStatusOnly("Set your Wi-Fi credentials");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectWifiIfNeeded();
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += urlEncode(OPENWEATHER_LOCATION);
  url += "&appid=";
  url += OPENWEATHER_API_KEY;
  url += "&units=";
  url += OPENWEATHER_UNITS;

  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    setStatusOnly("HTTP client setup failed");
    return false;
  }

  lv_label_set_text(ui.status, "Downloading weather");
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    String error = "OpenWeather HTTP ";
    error += String(statusCode);
    http.end();
    setStatusOnly(error);
    return false;
  }

  const String payload = http.getString();
  http.end();

  if (!parseWeatherPayload(payload)) {
    setStatusOnly(weatherData.lastError);
    return false;
  }

  updateDashboardUi();
  return true;
}

void initLvgl() {
  lv_init();

  drawBufferA = static_cast<lv_color_t *>(
      heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (drawBufferA == nullptr) {
    Serial.println("PSRAM LVGL buffer allocation failed, trying internal RAM");
    drawBufferA = static_cast<lv_color_t *>(
        heap_caps_malloc(DRAW_BUFFER_PIXEL_COUNT * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }

  if (drawBufferA == nullptr) {
    Serial.println("Failed to allocate any LVGL buffer");
    while (true) {
      delay(1000);
    }
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

  Serial.println("Initializing display panel");
  display.begin();
  display.setRotation(0);
  setBacklight(BACKLIGHT_BRIGHTNESS);
  display.fillScreen(TFT_RED);
  delay(STARTUP_COLOR_TEST_MS);
  display.fillScreen(TFT_GREEN);
  delay(STARTUP_COLOR_TEST_MS);
  display.fillScreen(TFT_BLUE);
  delay(STARTUP_COLOR_TEST_MS);
  display.fillScreen(TFT_WHITE);
  delay(STARTUP_COLOR_TEST_MS);
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(top_left);
  display.setTextSize(2);
  display.drawString("IoT-SmartHub boot", 24, 24);
  display.drawString(DISPLAY_MODEL == CrowPanelModel::FiveInch ? "Panel: 5-inch" : "Panel: 7-inch", 24, 56);
  Serial.println("Direct display test drawn");
}

void tickClock() {
  const unsigned long now = millis();
  const long elapsedSeconds = static_cast<long>((now - lastClockTick) / 1000UL);
  if (elapsedSeconds <= 0) {
    return;
  }

  currentLocalEpochUtc += elapsedSeconds;
  lastClockTick += static_cast<unsigned long>(elapsedSeconds) * 1000UL;

  if (weatherData.valid) {
    lv_label_set_text(ui.clock, formatLocalTime(currentLocalEpochUtc, weatherData.timezoneOffset).c_str());
    lv_label_set_text(ui.status, formatObservedLine().c_str());
  }
  updateClockScreenUi();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("Booting IoT-SmartHub");
  Serial.printf("Display model: %s\n", DISPLAY_MODEL == CrowPanelModel::FiveInch ? "5-inch" : "7-inch");
  randomSeed(micros());

  initDisplayHardware();
  initLvgl();
  initLifeBuffers();
  showStartupIconGallery();
  createDashboardUi();
  createClockUi();
  Serial.println("LVGL initialized");
  Serial.println("Serial commands: weather|w, clock|c, life|l, toggle|t, help|?");

  lastClockTick = millis();
  lastWeatherAttempt = millis() - WEATHER_REFRESH_MS;

  fetchWeather();
  Serial.println("Initial weather request finished");
}

void loop() {
  const unsigned long now = millis();
  handleSerialCommands();
  tickClock();

  if ((WiFi.status() != WL_CONNECTED && (now - lastWeatherAttempt) >= WIFI_RETRY_MS) ||
      (now - lastWeatherAttempt) >= WEATHER_REFRESH_MS) {
    lastWeatherAttempt = now;
    fetchWeather();
    lastClockTick = millis();
  }

  if (currentScreenMode == ScreenMode::Weather || currentScreenMode == ScreenMode::Clock) {
    lv_timer_handler();
  } else if ((now - lastLifeStep) >= LIFE_STEP_MS) {
    stepLifeBoard();
    renderLifeBoard();
    lastLifeStep = now;
  }

  delay(5);
}
