// Spotify Web API
// ---------------------------------------------------------------------------
bool spotifyConfigured() {
  return strlen(SPOTIFY_CLIENT_ID) > 0 &&
         strlen(SPOTIFY_CLIENT_SECRET) > 0 &&
         strlen(SPOTIFY_REFRESH_TOKEN) > 0;
}

String spotifyDeviceQuery() {
  if (strlen(SPOTIFY_DEVICE_ID) == 0) return "";
  return String("?device_id=") + urlEncode(SPOTIFY_DEVICE_ID);
}

bool spotifyRefreshAccessToken() {
  if (!spotifyConfigured()) {
    spotify.lastError = "MISSING SPOTIFY TOKEN";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, "https://accounts.spotify.com/api/token")) {
    spotify.lastError = "SPOTIFY AUTH HTTP";
    return false;
  }

  const String credentials = String(SPOTIFY_CLIENT_ID) + ":" + SPOTIFY_CLIENT_SECRET;
  http.addHeader("Authorization", "Basic " + base64Encode(credentials));
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "grant_type=refresh_token&refresh_token=";
  body += urlEncode(SPOTIFY_REFRESH_TOKEN);

  const int code = http.POST(body);
  const String payload = http.getString();
  http.end();
  if (code != HTTP_CODE_OK) {
    spotify.lastError = String("SPOTIFY AUTH ") + String(code);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    spotify.lastError = "SPOTIFY AUTH JSON";
    return false;
  }
  spotify.accessToken = String(doc["access_token"] | "");
  const int expiresIn = doc["expires_in"] | 3600;
  spotify.tokenExpiresAtMs = millis() + (unsigned long)max(expiresIn - 60, 60) * 1000UL;
  spotify.lastError = "";
  return !spotify.accessToken.isEmpty();
}

bool spotifyEnsureToken() {
  if (!spotify.accessToken.isEmpty() && millis() < spotify.tokenExpiresAtMs) return true;
  return spotifyRefreshAccessToken();
}

bool spotifyRequest(const char *method, const String &path, const String &body, String *payloadOut) {
  if (!spotifyEnsureToken()) return false;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  const String url = String("https://api.spotify.com/v1") + path;
  if (!http.begin(client, url)) {
    spotify.lastError = "SPOTIFY HTTP CLIENT";
    return false;
  }
  http.addHeader("Authorization", "Bearer " + spotify.accessToken);
  if (body.length() > 0) {
    http.addHeader("Content-Type", "application/json");
  } else if (strcmp(method, "GET") != 0) {
    http.addHeader("Content-Length", "0");
  }

  int code;
  if (strcmp(method, "GET") == 0) code = http.GET();
  else if (body.length() > 0) {
    code = http.sendRequest(method,
                            reinterpret_cast<uint8_t *>(const_cast<char *>(body.c_str())),
                            body.length());
  } else {
    uint8_t emptyBody = 0;
    code = http.sendRequest(method, &emptyBody, 0);
  }

  String payload;
  if (code != HTTP_CODE_NO_CONTENT) payload = http.getString();
  http.end();

  if (code == HTTP_CODE_UNAUTHORIZED) {
    spotify.accessToken = "";
    if (!spotifyRefreshAccessToken()) return false;
    return spotifyRequest(method, path, body, payloadOut);
  }

  if (code < 200 || code >= 300) {
    spotify.lastError = String("SPOTIFY HTTP ") + String(code);
    return false;
  }
  if (payloadOut) *payloadOut = payload;
  spotify.lastError = "";
  return true;
}

bool spotifyJpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!spotifyCoverBuffer) return false;
  for (uint16_t row = 0; row < h; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kSpotifyCoverSize) continue;
    for (uint16_t col = 0; col < w; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kSpotifyCoverSize) continue;
      const uint16_t p = bitmap[row * w + col];
      const uint8_t r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
      const uint8_t g = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
      const uint8_t b = (uint8_t)((p & 0x1F) * 255 / 31);
      spotifyCoverBuffer[dstY * kSpotifyCoverSize + dstX] = lv_color_make(r, g, b);
    }
  }
  return true;
}

void spotifyShowFallbackCover() {
  if (kMusicCoverSource == MusicCoverSource::SdCard) {
    ensureMusicCover();
    return;
  }
  if (musicUi.albumCanvas) lv_obj_add_flag(musicUi.albumCanvas, LV_OBJ_FLAG_HIDDEN);
  if (musicPortraitUi.albumCanvas) lv_obj_add_flag(musicPortraitUi.albumCanvas, LV_OBJ_FLAG_HIDDEN);
}

bool spotifyLoadAlbumCover(const String &imageUrl) {
  if (kMusicCoverSource == MusicCoverSource::SdCard) {
    ensureMusicCover();
    return true;
  }
  if (imageUrl.isEmpty() || imageUrl == spotify.coverLoadedUrl) return true;
  if (WiFi.status() != WL_CONNECTED) return false;

  if (!spotifyCoverBuffer) {
    spotifyCoverBuffer = static_cast<lv_color_t *>(heap_caps_malloc(
        kSpotifyCoverSize * kSpotifyCoverSize * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!spotifyCoverBuffer) {
      spotifyCoverBuffer = static_cast<lv_color_t *>(heap_caps_malloc(
          kSpotifyCoverSize * kSpotifyCoverSize * sizeof(lv_color_t),
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
  }
  if (!spotifyCoverBuffer) {
    spotify.lastError = "COVER BUFFER FAILED";
    return false;
  }

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, imageUrl)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0 || contentLength > 220000) {
    http.end();
    return false;
  }
  uint8_t *jpg = static_cast<uint8_t *>(heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!jpg) jpg = static_cast<uint8_t *>(heap_caps_malloc(contentLength, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!jpg) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  int readTotal = 0;
  while (http.connected() && readTotal < contentLength) {
    const int available = stream->available();
    if (available > 0) {
      const int toRead = min(available, contentLength - readTotal);
      const int readNow = stream->readBytes(jpg + readTotal, toRead);
      if (readNow <= 0) break;
      readTotal += readNow;
    } else {
      delay(1);
      lv_timer_handler();
    }
  }
  http.end();
  if (readTotal != contentLength) {
    heap_caps_free(jpg);
    return false;
  }

  lv_color_t fill = hex(0x111827);
  for (int i = 0; i < kSpotifyCoverSize * kSpotifyCoverSize; ++i) spotifyCoverBuffer[i] = fill;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(spotifyJpgOutput);
  const bool decoded = TJpgDec.drawJpg(0, 0, jpg, contentLength) == JDR_OK;
  heap_caps_free(jpg);
  if (!decoded) return false;

  spotifyCoverDsc.header.always_zero = 0;
  spotifyCoverDsc.header.w = kSpotifyCoverSize;
  spotifyCoverDsc.header.h = kSpotifyCoverSize;
  spotifyCoverDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  spotifyCoverDsc.data_size = kSpotifyCoverSize * kSpotifyCoverSize * sizeof(lv_color_t);
  spotifyCoverDsc.data = reinterpret_cast<const uint8_t *>(spotifyCoverBuffer);
  applyMusicCoverBufferToUi(musicUi, spotifyCoverBuffer);
  applyMusicCoverBufferToUi(musicPortraitUi, spotifyCoverBuffer);
  spotify.coverLoadedUrl = imageUrl;
  return true;
}

bool fetchSpotifyPlayback() {
  if (!spotifyConfigured()) return false;
  if (WiFi.status() != WL_CONNECTED) connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) return false;

  String path = "/me/player/currently-playing";
  if (strlen(SPOTIFY_MARKET) > 0) {
    path += "?market=";
    path += urlEncode(SPOTIFY_MARKET);
  }

  String payload;
  if (!spotifyRequest("GET", path, "", &payload)) return false;
  spotify.lastFetchMs = millis();
  if (payload.isEmpty()) {
    spotify.valid = false;
    spotify.lastError = "NOT PLAYING";
    spotifyShowFallbackCover();
    updateMusicTrackUi();
    updateMusicPlayIcon();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    spotify.lastError = "SPOTIFY JSON";
    return false;
  }
  JsonVariant item = doc["item"];
  if (item.isNull() || strcmp(item["type"] | "track", "track") != 0) {
    spotify.valid = false;
    spotify.lastError = "NO TRACK";
    spotifyShowFallbackCover();
    return false;
  }

  spotify.title = String(item["name"] | "Unknown track");
  spotify.album = String(item["album"]["name"] | "Spotify");
  spotify.artist = "";
  JsonArray artists = item["artists"].as<JsonArray>();
  for (JsonVariant artist : artists) {
    if (!spotify.artist.isEmpty()) spotify.artist += ", ";
    spotify.artist += String(artist["name"] | "");
  }
  if (spotify.artist.isEmpty()) spotify.artist = "Unknown artist";
  spotify.durationSec = max((int)((item["duration_ms"] | 0) / 1000), 1);
  spotify.progressSec = max((int)((doc["progress_ms"] | 0) / 1000), 0);
  spotify.isPlaying = doc["is_playing"] | false;
  spotify.lastProgressSyncMs = millis();

  JsonArray images = item["album"]["images"].as<JsonArray>();
  spotify.imageUrl = "";
  for (JsonVariant image : images) {
    const int w = image["width"] | 0;
    const char *url = image["url"] | "";
    if (w <= kSpotifyCoverSize && strlen(url) > 0) {
      spotify.imageUrl = url;
      break;
    }
    if (spotify.imageUrl.isEmpty() && strlen(url) > 0) spotify.imageUrl = url;
  }

  spotify.valid = true;
  spotify.lastError = "";
  spotifyLoadAlbumCover(spotify.imageUrl);
  updateMusicTrackUi();
  updateMusicPlayIcon();
  if (clockUi.nowPlayingTitle) lv_label_set_text(clockUi.nowPlayingTitle, musicCurrentTitle());
  if (clockUi.nowPlayingArtist) lv_label_set_text(clockUi.nowPlayingArtist, musicCurrentArtist());
  if (portraitUi.nowPlayingTitle) lv_label_set_text(portraitUi.nowPlayingTitle, musicCurrentTitle());
  if (portraitUi.nowPlayingArtist) lv_label_set_text(portraitUi.nowPlayingArtist, musicCurrentArtist());
  return true;
}

bool spotifyCommand(const char *path, const char *method) {
  if (!spotifyConfigured()) return false;
  return spotifyRequest(method, String(path) + spotifyDeviceQuery(), "", nullptr);
}

// ---------------------------------------------------------------------------
