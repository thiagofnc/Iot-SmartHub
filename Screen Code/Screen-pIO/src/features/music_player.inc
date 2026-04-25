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

void buildMusicScreen(lv_obj_t *parent) {
  musicUi.root = makePlain(parent);
  lv_obj_set_size(musicUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(musicUi.root, 0, 0);
  lv_obj_add_flag(musicUi.root, LV_OBJ_FLAG_HIDDEN);

  // Backdrop — very dark teal/navy, slightly lighter toward the right to
  // match the reference. A single subtle horizontal gradient does the job;
  // no ambient blobs, which kept the reference looking clean.
  lv_obj_set_style_bg_color(musicUi.root, hex(0x060A12), 0);
  lv_obj_set_style_bg_grad_color(musicUi.root, hex(0x10172A), 0);
  lv_obj_set_style_bg_grad_dir(musicUi.root, LV_GRAD_DIR_HOR, 0);
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
    const uint32_t leftEnd  = 0x5BDCE9;  // cyan
    const uint32_t rightEnd = 0xB06BFF;  // purple
    const uint8_t lr = (leftEnd  >> 16) & 0xFF;
    const uint8_t lg = (leftEnd  >>  8) & 0xFF;
    const uint8_t lb =  leftEnd          & 0xFF;
    const uint8_t rr = (rightEnd >> 16) & 0xFF;
    const uint8_t rg = (rightEnd >>  8) & 0xFF;
    const uint8_t rb =  rightEnd         & 0xFF;
    const int t = (kMusicSoundbarCount == 1) ? 0
                    : i * 1000 / (kMusicSoundbarCount - 1);  // 0..1000
    const uint8_t cr = (uint8_t)(lr + (rr - (int)lr) * t / 1000);
    const uint8_t cg = (uint8_t)(lg + (rg - (int)lg) * t / 1000);
    const uint8_t cb = (uint8_t)(lb + (rb - (int)lb) * t / 1000);
    const uint32_t color = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
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

  const int ctrlY = contentBottom - playSize - 2;
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
}

void updateMusicProgress() {
  if (!musicUi.progressFill || !musicUi.progressKnob) return;
  const int duration = musicCurrentDurationSec();
  const int elapsed = musicCurrentElapsedSec();

  const int rail = musicUi.progressRailWidth;
  const int fillW = (int)((long)elapsed * rail / duration);
  lv_obj_set_width(musicUi.progressFill, fillW < 2 ? 2 : fillW);
  lv_obj_set_x(musicUi.progressKnob, fillW - 6);
  if (musicUi.progressKnobHalo) lv_obj_set_x(musicUi.progressKnobHalo, fillW - 10);

  lv_label_set_text(musicUi.timeElapsed, musicFormatTime(elapsed).c_str());
  lv_label_set_text(musicUi.timeTotal,   musicFormatTime(duration).c_str());
}

void updateMusicTrackUi() {
  if (musicUi.title) {
    char buf[32];
    musicWriteTruncated(buf, sizeof(buf), musicCurrentTitle(), kMusicTitleMaxChars);
    lv_label_set_text(musicUi.title, buf);
  }
  if (musicUi.artistAlbum) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s | %s", musicCurrentArtist(), musicCurrentAlbum());
    lv_label_set_text(musicUi.artistAlbum, buf);
  }
  if (musicUi.albumCounter) {
    if (spotify.valid) {
      lv_label_set_text(musicUi.albumCounter, "LIVE FROM SPOTIFY");
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "TRACK %02d \xC2\xB7 OF %02d",
               musicTrackIndex + 1, kMusicTrackCount);
      lv_label_set_text(musicUi.albumCounter, buf);
    }
  }
  // The album art stays the same teal→purple radial backdrop across tracks
  // (matching the reference), so we no longer recolor the tile per-track.
  updateMusicProgress();
}

void updateMusicPlayIcon() {
  if (!musicUi.playIcon) return;
  lv_label_set_text(musicUi.playIcon, musicCurrentlyPlaying() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
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
