// Music player screen UI + state
// ---------------------------------------------------------------------------
constexpr int kMusicSoundbarCount = 14;

struct MusicTrack {
  const char *title;
  const char *artist;
  const char *album;
  uint32_t albumTop;     // gradient top color
  uint32_t albumBottom;  // gradient bottom color
  uint32_t accent;       // track-specific accent (progress + controls glow)
  int durationSec;
};

constexpr MusicTrack kMusicTracks[] = {
    {"Resonance - Extended Mix", "HOME",        "Odyssey",     0xB85C3A, 0x6B3A7A, 0x7CD4F2, 272},
    {"Midnight City",            "M83",         "Hurry Up",    0x2B1B4E, 0xE94E77, 0xE94E77, 244},
    {"Nightcall",                "Kavinsky",    "OutRun",      0x1A1A2E, 0xFF2975, 0xFF2975, 258},
    {"Strobe",                   "Deadmau5",    "For Lack Of", 0x0F2027, 0x2C5364, 0x38EF7D, 634},
    {"Glow",                     "Oliver",      "Full Circle", 0xFFB86B, 0xFF6B9D, 0xFFB86B, 218},
};
constexpr int kMusicTrackCount = sizeof(kMusicTracks) / sizeof(kMusicTracks[0]);

struct MusicUi {
  lv_obj_t *root = nullptr;
  lv_obj_t *backgroundCanvas = nullptr;
  StatusRow status;
  lv_obj_t *album = nullptr;
  lv_obj_t *albumCanvas = nullptr;
  lv_obj_t *albumGlowA = nullptr;   // inner radial highlight
  lv_obj_t *albumGlowB = nullptr;   // outer corner tint
  lv_obj_t *albumCounter = nullptr; // "TRACK 02 • OF 04" overlay label
  lv_obj_t *title = nullptr;
  lv_obj_t *artistAlbum = nullptr;  // "HOME · Odyssey"
  lv_obj_t *progressFill = nullptr;
  lv_obj_t *progressKnob = nullptr;
  lv_obj_t *progressKnobHalo = nullptr;
  lv_obj_t *timeElapsed = nullptr;
  lv_obj_t *timeTotal = nullptr;
  lv_obj_t *shuffleBtn = nullptr;
  lv_obj_t *prevBtn = nullptr;
  lv_obj_t *playBtn = nullptr;
  lv_obj_t *playGlow = nullptr;     // halo ring behind the play button
  lv_obj_t *playIcon = nullptr;
  lv_obj_t *nextBtn = nullptr;
  lv_obj_t *repeatBtn = nullptr;
  lv_obj_t *soundbars[kMusicSoundbarCount] = {nullptr};
  int progressRailWidth = 0;
  int soundbarBaseY = 0;
  int soundbarHeight = 0;
} musicUi;
MusicUi musicPortraitUi;
lv_color_t *musicBackgroundBuffer = nullptr;
lv_color_t *musicPortraitBackgroundBuffer = nullptr;
lv_color_t *musicCoverBuffer = nullptr;

int musicTrackIndex = 0;
bool musicIsPlaying = true;
int musicElapsedSec = 42;         // pre-seeded so the progress bar isn't empty on boot
unsigned long musicLastTickMs = 0;
unsigned long musicLastBarAnimMs = 0;
uint8_t musicSoundbarPhase[kMusicSoundbarCount] = {0};

struct SpotifyState {
  bool valid = false;
  bool isPlaying = false;
  String title;
  String artist;
  String album;
  String imageUrl;
  String coverLoadedUrl;
  String accessToken;
  String lastError;
  int lastHttpCode = 0;
  int durationSec = 1;
  int progressSec = 0;
  unsigned long tokenExpiresAtMs = 0;
  unsigned long lastFetchMs = 0;
  unsigned long lastProgressSyncMs = 0;
};

SpotifyState spotify;
TaskHandle_t spotifyPollTaskHandle = nullptr;
volatile bool spotifyPollInFlight = false;
volatile bool spotifyUiDirty = false;
lv_color_t *spotifyCoverBuffer = nullptr;
lv_img_dsc_t spotifyCoverDsc;
constexpr int kSpotifyCoverSize = 300;

// Weather poll task hand-off. Worker writes weatherData on core 0, sets
// weatherUiDirty, and the main loop calls applyWeatherUiUpdate() on its
// next tick so LVGL is only ever touched from one thread.
TaskHandle_t weatherPollTaskHandle = nullptr;
volatile bool weatherPollInFlight = false;
volatile bool weatherUiDirty = false;

struct WeatherUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *locLabel = nullptr;
  lv_obj_t *coordLabel = nullptr;
  lv_obj_t *bigTemp = nullptr;
  lv_obj_t *condLabel = nullptr;
  lv_obj_t *feelsLabel = nullptr;
  lv_obj_t *condIconCanvas = nullptr;
  lv_obj_t *hiVal = nullptr;
  lv_obj_t *loVal = nullptr;
  lv_obj_t *windVal = nullptr;
  lv_obj_t *rainVal = nullptr;

  lv_obj_t *humVal = nullptr;
  lv_obj_t *humBarFill = nullptr;
  lv_obj_t *pressVal = nullptr;
  lv_obj_t *pressSub = nullptr;
  lv_obj_t *windTileVal = nullptr;
  lv_obj_t *windTileSub = nullptr;
  lv_obj_t *windArrow = nullptr;
  lv_obj_t *aqiVal = nullptr;
  lv_obj_t *aqiSub = nullptr;
  lv_obj_t *aqiArc = nullptr;

  lv_obj_t *sunArc = nullptr;         // lv_arc for the daylight curve
  lv_obj_t *sunDot = nullptr;         // dot on the arc
  lv_obj_t *sunriseText = nullptr;
  lv_obj_t *sunsetText = nullptr;
  lv_obj_t *daylightText = nullptr;
  lv_obj_t *updated = nullptr;
} weatherUi;
lv_color_t *condIconBuf = nullptr;

constexpr int kBatteryDeviceCount = 4;

struct BatteryDeviceState {
  const char *name;
  const char *icon;
  int percent;
};

BatteryDeviceState batteryDevices[kBatteryDeviceCount] = {
    {"DEV 1", LV_SYMBOL_HOME, 92},
    {"DEV 2", LV_SYMBOL_SETTINGS, 53},
    {"DEV 3", LV_SYMBOL_POWER, 15},
    {"DEV 4", LV_SYMBOL_CHARGE, 50},
};

struct BatteryGaugeUi {
  lv_obj_t *arc = nullptr;
  lv_obj_t *value = nullptr;
  lv_obj_t *icon = nullptr;
  lv_obj_t *name = nullptr;
};

struct BatteryUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *updated = nullptr;
  BatteryGaugeUi gauges[kBatteryDeviceCount];
} batteryUi;
BatteryUi batteryPortraitUi;

// ---------------------------------------------------------------------------
// Nearby BLE devices (placeholder data — no real scan yet)
// ---------------------------------------------------------------------------
constexpr int kNearbyDeviceCount = 10;
constexpr int kNearbyMaxServices = 3;

struct NearbyDevice {
  const char *name;
  const char *mac;
  int rssi;                 // dBm
  const char *vendor;       // decoded from company ID
  uint16_t companyId;       // BLE assigned numbers — 0 if unknown
  const char *services[kNearbyMaxServices];
  int serviceCount;
  int txPower;              // dBm; INT_MIN if not advertised
  uint32_t accent;          // tint colour for the avatar + chips
  const char *icon;         // LV_SYMBOL_*
};

const NearbyDevice kNearbyDevices[kNearbyDeviceCount] = {
    {"AirPods Pro",     "A4:C1:38:2B:5F:E2", -42, "Apple",     0x004C, {"Audio", "Battery", nullptr},   2,     -16, 0x7CD4F2, LV_SYMBOL_AUDIO},
    {"Pixel Buds A",    "D8:71:33:8C:91:0B", -58, "Google",    0x00E0, {"Fast Pair", "Audio", nullptr}, 2,     -12, 0x38EF7D, LV_SYMBOL_BLUETOOTH},
    {"Galaxy Watch 5",  "7C:2E:DD:42:1A:F0", -67, "Samsung",   0x0075, {"HID", "Heart Rate", "Battery"},3,      -8, 0xE94E77, LV_SYMBOL_LOOP},
    {"Mi Band 7",       "C8:0F:10:88:42:E1", -73, "Xiaomi",    0x0157, {"Battery", "HID", nullptr},     2,      -4, 0xFFB86B, LV_SYMBOL_BATTERY_2},
    {"Surface Earbuds", "38:78:62:55:DD:01", -81, "Microsoft", 0x0006, {"HID", "Audio", nullptr},       2,      -8, 0x5EB6D6, LV_SYMBOL_AUDIO},
    {"Tile Mate",       "6E:8C:1A:00:7B:5F", -85, "Tile",      0x00E4, {"Tile Service", nullptr, nullptr}, 1,  -20, 0xB85C3A, LV_SYMBOL_GPS},
    {"(no name)",       "5E:2A:11:F3:0C:9D", -88, "Unknown",   0x0000, {nullptr, nullptr, nullptr},     0, INT_MIN, 0x6E7585, LV_SYMBOL_BLUETOOTH},
    {"Nest Cam IQ",     "18:B4:30:99:12:74", -76, "Google",    0x00E0, {"Setup", "Fast Pair", nullptr}, 2,     -12, 0x38EF7D, LV_SYMBOL_EYE_OPEN},
    {"WH-1000XM5",      "A0:E0:AF:42:51:B8", -65, "Sony",      0x012D, {"Audio", "Battery", nullptr},   2,      -8, 0xFF2975, LV_SYMBOL_AUDIO},
    {"Soundcore Q30",   "84:5C:F3:1E:9A:22", -71, "Anker",     0x048D, {"A2DP", "Battery", nullptr},    2,      -4, 0x2F6B9C, LV_SYMBOL_AUDIO},
};

struct NearbyCardUi {
  lv_obj_t *card = nullptr;
  lv_obj_t *avatar = nullptr;
  lv_obj_t *avatarIcon = nullptr;
  lv_obj_t *name = nullptr;
  lv_obj_t *meta = nullptr;
  lv_obj_t *chipRow = nullptr;
  lv_obj_t *chips[kNearbyMaxServices] = {nullptr};
  lv_obj_t *distance = nullptr;       // "~1.4 m" derived from RSSI + TX
  lv_obj_t *rssiBars[4] = {nullptr};  // still a visual cue for signal strength
};

struct NearbyUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *headerCount = nullptr;
  lv_obj_t *list = nullptr;          // scrollable container holding the cards
  NearbyCardUi cards[kNearbyDeviceCount];
};
NearbyUi nearbyUi;
NearbyUi nearbyPortraitUi;

// ---------------------------------------------------------------------------
// World Cup screen state (football-data.org)
// ---------------------------------------------------------------------------
struct WorldCupTeamSlot {
  String name;        // "United States"
  String shortName;   // "USA"
  String tla;         // FIFA three-letter code, "USA"
  String isoCode;     // ISO Alpha-2 lower-case, "us" — empty if unknown
  int score = -1;     // -1 when match hasn't kicked off / no data
};

struct WorldCupMatch {
  bool valid = false;
  WorldCupTeamSlot home;
  WorldCupTeamSlot away;
  String status;       // SCHEDULED / TIMED / IN_PLAY / PAUSED / FINISHED / POSTPONED / SUSPENDED
  String stage;        // GROUP_STAGE / LAST_16 / QUARTER_FINALS / SEMI_FINALS / FINAL
  String group;        // "Group A" or empty
  int matchday = 0;
  long utcDate = 0;    // epoch (seconds, UTC)
  int minute = 0;      // live elapsed minute; 0 when not in play
  String venue;
  String referee;
  String competition;  // "FIFA World Cup"
  String lastError;
  int lastHttpCode = 0;
  unsigned long lastFetchMs = 0;
};

WorldCupMatch wcMatch;

struct WorldCupUi {
  lv_obj_t *root = nullptr;
  StatusRow status;
  lv_obj_t *headerLabel = nullptr;       // "FIFA WORLD CUP 2026"
  lv_obj_t *stageLabel = nullptr;        // "GROUP A · MATCHDAY 1"
  lv_obj_t *heroPanel = nullptr;
  lv_obj_t *flagHome = nullptr;          // lv_img widget for home flag
  lv_obj_t *flagAway = nullptr;          // lv_img widget for away flag
  lv_obj_t *flagHomeFallback = nullptr;  // text placeholder when flag missing
  lv_obj_t *flagAwayFallback = nullptr;
  lv_obj_t *teamHomeShort = nullptr;     // "USA"
  lv_obj_t *teamHomeFull = nullptr;      // "United States"
  lv_obj_t *teamAwayShort = nullptr;
  lv_obj_t *teamAwayFull = nullptr;
  lv_obj_t *scoreHome = nullptr;
  lv_obj_t *scoreSep = nullptr;          // "-" or "vs"
  lv_obj_t *scoreAway = nullptr;
  lv_obj_t *statusPill = nullptr;
  lv_obj_t *statusPillLabel = nullptr;
  lv_obj_t *kickoffValue = nullptr;       // "SAT 8:00 PM" beneath status pill
  lv_obj_t *venueValue = nullptr;         // venue beneath kickoff
  lv_obj_t *updated = nullptr;
};
WorldCupUi wcUi;
WorldCupUi wcPortraitUi;

// Flag PNG buffers (shared between landscape + portrait views, since both
// render the same two flags from the same match data).
struct WorldCupFlagBuffer {
  uint8_t *data = nullptr;
  size_t size = 0;
  lv_img_dsc_t dsc;
  String loadedIso;
};
WorldCupFlagBuffer wcFlagHome;
WorldCupFlagBuffer wcFlagAway;

// Poll task + pending hand-off from worker to main thread.
TaskHandle_t wcPollTaskHandle = nullptr;
volatile bool wcPollInFlight = false;
volatile bool wcUiDirty = false;
WorldCupMatch wcPendingMatch;
uint8_t *wcPendingFlagHomeData = nullptr;
size_t wcPendingFlagHomeSize = 0;
String wcPendingFlagHomeIso;
uint8_t *wcPendingFlagAwayData = nullptr;
size_t wcPendingFlagAwaySize = 0;
String wcPendingFlagAwayIso;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
ScreenIndex currentScreen = ScreenIndex::Clock;
unsigned long lastWeatherAttempt = 0;
unsigned long lastClockTick = 0;
unsigned long lastAutoRotate = 0;
long currentLocalEpochUtc = 0;
uint8_t currentBacklightBrightness = BACKLIGHT_BRIGHTNESS;
String serialCommandBuffer;
bool colonVisible = true;
unsigned long lastColonBlink = 0;
bool ntpConfigured = false;
bool wifiConnectInProgress = false;
long lastRenderedClockMinute = LONG_MIN;
int lastRenderedLinkState = -1;
int lastRenderedWifiStatus = -1;

// ---------------------------------------------------------------------------
