#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>

#include "CameraConfig.h"
#include "camera_pins.h"

namespace {

WebServer server(80);

constexpr char STREAM_CONTENT_TYPE[] =
    "multipart/x-mixed-replace; boundary=123456789000000000000987654321";
constexpr char STREAM_BOUNDARY_LINE[] = "\r\n--123456789000000000000987654321\r\n";

constexpr const char* kLabels[] = {"up", "down", "lswipe", "rswipe", "rotate", "idle"};
constexpr int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);
constexpr int kIdleIndex = 5;

bool g_sdReady = false;

void sendPlain(int code, const char* message) {
  server.send(code, "text/plain", message);
}

bool initCamera() {
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_framesize(sensor, FRAMESIZE_96X96);
    sensor->set_quality(sensor, 12);
    sensor->set_special_effect(sensor, 2);  // grayscale
  }
  return true;
}

bool initSdCard() {
  // XIAO ESP32S3 Sense expansion board: built-in microSD slot uses SDIO,
  // 1-bit mode shares fewer pins with the camera bus.
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD_MMC mount failed (1-bit mode).");
    return false;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card detected.");
    return false;
  }
  Serial.printf("SD card mounted, size: %lluMB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));

  for (int i = 0; i < kLabelCount; ++i) {
    String dir = String("/dataset/") + kLabels[i];
    if (!SD_MMC.exists(dir)) {
      SD_MMC.mkdir("/dataset");
      SD_MMC.mkdir(dir);
    }
  }
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

void handleRecord() {
  if (!g_sdReady) {
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

  bool labelOk = false;
  for (int i = 0; i < kLabelCount; ++i) {
    if (label == kLabels[i]) { labelOk = true; break; }
  }
  if (!labelOk) {
    sendPlain(400, "Unknown label");
    return;
  }

  uint32_t timestamp = millis();
  String prefix = label + "_" + String(timestamp);
  String dir = String("/dataset/") + label;
  uint32_t frameInterval = 1000 / targetFps;
  uint32_t saved = 0;
  uint32_t firstFrameMs = millis();

  for (int i = 1; i <= frames; ++i) {
    uint32_t deadline = firstFrameMs + (i - 1) * frameInterval;
    int32_t wait = static_cast<int32_t>(deadline) - static_cast<int32_t>(millis());
    if (wait > 0) delay(wait);

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) continue;

    char idx[8];
    snprintf(idx, sizeof(idx), "_%03d.jpg", i);
    String path = dir + "/" + prefix + idx;
    File out = SD_MMC.open(path, FILE_WRITE);
    if (out) {
      out.write(fb->buf, fb->len);
      out.close();
      ++saved;
    } else {
      Serial.printf("Failed to open %s for write\n", path.c_str());
    }
    esp_camera_fb_return(fb);
  }

  uint32_t elapsed = millis() - firstFrameMs;
  String body = String("{\"label\":\"") + label +
                "\",\"prefix\":\"" + prefix +
                "\",\"saved\":" + String(saved) +
                ",\"requested\":" + String(frames) +
                ",\"elapsed_ms\":" + String(elapsed) + "}";
  server.send(200, "application/json", body);
  Serial.printf("Saved %u/%d frames for %s in %ums\n", saved, frames, label.c_str(), elapsed);
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
  html += F("<div id=\"banner\">Starting...</div>");
  html += F("<div class=\"row\">");
  html += F("<div class=\"col\"><img id=\"preview\" src=\"\"></div>");
  html += F("<div class=\"col\">");
  html += F("<label>Clip duration (s)<input id=\"dur\" type=\"number\" value=\"1.0\" step=\"0.1\" min=\"0.5\"></label>");
  html += F("<label>FPS<input id=\"fps\" type=\"number\" value=\"15\" min=\"5\" max=\"30\"></label>");
  html += F("<label>Pre-clip countdown (s)<input id=\"pre\" type=\"number\" value=\"2\" min=\"1\"></label>");
  html += F("<label>Inter-clip rest (s)<input id=\"rest\" type=\"number\" value=\"1\" min=\"0\"></label>");
  html += F("<label><input id=\"shuffle\" type=\"checkbox\"> Shuffle gesture order</label>");
  html += F("<div style=\"margin-top:12px\">");
  html += F("<button class=\"stop\" onclick=\"toggle()\" id=\"toggleBtn\">PAUSE</button>");
  html += F("<button class=\"sec\" onclick=\"deleteLast()\">Delete last clip</button></div>");
  html += F("<table id=\"counts\"></table>");
  html += F("<p><a href=\"/dataset.zip\">Download dataset.zip</a></p>");
  html += F("</div></div>");
  html += F("<script>");
  html += F("const GESTURES=['up','down','lswipe','rswipe','rotate'];");
  html += F("let running=true;let idx=0;let order=[...GESTURES];let lastLabel='idle';");
  html += F("const preview=document.getElementById('preview');");
  html += F("function showPreview(on){preview.src=on?('/stream?t='+Date.now()):'';}");
  html += F("function nextLabel(){");
  html += F("  if(idx%2===1)return 'idle';");
  html += F("  return order[Math.floor(idx/2)%order.length];");
  html += F("}");
  html += F("function buildOrder(shuf){");
  html += F("  order=[...GESTURES];");
  html += F("  if(shuf){for(let i=order.length-1;i>0;i--){let j=Math.floor(Math.random()*(i+1));[order[i],order[j]]=[order[j],order[i]];}}");
  html += F("}");
  html += F("function setBanner(t,cls){let b=document.getElementById('banner');b.textContent=t;b.className=cls||'';}");
  html += F("async function refreshCounts(){");
  html += F("  try{let r=await fetch('/counts');let j=await r.json();");
  html += F("  let h='<tr><th>Gesture</th><th>Clips</th></tr>';");
  html += F("  for(let k of Object.keys(j))h+='<tr><td>'+k+'</td><td>'+j[k]+'</td></tr>';");
  html += F("  document.getElementById('counts').innerHTML=h;}catch(e){}");
  html += F("}");
  html += F("function sleep(ms){return new Promise(r=>setTimeout(r,ms));}");
  html += F("async function recordOne(label,frames,fps){");
  html += F("  setBanner('RECORDING '+label.toUpperCase(),'rec');");
  html += F("  showPreview(false);");
  html += F("  let ok=false;");
  html += F("  try{let ctrl=new AbortController();");
  html += F("  let timer=setTimeout(()=>ctrl.abort(),Math.max(8000,frames*1000/fps*4));");
  html += F("  let r=await fetch('/record?label='+label+'&frames='+frames+'&fps='+fps,{method:'POST',signal:ctrl.signal});");
  html += F("  clearTimeout(timer);ok=r.ok;}catch(e){console.log('record err',e);}");
  html += F("  showPreview(true);");
  html += F("  return ok;");
  html += F("}");
  html += F("async function loop(){");
  html += F("  while(true){");
  html += F("    while(!running){setBanner('Paused');await sleep(300);}");
  html += F("    let frames=Math.max(1,Math.round(parseFloat(document.getElementById('dur').value)*parseInt(document.getElementById('fps').value)));");
  html += F("    let fps=parseInt(document.getElementById('fps').value);");
  html += F("    let pre=Math.max(1,parseInt(document.getElementById('pre').value));");
  html += F("    let rest=Math.max(0,parseInt(document.getElementById('rest').value));");
  html += F("    let label=nextLabel();lastLabel=label;");
  html += F("    for(let c=pre;c>0;c--){if(!running)break;setBanner('Get ready: '+label.toUpperCase()+'  '+c,'ready');await sleep(1000);}");
  html += F("    if(!running)continue;");
  html += F("    let ok=await recordOne(label,frames,fps);");
  html += F("    await refreshCounts();");
  html += F("    setBanner((ok?'Saved ':'FAILED ')+label.toUpperCase()+'. Rest...');");
  html += F("    await sleep(rest*1000);");
  html += F("    idx++;");
  html += F("  }");
  html += F("}");
  html += F("function toggle(){running=!running;document.getElementById('toggleBtn').textContent=running?'PAUSE':'RESUME';document.getElementById('toggleBtn').className=running?'stop':'start';}");
  html += F("async function deleteLast(){");
  html += F("  showPreview(false);");
  html += F("  try{let r=await fetch('/delete_last?label='+lastLabel,{method:'POST'});");
  html += F("  let j=await r.json();");
  html += F("  if(j.deleted)setBanner('Deleted '+j.prefix);else setBanner('Nothing to delete for '+lastLabel);}catch(e){}");
  html += F("  await refreshCounts();showPreview(true);");
  html += F("}");
  html += F("buildOrder(false);refreshCounts();showPreview(true);loop();");
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

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("SmartHub XIAO ESP32S3 Sense dataset capture firmware");
  Serial.printf("PSRAM: %s, free heap: %u bytes\n",
                psramFound() ? "yes" : "no",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));

  if (!initCamera()) return;
  g_sdReady = initSdCard();
  if (!g_sdReady) {
    Serial.println("WARNING: SD not available. /record will return 503.");
  }
  if (!connectWiFi()) return;
  startServer();
}

void loop() {
  server.handleClient();
}
