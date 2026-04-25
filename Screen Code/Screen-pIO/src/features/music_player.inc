// Music player screen
// ---------------------------------------------------------------------------
// Rotating neon palette for the animated soundbars.
constexpr uint32_t kSoundbarPalette[] = {
    0x7CD4F2,  // cyan
    0x38EF7D,  // mint
    0xFFE66D,  // yellow
    0xFF8C42,  // orange
    0xFF2975,  // magenta
    0xB06BFF,  // purple
    0x4CC9F0,  // sky
};
constexpr int kSoundbarPaletteCount = sizeof(kSoundbarPalette) / sizeof(kSoundbarPalette[0]);
constexpr const char *kMusicBackgroundImageName = "music_back";
enum class MusicCoverSource {
  SdCard,
  Spotify,
};
// Change this to MusicCoverSource::Spotify to go back to live Spotify art.
constexpr MusicCoverSource kMusicCoverSource = MusicCoverSource::SdCard;
// Change this one value to use a different SD-card cover image.
// The file must live in /images and use raw RGB565 .bin format.
constexpr const char *kMusicCoverImageName = "music_cover";
constexpr size_t kMusicCoverBytes = kSpotifyCoverSize * kSpotifyCoverSize * sizeof(lv_color_t);

const char *musicCoverSourceName() {
  return kMusicCoverSource == MusicCoverSource::SdCard ? "sd-card" : "spotify";
}

String musicFormatTime(int totalSec) {
  if (totalSec < 0) totalSec = 0;
  const int m = totalSec / 60;
  const int s = totalSec % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", m, s);
  return String(buf);
}

// Hard character cap for the song title so it never wraps to a second line.
// 12 visible chars + "..." when truncated, fits Montserrat 42 in the right column.
constexpr int kMusicTitleMaxChars = 16;

void musicWriteTruncated(char *out, size_t outSize, const char *src, int maxChars) {
  if (outSize == 0) return;
  int srcLen = (int)strlen(src);
  if (srcLen <= maxChars) {
    snprintf(out, outSize, "%s", src);
    return;
  }
  snprintf(out, outSize, "%.*s...", maxChars, src);
}

const MusicTrack &musicCurrentTrack() {
  return kMusicTracks[musicTrackIndex];
}

const char *musicCurrentTitle() {
  return spotify.valid && !spotify.title.isEmpty()
             ? spotify.title.c_str()
             : musicCurrentTrack().title;
}

const char *musicCurrentArtist() {
  return spotify.valid && !spotify.artist.isEmpty()
             ? spotify.artist.c_str()
             : musicCurrentTrack().artist;
}

const char *musicCurrentAlbum() {
  return spotify.valid && !spotify.album.isEmpty()
             ? spotify.album.c_str()
             : musicCurrentTrack().album;
}

int musicCurrentDurationSec() {
  return spotify.valid ? max(spotify.durationSec, 1) : musicCurrentTrack().durationSec;
}

int musicCurrentElapsedSec() {
  if (!spotify.valid) return musicElapsedSec;
  int elapsed = spotify.progressSec;
  if (spotify.isPlaying && spotify.lastProgressSyncMs > 0) {
    elapsed += (int)((millis() - spotify.lastProgressSyncMs) / 1000UL);
  }
  return min(max(elapsed, 0), musicCurrentDurationSec());
}

bool musicCurrentlyPlaying() {
  return spotify.valid ? spotify.isPlaying : musicIsPlaying;
}

bool spotifyCommand(const char *path, const char *method = "POST");
bool fetchSpotifyPlayback();

// Build one of the round secondary controls (shuffle/prev/next/repeat) so the
// trio of small buttons share one styling path and stay visually consistent.
lv_obj_t *makeMusicSecondaryBtn(lv_obj_t *parent, int size, const char *symbol,
                                const lv_font_t *font, uint32_t iconColor) {
  lv_obj_t *b = makePlain(parent);
  lv_obj_set_size(b, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_0, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_t *lbl = makeLabel(b, symbol, font, iconColor);
  lv_obj_center(lbl);
  return b;
}

bool isMusicScreen(ScreenIndex s) {
  return s == ScreenIndex::Music || s == ScreenIndex::MusicPortrait;
}

MusicUi &activeMusicUi() {
  return currentScreen == ScreenIndex::MusicPortrait ? musicPortraitUi : musicUi;
}

uint32_t musicSoundbarColor(int index) {
  const uint32_t leftEnd  = 0x35F3FF;  // electric cyan
  const uint32_t midPoint = 0x58F28A;  // vivid mint
  const uint32_t rightEnd = 0xB86BFF;  // bright violet
  const int t = (kMusicSoundbarCount == 1) ? 0
                  : index * 1000 / (kMusicSoundbarCount - 1);  // 0..1000
  const uint32_t from = (t <= 500) ? leftEnd : midPoint;
  const uint32_t to = (t <= 500) ? midPoint : rightEnd;
  const int localT = (t <= 500) ? t * 2 : (t - 500) * 2;
  const uint8_t fr = (from >> 16) & 0xFF;
  const uint8_t fg = (from >>  8) & 0xFF;
  const uint8_t fb =  from        & 0xFF;
  const uint8_t tr = (to >> 16) & 0xFF;
  const uint8_t tg = (to >>  8) & 0xFF;
  const uint8_t tb =  to        & 0xFF;
  const uint8_t cr = (uint8_t)(fr + (tr - (int)fr) * localT / 1000);
  const uint8_t cg = (uint8_t)(fg + (tg - (int)fg) * localT / 1000);
  const uint8_t cb = (uint8_t)(fb + (tb - (int)fb) * localT / 1000);
  return ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
}

bool loadMusicBackgroundFromSd() {
  Serial.printf("Music background: loading /images/%s.bin\n", kMusicBackgroundImageName);
  if (!sdReady) {
    Serial.println("Music background skipped: SD not available");
    return false;
  }

  String path = "/images/";
  path += kMusicBackgroundImageName;
  path += ".bin";

  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    Serial.printf("Music background open failed: %s\n", path.c_str());
    return false;
  }
  if ((size_t)f.size() != SD_IMAGE_BYTES) {
    Serial.printf("Music background size mismatch: %s is %u bytes, expected %u\n",
                  path.c_str(), (unsigned)f.size(), (unsigned)SD_IMAGE_BYTES);
    Serial.printf("Music background must be raw RGB565 at %ux%u pixels (%u bytes)\n",
                  SCREEN_WIDTH, SCREEN_HEIGHT, (unsigned)SD_IMAGE_BYTES);
    f.close();
    return false;
  }

  if (!musicBackgroundBuffer) {
    musicBackgroundBuffer = static_cast<lv_color_t *>(
        heap_caps_malloc(SD_IMAGE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!musicBackgroundBuffer) {
      Serial.println("Music background buffer alloc failed");
      f.close();
      return false;
    }
  }

  uint8_t *dst = reinterpret_cast<uint8_t *>(musicBackgroundBuffer);
  size_t remaining = SD_IMAGE_BYTES;
  while (remaining > 0) {
    const size_t chunk = remaining > 4096 ? 4096 : remaining;
    const int read = f.read(dst, chunk);
    if (read <= 0) {
      Serial.println("Music background read failed");
      f.close();
      return false;
    }
    for (int i = 0; i + 1 < read; i += 2) {
      const uint8_t t = dst[i];
      dst[i] = dst[i + 1];
      dst[i + 1] = t;
    }
    dst += read;
    remaining -= read;
  }
  f.close();
  Serial.printf("Music background loaded: %s\n", kMusicBackgroundImageName);
  return true;
}

bool readMusicCoverPixels(File &f, lv_color_t *buffer) {
  uint8_t *dst = reinterpret_cast<uint8_t *>(buffer);
  size_t remaining = kMusicCoverBytes;
  while (remaining > 0) {
    const size_t chunk = remaining > 4096 ? 4096 : remaining;
    const int read = f.read(dst, chunk);
    if (read <= 0) return false;
    for (int i = 0; i + 1 < read; i += 2) {
      const uint8_t t = dst[i];
      dst[i] = dst[i + 1];
      dst[i + 1] = t;
    }
    dst += read;
    remaining -= read;
  }
  return true;
}

bool readMusicCoverFromFullscreenImage(File &f, lv_color_t *buffer) {
  const int srcX = (SCREEN_WIDTH - kSpotifyCoverSize) / 2;
  const int srcY = (SCREEN_HEIGHT - kSpotifyCoverSize) / 2;
  for (int y = 0; y < kSpotifyCoverSize; ++y) {
    const uint32_t offset = ((srcY + y) * SCREEN_WIDTH + srcX) * sizeof(lv_color_t);
    if (!f.seek(offset)) return false;
    uint8_t *row = reinterpret_cast<uint8_t *>(buffer + y * kSpotifyCoverSize);
    const int expected = kSpotifyCoverSize * sizeof(lv_color_t);
    const int read = f.read(row, expected);
    if (read != expected) return false;
    for (int i = 0; i + 1 < read; i += 2) {
      const uint8_t t = row[i];
      row[i] = row[i + 1];
      row[i + 1] = t;
    }
  }
  return true;
}

bool loadMusicCoverFromSd() {
  if (kMusicCoverSource != MusicCoverSource::SdCard) {
    Serial.printf("Music cover: source is %s, SD cover load skipped\n",
                  musicCoverSourceName());
    return false;
  }
  if (musicCoverBuffer) {
    Serial.printf("Music cover already loaded: /images/%s.bin\n", kMusicCoverImageName);
    return true;
  }

  Serial.printf("Music cover: loading /images/%s.bin\n", kMusicCoverImageName);
  if (!sdReady) {
    Serial.println("Music cover skipped: SD not available");
    return false;
  }

  String path = "/images/";
  path += kMusicCoverImageName;
  path += ".bin";

  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    Serial.printf("Music cover open failed: %s\n", path.c_str());
    return false;
  }

  const size_t fileSize = (size_t)f.size();
  const bool squareCover = fileSize == kMusicCoverBytes;
  const bool fullscreenImage = fileSize == SD_IMAGE_BYTES;
  if (!squareCover && !fullscreenImage) {
    Serial.printf("Music cover size mismatch: %s is %u bytes, expected %u or %u\n",
                  path.c_str(), (unsigned)fileSize, (unsigned)kMusicCoverBytes,
                  (unsigned)SD_IMAGE_BYTES);
    Serial.printf("Music cover must be raw RGB565 at %ux%u or %ux%u pixels\n",
                  kSpotifyCoverSize, kSpotifyCoverSize, SCREEN_WIDTH, SCREEN_HEIGHT);
    f.close();
    return false;
  }

  musicCoverBuffer = static_cast<lv_color_t *>(
      heap_caps_malloc(kMusicCoverBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!musicCoverBuffer) {
    musicCoverBuffer = static_cast<lv_color_t *>(
        heap_caps_malloc(kMusicCoverBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!musicCoverBuffer) {
    Serial.println("Music cover buffer alloc failed");
    f.close();
    return false;
  }

  const bool ok = squareCover ? readMusicCoverPixels(f, musicCoverBuffer)
                              : readMusicCoverFromFullscreenImage(f, musicCoverBuffer);
  f.close();
  if (!ok) {
    Serial.println("Music cover read failed");
    heap_caps_free(musicCoverBuffer);
    musicCoverBuffer = nullptr;
    return false;
  }

  Serial.printf("Music cover loaded: %s\n", kMusicCoverImageName);
  return true;
}

void applyMusicCoverToUi(MusicUi &ui) {
  if (!ui.albumCanvas || !musicCoverBuffer) return;
  lv_canvas_set_buffer(ui.albumCanvas, musicCoverBuffer,
                       kSpotifyCoverSize, kSpotifyCoverSize, LV_IMG_CF_TRUE_COLOR);
  lv_obj_clear_flag(ui.albumCanvas, LV_OBJ_FLAG_HIDDEN);
}

bool ensureMusicCover() {
  Serial.printf("Music cover: source=%s\n", musicCoverSourceName());
  if (kMusicCoverSource != MusicCoverSource::SdCard) return false;
  if (!loadMusicCoverFromSd()) {
    Serial.println("Music cover: inactive");
    return false;
  }
  applyMusicCoverToUi(musicUi);
  applyMusicCoverToUi(musicPortraitUi);
  Serial.println("Music cover: active");
  return true;
}

bool prepareMusicPortraitBackground() {
  if (musicPortraitBackgroundBuffer) return true;
  if (!musicBackgroundBuffer) return false;

  musicPortraitBackgroundBuffer = static_cast<lv_color_t *>(
      heap_caps_malloc(SD_IMAGE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!musicPortraitBackgroundBuffer) {
    Serial.println("Music portrait background buffer alloc failed");
    return false;
  }

  for (int y = 0; y < PORTRAIT_HEIGHT; ++y) {
    for (int x = 0; x < PORTRAIT_WIDTH; ++x) {
      const int srcX = y;
      const int srcY = SCREEN_HEIGHT - 1 - x;
      musicPortraitBackgroundBuffer[y * PORTRAIT_WIDTH + x] =
          musicBackgroundBuffer[srcY * SCREEN_WIDTH + srcX];
    }
  }
  return true;
}

void attachMusicBackground(lv_obj_t *root, MusicUi &ui, lv_color_t *buffer,
                           int width, int height) {
  if (ui.backgroundCanvas) {
    lv_obj_clear_flag(ui.backgroundCanvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(ui.backgroundCanvas);
    return;
  }

  ui.backgroundCanvas = lv_canvas_create(root);
  lv_obj_remove_style_all(ui.backgroundCanvas);
  lv_obj_set_size(ui.backgroundCanvas, width, height);
  lv_obj_set_pos(ui.backgroundCanvas, 0, 0);
  lv_canvas_set_buffer(ui.backgroundCanvas, buffer, width, height,
                       LV_IMG_CF_TRUE_COLOR);
  lv_obj_move_background(ui.backgroundCanvas);
}

bool ensureMusicBackground() {
  if (!musicUi.root) {
    Serial.println("Music background skipped: music UI not built");
    return false;
  }

  if (!musicBackgroundBuffer && !loadMusicBackgroundFromSd()) {
    return false;
  }

  attachMusicBackground(musicUi.root, musicUi, musicBackgroundBuffer,
                        SCREEN_WIDTH, SCREEN_HEIGHT);
  if (musicPortraitUi.root && prepareMusicPortraitBackground()) {
    attachMusicBackground(musicPortraitUi.root, musicPortraitUi,
                          musicPortraitBackgroundBuffer,
                          PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
  }
  Serial.println("Music background: active");
  return true;
}

void buildMusicScreen(lv_obj_t *parent) {
  musicUi.root = makePlain(parent);
  lv_obj_set_size(musicUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(musicUi.root, 0, 0);
  lv_obj_add_flag(musicUi.root, LV_OBJ_FLAG_HIDDEN);

  // Backdrop: solid deep navy so the album art, controls, and soundbars carry
  // the visual energy without a page-level gradient.
  lv_obj_set_style_bg_color(musicUi.root, hex(0x060A12), 0);
  lv_obj_set_style_bg_grad_dir(musicUi.root, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(musicUi.root, LV_OPA_COVER, 0);
  // Status strip — left: accent dot + "NOW PLAYING · SPOTIFY"; right: room + battery.
  buildStatusRow(musicUi.root, musicUi.status, "NOW PLAYING \xC2\xB7 SPOTIFY",
                 "LIVING ROOM \xC2\xB7 68%");

  // ---------------- Layout constants ----------------
  const int padX = 32;
  const int contentTop = 48;
  const int contentBottom = SCREEN_HEIGHT - 20;
  const int albumSize = 300;
  const int albumX = padX;
  const int albumY = contentTop + 20;  // leaves air under the status strip

  const int rightX = albumX + albumSize + 36;
  const int rightW = SCREEN_WIDTH - rightX - padX;

  // ---------------- Album art ----------------
  // Stacked layers: base deep-purple square, a teal radial "sun" pushed to the
  // upper-left, and a subtle dark corner vignette on the lower-right. Together
  // they fake the oklch radial gradient from the mockup on an LVGL build that
  // has no real radial gradient.
  musicUi.album = makePlain(musicUi.root);
  lv_obj_set_size(musicUi.album, albumSize, albumSize);
  lv_obj_set_pos(musicUi.album, albumX, albumY);
  lv_obj_set_style_radius(musicUi.album, 20, 0);
  lv_obj_set_style_bg_color(musicUi.album, hex(0x3B2656), 0);      // mid purple
  lv_obj_set_style_bg_grad_color(musicUi.album, hex(0x1B1430), 0); // deep purple
  lv_obj_set_style_bg_grad_dir(musicUi.album, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(musicUi.album, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(musicUi.album, true, 0);

  // Teal radial "sun" — offset circle larger than the tile, clipped by the
  // album's rounded corners so it becomes a soft radial bloom in the UL area.
  musicUi.albumGlowA = makePlain(musicUi.album);
  const int glowA = 360;
  lv_obj_set_size(musicUi.albumGlowA, glowA, glowA);
  lv_obj_set_pos(musicUi.albumGlowA, -120, -140);
  lv_obj_set_style_radius(musicUi.albumGlowA, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.albumGlowA, hex(0x2AB6C9), 0);
  lv_obj_set_style_bg_opa(musicUi.albumGlowA, LV_OPA_70, 0);

  // Inner smaller bright core to fake the radial intensity falloff.
  lv_obj_t *core = makePlain(musicUi.album);
  lv_obj_set_size(core, 200, 200);
  lv_obj_set_pos(core, -40, -60);
  lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(core, hex(0x5BDCE9), 0);
  lv_obj_set_style_bg_opa(core, LV_OPA_40, 0);

  // Purple wash on the right half to blend into the deep-purple base.
  musicUi.albumGlowB = makePlain(musicUi.album);
  lv_obj_set_size(musicUi.albumGlowB, 280, 280);
  lv_obj_set_pos(musicUi.albumGlowB, 120, 120);
  lv_obj_set_style_radius(musicUi.albumGlowB, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.albumGlowB, hex(0x8B3DBB), 0);
  lv_obj_set_style_bg_opa(musicUi.albumGlowB, LV_OPA_40, 0);

  musicUi.albumCanvas = lv_canvas_create(musicUi.album);
  lv_obj_remove_style_all(musicUi.albumCanvas);
  lv_obj_set_size(musicUi.albumCanvas, albumSize, albumSize);
  lv_obj_set_pos(musicUi.albumCanvas, 0, 0);
  lv_obj_add_flag(musicUi.albumCanvas, LV_OBJ_FLAG_HIDDEN);

  // Track counter in the bottom-left corner.
  musicUi.albumCounter = makeLabel(musicUi.album, "TRACK 02 \xC2\xB7 OF 04",
                                   &lv_font_montserrat_12, COL_INK_2);
  lv_obj_set_style_text_letter_space(musicUi.albumCounter, 2, 0);
  lv_obj_align(musicUi.albumCounter, LV_ALIGN_BOTTOM_LEFT, 18, -18);

  // ---------------- Eyebrow + title + artist ----------------
  // Small cyan accent bar + "NOW PLAYING" caption, just above the title.
  const int eyebrowY = albumY + 14;
  lv_obj_t *eyebrowBar = makePlain(musicUi.root);
  lv_obj_set_size(eyebrowBar, 20, 2);
  lv_obj_set_pos(eyebrowBar, rightX, eyebrowY + 7);
  lv_obj_set_style_bg_color(eyebrowBar, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(eyebrowBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(eyebrowBar, 1, 0);

  lv_obj_t *eyebrow = makeLabel(musicUi.root, "NOW PLAYING",
                                &lv_font_montserrat_12, COL_ACCENT);
  lv_obj_set_style_text_letter_space(eyebrow, 2, 0);
  lv_obj_set_pos(eyebrow, rightX + 28, eyebrowY);

  // Title — big and bold. Hard-capped at 12 chars + "..." so it never wraps
  // and the artist line below always sits at a fixed offset.
  char titleBuf[32];
  musicWriteTruncated(titleBuf, sizeof(titleBuf), kMusicTracks[0].title,
                      kMusicTitleMaxChars);
  musicUi.title = makeLabel(musicUi.root, titleBuf,
                            &lv_font_montserrat_42, COL_INK);
  lv_obj_set_style_text_letter_space(musicUi.title, -1, 0);
  lv_obj_set_pos(musicUi.title, rightX, eyebrowY + 24);

  // Secondary line — "ARTIST | ALBUM". Uses a plain pipe separator because
  // the fonts baked into this build don't render the middle-dot glyph.
  char artistAlbumBuf[96];
  snprintf(artistAlbumBuf, sizeof(artistAlbumBuf), "%s | %s",
           kMusicTracks[0].artist, kMusicTracks[0].album);
  musicUi.artistAlbum = makeLabel(musicUi.root, artistAlbumBuf,
                                  &lv_font_montserrat_18, COL_INK_2);
  lv_obj_set_width(musicUi.artistAlbum, rightW);
  lv_label_set_long_mode(musicUi.artistAlbum, LV_LABEL_LONG_DOT);
  // Sits just under the single-line title.
  lv_obj_set_pos(musicUi.artistAlbum, rightX, eyebrowY + 78);

  // ---------------- Soundbars (between title and progress) ----------------
  const int barsAreaX = rightX;
  const int barsAreaW = rightW;
  const int barsTop = eyebrowY + 120;
  const int barsBottomY = barsTop + 110;
  const int barsH = barsBottomY - barsTop;
  const int barW = 18;
  const int barGap = (barsAreaW - barW * kMusicSoundbarCount) / (kMusicSoundbarCount - 1);

  musicUi.soundbarBaseY = barsBottomY;
  musicUi.soundbarHeight = barsH;

  for (int i = 0; i < kMusicSoundbarCount; ++i) {
    lv_obj_t *bar = makePlain(musicUi.root);
    const int h = 24 + (i * 13) % (barsH - 32);
    lv_obj_set_size(bar, barW, h);
    lv_obj_set_pos(bar, barsAreaX + i * (barW + barGap), barsBottomY - h);
    lv_obj_set_style_radius(bar, 4, 0);
    // Each bar is a single solid color stepped along the row — together the
    // bars form a left-to-right gradient across the whole soundbar strip
    // (rather than each bar carrying its own internal gradient).
    const uint32_t color = musicSoundbarColor(i);
    lv_obj_set_style_bg_color(bar, hex(color), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    // Slight halo so the bars bloom on the dark backdrop.
    lv_obj_set_style_shadow_width(bar, 12, 0);
    lv_obj_set_style_shadow_color(bar, hex(color), 0);
    lv_obj_set_style_shadow_opa(bar, LV_OPA_40, 0);
    musicUi.soundbars[i] = bar;
    musicSoundbarPhase[i] = (uint8_t)((i * 37) & 0xFF);
  }

  // ---------------- Progress rail ----------------
  const int railW = rightW;
  const int railH = 4;
  const int railY = barsBottomY + 20;
  musicUi.progressRailWidth = railW;
  lv_obj_t *rail = makePlain(musicUi.root);
  lv_obj_set_size(rail, railW, railH);
  lv_obj_set_pos(rail, rightX, railY);
  lv_obj_set_style_radius(rail, railH / 2, 0);
  lv_obj_set_style_bg_color(rail, hex(0x253146), 0);
  lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);

  musicUi.progressFill = makePlain(rail);
  lv_obj_set_size(musicUi.progressFill, railW / 4, railH);
  lv_obj_set_pos(musicUi.progressFill, 0, 0);
  lv_obj_set_style_radius(musicUi.progressFill, railH / 2, 0);
  lv_obj_set_style_bg_color(musicUi.progressFill, hex(0x4A6B8A), 0);
  lv_obj_set_style_bg_grad_color(musicUi.progressFill, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_grad_dir(musicUi.progressFill, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(musicUi.progressFill, LV_OPA_COVER, 0);

  // Ring-style knob: white inner with a translucent outer "halo" layer.
  musicUi.progressKnobHalo = makePlain(rail);
  lv_obj_set_size(musicUi.progressKnobHalo, 20, 20);
  lv_obj_set_style_radius(musicUi.progressKnobHalo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.progressKnobHalo, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(musicUi.progressKnobHalo, LV_OPA_30, 0);
  lv_obj_set_pos(musicUi.progressKnobHalo, railW / 4 - 10, (railH - 20) / 2);

  musicUi.progressKnob = makePlain(rail);
  lv_obj_set_size(musicUi.progressKnob, 12, 12);
  lv_obj_set_style_radius(musicUi.progressKnob, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.progressKnob, hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(musicUi.progressKnob, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(musicUi.progressKnob, 18, 0);
  lv_obj_set_style_shadow_color(musicUi.progressKnob, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(musicUi.progressKnob, LV_OPA_80, 0);
  lv_obj_set_pos(musicUi.progressKnob, railW / 4 - 6, (railH - 12) / 2);

  // Time labels under the rail
  musicUi.timeElapsed = makeLabel(musicUi.root, "0:00",
                                  &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(musicUi.timeElapsed, rightX, railY + 14);

  musicUi.timeTotal = makeLabel(musicUi.root, "0:00",
                                &lv_font_montserrat_14, COL_INK_3);
  lv_obj_set_style_text_align(musicUi.timeTotal, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(musicUi.timeTotal, 70);
  lv_obj_set_pos(musicUi.timeTotal, rightX + railW - 70, railY + 14);

  // ---------------- Controls row ----------------
  // shuffle, prev, [big cyan play], next, repeat — centered inside the right column.
  const int smallSize = 36;
  const int prevNextSize = 44;
  const int playSize = 66;
  const int gap = 18;

  const int ctrlY = contentBottom - playSize - 18;
  const int ctrlGroupW = smallSize + gap + prevNextSize + gap + playSize + gap + prevNextSize + gap + smallSize;
  const int ctrlX = rightX + (rightW - ctrlGroupW) / 2;
  const int midY = ctrlY + playSize / 2;

  int x = ctrlX;
  // Shuffle
  musicUi.shuffleBtn = makeMusicSecondaryBtn(musicUi.root, smallSize, LV_SYMBOL_SHUFFLE,
                                             &lv_font_montserrat_18, COL_INK_2);
  lv_obj_set_pos(musicUi.shuffleBtn, x, midY - smallSize / 2);
  x += smallSize + gap;

  // Prev
  musicUi.prevBtn = makeMusicSecondaryBtn(musicUi.root, prevNextSize, LV_SYMBOL_PREV,
                                          &lv_font_montserrat_24, COL_INK);
  lv_obj_set_pos(musicUi.prevBtn, x, midY - prevNextSize / 2);
  x += prevNextSize + gap;

  // Play/pause halo — sits behind the button as a larger soft-cyan disk.
  musicUi.playGlow = makePlain(musicUi.root);
  const int glowSize = playSize + 26;
  lv_obj_set_size(musicUi.playGlow, glowSize, glowSize);
  lv_obj_set_pos(musicUi.playGlow, x - (glowSize - playSize) / 2,
                 midY - glowSize / 2);
  lv_obj_set_style_radius(musicUi.playGlow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.playGlow, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(musicUi.playGlow, LV_OPA_20, 0);

  musicUi.playBtn = makePlain(musicUi.root);
  lv_obj_set_size(musicUi.playBtn, playSize, playSize);
  lv_obj_set_pos(musicUi.playBtn, x, midY - playSize / 2);
  lv_obj_set_style_radius(musicUi.playBtn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicUi.playBtn, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_grad_color(musicUi.playBtn, hex(0x5BDCE9), 0);
  lv_obj_set_style_bg_grad_dir(musicUi.playBtn, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(musicUi.playBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(musicUi.playBtn, 40, 0);
  lv_obj_set_style_shadow_color(musicUi.playBtn, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(musicUi.playBtn, LV_OPA_70, 0);
  musicUi.playIcon = makeLabel(musicUi.playBtn, LV_SYMBOL_PAUSE,
                               &lv_font_montserrat_28, 0x062028);
  lv_obj_center(musicUi.playIcon);
  x += playSize + gap;

  // Next
  musicUi.nextBtn = makeMusicSecondaryBtn(musicUi.root, prevNextSize, LV_SYMBOL_NEXT,
                                          &lv_font_montserrat_24, COL_INK);
  lv_obj_set_pos(musicUi.nextBtn, x, midY - prevNextSize / 2);
  x += prevNextSize + gap;

  // Repeat
  musicUi.repeatBtn = makeMusicSecondaryBtn(musicUi.root, smallSize, LV_SYMBOL_LOOP,
                                            &lv_font_montserrat_18, COL_INK_2);
  lv_obj_set_pos(musicUi.repeatBtn, x, midY - smallSize / 2);

  ensureMusicCover();
}

void buildMusicPortraitScreen(lv_obj_t *parent) {
  musicPortraitUi.root = makePlain(parent);
  lv_obj_set_size(musicPortraitUi.root, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
  lv_obj_set_pos(musicPortraitUi.root, 0, 0);
  lv_obj_add_flag(musicPortraitUi.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(musicPortraitUi.root, hex(0x060A12), 0);
  lv_obj_set_style_bg_grad_dir(musicPortraitUi.root, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.root, LV_OPA_COVER, 0);

  buildStatusRow(musicPortraitUi.root, musicPortraitUi.status,
                 "NOW PLAYING \xC2\xB7 SPOTIFY", "LIVING ROOM \xC2\xB7 68%");

  const int screenW = PORTRAIT_WIDTH;
  const int albumSize = 360;
  const int albumX = (screenW - albumSize) / 2;
  const int albumY = 52;

  musicPortraitUi.album = makePlain(musicPortraitUi.root);
  lv_obj_set_size(musicPortraitUi.album, albumSize, albumSize);
  lv_obj_set_pos(musicPortraitUi.album, albumX, albumY);
  lv_obj_set_style_radius(musicPortraitUi.album, 22, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.album, hex(0x3B2656), 0);
  lv_obj_set_style_bg_grad_color(musicPortraitUi.album, hex(0x1B1430), 0);
  lv_obj_set_style_bg_grad_dir(musicPortraitUi.album, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.album, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(musicPortraitUi.album, true, 0);

  musicPortraitUi.albumGlowA = makePlain(musicPortraitUi.album);
  lv_obj_set_size(musicPortraitUi.albumGlowA, 420, 420);
  lv_obj_set_pos(musicPortraitUi.albumGlowA, -120, -150);
  lv_obj_set_style_radius(musicPortraitUi.albumGlowA, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.albumGlowA, hex(0x2AB6C9), 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.albumGlowA, LV_OPA_70, 0);

  lv_obj_t *warmCore = makePlain(musicPortraitUi.album);
  lv_obj_set_size(warmCore, 220, 220);
  lv_obj_set_pos(warmCore, 16, 28);
  lv_obj_set_style_radius(warmCore, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(warmCore, hex(0xFF6B5E), 0);
  lv_obj_set_style_bg_opa(warmCore, LV_OPA_30, 0);

  musicPortraitUi.albumGlowB = makePlain(musicPortraitUi.album);
  lv_obj_set_size(musicPortraitUi.albumGlowB, 340, 340);
  lv_obj_set_pos(musicPortraitUi.albumGlowB, 120, 150);
  lv_obj_set_style_radius(musicPortraitUi.albumGlowB, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.albumGlowB, hex(0xB86BFF), 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.albumGlowB, LV_OPA_40, 0);

  musicPortraitUi.albumCanvas = lv_canvas_create(musicPortraitUi.album);
  lv_obj_remove_style_all(musicPortraitUi.albumCanvas);
  lv_obj_set_size(musicPortraitUi.albumCanvas, kSpotifyCoverSize, kSpotifyCoverSize);
  lv_obj_set_pos(musicPortraitUi.albumCanvas, (albumSize - kSpotifyCoverSize) / 2,
                 (albumSize - kSpotifyCoverSize) / 2);
  lv_obj_add_flag(musicPortraitUi.albumCanvas, LV_OBJ_FLAG_HIDDEN);

  musicPortraitUi.albumCounter = makeLabel(musicPortraitUi.album,
                                           "TRACK 02 \xC2\xB7 OF 04",
                                           &lv_font_montserrat_12, COL_INK_2);
  lv_obj_set_style_text_letter_space(musicPortraitUi.albumCounter, 2, 0);
  lv_obj_align(musicPortraitUi.albumCounter, LV_ALIGN_BOTTOM_LEFT, 18, -18);

  lv_obj_t *eyebrow = makeLabel(musicPortraitUi.root, "NOW PLAYING",
                                &lv_font_montserrat_12, COL_ACCENT);
  lv_obj_set_style_text_letter_space(eyebrow, 2, 0);
  lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 448);

  char titleBuf[32];
  musicWriteTruncated(titleBuf, sizeof(titleBuf), kMusicTracks[0].title,
                      kMusicTitleMaxChars);
  musicPortraitUi.title = makeLabel(musicPortraitUi.root, titleBuf,
                                    &lv_font_montserrat_28, COL_INK);
  lv_obj_set_width(musicPortraitUi.title, 420);
  lv_obj_set_style_text_align(musicPortraitUi.title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(musicPortraitUi.title, LV_LABEL_LONG_DOT);
  lv_obj_align(musicPortraitUi.title, LV_ALIGN_TOP_MID, 0, 480);

  char artistAlbumBuf[96];
  snprintf(artistAlbumBuf, sizeof(artistAlbumBuf), "%s | %s",
           kMusicTracks[0].artist, kMusicTracks[0].album);
  musicPortraitUi.artistAlbum = makeLabel(musicPortraitUi.root, artistAlbumBuf,
                                          &lv_font_montserrat_16, COL_INK_2);
  lv_obj_set_width(musicPortraitUi.artistAlbum, 420);
  lv_obj_set_style_text_align(musicPortraitUi.artistAlbum, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(musicPortraitUi.artistAlbum, LV_LABEL_LONG_DOT);
  lv_obj_align(musicPortraitUi.artistAlbum, LV_ALIGN_TOP_MID, 0, 522);

  const int barsAreaX = 26;
  const int barsAreaW = screenW - 52;
  const int barsTop = 584;
  const int barsBottomY = 656;
  const int barsH = barsBottomY - barsTop;
  const int barW = 18;
  const int barGap = (barsAreaW - barW * kMusicSoundbarCount) / (kMusicSoundbarCount - 1);
  musicPortraitUi.soundbarBaseY = barsBottomY;
  musicPortraitUi.soundbarHeight = barsH;

  for (int i = 0; i < kMusicSoundbarCount; ++i) {
    lv_obj_t *bar = makePlain(musicPortraitUi.root);
    const int h = 18 + (i * 11) % (barsH - 20);
    lv_obj_set_size(bar, barW, h);
    lv_obj_set_pos(bar, barsAreaX + i * (barW + barGap), barsBottomY - h);
    lv_obj_set_style_radius(bar, 4, 0);
    const uint32_t color = musicSoundbarColor(i);
    lv_obj_set_style_bg_color(bar, hex(color), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(bar, 12, 0);
    lv_obj_set_style_shadow_color(bar, hex(color), 0);
    lv_obj_set_style_shadow_opa(bar, LV_OPA_40, 0);
    musicPortraitUi.soundbars[i] = bar;
  }

  const int railW = screenW - 48;
  const int railH = 4;
  const int railY = 684;
  musicPortraitUi.progressRailWidth = railW;
  lv_obj_t *rail = makePlain(musicPortraitUi.root);
  lv_obj_set_size(rail, railW, railH);
  lv_obj_set_pos(rail, 24, railY);
  lv_obj_set_style_radius(rail, railH / 2, 0);
  lv_obj_set_style_bg_color(rail, hex(0x253146), 0);
  lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);

  musicPortraitUi.progressFill = makePlain(rail);
  lv_obj_set_size(musicPortraitUi.progressFill, railW / 4, railH);
  lv_obj_set_pos(musicPortraitUi.progressFill, 0, 0);
  lv_obj_set_style_radius(musicPortraitUi.progressFill, railH / 2, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.progressFill, hex(0x4A6B8A), 0);
  lv_obj_set_style_bg_grad_color(musicPortraitUi.progressFill, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_grad_dir(musicPortraitUi.progressFill, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.progressFill, LV_OPA_COVER, 0);

  musicPortraitUi.progressKnobHalo = makePlain(rail);
  lv_obj_set_size(musicPortraitUi.progressKnobHalo, 20, 20);
  lv_obj_set_style_radius(musicPortraitUi.progressKnobHalo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.progressKnobHalo, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.progressKnobHalo, LV_OPA_30, 0);
  lv_obj_set_pos(musicPortraitUi.progressKnobHalo, railW / 4 - 10, (railH - 20) / 2);

  musicPortraitUi.progressKnob = makePlain(rail);
  lv_obj_set_size(musicPortraitUi.progressKnob, 12, 12);
  lv_obj_set_style_radius(musicPortraitUi.progressKnob, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.progressKnob, hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.progressKnob, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(musicPortraitUi.progressKnob, 18, 0);
  lv_obj_set_style_shadow_color(musicPortraitUi.progressKnob, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(musicPortraitUi.progressKnob, LV_OPA_80, 0);
  lv_obj_set_pos(musicPortraitUi.progressKnob, railW / 4 - 6, (railH - 12) / 2);

  musicPortraitUi.timeElapsed = makeLabel(musicPortraitUi.root, "0:00",
                                          &lv_font_montserrat_12, COL_INK_2);
  lv_obj_set_pos(musicPortraitUi.timeElapsed, 24, railY + 14);

  musicPortraitUi.timeTotal = makeLabel(musicPortraitUi.root, "0:00",
                                        &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_align(musicPortraitUi.timeTotal, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(musicPortraitUi.timeTotal, 70);
  lv_obj_set_pos(musicPortraitUi.timeTotal, screenW - 94, railY + 14);

  const int smallSize = 38;
  const int prevNextSize = 58;
  const int playSize = 72;
  const int ctrlY = 710;
  musicPortraitUi.shuffleBtn = makeMusicSecondaryBtn(musicPortraitUi.root, smallSize,
                                                     LV_SYMBOL_SHUFFLE,
                                                     &lv_font_montserrat_18, COL_INK_2);
  lv_obj_set_pos(musicPortraitUi.shuffleBtn, 48, ctrlY + (playSize - smallSize) / 2);

  musicPortraitUi.prevBtn = makeMusicSecondaryBtn(musicPortraitUi.root, prevNextSize,
                                                  LV_SYMBOL_PREV,
                                                  &lv_font_montserrat_24, COL_INK);
  lv_obj_set_pos(musicPortraitUi.prevBtn, 130, ctrlY + (playSize - prevNextSize) / 2);

  musicPortraitUi.playGlow = makePlain(musicPortraitUi.root);
  const int glowSize = playSize + 26;
  lv_obj_set_size(musicPortraitUi.playGlow, glowSize, glowSize);
  lv_obj_set_pos(musicPortraitUi.playGlow, (screenW - playSize) / 2 - (glowSize - playSize) / 2,
                 ctrlY - (glowSize - playSize) / 2);
  lv_obj_set_style_radius(musicPortraitUi.playGlow, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.playGlow, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.playGlow, LV_OPA_20, 0);

  musicPortraitUi.playBtn = makePlain(musicPortraitUi.root);
  lv_obj_set_size(musicPortraitUi.playBtn, playSize, playSize);
  lv_obj_set_pos(musicPortraitUi.playBtn, (screenW - playSize) / 2, ctrlY);
  lv_obj_set_style_radius(musicPortraitUi.playBtn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(musicPortraitUi.playBtn, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_grad_color(musicPortraitUi.playBtn, hex(0x5BDCE9), 0);
  lv_obj_set_style_bg_grad_dir(musicPortraitUi.playBtn, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(musicPortraitUi.playBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(musicPortraitUi.playBtn, 40, 0);
  lv_obj_set_style_shadow_color(musicPortraitUi.playBtn, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(musicPortraitUi.playBtn, LV_OPA_70, 0);
  musicPortraitUi.playIcon = makeLabel(musicPortraitUi.playBtn, LV_SYMBOL_PAUSE,
                                       &lv_font_montserrat_28, 0x062028);
  lv_obj_center(musicPortraitUi.playIcon);

  musicPortraitUi.nextBtn = makeMusicSecondaryBtn(musicPortraitUi.root, prevNextSize,
                                                  LV_SYMBOL_NEXT,
                                                  &lv_font_montserrat_24, COL_INK);
  lv_obj_set_pos(musicPortraitUi.nextBtn, 292, ctrlY + (playSize - prevNextSize) / 2);

  musicPortraitUi.repeatBtn = makeMusicSecondaryBtn(musicPortraitUi.root, smallSize,
                                                    LV_SYMBOL_LOOP,
                                                    &lv_font_montserrat_18, COL_INK_2);
  lv_obj_set_pos(musicPortraitUi.repeatBtn, 394, ctrlY + (playSize - smallSize) / 2);

  ensureMusicCover();
}

void updateMusicProgressUi(MusicUi &ui) {
  if (!ui.progressFill || !ui.progressKnob) return;
  const int duration = musicCurrentDurationSec();
  const int elapsed = musicCurrentElapsedSec();

  const int rail = ui.progressRailWidth;
  const int fillW = (int)((long)elapsed * rail / duration);
  lv_obj_set_width(ui.progressFill, fillW < 2 ? 2 : fillW);
  lv_obj_set_x(ui.progressKnob, fillW - 6);
  if (ui.progressKnobHalo) lv_obj_set_x(ui.progressKnobHalo, fillW - 10);

  if (ui.timeElapsed) lv_label_set_text(ui.timeElapsed, musicFormatTime(elapsed).c_str());
  if (ui.timeTotal)   lv_label_set_text(ui.timeTotal,   musicFormatTime(duration).c_str());
}

void updateMusicProgress() {
  updateMusicProgressUi(musicUi);
  updateMusicProgressUi(musicPortraitUi);
}

void updateMusicSoundbarVisibilityUi(MusicUi &ui) {
  const bool playing = musicCurrentlyPlaying();
  for (int i = 0; i < kMusicSoundbarCount; ++i) {
    if (!ui.soundbars[i]) continue;
    if (playing) lv_obj_clear_flag(ui.soundbars[i], LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(ui.soundbars[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void updateMusicSoundbarVisibility() {
  updateMusicSoundbarVisibilityUi(musicUi);
  updateMusicSoundbarVisibilityUi(musicPortraitUi);
}

void updateMusicTrackOne(MusicUi &ui) {
  if (ui.title) {
    char buf[32];
    musicWriteTruncated(buf, sizeof(buf), musicCurrentTitle(), kMusicTitleMaxChars);
    lv_label_set_text(ui.title, buf);
  }
  if (ui.artistAlbum) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s | %s", musicCurrentArtist(), musicCurrentAlbum());
    lv_label_set_text(ui.artistAlbum, buf);
  }
  if (ui.albumCounter) {
    if (spotify.valid) {
      lv_label_set_text(ui.albumCounter, "LIVE FROM SPOTIFY");
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "TRACK %02d \xC2\xB7 OF %02d",
               musicTrackIndex + 1, kMusicTrackCount);
      lv_label_set_text(ui.albumCounter, buf);
    }
  }
}

void updateMusicTrackUi() {
  updateMusicTrackOne(musicUi);
  updateMusicTrackOne(musicPortraitUi);
  // The album art stays the same teal→purple radial backdrop across tracks
  // (matching the reference), so we no longer recolor the tile per-track.
  updateMusicProgress();
}

void updateMusicPlayIconOne(MusicUi &ui) {
  if (!ui.playIcon) return;
  lv_label_set_text(ui.playIcon, musicCurrentlyPlaying() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

void updateMusicPlayIcon() {
  updateMusicPlayIconOne(musicUi);
  updateMusicPlayIconOne(musicPortraitUi);
  updateMusicSoundbarVisibility();
}

void musicTogglePlay() {
  if (spotify.valid) {
    const bool wasPlaying = spotify.isPlaying;
    if (!spotifyCommand(wasPlaying ? "/me/player/pause" : "/me/player/play", "PUT")) {
      Serial.println(spotify.lastError.c_str());
      return;
    }
  }
  if (spotify.valid) {
    spotify.isPlaying = !spotify.isPlaying;
    spotify.progressSec = musicCurrentElapsedSec();
    spotify.lastProgressSyncMs = millis();
  }
  musicIsPlaying = !musicIsPlaying;
  musicLastTickMs = millis();
  updateMusicPlayIcon();
}

void musicNextTrack() {
  if (spotify.valid && spotifyCommand("/me/player/next")) {
    spotify.valid = false;
    fetchSpotifyPlayback();
    return;
  }
  spotify.valid = false;
  musicTrackIndex = (musicTrackIndex + 1) % kMusicTrackCount;
  musicElapsedSec = 0;
  musicLastTickMs = millis();
  updateMusicTrackUi();
}

void musicPrevTrack() {
  if (spotify.valid && spotifyCommand("/me/player/previous")) {
    spotify.valid = false;
    fetchSpotifyPlayback();
    return;
  }
  spotify.valid = false;
  // If we're past the first few seconds, a "prev" first restarts the current
  // track (matches typical music-app behavior) before stepping back.
  if (musicElapsedSec > 3) {
    musicElapsedSec = 0;
  } else {
    musicTrackIndex = (musicTrackIndex - 1 + kMusicTrackCount) % kMusicTrackCount;
    musicElapsedSec = 0;
    updateMusicTrackUi();
  }
  musicLastTickMs = millis();
  updateMusicProgress();
}

// ---------------------------------------------------------------------------
