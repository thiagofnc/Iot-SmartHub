#include "WorldCupFetcher.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Use UART1 for communication with the display ESP32
// (UART0 / Serial is used for USB debug)
HardwareSerial DisplaySerial(1);

WorldCupFetcher::WorldCupFetcher(const char* apiKey) {
    _apiKey = apiKey;
    _matchesUrl = "https://api.football-data.org/v4/competitions/WC/matches";
    _lastFetchTime = 0;
    nextMatchHome = "";
    nextMatchAway = "";
    nextMatchDate = "";
    nextMatchStatus = "";
}

void WorldCupFetcher::begin() {
    // Initialize UART1 on the configured pins for display communication
    DisplaySerial.begin(9600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.println("WorldCupFetcher initialized. UART1 ready on TX=" + String(UART_TX_PIN) + " RX=" + String(UART_RX_PIN));
}

void WorldCupFetcher::update() {
    unsigned long now = millis();
    // Fetch on first call (_lastFetchTime == 0) or when the interval has elapsed
    if (_lastFetchTime == 0 || (now - _lastFetchTime >= FETCH_INTERVAL)) {
        _lastFetchTime = now;
        if (fetchMatches()) {
            sendToDisplay();
        }
    }
}

bool WorldCupFetcher::fetchMatches() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error: WiFi not connected! Cannot fetch World Cup data.");
        return false;
    }

    HTTPClient* http = new HTTPClient();
    WiFiClientSecure* client = new WiFiClientSecure();
    client->setInsecure(); // Skip certificate verification (free tier doesn't need it)
    
    Serial.println("[WC] Fetching matches from football-data.org...");
    
    http->begin(*client, _matchesUrl);
    http->addHeader("X-Auth-Token", _apiKey); // API key goes in the header
    http->addHeader("User-Agent", "ESP32");
    http->useHTTP10(true); // VERY IMPORTANT when using ArduinoJson with getStream()
    http->setTimeout(5000);

    int httpCode = http->GET();
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        // Use a filter to only parse the fields we care about (saves memory)
        JsonDocument filter;
        
        filter["matches"][0]["homeTeam"]["name"] = true;
        filter["matches"][0]["homeTeam"]["shortName"] = true;
        filter["matches"][0]["homeTeam"]["tla"] = true;
        filter["matches"][0]["awayTeam"]["name"] = true;
        filter["matches"][0]["awayTeam"]["shortName"] = true;
        filter["matches"][0]["awayTeam"]["tla"] = true;
        filter["matches"][0]["status"] = true;
        filter["matches"][0]["utcDate"] = true;
        filter["matches"][0]["score"]["fullTime"]["home"] = true;
        filter["matches"][0]["score"]["fullTime"]["away"] = true;

        JsonDocument* doc = new JsonDocument();
        DeserializationError error = deserializeJson(*doc, http->getStream(), DeserializationOption::Filter(filter));
        
        if (!error) {
            JsonArray matches = (*doc)["matches"];
            Serial.printf("[WC] Parsed %d matches array elements.\n", matches.size());
            
            bool foundUpcoming = false;
            
            Serial.printf("[WC] Parsed %d matches array elements.\n", matches.size());

            for (JsonObject match : matches) {
                const char* status = match["status"];
                
                // Print all home teams to the Serial Monitor
                const char* anyHomeTeam = match["homeTeam"]["shortName"] | match["homeTeam"]["name"] | "TBD";
                Serial.printf("[WC] Found Home Team: %s\n", anyHomeTeam);
                
                // football-data.org statuses:
                //   SCHEDULED, TIMED — upcoming matches
                //   IN_PLAY, PAUSED — live matches
                //   FINISHED — completed
                //   POSTPONED, CANCELLED, SUSPENDED

                if (status && (strcmp(status, "SCHEDULED") == 0 || 
                               strcmp(status, "TIMED") == 0 ||
                               strcmp(status, "IN_PLAY") == 0 ||
                               strcmp(status, "PAUSED") == 0)) {
                    
                    const char* homeTeam = match["homeTeam"]["shortName"] | match["homeTeam"]["name"] | "TBD";
                    const char* awayTeam = match["awayTeam"]["shortName"] | match["awayTeam"]["name"] | "TBD";
                    const char* utcDate  = match["utcDate"];

                    if (homeTeam && awayTeam) {
                        nextMatchHome   = String(homeTeam);
                        nextMatchAway   = String(awayTeam);
                        nextMatchDate   = utcDate ? String(utcDate) : "TBD";
                        nextMatchStatus = String(status);
                        foundUpcoming = true;
                        
                        Serial.printf("[WC] Next match: %s vs %s (%s) [%s]\n", 
                                      homeTeam, awayTeam, utcDate, status);
                        break; // Take the first upcoming/live match
                    }
                }
            }

            if (!foundUpcoming) {
                // No upcoming matches — show the last finished match instead
                Serial.println("[WC] No upcoming matches found. Showing last result.");
                for (int i = matches.size() - 1; i >= 0; i--) {
                    JsonObject match = matches[i];
                    const char* status = match["status"];
                    if (status && strcmp(status, "FINISHED") == 0) {
                        const char* homeTeam = match["homeTeam"]["shortName"];
                        const char* awayTeam = match["awayTeam"]["shortName"];
                        int homeScore = match["score"]["fullTime"]["home"] | 0;
                        int awayScore = match["score"]["fullTime"]["away"] | 0;

                        if (homeTeam && awayTeam) {
                            nextMatchHome   = String(homeTeam);
                            nextMatchAway   = String(awayTeam);
                            nextMatchDate   = String(homeScore) + " - " + String(awayScore);
                            nextMatchStatus = "FINISHED";
                            
                            Serial.printf("[WC] Last result: %s %d - %d %s\n", 
                                          homeTeam, homeScore, awayScore, awayTeam);
                        }
                        break;
                    }
                }
            }

            success = true;
        } else {
            Serial.print("[WC] JSON Parse failed: ");
            Serial.println(error.c_str());
        }
        delete doc;
    } else {
        Serial.printf("[WC] HTTP GET failed, code: %d\n", httpCode);
    }
    
    http->end();
    delete http;
    delete client;
    return success;
}

void WorldCupFetcher::sendToDisplay() {
    if (nextMatchHome.length() == 0) {
        Serial.println("[WC] No match data to send.");
        return;
    }

    // Send a structured line over UART that the display ESP32 can parse
    // Format: WC:<status>|<home>|<away>|<date>\n
    String msg = "WC:" + nextMatchStatus + "|" + nextMatchHome + "|" + nextMatchAway + "|" + nextMatchDate;
    
    DisplaySerial.println(msg);
    Serial.println("[WC] Sent to display: " + msg);
}
