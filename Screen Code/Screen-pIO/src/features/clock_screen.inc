// Clock screen
// ---------------------------------------------------------------------------
void buildClockScreen(lv_obj_t *parent) {
  clockUi.root = makePlain(parent);
  lv_obj_set_size(clockUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(clockUi.root, 0, 0);

  // Solid background
  lv_obj_set_style_bg_color(clockUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(clockUi.root, LV_OPA_COVER, 0);

  // Status row
  buildStatusRow(clockUi.root, clockUi.status, "CONNECTING", "");
  lv_obj_t *mainStatus = lv_obj_get_parent(clockUi.status.dot);
  lv_obj_set_size(mainStatus, 160, 28);
  lv_obj_set_pos(mainStatus, 12, 12);
  lv_obj_set_style_bg_color(mainStatus, hex(0x000000), 0);
  lv_obj_set_style_bg_opa(mainStatus, LV_OPA_40, 0);
  lv_obj_set_style_radius(mainStatus, 14, 0);
  lv_obj_set_style_pad_left(mainStatus, 10, 0);
  lv_obj_set_style_pad_right(mainStatus, 10, 0);
  lv_obj_set_style_border_width(mainStatus, 1, 0);
  lv_obj_set_style_border_color(mainStatus, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_border_opa(mainStatus, LV_OPA_70, 0);
  lv_obj_set_size(clockUi.status.dot, 10, 10);
  lv_obj_align(clockUi.status.dot, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_text_color(clockUi.status.connected, hex(COL_INK), 0);
  lv_obj_set_style_text_font(clockUi.status.connected, &lv_font_montserrat_14, 0);
  lv_obj_align(clockUi.status.connected, LV_ALIGN_LEFT_MID, 28, 0);
  lv_obj_add_flag(clockUi.status.rightInfo, LV_OBJ_FLAG_HIDDEN);
  const uint32_t mainStatusChildCount = lv_obj_get_child_cnt(mainStatus);
  for (uint32_t i = 0; i < mainStatusChildCount; ++i) {
    lv_obj_t *child = lv_obj_get_child(mainStatus, i);
    if (child != clockUi.status.dot &&
        child != clockUi.status.connected &&
        child != clockUi.status.rightInfo) {
      lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_obj_move_foreground(mainStatus);

  // Date strip
  lv_obj_t *dateStrip = makePlain(clockUi.root);
  lv_obj_set_size(dateStrip, SCREEN_WIDTH - 80, 20);
  lv_obj_set_pos(dateStrip, 40, 60);

  clockUi.dayName = makeLabel(dateStrip, "Thursday", &lv_font_montserrat_16, COL_INK);
  lv_obj_align(clockUi.dayName, LV_ALIGN_LEFT_MID, 0, 0);

  clockUi.dateLong = makeLabel(dateStrip, "23  APRIL  2026", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(clockUi.dateLong, LV_ALIGN_RIGHT_MID, 0, 0);

  // thin separator line
  lv_obj_t *sepL = makePlain(dateStrip);
  lv_obj_set_size(sepL, 380, 1);
  lv_obj_set_style_bg_color(sepL, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_bg_opa(sepL, LV_OPA_50, 0);
  lv_obj_align(sepL, LV_ALIGN_CENTER, 0, 0);

  // Hero clock — huge digits using the custom montserrat_hero_180 font.
  // A native font renders directly (no transform_zoom, which can't work in
  // this LVGL build because LV_COLOR_SCREEN_TRANSP is 0, making layer
  // allocation fail for labels that need alpha compositing).
  //
  // Digit glyph width is ~110-115 px at 180 px font size; "08" measures
  // ~215 px wide, so each two-digit block is given a 240 px slot.
  constexpr int kDigitBoxW = 240;
  constexpr int kDigitBoxH = 160;
  constexpr int kColonBoxW = 40;
  constexpr int kGap = 6;
  constexpr int kHeroTotalW = kDigitBoxW + kColonBoxW + kDigitBoxW + kGap * 2;
  // Anchor the hero block to the left side of the screen so the PM/AM
  // superscript and top-right weather glance have clear breathing room.
  const int kHeroLeft = 40;
  const int kHeroY = 140;

  auto styleHero = [](lv_obj_t *l, int w, int h) {
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  };

  clockUi.hh = makeLabel(clockUi.root, "10", &montserrat_hero_180, COL_INK);
  styleHero(clockUi.hh, kDigitBoxW, kDigitBoxH);
  lv_obj_set_pos(clockUi.hh, kHeroLeft, kHeroY);

  clockUi.colon = makeLabel(clockUi.root, ":", &montserrat_hero_180, COL_ACCENT);
  styleHero(clockUi.colon, kColonBoxW, kDigitBoxH);
  lv_obj_set_pos(clockUi.colon, kHeroLeft + kDigitBoxW + kGap, kHeroY);

  clockUi.mm = makeLabel(clockUi.root, "42", &montserrat_hero_180, COL_INK);
  styleHero(clockUi.mm, kDigitBoxW, kDigitBoxH);
  lv_obj_set_pos(clockUi.mm, kHeroLeft + kDigitBoxW + kColonBoxW + kGap * 2, kHeroY);

  // PM / AM superscript, sitting near the top-right corner of the minutes.
  clockUi.ampm = makeLabel(clockUi.root, "AM", &lv_font_montserrat_28, COL_INK_3);
  lv_obj_set_size(clockUi.ampm, 48, 34);
  lv_obj_set_pos(clockUi.ampm,
                 kHeroLeft + kHeroTotalW + 6,
                 kHeroY + 26);

  // Weather glance — top-right corner, matching the reference screenshot.
  // Icon on top-right, temp with degree symbol below, condition + city on
  // two small lines underneath.
  lv_obj_t *glance = makePlain(clockUi.root);
  lv_obj_set_size(glance, 150, 200);
  lv_obj_set_pos(glance, SCREEN_WIDTH - 150 - 24, 90);

  clockUi.glanceIconCanvas = lv_canvas_create(glance);
  lv_obj_remove_style_all(clockUi.glanceIconCanvas);
  lv_obj_set_size(clockUi.glanceIconCanvas, 48, 48);
  lv_canvas_set_buffer(clockUi.glanceIconCanvas, glanceIconBuf, 48, 48, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(clockUi.glanceIconCanvas, LV_ALIGN_TOP_RIGHT, -4, 0);

  clockUi.glanceTemp = makeLabel(glance, "--\xC2\xB0", &lv_font_montserrat_48, COL_INK);
  lv_obj_align(clockUi.glanceTemp, LV_ALIGN_TOP_RIGHT, 0, 60);

  clockUi.glanceCond = makeLabel(glance, "WAITING", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_align(clockUi.glanceCond, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(clockUi.glanceCond, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(clockUi.glanceCond, 150);
  lv_obj_align(clockUi.glanceCond, LV_ALIGN_TOP_RIGHT, 0, 130);

  // Now Playing card
  lv_obj_t *np = makeGlass(clockUi.root, SCREEN_WIDTH - 80, 72, 34);
  lv_obj_set_pos(np, 40, SCREEN_HEIGHT - 130);
  lv_obj_set_style_bg_opa(np, LV_OPA_70, 0);
  lv_obj_set_style_border_color(np, hex(COL_ACCENT_SOFT), 0);
  lv_obj_set_style_border_opa(np, LV_OPA_60, 0);
  lv_obj_set_style_shadow_width(np, 18, 0);
  lv_obj_set_style_shadow_ofs_y(np, 3, 0);

  // Compact audio badge, matching the portrait clock widget.
  lv_obj_t *album = makePlain(np);
  lv_obj_set_size(album, 48, 48);
  lv_obj_set_style_radius(album, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(album, hex(0x153A55), 0);
  lv_obj_set_style_bg_opa(album, LV_OPA_COVER, 0);
  lv_obj_set_pos(album, 16, 12);

  lv_obj_t *musicIcon = makeLabel(album, LV_SYMBOL_AUDIO, &lv_font_montserrat_20, COL_INK_2);
  lv_obj_center(musicIcon);

  clockUi.nowPlayingTitle = makeLabel(np, "Resonance - Extended Mix", &lv_font_montserrat_14, COL_INK);
  lv_label_set_long_mode(clockUi.nowPlayingTitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(clockUi.nowPlayingTitle, 490, 18);
  lv_obj_set_pos(clockUi.nowPlayingTitle, 88, 17);

  clockUi.nowPlayingArtist = makeLabel(np, "HOME  \xE2\x80\xA2  Odyssey", &lv_font_montserrat_12, COL_INK_3);
  lv_label_set_long_mode(clockUi.nowPlayingArtist, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(clockUi.nowPlayingArtist, 470, 16);
  lv_obj_set_pos(clockUi.nowPlayingArtist, 88, 40);

  const int ctrlX = SCREEN_WIDTH - 80 - 126;
  lv_obj_t *prev = makeLabel(np, LV_SYMBOL_PREV, &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(prev, ctrlX, 28);
  lv_obj_t *play = makeLabel(np, LV_SYMBOL_PLAY, &lv_font_montserrat_12, COL_INK);
  lv_obj_set_pos(play, ctrlX + 40, 29);
  lv_obj_t *next = makeLabel(np, LV_SYMBOL_NEXT, &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(next, ctrlX + 78, 28);
}

// ---------------------------------------------------------------------------
