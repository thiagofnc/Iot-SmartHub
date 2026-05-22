#include "WebServerManager.h"

#if __has_include("MainConfig.h")
#include "MainConfig.h"
#else
#include "MainConfig.example.h"
#endif

namespace {
constexpr unsigned long kWeatherUpdateIntervalMs = 10UL * 60UL * 1000UL;
const char *kHeaderKeys[] = {"User-Agent", "Cookie"};

const char kLoginHtml[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartHub Login</title></head><body>
<h1>SmartHub Admin</h1><form method="post" action="/login">
<input name="USERNAME" placeholder="Username"><input name="PASSWORD" type="password" placeholder="Password">
<button type="submit">Sign in</button></form><p><a href="/">Back</a></p></body></html>
)HTML";

const char kHomeHtml[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartHub</title></head><body>
<h1>SmartHub Main Controller</h1>
<p><a href="/login">Admin login</a> | <a href="/api/weather">Weather JSON</a></p>
<p>Brightness: <input id="b" type="range" min="0" max="255" value="128"></p>
<p>Rotation: <button onclick="rot(0)">0</button> <button onclick="rot(90)">90</button></p>
<script>
document.getElementById('b').oninput=e=>fetch('/api/brightness?value='+e.target.value);
function rot(v){fetch('/api/rotation?value='+v)}
</script></body></html>
)HTML";
}  // namespace

WebServerManager::WebServerManager() : server(80) {}

void WebServerManager::begin() {
  connectWifi();
  updateOpenWeather();

  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/login", HTTP_GET, [this]() { handleLogin(); });
  server.on("/login", HTTP_POST, [this]() { handleLogin(); });
  server.on("/logout", HTTP_GET, [this]() { handleLogout(); });
  server.on("/api/brightness", HTTP_GET, [this]() { handleBrightness(); });
  server.on("/api/rotation", HTTP_GET, [this]() { handleRotation(); });
  server.on("/api/weather", HTTP_GET, [this]() { handleWeather(); });
  server.on("/battery", HTTP_GET, [this]() { handleBattery(); });
  server.collectHeaders(kHeaderKeys, sizeof(kHeaderKeys) / sizeof(kHeaderKeys[0]));
  server.begin();
  Serial.println("HTTP web server ready");
}

void WebServerManager::tick() {
  server.handleClient();
  if (millis() - lastWeatherUpdateMs >= kWeatherUpdateIntervalMs) {
    updateOpenWeather();
  }
}

void WebServerManager::connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(MAIN_WIFI_SSID, MAIN_WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi: %s", MAIN_WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nMain controller IP: %s\n", WiFi.localIP().toString().c_str());
}

void WebServerManager::updateOpenWeather() {
  lastWeatherUpdateMs = millis();
  if (WiFi.status() != WL_CONNECTED || MAIN_OPENWEATHER_API_KEY[0] == '\0') return;

  HTTPClient http;
  http.setTimeout(1500);
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=";
  url += MAIN_OPENWEATHER_LATITUDE;
  url += "&lon=";
  url += MAIN_OPENWEATHER_LONGITUDE;
  url += "&appid=";
  url += MAIN_OPENWEATHER_API_KEY;
  url += "&units=";
  url += MAIN_OPENWEATHER_UNITS;

  http.begin(url);
  const int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      currentTemp = doc["main"]["temp"].as<double>();
      currentHumidity = doc["main"]["humidity"].as<double>();
      currentDescription = doc["weather"][0]["description"].as<String>();
    }
  } else {
    Serial.printf("OpenWeather request failed: %d\n", code);
  }
  http.end();
}

bool WebServerManager::isAuthenticated() {
  return server.hasHeader("Cookie") && server.header("Cookie").indexOf("ESPSESSIONID=1") >= 0;
}

void WebServerManager::handleRoot() {
  server.send(200, "text/html", kHomeHtml);
}

void WebServerManager::handleLogin() {
  if (server.method() == HTTP_POST) {
    if (server.arg("USERNAME") == MAIN_ADMIN_USERNAME &&
        server.arg("PASSWORD") == MAIN_ADMIN_PASSWORD) {
      server.sendHeader("Location", "/");
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
      server.send(303);
      return;
    }
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  server.send(200, "text/html", kLoginHtml);
}

void WebServerManager::handleLogout() {
  server.sendHeader("Location", "/");
  server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
  server.send(303);
}

void WebServerManager::handleBrightness() {
  requestedBrightness = constrain(server.arg("value").toInt(), 0, 255);
  server.send(200, "text/plain", "OK");
}

void WebServerManager::handleRotation() {
  if (!isAuthenticated()) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  requestedRotation = constrain(server.arg("value").toInt(), 0, 90);
  server.send(200, "text/plain", "OK");
}

void WebServerManager::handleBattery() {
  // Accept ?phone= / ?ipad= / ?watch= / ?device= in a single request (Apple
  // Shortcuts posts one slot at a time but we tolerate batched calls).
  bool updated = false;
  const struct { const char* key; int* dst; } slots[] = {
      {"phone",  &phoneBattery},
      {"ipad",   &ipadBattery},
      {"watch",  &watchBattery},
      {"device", &deviceBattery},
  };
  for (const auto& s : slots) {
    if (!server.hasArg(s.key)) continue;
    const int v = constrain(server.arg(s.key).toInt(), 0, 100);
    *s.dst = v;
    Serial.printf("Battery %s = %d%%\n", s.key, v);
    updated = true;
  }
  server.send(updated ? 200 : 400, "text/plain", updated ? "OK" : "use ?phone=85");
}

void WebServerManager::handleWeather() {
  JsonDocument doc;
  doc["temp"] = currentTemp;
  doc["humidity"] = currentHumidity;
  doc["description"] = currentDescription;
  doc["remoteTemp"] = remoteTemp;
  doc["remoteHumidity"] = remoteHumidity;

  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}
