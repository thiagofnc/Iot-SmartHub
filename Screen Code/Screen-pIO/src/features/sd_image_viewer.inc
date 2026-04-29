// SD card / image viewer
// ---------------------------------------------------------------------------
bool initSdCard() {
  static SPIClass sdSpi(HSPI);
  sdSpi.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, sdSpi, 20000000)) {
    Serial.println("SD mount failed");
    return false;
  }
  const uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    Serial.println("No SD card detected");
    return false;
  }
  Serial.printf("SD mounted (%lluMB)\n", SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

void hideSdImage() {
  if (sdImageOverlay) {
    lv_obj_del(sdImageOverlay);
    sdImageOverlay = nullptr;
    sdImageCanvas = nullptr;
  }
}

bool showSdImage(const String &name) {
  if (!sdReady) { Serial.println("SD not available"); return false; }

  String path = "/images/";
  path += name;
  path += ".bin";

  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) { Serial.printf("open failed: %s\n", path.c_str()); return false; }
  if ((size_t)f.size() != SD_IMAGE_BYTES) {
    Serial.printf("size mismatch: %s is %u bytes, expected %u\n",
                  path.c_str(), (unsigned)f.size(), (unsigned)SD_IMAGE_BYTES);
    f.close();
    return false;
  }

  if (!sdImageBuffer) {
    sdImageBuffer = static_cast<lv_color_t *>(
        heap_caps_malloc(SD_IMAGE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!sdImageBuffer) {
      Serial.println("Image buffer alloc failed");
      f.close();
      return false;
    }
  }

  uint8_t *dst = reinterpret_cast<uint8_t *>(sdImageBuffer);
  size_t remaining = SD_IMAGE_BYTES;
  while (remaining > 0) {
    const size_t chunk = remaining > 4096 ? 4096 : remaining;
    const int read = f.read(dst, chunk);
    if (read <= 0) {
      Serial.println("SD read failed");
      f.close();
      return false;
    }
    // File is little-endian RGB565; LVGL here uses LV_COLOR_16_SWAP=1
    // (big-endian), so swap each pixel's two bytes in place.
    for (int i = 0; i + 1 < read; i += 2) {
      const uint8_t t = dst[i];
      dst[i] = dst[i + 1];
      dst[i + 1] = t;
    }
    dst += read;
    remaining -= read;
  }
  f.close();

  if (!sdImageOverlay) {
    sdImageOverlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(sdImageOverlay);
    lv_obj_set_size(sdImageOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(sdImageOverlay, 0, 0);
    lv_obj_clear_flag(sdImageOverlay, LV_OBJ_FLAG_SCROLLABLE);

    sdImageCanvas = lv_canvas_create(sdImageOverlay);
    lv_obj_remove_style_all(sdImageCanvas);
    lv_obj_set_pos(sdImageCanvas, 0, 0);
  }
  lv_canvas_set_buffer(sdImageCanvas, sdImageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT,
                       LV_IMG_CF_TRUE_COLOR);
  lv_obj_move_foreground(sdImageOverlay);
  Serial.printf("showing %s\n", name.c_str());
  return true;
}

// ---------------------------------------------------------------------------
