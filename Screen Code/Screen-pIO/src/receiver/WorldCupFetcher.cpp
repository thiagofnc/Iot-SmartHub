#include "WorldCupFetcher.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WorldCupFetcher::WorldCupFetcher(const char* apiKey) {
    _apiKey = apiKey;
    _matchesUrl = "https://api.football-data.org/v4/competitions/WC/matches";
}

void WorldCupFetcher::begin() {
    // Basic initialization could go here
    Serial.println("WorldCupFetcher initialized.");
}

bool WorldCupFetcher::fetchMatches() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error: WiFi not connected! Cannot fetch World Cup data.");
        return false;
    }

    HTTPClient http;
    Serial.print("Connecting to: ");
    Serial.println(_matchesUrl);
    
    http.begin(_matchesUrl);
    http.addHeader("X-Auth-Token", _apiKey); // API key goes in the header

    int httpCode = http.GET();
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        // Since the API response could be large, ArduinoJson 7 will adjust dynamically,
        // but we still want to be careful with RAM on the ESP32.
        
        // Use a filter to only parse the fields we care about (saves massive amounts of memory)
        JsonDocument filter;
        filter["matches"][0]["homeTeam"]["shortName"] = true;
        filter["matches"][0]["awayTeam"]["shortName"] = true;
        filter["matches"][0]["status"] = true;
        filter["matches"][0]["score"]["fullTime"]["home"] = true;
        filter["matches"][0]["score"]["fullTime"]["away"] = true;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        
        if (!error) {
            Serial.println("--- World Cup Matches ---");
            JsonArray matches = doc["matches"];
            
            for (JsonObject match : matches) {
                const char* homeTeam = match["homeTeam"]["shortName"];
                const char* awayTeam = match["awayTeam"]["shortName"];
                const char* status = match["status"];
                
                int homeScore = match["score"]["fullTime"]["home"] | 0;
                int awayScore = match["score"]["fullTime"]["away"] | 0;
                
                if (homeTeam && awayTeam) {
                    Serial.printf("[%s] %s %d - %d %s\n", status, homeTeam, homeScore, awayScore, awayTeam);
                    Serial.println(status + String(" ") + homeTeam + String(" ") + homeScore + String(" - ") + awayScore + String(" ") + awayTeam);

                }
            }
            Serial.println("-------------------------");
            success = true;
        } else {
            Serial.print("JSON Parse failed: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
        String payload = http.getString();
        Serial.println("Response payload: " + payload);
    }
    
    http.end();
    return success;
}

