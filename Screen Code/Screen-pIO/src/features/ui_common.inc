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

// Reached only from worker tasks via connectWifiIfNeeded(). LVGL is owned by
// the main loop, so do not call lv_timer_handler here — delay() yields to
// FreeRTOS so the UI core keeps running on its own.
void waitForTimeSync() {
  const unsigned long start = millis();
  while (readSystemEpochUtc() <= 0 && (millis() - start) < NTP_SYNC_WAIT_MS) {
    delay(100);
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

String base64Encode(const String &value) {
  size_t outLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outLen,
                        reinterpret_cast<const unsigned char *>(value.c_str()),
                        value.length());
  if (outLen == 0) return "";
  unsigned char *out = static_cast<unsigned char *>(malloc(outLen + 1));
  if (!out) return "";
  if (mbedtls_base64_encode(out, outLen, &outLen,
                            reinterpret_cast<const unsigned char *>(value.c_str()),
                            value.length()) != 0) {
    free(out);
    return "";
  }
  out[outLen] = '\0';
  String encoded(reinterpret_cast<char *>(out));
  free(out);
  return encoded;
}

String compassDirection(int degrees) {
  static const char *kDirections[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int normalized = ((degrees % 360) + 360) % 360;
  const int index = (normalized + 22) / 45 % 8;
  return kDirections[index];
}

const char *airQualityLabel(int aqi) {
  switch (aqi) {
    case 1: return "GOOD";
    case 2: return "FAIR";
    case 3: return "MODERATE";
    case 4: return "POOR";
    case 5: return "VERY POOR";
    default: return "--";
  }
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
  const lv_coord_t parentW = lv_obj_get_width(parent);
  lv_obj_t *row = makePlain(parent);
  lv_obj_set_size(row, parentW - 56, 18);
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
  const int barXBase = lv_obj_get_width(row) - 22;
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
