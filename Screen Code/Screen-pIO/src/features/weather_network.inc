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
  weatherData.latitude = doc["coord"]["lat"] | 0.0f;
  weatherData.longitude = doc["coord"]["lon"] | 0.0f;
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

bool fetchAirPollution() {
  if (strlen(OPENWEATHER_API_KEY) == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/air_pollution?lat=";
  url += String(weatherData.latitude, 6);
  url += "&lon=";
  url += String(weatherData.longitude, 6);
  url += "&appid=";
  url += OPENWEATHER_API_KEY;

  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return false;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonVariant current = doc["list"][0];
  if (current.isNull()) return false;

  weatherData.airQualityIndex = current["main"]["aqi"] | 0;
  weatherData.pm25 = lround(current["components"]["pm2_5"] | 0.0);
  weatherData.pm10 = lround(current["components"]["pm10"] | 0.0);
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
  fetchAirPollution();
  updateWeatherUi();
  updateGlanceTile();
  updatePortraitClockUi();
  return true;
}

// ---------------------------------------------------------------------------
