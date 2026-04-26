#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>

#include "CameraConfig.h"
#include "camera_pins.h"

namespace {

WebServer server(80);

constexpr char STREAM_CONTENT_TYPE[] =
    "multipart/x-mixed-replace; boundary=123456789000000000000987654321";
constexpr char STREAM_BOUNDARY_LINE[] = "\r\n--123456789000000000000987654321\r\n";

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
    sensor->set_framesize(sensor, psramFound() ? FRAMESIZE_SVGA : FRAMESIZE_VGA);
    sensor->set_quality(sensor, 12);
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

void handleRoot() {
  String html;
  html.reserve(700);
  html += F("<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>SmartHub Camera</title></head><body style=\"font-family:sans-serif;margin:24px;background:#111;color:#eee\">");
  html += F("<h1>SmartHub Camera</h1><p><a style=\"color:#7cd4f2\" href=\"/capture\">Capture still</a></p>");
  html += F("<img src=\"/stream\" style=\"max-width:100%;height:auto;border:1px solid #333\">");
  html += F("</body></html>");
  server.send(200, "text/html", html);
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

void handleStream() {
  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: %s\r\n", STREAM_CONTENT_TYPE);
  client.print("Cache-Control: no-cache\r\n");
  client.print("Connection: close\r\n\r\n");

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
      break;
    }

    client.print(STREAM_BOUNDARY_LINE);
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", static_cast<unsigned>(fb->len));
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (!client.connected()) {
      break;
    }
    delay(30);
  }
}

void startServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/stream", HTTP_GET, handleStream);
  server.onNotFound([]() { sendPlain(404, "Not found"); });
  server.begin();
  Serial.println("Camera HTTP server started.");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("SmartHub XIAO ESP32S3 Sense camera firmware");
  Serial.printf("PSRAM: %s, free heap: %u bytes\n",
                psramFound() ? "yes" : "no",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));

  if (!initCamera()) {
    return;
  }
  if (!connectWiFi()) {
    return;
  }
  startServer();
}

void loop() {
  server.handleClient();
}
