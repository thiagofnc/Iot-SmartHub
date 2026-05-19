#include "WebServerManager.h"
#include "webPages.h"
#include "secret_do_not_open.h"

// API Key referenced from OpenWeatherMapTemplate project
const String apiKey = "29f5821965081acda326d928f69ea78d";
const String latitud = "39.4835";
const String longitud = "-87.3237";
const String units = "imperial"; // Using imperial since Rose-Hulman is in the US

static const char *headerKeys[] = {"User-Agent", "Cookie"};

WebServerManager::WebServerManager() : server(80) {
    ip = "";
    port = 80;
    requestedBrightness = 128;
    requestedRotation = 0;
    currentTemp = 0;
    currentHum = 0;
    currentDesc = "Loading...";
    remoteTemp = 0;
    remoteHum = 0;
    lastWeatherUpdate = 0;
}

void WebServerManager::init() {
    Serial.println("WebServerManager initialized");

    WiFi.mode(WIFI_STA);
    WiFi.begin("RHIT-OPEN", "");
    Serial.print("\nConnecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected.");
    Serial.print("ESP32 Web Server IP Address: ");
    Serial.println(WiFi.localIP());

    updateOpenWeatherMap();

    // Setup routes
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/login", HTTP_GET,  [this]() { this->handleLogin(); });
    server.on("/login", HTTP_POST, [this]() { this->handleLogin(); });
    server.on("/logout", HTTP_GET, [this]() { this->handleLogout(); });
    server.on("/api/brightness", HTTP_GET, [this]() { this->handleBrightness(); });
    server.on("/api/rotation",   HTTP_GET, [this]() { this->handleRotation(); });
    server.on("/api/weather",    HTTP_GET, [this]() { this->handleWeather(); });

    server.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(char *));
    server.begin();
    Serial.println("HTTP WebServer ready!");
}

void WebServerManager::getRequest() {
    server.handleClient();

    if (millis() - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
        lastWeatherUpdate = millis();
        updateOpenWeatherMap();
    }
}

bool WebServerManager::isAuthenticated() {
    if (server.hasHeader("Cookie")) {
        String cookie = server.header("Cookie");
        if (cookie.indexOf("ESPSESSIONID=1") != -1) {
            return true;
        }
    }
    return false;
}

void WebServerManager::updateOpenWeatherMap() {
    Serial.println("Polling OpenWeatherMap API...");
    if(WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.setTimeout(1500);
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + latitud +
                 "&lon=" + longitud + "&appid=" + apiKey + "&units=" + units;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            currentTemp = doc["main"]["temp"].as<double>();
            currentHum = doc["main"]["humidity"].as<double>();
            currentDesc = doc["weather"][0]["description"].as<String>();
            Serial.println("Successfully updated OpenWeatherMap data.");
        } else {
            Serial.println("Failed to parse JSON.");
        }
    } else {
        Serial.printf("HTTP Error while requesting OpenWeatherMap. Code: %d, Error: %s\n", httpCode, http.errorToString(httpCode).c_str());
    }
    http.end();
}

void WebServerManager::handleRoot() {
    if (isAuthenticated()) {
        server.send(200, "text/html", indexAuthHtml);
    } else {
        server.send(200, "text/html", indexUnauthHtml);
    }
}

void WebServerManager::handleLogin() {
    if (server.method() == HTTP_POST) {
        if (server.hasArg("USERNAME") && server.hasArg("PASSWORD") &&
            server.arg("USERNAME") == ADMIN_USERNAME &&
            server.arg("PASSWORD") == ADMIN_PASSWORD) {
            server.sendHeader("Location", "/");
            server.sendHeader("Cache-Control", "no-cache");
            server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
            server.send(303);
            Serial.println("Admin login successful");
            return;
        }
        // Wrong credentials — redirect back with error flag
        server.sendHeader("Location", "/login?error=1");
        server.sendHeader("Cache-Control", "no-cache");
        server.send(303);
        Serial.println("Login failed: wrong credentials");
        return;
    }
    // GET — serve login page
    server.send(200, "text/html", loginHtml);
}

void WebServerManager::handleLogout() {
    server.sendHeader("Location", "/");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
    server.send(303);
    Serial.println("Admin logged out");
}

void WebServerManager::handleBrightness() {
    if (server.hasArg("value")) {
        requestedBrightness = server.arg("value").toInt();
        Serial.printf("New Brightness: %d\n", requestedBrightness);
    }
    server.send(200, "text/plain", "OK");
}

void WebServerManager::handleRotation() {
    if (!isAuthenticated()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    if (server.hasArg("value")) {
        requestedRotation = server.arg("value").toInt();
        Serial.printf("New Rotation: %d\n", requestedRotation);
    }
    server.send(200, "text/plain", "OK");
}

void WebServerManager::handleWeather() {
    String json = "{\"temp\": " + String(currentTemp, 1) +
                  ", \"humidity\": " + String(currentHum, 1) +
                  ", \"description\": \"" + currentDesc + "\"" +
                  ", \"remoteTemp\": " + String(remoteTemp, 1) +
                  ", \"remoteHum\": " + String(remoteHum, 1) + "}";
    server.send(200, "application/json", json);
}
