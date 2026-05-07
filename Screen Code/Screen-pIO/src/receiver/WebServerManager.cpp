#include "WebServerManager.h"
#include "webPages.h"

// API Key referenced from OpenWeatherMapTemplate project
const String apiKey = "29f5821965081acda326d928f69ea78d";
const String latitud = "39.4835";
const String longitud = "-87.3237";
const String units = "imperial"; // Using imperial since Rose-Hulman is in the US

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
    server.on("/api/brightness", HTTP_GET, [this]() { this->handleBrightness(); });
    server.on("/api/rotation", HTTP_GET, [this]() { this->handleRotation(); });
    server.on("/api/weather", HTTP_GET, [this]() { this->handleWeather(); });

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
    server.send(200, "text/html", indexHtml);
}

void WebServerManager::handleBrightness() {
    if (server.hasArg("value")) {
        requestedBrightness = server.arg("value").toInt();
        Serial.printf("New Brightness: %d\n", requestedBrightness);
    }
    server.send(200, "text/plain", "OK");
}

void WebServerManager::handleRotation() {
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
