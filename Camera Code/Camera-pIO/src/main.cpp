#include <Arduino.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "camera_pins.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

namespace {

// Local override of the FOMO detection threshold. Don't edit
// EI_CLASSIFIER_OBJECT_DETECTION_THRESHOLD in model_metadata.h — that file is
// regenerated on every Edge Impulse model export.
constexpr float kHandConfThreshold = 0.32f;

// ---- Camera --------------------------------------------------------------

bool g_camInitialized = false;

bool initCamera() {
  if (g_camInitialized) {
    esp_camera_deinit();
    g_camInitialized = false;
    delay(100);
  }

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  // Init at the inference resolution directly so the cam_task DMA chain is
  // sized for 96x96 RGB565 from the start.
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_96X96;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_special_effect(sensor, 2);  // grayscale (model trained on grayscale)
  }
  g_camInitialized = true;
  return true;
}

// ---- Gesture recognizer --------------------------------------------------
// 4-direction swipe detector. Mirrors tools/live_viewer.py::GestureRecognizer
// so on-device output matches what the host viewer would produce on the same
// centroid stream — same thresholds, same state machine.

struct GestureRecognizer {
  static constexpr float kFrameSize = 96.0f;
  static constexpr float kMinDisplacement = 0.30f * kFrameSize;  // ~29 px
  static constexpr float kMaxOffAxisRatio = 0.6f;
  static constexpr uint32_t kMinDurationMs = 100;
  static constexpr uint32_t kMaxDurationMs = 800;
  static constexpr uint32_t kCooldownMs = 600;
  static constexpr uint32_t kLostHandResetMs = 250;
  static constexpr float kMinMonotonicFrac = 0.70f;
  static constexpr int kMinSamples = 3;
  static constexpr int kMaxSamples = 32;  // > kMaxDurationMs * realistic FPS

  struct Sample { uint32_t t; float x; float y; };
  Sample samples[kMaxSamples];
  int count = 0;
  uint32_t cooldownUntil = 0;
  uint32_t lastHandT = 0;
  bool sawHand = false;

  void reset() {
    count = 0;
    cooldownUntil = 0;
    sawHand = false;
  }

  // Returns nullptr if no gesture this tick, else a stable string literal.
  const char* update(uint32_t t, bool hasHand, float cx, float cy) {
    if (!hasHand) {
      // Drop the buffer if the hand has been gone too long — stops a hand
      // that re-enters far away from chaining into a phantom swipe.
      if (!sawHand || (t - lastHandT) > kLostHandResetMs) {
        count = 0;
      }
      return nullptr;
    }

    sawHand = true;
    lastHandT = t;

    if (count >= kMaxSamples) {
      for (int i = 0; i < kMaxSamples - 1; ++i) samples[i] = samples[i + 1];
      count = kMaxSamples - 1;
    }
    samples[count++] = {t, cx, cy};

    int firstKeep = 0;
    while (firstKeep < count && (t - samples[firstKeep].t) > kMaxDurationMs) {
      ++firstKeep;
    }
    if (firstKeep > 0) {
      for (int i = 0; i + firstKeep < count; ++i) samples[i] = samples[i + firstKeep];
      count -= firstKeep;
    }

    if (t < cooldownUntil) return nullptr;
    if (count < kMinSamples) return nullptr;

    const Sample& a = samples[0];
    const Sample& b = samples[count - 1];
    uint32_t dt = b.t - a.t;
    if (dt < kMinDurationMs) return nullptr;

    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float adx = fabsf(dx);
    float ady = fabsf(dy);

    const char* gesture = nullptr;
    if (adx >= kMinDisplacement && ady < adx * kMaxOffAxisRatio) {
      int sign = dx > 0 ? 1 : -1;
      if (monotonic(/*axisIsY=*/false, sign)) {
        gesture = (dx > 0) ? "RIGHT" : "LEFT";
      }
    } else if (ady >= kMinDisplacement && adx < ady * kMaxOffAxisRatio) {
      // Image y grows downward, so dy>0 is a downward swipe.
      int sign = dy > 0 ? 1 : -1;
      if (monotonic(/*axisIsY=*/true, sign)) {
        gesture = (dy > 0) ? "DOWN" : "UP";
      }
    }

    if (gesture != nullptr) {
      count = 0;
      cooldownUntil = t + kCooldownMs;
    }
    return gesture;
  }

  bool monotonic(bool axisIsY, int sign) {
    int forward = 0;
    int total = 0;
    float prev = axisIsY ? samples[0].y : samples[0].x;
    for (int i = 1; i < count; ++i) {
      float v = axisIsY ? samples[i].y : samples[i].x;
      if ((v - prev) * static_cast<float>(sign) > 0.0f) ++forward;
      ++total;
      prev = v;
    }
    return total > 0 && static_cast<float>(forward) / static_cast<float>(total) >= kMinMonotonicFrac;
  }
};

GestureRecognizer g_gesture;

// ---- Inference -----------------------------------------------------------

constexpr int kEiInputW = EI_CLASSIFIER_INPUT_WIDTH;
constexpr int kEiInputH = EI_CLASSIFIER_INPUT_HEIGHT;
constexpr int kEiPixels = kEiInputW * kEiInputH;

// Packed-RGB888 float buffer for the EI image-DSP block. PSRAM-resident.
float* g_eiFeatures = nullptr;

bool ensureFeatureBuffer() {
  if (g_eiFeatures != nullptr) return true;
  size_t bytes = static_cast<size_t>(kEiPixels) * sizeof(float);
  g_eiFeatures = static_cast<float*>(
      psramFound() ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM)
                   : malloc(bytes));
  if (g_eiFeatures == nullptr) {
    Serial.println("[ERR]   Failed to allocate inference feature buffer");
    return false;
  }
  return true;
}

int eiSignalGetData(size_t offset, size_t length, float* out) {
  memcpy(out, g_eiFeatures + offset, length * sizeof(float));
  return 0;
}

// RGB565 framebuffers from esp_camera are stored byte-swapped: the high byte
// of the 16-bit pixel comes FIRST in memory. EI's image block expects packed
// RGB888 floats (one float per pixel, value = (r<<16)|(g<<8)|b). Sensor is in
// grayscale special_effect, so R≈G≈B and the packed value carries luminance.
void rgb565ToPackedFloats(const uint8_t* fb, size_t pixelCount, float* out) {
  for (size_t i = 0; i < pixelCount; ++i) {
    uint16_t hi = fb[i * 2];
    uint16_t lo = fb[i * 2 + 1];
    uint16_t px = (hi << 8) | lo;
    uint8_t r = ((px & 0xF800) >> 8) | ((px & 0xE000) >> 13);
    uint8_t g = ((px & 0x07E0) >> 3) | ((px & 0x0600) >> 9);
    uint8_t b = ((px & 0x001F) << 3) | ((px & 0x001C) >> 2);
    uint32_t packed = (static_cast<uint32_t>(r) << 16) |
                      (static_cast<uint32_t>(g) << 8) |
                      static_cast<uint32_t>(b);
    out[i] = static_cast<float>(packed);
  }
}

// ---- Run modes -----------------------------------------------------------
// MODE_NORMAL  : run gesture detection on-device, emit "[GESTURE] X" lines.
// MODE_VIEWER  : stream FRAME packets to the host viewer; the viewer runs its
//                own gesture detection from the centroids in the header, so
//                we don't print gestures here (would duplicate + clutter the
//                binary protocol).

enum RunMode { MODE_NORMAL, MODE_VIEWER };
RunMode g_mode = MODE_NORMAL;

void setMode(RunMode m) {
  if (g_mode == m) return;
  g_mode = m;
  g_gesture.reset();
  if (m == MODE_VIEWER) {
    Serial.println("[MODE]  viewer (streaming FRAME packets to host)");
  } else {
    Serial.println("[MODE]  normal (on-device gesture detection)");
  }
}

void pollSerialControls() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;
    switch (c) {
      case 't': case 'T':
        setMode(g_mode == MODE_NORMAL ? MODE_VIEWER : MODE_NORMAL);
        break;
      case '?': case 'h': case 'H':
        Serial.println("[HELP]  t=toggle viewer/normal  ?=help");
        break;
      default:
        break;  // ignore newlines / stray chars
    }
  }
}

void runInferenceCycle() {
  pollSerialControls();

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    Serial.println("[INF]   null framebuffer");
    delay(50);
    return;
  }
  if (fb->width != kEiInputW || fb->height != kEiInputH ||
      fb->len < static_cast<size_t>(kEiPixels) * 2) {
    Serial.printf("[INF]   unexpected frame %ux%u len=%u\n",
                  fb->width, fb->height, static_cast<unsigned>(fb->len));
    esp_camera_fb_return(fb);
    delay(50);
    return;
  }

  rgb565ToPackedFloats(fb->buf, kEiPixels, g_eiFeatures);

  signal_t signal;
  signal.total_length = kEiPixels;
  signal.get_data = &eiSignalGetData;

  ei_impulse_result_t result = {};
  EI_IMPULSE_ERROR rc = run_classifier(&signal, &result, false);
  if (rc != EI_IMPULSE_OK) {
    Serial.printf("[INF]   run_classifier failed (%d)\n", rc);
    esp_camera_fb_return(fb);
    delay(100);
    return;
  }

  float sumX = 0, sumY = 0, sumW = 0;
  uint32_t hits = 0;
  for (uint32_t i = 0; i < result.bounding_boxes_count; ++i) {
    const auto& bb = result.bounding_boxes[i];
    if (bb.value < kHandConfThreshold) continue;
    float cx = bb.x + bb.width  * 0.5f;
    float cy = bb.y + bb.height * 0.5f;
    sumX += cx * bb.value;
    sumY += cy * bb.value;
    sumW += bb.value;
    ++hits;
  }

  bool hasHand = (hits > 0 && sumW > 0.0f);
  float cx = hasHand ? sumX / sumW : 0.0f;
  float cy = hasHand ? sumY / sumW : 0.0f;

  if (g_mode == MODE_VIEWER) {
    // Header is plain text so a human can grep the log; payload is raw RGB565
    // (little-endian as stored in the framebuffer).
    Serial.printf("FRAME %u %d %.2f %.2f\n",
                  static_cast<unsigned>(fb->len),
                  hasHand ? 1 : 0, cx, cy);
    Serial.write(fb->buf, fb->len);
    Serial.write('\n');
  } else {
    const char* g = g_gesture.update(millis(), hasHand, cx, cy);
    if (g != nullptr) {
      Serial.printf("[GESTURE] %s\n", g);
    }
  }

  esp_camera_fb_return(fb);
}

}  // namespace

void setup() {
  Serial.begin(921600);
  delay(500);
  Serial.println();
  Serial.println("SmartHub camera firmware (inference + gestures)");
  Serial.printf("PSRAM: %s, free heap: %u bytes\n",
                psramFound() ? "yes" : "no",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));

  if (!ensureFeatureBuffer()) return;
  if (!initCamera()) return;

  Serial.println();
  Serial.printf("=== Inference active (input %dx%d, threshold %.2f) ===\n",
                kEiInputW, kEiInputH, kHandConfThreshold);
  Serial.println("Default mode: normal (gestures on serial). Press 't' to toggle viewer mode.");
  Serial.println();
}

void loop() {
  runInferenceCycle();
}
