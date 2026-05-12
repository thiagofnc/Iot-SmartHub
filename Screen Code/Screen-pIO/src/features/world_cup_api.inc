// World Cup / football-data.org integration
// ---------------------------------------------------------------------------
// Polls https://api.football-data.org/v4/competitions/WC/matches every 60 s
// from a one-shot FreeRTOS task, picks the most "interesting" match (live >
// next scheduled > most recently finished), downloads both team flags as PNG
// from https://flagcdn.com, and hands the result over to the main thread via
// the wcUiDirty flag (same pattern as the Spotify poller).
// ---------------------------------------------------------------------------

struct FifaIsoEntry {
  const char *fifa;   // 3-letter FIFA / football-data TLA, upper case
  const char *iso;    // 2-letter ISO 3166-1 alpha-2, lower case (flagcdn)
};

// Mapping covers WC 2026 participants + likely qualifiers. flagcdn uses ISO
// alpha-2 only — UK home nations all collapse to "gb" (St George's / Saltire
// aren't on flagcdn). Anything not on this list falls back to a coloured TLA
// placeholder in the UI.
constexpr FifaIsoEntry kFifaIsoMap[] = {
    // Hosts
    {"USA", "us"}, {"MEX", "mx"}, {"CAN", "ca"},
    // CONMEBOL
    {"ARG", "ar"}, {"BRA", "br"}, {"URU", "uy"}, {"COL", "co"},
    {"ECU", "ec"}, {"PAR", "py"}, {"VEN", "ve"}, {"CHI", "cl"},
    {"PER", "pe"}, {"BOL", "bo"},
    // UEFA
    {"FRA", "fr"}, {"ESP", "es"}, {"GER", "de"}, {"ENG", "gb"},
    {"POR", "pt"}, {"NED", "nl"}, {"BEL", "be"}, {"ITA", "it"},
    {"CRO", "hr"}, {"DEN", "dk"}, {"SUI", "ch"}, {"POL", "pl"},
    {"AUT", "at"}, {"TUR", "tr"}, {"CZE", "cz"}, {"NOR", "no"},
    {"SWE", "se"}, {"GRE", "gr"}, {"SRB", "rs"}, {"UKR", "ua"},
    {"SCO", "gb"}, {"WAL", "gb"}, {"IRL", "ie"}, {"ROU", "ro"},
    {"FIN", "fi"}, {"ISL", "is"}, {"HUN", "hu"}, {"SVN", "si"},
    {"SVK", "sk"}, {"NIR", "gb"}, {"ALB", "al"}, {"BIH", "ba"},
    {"BUL", "bg"}, {"BLR", "by"}, {"MNE", "me"}, {"MKD", "mk"},
    {"KOS", "xk"}, {"LUX", "lu"}, {"MLT", "mt"}, {"LVA", "lv"},
    {"LTU", "lt"}, {"EST", "ee"}, {"MDA", "md"}, {"GEO", "ge"},
    {"ARM", "am"}, {"AZE", "az"},
    // AFC
    {"JPN", "jp"}, {"KOR", "kr"}, {"AUS", "au"}, {"IRN", "ir"},
    {"KSA", "sa"}, {"QAT", "qa"}, {"UAE", "ae"}, {"IRQ", "iq"},
    {"UZB", "uz"}, {"JOR", "jo"}, {"CHN", "cn"}, {"IND", "in"},
    // CAF
    {"MAR", "ma"}, {"SEN", "sn"}, {"TUN", "tn"}, {"EGY", "eg"},
    {"ALG", "dz"}, {"NGA", "ng"}, {"GHA", "gh"}, {"CMR", "cm"},
    {"CIV", "ci"}, {"RSA", "za"}, {"MLI", "ml"},
    // CONCACAF
    {"PAN", "pa"}, {"CRC", "cr"}, {"HON", "hn"}, {"JAM", "jm"},
    {"CUB", "cu"}, {"HAI", "ht"}, {"GUA", "gt"}, {"SLV", "sv"},
    {"TRI", "tt"},
    // OFC
    {"NZL", "nz"}, {"FJI", "fj"},
};
constexpr int kFifaIsoMapCount = sizeof(kFifaIsoMap) / sizeof(kFifaIsoMap[0]);

String fifaToIso(const String &tla) {
  for (int i = 0; i < kFifaIsoMapCount; ++i) {
    if (tla.equalsIgnoreCase(kFifaIsoMap[i].fifa)) return String(kFifaIsoMap[i].iso);
  }
  return "";
}

bool footballDataConfigured() { return strlen(FOOTBALLDATA_API_KEY) > 0; }

// Rank a match status for "what's most interesting to show".
// Larger number = preferred.
int matchStatusPriority(const String &status) {
  if (status == "IN_PLAY") return 100;
  if (status == "PAUSED") return 95;
  if (status == "LIVE") return 100;
  if (status == "TIMED") return 60;
  if (status == "SCHEDULED") return 55;
  if (status == "POSTPONED") return 30;
  if (status == "SUSPENDED") return 25;
  if (status == "FINISHED") return 10;
  return 0;
}

// Convert a "YYYY-MM-DDTHH:MM:SSZ" string to a UTC epoch. We can't use
// timegm() (GNU extension, missing on ESP-IDF's newlib) so this folds the
// calendar math by hand. Returns 0 on parse failure.
long parseUtcDate(const char *iso) {
  if (!iso || !*iso) return 0;
  int year, mon, day, hh, mm, ss;
  if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d",
             &year, &mon, &day, &hh, &mm, &ss) != 6) return 0;
  if (year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
  auto isLeap = [](int y) {
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
  };
  static const int daysPerMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  long days = 0;
  for (int y = 1970; y < year; ++y) days += isLeap(y) ? 366 : 365;
  for (int m = 1; m < mon; ++m) {
    days += daysPerMonth[m - 1];
    if (m == 2 && isLeap(year)) ++days;
  }
  days += day - 1;
  return days * 86400L + hh * 3600L + mm * 60L + ss;
}

// Download a PNG into a freshly-allocated PSRAM buffer. Caller owns the
// returned buffer (free with heap_caps_free) on success.
bool downloadFlagPng(const String &isoCode, uint8_t **outData, size_t *outSize) {
  *outData = nullptr;
  *outSize = 0;
  if (isoCode.isEmpty()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  const String url = "https://flagcdn.com/w160/" + isoCode + ".png";
  if (!http.begin(client, url)) {
    Serial.printf("[wc] flag http.begin failed for %s\n", isoCode.c_str());
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  const int contentLength = http.getSize();
  if (contentLength <= 0 || contentLength > 64 * 1024) {
    http.end();
    return false;
  }

  uint8_t *buf = static_cast<uint8_t *>(
      heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buf) {
    buf = static_cast<uint8_t *>(
        heap_caps_malloc(contentLength, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!buf) { http.end(); return false; }

  WiFiClient *stream = http.getStreamPtr();
  int readTotal = 0;
  while (http.connected() && readTotal < contentLength) {
    const int available = stream->available();
    if (available > 0) {
      const int toRead = min(available, contentLength - readTotal);
      const int readNow = stream->readBytes(buf + readTotal, toRead);
      if (readNow <= 0) break;
      readTotal += readNow;
    } else {
      delay(1);
    }
  }
  http.end();
  if (readTotal != contentLength) {
    heap_caps_free(buf);
    return false;
  }

  *outData = buf;
  *outSize = static_cast<size_t>(contentLength);
  return true;
}

// Fetch the football-data.org WC match list and pick the most interesting
// match. Writes the result into wcPendingMatch + pending flag buffers and
// sets wcUiDirty when done.
bool fetchFootballData() {
  if (!footballDataConfigured()) {
    wcMatch.lastError = "MISSING FOOTBALLDATA KEY";
    Serial.println("[wc] missing API key");
    wcUiDirty = true;
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) connectWifiIfNeeded();
  if (WiFi.status() != WL_CONNECTED) {
    wcMatch.lastError = "WIFI OFFLINE";
    Serial.println("[wc] wifi offline");
    wcUiDirty = true;
    return false;
  }
  wcMatch.lastFetchMs = millis();
  Serial.printf("[wc] fetch starting (free heap %u, largest internal %u)\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  // Narrow the response to the next ~45 days so we don't pull the full 104-
  // match tournament schedule (~80 KB JSON) every minute.
  String url = "https://api.football-data.org/v4/competitions/WC/matches";
  const time_t nowT = time(nullptr);
  if (nowT > MIN_VALID_EPOCH_UTC) {
    char dateFrom[16], dateTo[16];
    struct tm tm_from, tm_to;
    gmtime_r(&nowT, &tm_from);
    const time_t laterT = nowT + 45L * 86400L;
    gmtime_r(&laterT, &tm_to);
    strftime(dateFrom, sizeof(dateFrom), "%Y-%m-%d", &tm_from);
    strftime(dateTo, sizeof(dateTo), "%Y-%m-%d", &tm_to);
    url += "?dateFrom=";
    url += dateFrom;
    url += "&dateTo=";
    url += dateTo;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    wcMatch.lastError = "FOOTBALL HTTP";
    Serial.println("[wc] http.begin failed");
    wcUiDirty = true;
    return false;
  }
  http.addHeader("X-Auth-Token", FOOTBALLDATA_API_KEY);
  const int code = http.GET();
  wcMatch.lastHttpCode = code;
  Serial.printf("[wc] HTTP %d\n", code);
  if (code != HTTP_CODE_OK) {
    http.end();
    wcMatch.lastError = String("HTTP ") + String(code);
    wcUiDirty = true;
    return false;
  }
  String payload = http.getString();
  http.end();
  Serial.printf("[wc] payload %u bytes\n", (unsigned)payload.length());

  // ArduinoJson v7 - dynamic doc; payload can be large (50+ matches).
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    wcMatch.lastError = String("JSON ") + err.c_str();
    Serial.printf("[wc] JSON error: %s\n", err.c_str());
    wcUiDirty = true;
    return false;
  }

  JsonArray matches = doc["matches"].as<JsonArray>();
  if (matches.size() == 0) {
    wcMatch.lastError = "NO WC MATCHES";
    Serial.println("[wc] no matches");
    wcUiDirty = true;
    return false;
  }
  Serial.printf("[wc] %u matches in response\n", (unsigned)matches.size());

  // Pick best match: highest status priority; among ties, soonest future kickoff
  // (for live = lowest minute, for scheduled = nearest start).
  const long nowEpoch = static_cast<long>(time(nullptr));
  JsonObject best;
  int bestPriority = -1;
  long bestTimeKey = LONG_MAX;
  for (JsonObject m : matches) {
    const String status = String(m["status"] | "");
    const int prio = matchStatusPriority(status);
    if (prio < bestPriority) continue;
    const long utc = parseUtcDate(m["utcDate"] | "");
    long key;
    if (prio >= 95) {
      key = 0;                                            // any live match wins outright
    } else if (prio >= 55) {
      key = (utc >= nowEpoch) ? (utc - nowEpoch) : LONG_MAX;  // upcoming: soonest first
    } else if (status == "FINISHED") {
      key = (utc <= nowEpoch) ? (nowEpoch - utc) : LONG_MAX;  // finished: most recent
    } else {
      key = LONG_MAX;
    }
    if (prio > bestPriority || key < bestTimeKey) {
      bestPriority = prio;
      bestTimeKey = key;
      best = m;
    }
  }

  if (best.isNull()) {
    wcMatch.lastError = "NO MATCH PICKED";
    Serial.println("[wc] no match picked");
    wcUiDirty = true;
    return false;
  }

  // Stamp into the pending match object (worker thread only).
  wcPendingMatch = WorldCupMatch{};
  wcPendingMatch.valid = true;
  wcPendingMatch.home.name      = String(best["homeTeam"]["name"] | "TBD");
  wcPendingMatch.home.shortName = String(best["homeTeam"]["shortName"] | wcPendingMatch.home.name);
  wcPendingMatch.home.tla       = String(best["homeTeam"]["tla"] | "");
  wcPendingMatch.home.isoCode   = fifaToIso(wcPendingMatch.home.tla);
  wcPendingMatch.away.name      = String(best["awayTeam"]["name"] | "TBD");
  wcPendingMatch.away.shortName = String(best["awayTeam"]["shortName"] | wcPendingMatch.away.name);
  wcPendingMatch.away.tla       = String(best["awayTeam"]["tla"] | "");
  wcPendingMatch.away.isoCode   = fifaToIso(wcPendingMatch.away.tla);

  JsonVariant scoreHome = best["score"]["fullTime"]["home"];
  JsonVariant scoreAway = best["score"]["fullTime"]["away"];
  wcPendingMatch.home.score = scoreHome.isNull() ? -1 : (int)scoreHome.as<int>();
  wcPendingMatch.away.score = scoreAway.isNull() ? -1 : (int)scoreAway.as<int>();

  wcPendingMatch.status      = String(best["status"] | "SCHEDULED");
  wcPendingMatch.stage       = String(best["stage"] | "");
  wcPendingMatch.group       = String(best["group"] | "");
  wcPendingMatch.matchday    = best["matchday"] | 0;
  wcPendingMatch.utcDate     = parseUtcDate(best["utcDate"] | "");
  wcPendingMatch.minute      = best["minute"] | 0;
  wcPendingMatch.venue       = String(best["venue"] | "");
  wcPendingMatch.competition = String(doc["competition"]["name"] | "FIFA World Cup");

  JsonArray refs = best["referees"].as<JsonArray>();
  if (refs.size() > 0) {
    wcPendingMatch.referee = String(refs[0]["name"] | "");
  }

  wcPendingMatch.lastError = "";
  wcPendingMatch.lastHttpCode = code;
  wcPendingMatch.lastFetchMs = millis();
  Serial.printf("[wc] picked %s vs %s (%s)\n",
                wcPendingMatch.home.tla.c_str(), wcPendingMatch.away.tla.c_str(),
                wcPendingMatch.status.c_str());

  // Free any prior pending flag buffers, then download fresh ones if the team
  // ISO code changed since last apply.
  auto freePending = [](uint8_t *&p, size_t &n) {
    if (p) heap_caps_free(p);
    p = nullptr; n = 0;
  };
  freePending(wcPendingFlagHomeData, wcPendingFlagHomeSize);
  freePending(wcPendingFlagAwayData, wcPendingFlagAwaySize);

  if (!wcPendingMatch.home.isoCode.isEmpty() &&
      wcPendingMatch.home.isoCode != wcFlagHome.loadedIso) {
    downloadFlagPng(wcPendingMatch.home.isoCode, &wcPendingFlagHomeData, &wcPendingFlagHomeSize);
    wcPendingFlagHomeIso = wcPendingMatch.home.isoCode;
  } else {
    wcPendingFlagHomeIso = wcFlagHome.loadedIso;  // keep what's already loaded
  }
  if (!wcPendingMatch.away.isoCode.isEmpty() &&
      wcPendingMatch.away.isoCode != wcFlagAway.loadedIso) {
    downloadFlagPng(wcPendingMatch.away.isoCode, &wcPendingFlagAwayData, &wcPendingFlagAwaySize);
    wcPendingFlagAwayIso = wcPendingMatch.away.isoCode;
  } else {
    wcPendingFlagAwayIso = wcFlagAway.loadedIso;
  }

  wcUiDirty = true;
  return true;
}

void wcPollTask(void *param) {
  (void)param;
  fetchFootballData();
  wcPollInFlight = false;
  wcPollTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool startWcPollTask() {
  if (wcPollInFlight || !footballDataConfigured()) return false;
  wcPollInFlight = true;
  const BaseType_t ok = xTaskCreatePinnedToCore(
      wcPollTask, "wc_poll", 16384, nullptr, 1, &wcPollTaskHandle, 0);
  if (ok != pdPASS) {
    wcPollInFlight = false;
    wcPollTaskHandle = nullptr;
    wcMatch.lastError = "WC TASK FAILED";
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
