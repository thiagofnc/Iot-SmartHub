#ifndef WORLDCUPFETCHER_H
#define WORLDCUPFETCHER_H

#include <Arduino.h>

class WorldCupFetcher {
public:
    // Pass your API key when initializing the class
    WorldCupFetcher(const char* apiKey);
    
    // Call this in setup() if needed
    void begin();
    
    // Fetches and prints out the matches
    bool fetchMatches();
    
    // You can add more methods here like fetchStandings() later

private:
    const char* _apiKey;
    const char* _matchesUrl;
};

#endif // WORLDCUPFETCHER_H
