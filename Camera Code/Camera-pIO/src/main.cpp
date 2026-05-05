#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>

#include "CameraConfig.h"
#include "camera_pins.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

namespace {

WebServer server(80);

constexpr char STREAM_CONTENT_TYPE[] =
    "multipart/x-mixed-replace; boundary=123456789000000000000987654321";
constexpr char STREAM_BOUNDARY_LINE[] = "\r\n--123456789000000000000987654321\r\n";

constexpr const char* kLabels[] = {"up", "down", "lswipe", "rswipe", "rotate", "idle"};
constexpr int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);
constexpr int kIdleIndex = 5;

// Local override of the FOMO detection threshold. Don't edit
// EI_CLASSIFIER_OBJECT_DETECTION_THRESHOLD in model_metadata.h — that file is
// regenerated on every Edge Impulse model export.
constexpr float kHandConfThreshold = 0.75f;

bool g_sdReady = false;

void sendPlain(int code, const char* message) {
  server.send(code, "text/plain", message);
}

enum CameraMode { CAM_MODE_CAPTURE, CAM_MODE_INFERENCE };
CameraMode g_camMode = CAM_MODE_CAPTURE;
bool g_camInitialized = false;

bool initCamera(CameraMode mode) {
  if (g_camInitialized) {
    esp_camera_deinit();
    g_camInitialized = false;
    // Give cam_task time to actually exit and free its DMA descriptors before
    // we re-enter init. Without this, init reuses partially-released state and
    // the next cam_task blows its stack canary on the first frame.
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

  if (mode == CAM_MODE_INFERENCE) {
    // Init the camera at 96x96 RGB565 directly. Initing at QVGA blows the
    // cam_task stack on the first frame because RGB565 QVGA is 153KB per FB
    // and the DMA descriptor chain sized for QVGA stays in place even after
    // sensor->set_framesize(96X96).
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_96X96;
  } else {
    // OV2640 driver init only accepts a subset of frame sizes; 96x96 is reachable
    // only via post-init sensor downscale (set_framesize below). Init at QVGA so
    // buffers stay small but valid, and use fb_count=1 to avoid stale double-buffered frames.
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    if (mode == CAM_MODE_CAPTURE) {
      sensor->set_framesize(sensor, FRAMESIZE_96X96);
      sensor->set_quality(sensor, 12);
      sensor->set_special_effect(sensor, 2);  // grayscale
    } else {
      // Match capture-mode preprocessing: model was trained on grayscale 96x96.
      sensor->set_special_effect(sensor, 2);  // grayscale
    }
  }
  g_camMode = mode;
  g_camInitialized = true;
  return true;
}

bool initSdCard() {
  // XIAO ESP32S3 Sense expansion board built-in microSD slot.
  // Per Seeed's wiki the slot uses fixed SDIO pins: CLK=GPIO7, CMD=GPIO9, D0=GPIO8.
  // Pin them explicitly; auto-detection picks the wrong pins on some core versions.
  SD_MMC.setPins(7, 9, 8);

  // 1-bit mode, format-on-fail = false. Higher freq (40MHz) needed on some boards.
  for (int attempt = 1; attempt <= 3; ++attempt) {
    if (SD_MMC.begin("/sdcard", true, false, 40000)) {
      uint8_t cardType = SD_MMC.cardType();
      if (cardType == CARD_NONE) {
        Serial.println("SD_MMC.begin returned ok but cardType=CARD_NONE. Card not seated?");
        SD_MMC.end();
        delay(200);
        continue;
      }
      const char* typeStr = (cardType == CARD_MMC) ? "MMC" :
                            (cardType == CARD_SD)  ? "SD"  :
                            (cardType == CARD_SDHC)? "SDHC": "UNKNOWN";
      Serial.printf("SD card mounted (%s), size: %lluMB\n", typeStr,
                    SD_MMC.cardSize() / (1024ULL * 1024ULL));
      goto mounted;
    }
    Serial.printf("SD_MMC.begin attempt %d failed.\n", attempt);
    SD_MMC.end();
    delay(300);
  }
  Serial.println("SD_MMC mount failed after 3 attempts.");
  Serial.println("  - Confirm card is FAT32 (not exFAT) and <=32GB or reformatted.");
  Serial.println("  - Confirm card is fully seated in the Sense expansion slot.");
  Serial.println("  - Confirm you are using the Sense expansion board, not a bare XIAO ESP32S3.");
  return false;

mounted:

  if (!SD_MMC.exists("/dataset")) SD_MMC.mkdir("/dataset");
  if (!SD_MMC.exists("/dataset/new_data")) SD_MMC.mkdir("/dataset/new_data");
  return true;
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(CAMERA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to Wi-Fi SSID '%s'", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection failed.");
    return false;
  }

  Serial.printf("Wi-Fi connected. Open http://%s/\n", WiFi.localIP().toString().c_str());
  return true;
}

int countClipsForLabel(const char* label) {
  String dir = String("/dataset/") + label;
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) return 0;

  // Each clip = N frames sharing a "<label>_<timestamp>" prefix.
  // Count unique prefixes by counting "_001.jpg" suffix files.
  int count = 0;
  File f = d.openNextFile();
  while (f) {
    String name = String(f.name());
    if (name.endsWith("_001.jpg")) ++count;
    f = d.openNextFile();
  }
  d.close();
  return count;
}

String lastClipPrefixForLabel(const char* label) {
  String dir = String("/dataset/") + label;
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) return String();

  String latestPrefix;
  uint32_t latestStamp = 0;
  File f = d.openNextFile();
  while (f) {
    String name = String(f.name());
    int slash = name.lastIndexOf('/');
    String base = (slash >= 0) ? name.substring(slash + 1) : name;
    int lastUnderscore = base.lastIndexOf('_');
    if (lastUnderscore > 0) {
      String prefix = base.substring(0, lastUnderscore);
      int firstUnderscore = prefix.indexOf('_');
      if (firstUnderscore > 0) {
        uint32_t stamp = strtoul(prefix.substring(firstUnderscore + 1).c_str(), nullptr, 10);
        if (stamp > latestStamp) {
          latestStamp = stamp;
          latestPrefix = prefix;
        }
      }
    }
    f = d.openNextFile();
  }
  d.close();
  return latestPrefix;
}

bool deleteClip(const char* label, const String& prefix) {
  if (prefix.length() == 0) return false;
  String dir = String("/dataset/") + label;
  File d = SD_MMC.open(dir);
  if (!d || !d.isDirectory()) return false;

  std::vector<String> toDelete;
  File f = d.openNextFile();
  while (f) {
    String name = String(f.name());
    int slash = name.lastIndexOf('/');
    String base = (slash >= 0) ? name.substring(slash + 1) : name;
    if (base.startsWith(prefix + "_")) {
      toDelete.push_back(name);
    }
    f = d.openNextFile();
  }
  d.close();

  for (const auto& path : toDelete) {
    SD_MMC.remove(path);
  }
  return !toDelete.empty();
}

void handleStream() {
  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: %s\r\n", STREAM_CONTENT_TYPE);
  client.print("Cache-Control: no-cache\r\n");
  client.print("Connection: close\r\n\r\n");

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) break;

    client.print(STREAM_BOUNDARY_LINE);
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", static_cast<unsigned>(fb->len));
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (!client.connected()) break;
    delay(30);
  }
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    sendPlain(503, "Failed to capture frame");
    return;
  }
  WiFiClient client = server.client();
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

struct RecordResult {
  String label;
  String prefix;
  uint32_t saved;
  uint32_t requested;
  uint32_t nullFb;
  uint32_t openFail;
  uint32_t elapsedMs;
  String error;
};

bool isValidLabel(const String& label) {
  for (int i = 0; i < kLabelCount; ++i) {
    if (label == kLabels[i]) return true;
  }
  return false;
}

RecordResult recordClip(const String& label, int frames, int targetFps) {
  RecordResult r{label, "", 0, (uint32_t)frames, 0, 0, 0, ""};
  String dir = String("/dataset/") + label;
  uint32_t timestamp = millis();
  r.prefix = label + "_" + String(timestamp);
  uint32_t frameInterval = 1000 / targetFps;
  uint32_t firstFrameMs = millis();

  for (int i = 1; i <= frames; ++i) {
    uint32_t deadline = firstFrameMs + (i - 1) * frameInterval;
    int32_t wait = static_cast<int32_t>(deadline) - static_cast<int32_t>(millis());
    if (wait > 0) delay(wait);

    uint32_t t0 = millis();
    camera_fb_t* fb = esp_camera_fb_get();
    uint32_t tGrab = millis() - t0;
    if (fb == nullptr) {
      ++r.nullFb;
      if (r.error.length() == 0) r.error = "null framebuffer";
      Serial.printf("  frame %d: fb_get NULL after %ums\n", i, tGrab);
      continue;
    }
    if (tGrab > 200) Serial.printf("  frame %d: slow grab %ums\n", i, tGrab);
    if (fb->len == 0) {
      ++r.nullFb;
      if (r.error.length() == 0) r.error = "empty framebuffer";
      esp_camera_fb_return(fb);
      continue;
    }

    char idx[12];
    snprintf(idx, sizeof(idx), "_%03d.jpg", i);
    String path = dir + "/" + r.prefix + idx;
    File out = SD_MMC.open(path, FILE_WRITE);
    if (!out) {
      ++r.openFail;
      if (r.error.length() == 0) r.error = String("open failed: ") + path;
      Serial.printf("Failed to open %s for write\n", path.c_str());
      esp_camera_fb_return(fb);
      continue;
    }
    size_t written = out.write(fb->buf, fb->len);
    out.close();
    if (written != fb->len) {
      ++r.openFail;
      if (r.error.length() == 0) r.error = "short write (SD full?)";
    } else {
      ++r.saved;
    }
    esp_camera_fb_return(fb);
  }

  r.elapsedMs = millis() - firstFrameMs;
  return r;
}

void handleRecord() {
  if (!g_sdReady) {
    Serial.println("/record: SD not ready");
    sendPlain(503, "SD card not ready");
    return;
  }
  if (!server.hasArg("label") || !server.hasArg("frames")) {
    sendPlain(400, "Missing label or frames");
    return;
  }
  String label = server.arg("label");
  int frames = server.arg("frames").toInt();
  int targetFps = server.hasArg("fps") ? server.arg("fps").toInt() : 15;
  if (frames < 1 || frames > 60) frames = 15;
  if (targetFps < 5 || targetFps > 30) targetFps = 15;
  if (!isValidLabel(label)) { sendPlain(400, "Unknown label"); return; }

  RecordResult r = recordClip(label, frames, targetFps);
  Serial.printf("/record %s: saved=%u/%u nullFb=%u openFail=%u elapsed=%ums\n",
                r.label.c_str(), r.saved, r.requested, r.nullFb, r.openFail, r.elapsedMs);

  int httpCode = (r.saved == r.requested) ? 200 : 207;
  if (r.saved == 0) httpCode = 500;

  String escapedErr;
  for (size_t i = 0; i < r.error.length(); ++i) {
    char c = r.error[i];
    if (c == '"' || c == '\\') escapedErr += '\\';
    escapedErr += c;
  }
  String body = String("{\"label\":\"") + r.label +
                "\",\"prefix\":\"" + r.prefix +
                "\",\"saved\":" + String(r.saved) +
                ",\"requested\":" + String(r.requested) +
                ",\"null_fb\":" + String(r.nullFb) +
                ",\"open_fail\":" + String(r.openFail) +
                ",\"elapsed_ms\":" + String(r.elapsedMs) +
                ",\"error\":\"" + escapedErr + "\"}";
  server.send(httpCode, "application/json", body);
}

void handleCounts() {
  String body = "{";
  for (int i = 0; i < kLabelCount; ++i) {
    if (i > 0) body += ",";
    body += "\"";
    body += kLabels[i];
    body += "\":";
    body += String(g_sdReady ? countClipsForLabel(kLabels[i]) : 0);
  }
  body += "}";
  server.send(200, "application/json", body);
}

void handleDeleteLast() {
  if (!g_sdReady) { sendPlain(503, "SD not ready"); return; }
  if (!server.hasArg("label")) { sendPlain(400, "Missing label"); return; }
  String label = server.arg("label");
  String prefix = lastClipPrefixForLabel(label.c_str());
  bool ok = deleteClip(label.c_str(), prefix);
  String body = String("{\"deleted\":") + (ok ? "true" : "false") +
                ",\"prefix\":\"" + prefix + "\"}";
  server.send(200, "application/json", body);
}

void writeUint32LE(File& f, uint32_t v) {
  uint8_t b[4] = {
      static_cast<uint8_t>(v & 0xFF),
      static_cast<uint8_t>((v >> 8) & 0xFF),
      static_cast<uint8_t>((v >> 16) & 0xFF),
      static_cast<uint8_t>((v >> 24) & 0xFF)};
  f.write(b, 4);
}

void streamUint32LE(WiFiClient& c, uint32_t v) {
  uint8_t b[4] = {
      static_cast<uint8_t>(v & 0xFF),
      static_cast<uint8_t>((v >> 8) & 0xFF),
      static_cast<uint8_t>((v >> 16) & 0xFF),
      static_cast<uint8_t>((v >> 24) & 0xFF)};
  c.write(b, 4);
}

void streamUint16LE(WiFiClient& c, uint16_t v) {
  uint8_t b[2] = {
      static_cast<uint8_t>(v & 0xFF),
      static_cast<uint8_t>((v >> 8) & 0xFF)};
  c.write(b, 2);
}

// CRC32 (IEEE 802.3) for ZIP entries.
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (int k = 0; k < 8; ++k) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(int32_t)(crc & 1));
    }
  }
  return ~crc;
}

struct ZipEntry {
  String name;
  uint32_t crc;
  uint32_t size;
  uint32_t offset;
};

void streamFileToZip(WiFiClient& client, const String& fsPath, const String& zipName,
                     std::vector<ZipEntry>& entries, uint32_t& runningOffset) {
  File f = SD_MMC.open(fsPath);
  if (!f || f.isDirectory()) return;

  uint32_t size = f.size();
  uint32_t crc = 0;

  // Local file header
  ZipEntry e;
  e.name = zipName;
  e.size = size;
  e.offset = runningOffset;

  // Compute CRC by reading file once first (we use stored, so no compression).
  // To avoid reading the file twice we buffer in chunks: write LFH with placeholder
  // crc/size is not allowed without data descriptor; simplest is to read once for CRC.
  uint8_t buf[512];
  while (true) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    crc = crc32_update(crc, buf, n);
  }
  f.close();
  e.crc = crc;

  client.write((const uint8_t*)"PK\x03\x04", 4);
  streamUint16LE(client, 20);          // version needed
  streamUint16LE(client, 0);           // flags
  streamUint16LE(client, 0);           // method: stored
  streamUint16LE(client, 0);           // mod time
  streamUint16LE(client, 0);           // mod date
  streamUint32LE(client, crc);
  streamUint32LE(client, size);        // compressed size
  streamUint32LE(client, size);        // uncompressed size
  streamUint16LE(client, zipName.length());
  streamUint16LE(client, 0);           // extra length
  client.write((const uint8_t*)zipName.c_str(), zipName.length());

  uint32_t headerSize = 30 + zipName.length();

  // Stream file data
  File f2 = SD_MMC.open(fsPath);
  if (f2) {
    while (true) {
      int n = f2.read(buf, sizeof(buf));
      if (n <= 0) break;
      client.write(buf, n);
    }
    f2.close();
  }

  runningOffset += headerSize + size;
  entries.push_back(e);
}

void streamDirToZip(WiFiClient& client, const String& dirPath, const String& zipPrefix,
                    std::vector<ZipEntry>& entries, uint32_t& runningOffset) {
  File d = SD_MMC.open(dirPath);
  if (!d || !d.isDirectory()) return;
  File f = d.openNextFile();
  while (f) {
    String name = String(f.name());
    int slash = name.lastIndexOf('/');
    String base = (slash >= 0) ? name.substring(slash + 1) : name;
    if (f.isDirectory()) {
      streamDirToZip(client, name, zipPrefix + base + "/", entries, runningOffset);
    } else {
      streamFileToZip(client, name, zipPrefix + base, entries, runningOffset);
    }
    f = d.openNextFile();
  }
  d.close();
}

void handleDatasetZip() {
  if (!g_sdReady) { sendPlain(503, "SD not ready"); return; }

  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: application/zip\r\n");
  client.print("Content-Disposition: attachment; filename=dataset.zip\r\n");
  client.print("Connection: close\r\n\r\n");

  std::vector<ZipEntry> entries;
  uint32_t offset = 0;
  streamDirToZip(client, "/dataset", "dataset/", entries, offset);

  uint32_t centralStart = offset;
  for (auto& e : entries) {
    client.write((const uint8_t*)"PK\x01\x02", 4);
    streamUint16LE(client, 20);          // version made
    streamUint16LE(client, 20);          // version needed
    streamUint16LE(client, 0);
    streamUint16LE(client, 0);
    streamUint16LE(client, 0);
    streamUint16LE(client, 0);
    streamUint32LE(client, e.crc);
    streamUint32LE(client, e.size);
    streamUint32LE(client, e.size);
    streamUint16LE(client, e.name.length());
    streamUint16LE(client, 0);
    streamUint16LE(client, 0);           // comment len
    streamUint16LE(client, 0);           // disk number
    streamUint16LE(client, 0);           // internal attrs
    streamUint32LE(client, 0);           // external attrs
    streamUint32LE(client, e.offset);
    client.write((const uint8_t*)e.name.c_str(), e.name.length());
    offset += 46 + e.name.length();
  }
  uint32_t centralSize = offset - centralStart;

  client.write((const uint8_t*)"PK\x05\x06", 4);
  streamUint16LE(client, 0);
  streamUint16LE(client, 0);
  streamUint16LE(client, entries.size());
  streamUint16LE(client, entries.size());
  streamUint32LE(client, centralSize);
  streamUint32LE(client, centralStart);
  streamUint16LE(client, 0);
}

void handleRoot() {
  String html;
  html.reserve(6000);
  html += F("<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>SmartHub Dataset Capture</title>");
  html += F("<style>");
  html += F("body{font-family:sans-serif;margin:0;background:#111;color:#eee}");
  html += F(".wrap{max-width:920px;margin:0 auto;padding:16px}");
  html += F("h1{margin:8px 0 16px}");
  html += F(".row{display:flex;gap:16px;flex-wrap:wrap}");
  html += F(".col{flex:1;min-width:300px}");
  html += F("img{width:100%;max-width:480px;border:1px solid #333;background:#000;image-rendering:pixelated}");
  html += F("label{display:block;margin:8px 0 4px;color:#aaa}");
  html += F("input,select{width:120px;padding:6px;background:#222;color:#eee;border:1px solid #333;border-radius:4px}");
  html += F("button{padding:10px 18px;margin:4px 4px 4px 0;border-radius:4px;border:0;cursor:pointer;font-size:14px}");
  html += F(".start{background:#2a7;color:#fff}.stop{background:#a33;color:#fff}");
  html += F(".sec{background:#444;color:#ddd}");
  html += F("#banner{padding:24px;text-align:center;font-size:28px;font-weight:bold;background:#222;border-radius:8px;margin:12px 0}");
  html += F("#banner.rec{background:#a22;animation:pulse 0.5s infinite alternate}");
  html += F("#banner.ready{background:#262}");
  html += F("@keyframes pulse{from{opacity:0.7}to{opacity:1}}");
  html += F("table{width:100%;border-collapse:collapse;margin-top:12px}");
  html += F("td,th{padding:6px;border-bottom:1px solid #333;text-align:left}");
  html += F("a{color:#7cd4f2}");
  html += F("</style></head><body><div class=\"wrap\">");
  html += F("<h1>SmartHub Dataset Capture</h1>");
  // Capture is driven on-device over Serial. This page is preview + counts only.
  html += F("<div id=\"banner\">Capture runs over Serial. Watch the serial monitor for prompts.</div>");
  html += F("<div class=\"row\">");
  html += F("<div class=\"col\"><img id=\"preview\" src=\"\"></div>");
  html += F("<div class=\"col\">");
  html += F("<table id=\"counts\"></table>");
  html += F("<p><a href=\"/dataset.zip\">Download dataset.zip</a></p>");
  html += F("</div></div>");
  html += F("<script>");
  html += F("const preview=document.getElementById('preview');");
  html += F("preview.src='/stream?t='+Date.now();");
  html += F("async function refreshCounts(){");
  html += F("  try{let r=await fetch('/counts');let j=await r.json();");
  html += F("  let h='<tr><th>Gesture</th><th>Clips</th></tr>';");
  html += F("  for(let k of Object.keys(j))h+='<tr><td>'+k+'</td><td>'+j[k]+'</td></tr>';");
  html += F("  document.getElementById('counts').innerHTML=h;}catch(e){}");
  html += F("}");
  html += F("refreshCounts();setInterval(refreshCounts,3000);");
  html += F("</script></body></html>");
  server.send(200, "text/html", html);
}

void startServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/record", HTTP_POST, handleRecord);
  server.on("/counts", HTTP_GET, handleCounts);
  server.on("/delete_last", HTTP_POST, handleDeleteLast);
  server.on("/dataset.zip", HTTP_GET, handleDatasetZip);
  server.onNotFound([]() { sendPlain(404, "Not found"); });
  server.begin();
  Serial.println("Dataset capture HTTP server started.");
}

// ---- Continuous capture loop ------------------------------------------
// Stream raw frames to /dataset/new_data/ at a fixed rate. No labeling,
// no countdowns — every frame is saved as a JPEG and a Serial line is
// emitted per save so the host can monitor progress.

constexpr int kStreamTargetFps = 15;
constexpr uint32_t kStreamFrameIntervalMs = 1000 / kStreamTargetFps;
constexpr const char* kStreamDir = "/dataset/new_data";
constexpr uint32_t kStreamMaxFrames = 2000;  // hard cap per boot

bool g_capturePaused = false;
bool g_streamCapReached = false;
uint32_t g_streamSeq = 0;          // sequential index within this boot
uint32_t g_streamBootStamp = 0;    // millis at first save, used as filename prefix
uint32_t g_streamNextDeadlineMs = 0;

// Forward decl for the test-model toggle.
bool switchToInferenceMode();
bool switchToCaptureMode();

void pollSerialControls() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;
    switch (c) {
      case 'p': case 'P':
        g_capturePaused = !g_capturePaused;
        Serial.printf("[CTRL]  %s\n", g_capturePaused ? "Paused" : "Resumed");
        break;
      case 't': case 'T':
        if (g_camMode == CAM_MODE_CAPTURE) switchToInferenceMode();
        else                               switchToCaptureMode();
        break;
      case '?': case 'h': case 'H':
        Serial.println("[HELP]  p=pause/resume  t=toggle test-model  ?=help");
        break;
      default:
        break;  // ignore newlines, stray chars
    }
  }
}

void runCaptureCycle() {
  pollSerialControls();
  if (g_capturePaused) {
    static uint32_t lastNote = 0;
    if (millis() - lastNote > 2000) {
      Serial.println("[PAUSE] (press p to resume)");
      lastNote = millis();
    }
    delay(100);
    return;
  }
  if (!g_sdReady) {
    Serial.println("[ERR]   SD not ready; capture loop idle");
    delay(2000);
    return;
  }
  if (g_streamSeq >= kStreamMaxFrames) {
    if (!g_streamCapReached) {
      Serial.printf("[DONE]  reached cap of %u frames; idling. Reset board to record more.\n",
                    static_cast<unsigned>(kStreamMaxFrames));
      g_streamCapReached = true;
    }
    delay(500);
    return;
  }

  uint32_t now = millis();
  if (g_streamNextDeadlineMs == 0) g_streamNextDeadlineMs = now;
  int32_t wait = static_cast<int32_t>(g_streamNextDeadlineMs) - static_cast<int32_t>(now);
  if (wait > 0) {
    delay(wait);
    now = millis();
  }
  g_streamNextDeadlineMs = now + kStreamFrameIntervalMs;

  uint32_t t0 = millis();
  camera_fb_t* fb = esp_camera_fb_get();
  uint32_t tGrab = millis() - t0;
  if (fb == nullptr) {
    Serial.printf("[ERR]   fb_get NULL after %ums\n", tGrab);
    return;
  }
  if (fb->len == 0) {
    Serial.println("[ERR]   empty framebuffer");
    esp_camera_fb_return(fb);
    return;
  }

  if (g_streamBootStamp == 0) g_streamBootStamp = millis();
  ++g_streamSeq;

  char filename[64];
  snprintf(filename, sizeof(filename), "%s/%lu_%06lu.jpg",
           kStreamDir,
           static_cast<unsigned long>(g_streamBootStamp),
           static_cast<unsigned long>(g_streamSeq));

  File out = SD_MMC.open(filename, FILE_WRITE);
  if (!out) {
    Serial.printf("[ERR]   open failed: %s\n", filename);
    esp_camera_fb_return(fb);
    return;
  }
  size_t written = out.write(fb->buf, fb->len);
  out.close();
  esp_camera_fb_return(fb);

  if (written == fb->len) {
    Serial.printf("[SAVE]  %s (%u bytes, grab=%ums)\n",
                  filename, static_cast<unsigned>(written), tGrab);
  } else {
    Serial.printf("[FAIL]  short write %s (%u/%u, SD full?)\n",
                  filename, static_cast<unsigned>(written),
                  static_cast<unsigned>(fb->len));
  }
}

// ---- Test-model (FOMO inference) loop ----------------------------------
// In inference mode the camera runs in RGB565 96x96. Each frame is converted
// into the packed-RGB888 float format the EI image-DSP block expects, then
// run_classifier produces FOMO bounding boxes. We average box centroids and
// print either "NO HAND" or "HAND <x,y>" (pixel coords in 96x96 input space).

constexpr int kEiInputW = EI_CLASSIFIER_INPUT_WIDTH;   // 96
constexpr int kEiInputH = EI_CLASSIFIER_INPUT_HEIGHT;  // 96
constexpr int kEiPixels = kEiInputW * kEiInputH;

// Buffer holding one packed-RGB888 pixel per element, expressed as float for
// the SDK's signal_t callback. Allocated in PSRAM to keep DRAM free.
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
// comes second in memory. Reconstruct the 16-bit pixel before unpacking.
void rgb565ToPackedFloats(const uint8_t* fb, size_t pixelCount, float* out) {
  // ESP32 camera RGB565 framebuffers are stored byte-swapped: the high byte
  // of the 16-bit pixel comes FIRST in memory. The Edge Impulse FOMO image
  // block expects packed RGB888 floats (one float per pixel, value =
  // (r<<16)|(g<<8)|b). For a grayscale model trained on this firmware's
  // capture pipeline, the sensor's grayscale special_effect makes R≈G≈B,
  // so the packed value carries the luminance EI's grayscale block expects.
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

bool switchToInferenceMode() {
  Serial.println("[MODE]  switching to test-model (inference)...");
  if (!ensureFeatureBuffer()) return false;
  if (!initCamera(CAM_MODE_INFERENCE)) {
    Serial.println("[ERR]   inference camera init failed");
    return false;
  }
  Serial.printf("[MODE]  inference active (input %dx%d, threshold %.2f)\n",
                kEiInputW, kEiInputH, kHandConfThreshold);
  Serial.println("[MODE]  press 't' again to return to capture mode.");
  return true;
}

bool switchToCaptureMode() {
  Serial.println("[MODE]  switching to capture...");
  if (!initCamera(CAM_MODE_CAPTURE)) {
    Serial.println("[ERR]   capture camera init failed");
    return false;
  }
  Serial.println("[MODE]  capture active.");
  return true;
}

void runInferenceCycle() {
  pollSerialControls();
  // Mode may have flipped back inside pollSerialControls.
  if (g_camMode != CAM_MODE_INFERENCE) return;

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

  // Always dump frames in inference mode — consumed by tools/live_viewer.py.
  // Header is plain text so a human can still grep the serial log; payload
  // is raw RGB565 (little-endian as stored in the framebuffer).
  Serial.printf("FRAME %u %d %.2f %.2f\n",
                static_cast<unsigned>(fb->len),
                hasHand ? 1 : 0, cx, cy);
  Serial.write(fb->buf, fb->len);
  Serial.write('\n');

  esp_camera_fb_return(fb);
}

}  // namespace

void setup() {
  Serial.begin(921600);
  delay(500);
  Serial.println();
  Serial.println("SmartHub XIAO ESP32S3 Sense dataset capture firmware");
  Serial.printf("PSRAM: %s, free heap: %u bytes\n",
                psramFound() ? "yes" : "no",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));

  if (!initCamera(CAM_MODE_CAPTURE)) return;
  g_sdReady = initSdCard();
  if (!g_sdReady) {
    Serial.println("WARNING: SD not available. /record will return 503.");
  }
  if (!connectWiFi()) return;
  startServer();

  Serial.println();
  Serial.println("=== Continuous capture active ===");
  Serial.println("Controls: p=pause/resume  t=toggle test-model  ?=help");
  Serial.printf("Streaming to %s @ %d fps, cap %u frames per boot\n",
                kStreamDir, kStreamTargetFps,
                static_cast<unsigned>(kStreamMaxFrames));
  Serial.println();
}

void loop() {
  server.handleClient();
  if (g_camMode == CAM_MODE_INFERENCE) {
    runInferenceCycle();
  } else {
    runCaptureCycle();
  }
}
