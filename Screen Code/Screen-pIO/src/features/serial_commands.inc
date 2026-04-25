// Serial command handling
// ---------------------------------------------------------------------------
void handleSerialCommands() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      String raw = serialCommandBuffer;
      serialCommandBuffer = "";
      raw.trim();
      String cmd = raw;
      cmd.toLowerCase();
      if (cmd.isEmpty()) return;

      if (cmd.startsWith("show ")) {
        String name = raw.substring(5); name.trim();
        if (name.isEmpty()) { Serial.println("use: show <name>"); return; }
        showSdImage(name);
        return;
      }
      if (cmd == "hide") { hideSdImage(); Serial.println("hidden"); return; }

      if (cmd == "clock" || cmd == "c") { showScreen(ScreenIndex::Clock); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "weather" || cmd == "w") { showScreen(ScreenIndex::Weather); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "port") { showScreen(ScreenIndex::Portrait); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "music" || cmd == "m") { showScreen(ScreenIndex::Music); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "toggle" || cmd == "t") { toggleScreen(); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "up" || cmd == "^") {
        // "up" is the gesture that lifts you from the Clock dashboard into the
        // music player. From anywhere else it's a no-op today.
        if (currentScreen == ScreenIndex::Clock) {
          showScreen(ScreenIndex::Music);
        }
        Serial.println(screenName(currentScreen));
        return;
      }
      if (cmd == "down" || cmd == "v") {
        // On Music, "down" is the play/pause toggle. Elsewhere it returns to Clock.
        if (currentScreen == ScreenIndex::Music) {
          musicTogglePlay();
          Serial.println(musicCurrentlyPlaying() ? "play" : "pause");
        } else {
          showScreen(ScreenIndex::Clock);
          Serial.println(screenName(currentScreen));
        }
        return;
      }
      if (cmd == "right" || cmd == ">") {
        // On Music, "right" advances to the next track. Elsewhere it swipes.
        if (currentScreen == ScreenIndex::Music) {
          musicNextTrack();
          Serial.println("next");
        } else {
          swipeScreen(+1);
          Serial.println(screenName(currentScreen));
        }
        return;
      }
      if (cmd == "left"  || cmd == "<") {
        if (currentScreen == ScreenIndex::Music) {
          musicPrevTrack();
          Serial.println("prev");
        } else {
          swipeScreen(-1);
          Serial.println(screenName(currentScreen));
        }
        return;
      }
      if (cmd == "refresh" || cmd == "r") { fetchWeather(); Serial.println("refreshed"); return; }
      if (cmd == "spotify" || cmd == "sp") {
        if (fetchSpotifyPlayback()) Serial.println("spotify refreshed");
        else Serial.println(spotify.lastError.c_str());
        return;
      }
      if (cmd.length() >= 2 && cmd[0] == 'd') {
        const String v = cmd.substring(1);
        bool ok = !v.isEmpty();
        for (size_t i = 0; i < v.length() && ok; ++i) ok = isdigit((unsigned char)v[i]) != 0;
        if (ok) {
          const int b = v.toInt();
          if (b >= 0 && b <= 255) { setBacklight((uint8_t)b); Serial.printf("backlight %u\n", currentBacklightBrightness); return; }
        }
        Serial.println("use d<0-255>"); return;
      }
      if (cmd == "help" || cmd == "?") {
        Serial.println("commands: clock|c, weather|w, port, music|m, toggle|t, up|^, down|v, left|<, right|>, refresh|r, spotify|sp, d<0-255>, show <name>, hide, help|?");
        return;
      }
      Serial.printf("unknown: %s\n", cmd.c_str());
      return;
    }
    if (serialCommandBuffer.length() < 64) serialCommandBuffer += ch;
    else { serialCommandBuffer = ""; Serial.println("cmd too long"); return; }
  }
}

void tickSpotify() {
  if (!spotifyConfigured()) return;
  const unsigned long now = millis();
  if (now - spotify.lastFetchMs < SPOTIFY_REFRESH_MS) {
    if (spotify.valid && currentScreen == ScreenIndex::Music) updateMusicProgress();
    return;
  }
  fetchSpotifyPlayback();
}

// ---------------------------------------------------------------------------
