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
