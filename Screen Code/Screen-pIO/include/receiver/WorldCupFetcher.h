#ifndef WORLDCUPFETCHER_H
#define WORLDCUPFETCHER_H

#include <Arduino.h>

// UART pins for communication with the Display ESP32
#define UART_TX_PIN 43   // Receiver TX -> Display RX
#define UART_RX_PIN 44   // Receiver RX <- Display TX

class WorldCupFetcher {
public:
    // Pass your API key when initializing the class
    WorldCupFetcher(const char* apiKey);
    
    // Call this in setup() to initialize UART
    void begin();
    
    // Fetches matches from the API, finds the next upcoming match, and sends it over UART
    bool fetchMatches();
    
    // Sends the next upcoming match info over UART to the display ESP32
    void sendToDisplay();

    // Periodically poll (call from loop)
    void update();

    // Last fetched next match info (accessible for other uses)
    String nextMatchHome;
    String nextMatchAway;
    String nextMatchDate;
    String nextMatchStatus;

private:
    const char* _apiKey;
    const char* _matchesUrl;
    
    unsigned long _lastFetchTime;
    static const unsigned long FETCH_INTERVAL = 30UL * 60UL * 1000UL; // 30 minutes
};

#endif // WORLDCUPFETCHER_H
