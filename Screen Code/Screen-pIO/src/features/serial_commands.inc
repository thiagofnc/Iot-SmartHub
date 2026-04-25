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
      if (cmd == "battery" || cmd == "b") { showScreen(ScreenIndex::Battery); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "battery-port" || cmd == "bport" || cmd == "bp") { showScreen(ScreenIndex::BatteryPortrait); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "music" || cmd == "m") { showScreen(ScreenIndex::Music); Serial.println(screenName(currentScreen)); return; }
      if (cmd == "rotate") {
        if (currentScreen == ScreenIndex::Clock) showScreen(ScreenIndex::Portrait);
        else if (currentScreen == ScreenIndex::Portrait) showScreen(ScreenIndex::Clock);
        else if (currentScreen == ScreenIndex::Battery) showScreen(ScreenIndex::BatteryPortrait);
        else if (currentScreen == ScreenIndex::BatteryPortrait) showScreen(ScreenIndex::Battery);
        else if (currentScreen == ScreenIndex::Music) showScreen(ScreenIndex::MusicPortrait);
        else if (currentScreen == ScreenIndex::MusicPortrait) showScreen(ScreenIndex::Music);
        Serial.println(screenName(currentScreen));
        return;
      }
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
        if (isMusicScreen(currentScreen)) {
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
        if (isMusicScreen(currentScreen)) {
          musicNextTrack();
          Serial.println("next");
        } else {
          swipeScreen(+1);
          Serial.println(screenName(currentScreen));
        }
        return;
      }
      if (cmd == "left"  || cmd == "<") {
        if (isMusicScreen(currentScreen)) {
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
      if (cmd.startsWith("dev")) {
        int pos = 3;
        while (pos < (int)cmd.length() && isdigit((unsigned char)cmd[pos])) ++pos;
        if (pos == 3) { Serial.println("use: dev<number> <0-100>"); return; }
        const int deviceNumber = cmd.substring(3, pos).toInt();
        String level = cmd.substring(pos);
        level.trim();
        bool ok = !level.isEmpty();
        for (size_t i = 0; i < level.length() && ok; ++i) {
          ok = isdigit((unsigned char)level[i]) != 0;
        }
        if (ok) {
          const int percent = level.toInt();
          if (setDeviceBattery(deviceNumber, percent)) {
            Serial.printf("dev%d %d%%\n", deviceNumber, percent);
            return;
          }
        }
        Serial.printf("use: dev<1-%d> <0-100>\n", kBatteryDeviceCount);
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
        Serial.println("commands: clock|c, weather|w, port, battery|b, battery-port|bp, music|m, rotate, toggle|t, up|^, down|v, left|<, right|>, refresh|r, spotify|sp, dev<number> <0-100>, d<0-255>, show <name>, hide, help|?");
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
    if (spotify.valid && isMusicScreen(currentScreen)) updateMusicProgress();
    return;
  }
  fetchSpotifyPlayback();
}

// ---------------------------------------------------------------------------
