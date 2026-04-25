#pragma once

// Copy this file to AppConfig.h and fill in your own values.

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char OPENWEATHER_API_KEY[] = "YOUR_OPENWEATHER_API_KEY";
constexpr char OPENWEATHER_LOCATION[] = "City,CountryCode";
constexpr char OPENWEATHER_UNITS[] = "imperial";

// Spotify Web API. Client ID/secret alone are not enough for user playback
// control; generate a refresh token for your Spotify account with these scopes:
// user-read-currently-playing user-read-playback-state user-modify-playback-state
constexpr char SPOTIFY_CLIENT_ID[] = "YOUR_SPOTIFY_CLIENT_ID";
constexpr char SPOTIFY_CLIENT_SECRET[] = "YOUR_SPOTIFY_CLIENT_SECRET";
constexpr char SPOTIFY_REFRESH_TOKEN[] = "YOUR_SPOTIFY_REFRESH_TOKEN";
constexpr char SPOTIFY_MARKET[] = "US";

// Optional. Leave empty to control Spotify's currently active device.
constexpr char SPOTIFY_DEVICE_ID[] = "";
