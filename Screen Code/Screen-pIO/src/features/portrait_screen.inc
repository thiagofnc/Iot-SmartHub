void buildPortraitClockScreen(lv_obj_t *parent) {
  portraitUi.root = makePlain(parent);
  lv_obj_set_size(portraitUi.root, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
  lv_obj_set_pos(portraitUi.root, 0, 0);
  lv_obj_add_flag(portraitUi.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(portraitUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(portraitUi.root, LV_OPA_COVER, 0);

  ambientBlob(portraitUi.root, 170, 132, 360, COL_AMB_BLUE, LV_OPA_20);
  ambientBlob(portraitUi.root, 392, 666, 360, COL_AMB_PURPLE, LV_OPA_20);
  ambientBlob(portraitUi.root, 234, 410, 300, COL_AMB_TEAL, LV_OPA_10);

  buildStatusRow(portraitUi.root, portraitUi.status, "CONNECTING", "--\xC2\xB0  \xE2\x80\xA2  41%");
  lv_obj_t *status = lv_obj_get_parent(portraitUi.status.dot);
  lv_obj_set_size(status, PORTRAIT_WIDTH - 56, 22);
  lv_obj_set_pos(status, 28, 20);
  lv_obj_set_style_bg_opa(status, LV_OPA_0, 0);
  lv_obj_set_size(portraitUi.status.dot, 7, 7);
  lv_obj_align(portraitUi.status.dot, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_shadow_width(portraitUi.status.dot, 10, 0);
  lv_obj_set_style_shadow_color(portraitUi.status.dot, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(portraitUi.status.dot, LV_OPA_70, 0);
  lv_obj_set_style_text_color(portraitUi.status.connected, hex(COL_INK_3), 0);
  lv_obj_align(portraitUi.status.connected, LV_ALIGN_LEFT_MID, 18, 0);
  lv_obj_set_style_text_align(portraitUi.status.rightInfo, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(portraitUi.status.rightInfo, LV_ALIGN_RIGHT_MID, -38, 0);

  lv_obj_t *deviceRing = makePlain(portraitUi.root);
  lv_obj_set_size(deviceRing, 36, 36);
  lv_obj_set_pos(deviceRing, PORTRAIT_WIDTH - 54, 14);
  lv_obj_set_style_radius(deviceRing, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(deviceRing, LV_OPA_20, 0);
  lv_obj_set_style_bg_color(deviceRing, hex(COL_ACCENT), 0);
  lv_obj_set_style_border_width(deviceRing, 2, 0);
  lv_obj_set_style_border_color(deviceRing, hex(COL_ACCENT), 0);
  lv_obj_set_style_border_opa(deviceRing, LV_OPA_60, 0);
  lv_obj_t *deviceIcon = makeLabel(deviceRing, LV_SYMBOL_CHARGE, &lv_font_montserrat_16, COL_ACCENT);
  lv_obj_center(deviceIcon);

  lv_obj_t *dateStrip = makePlain(portraitUi.root);
  lv_obj_set_size(dateStrip, PORTRAIT_WIDTH - 84, 28);
  lv_obj_set_pos(dateStrip, 42, 74);

  portraitUi.dayName = makeLabel(dateStrip, "Thursday", &lv_font_montserrat_18, COL_INK);
  lv_obj_align(portraitUi.dayName, LV_ALIGN_LEFT_MID, 34, 0);

  portraitUi.dateLong = makeLabel(dateStrip, "23  APRIL  2026", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_align(portraitUi.dateLong, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(portraitUi.dateLong, LV_ALIGN_RIGHT_MID, -16, 1);

  lv_obj_t *dateDotA = makeDot(dateStrip, 3, COL_INK_4);
  lv_obj_align(dateDotA, LV_ALIGN_CENTER, -46, 1);
  lv_obj_t *dateDotB = makeDot(dateStrip, 3, COL_INK_4);
  lv_obj_align(dateDotB, LV_ALIGN_CENTER, 42, 1);

  auto stylePortraitHero = [](lv_obj_t *l, int w, int h) {
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  };

  constexpr int kHeroW = 314;
  constexpr int kHeroH = 160;
  constexpr int kHeroX = (PORTRAIT_WIDTH - kHeroW) / 2;

  portraitUi.hh = makeLabel(portraitUi.root, "10", &montserrat_hero_180, COL_INK);
  stylePortraitHero(portraitUi.hh, kHeroW, kHeroH);
  lv_obj_set_pos(portraitUi.hh, kHeroX, 160);

  portraitUi.mm = makeLabel(portraitUi.root, "42", &montserrat_hero_180, COL_ACCENT);
  stylePortraitHero(portraitUi.mm, kHeroW, kHeroH);
  lv_obj_set_pos(portraitUi.mm, kHeroX, 328);

  portraitUi.colon = makeLabel(portraitUi.root, ":", &montserrat_hero_180, COL_ACCENT);
  stylePortraitHero(portraitUi.colon, 28, 78);
  lv_obj_set_pos(portraitUi.colon, -80, -80);

  portraitUi.ampm = makeLabel(portraitUi.root, "AM", &lv_font_montserrat_14, COL_INK_3);
  lv_obj_set_size(portraitUi.ampm, 34, 20);
  lv_obj_set_pos(portraitUi.ampm, kHeroX + kHeroW - 22, 170);

  lv_obj_t *weatherCard = makeGlass(portraitUi.root, 286, 40, 20);
  lv_obj_set_pos(weatherCard, (PORTRAIT_WIDTH - 286) / 2, 542);
  lv_obj_set_style_bg_opa(weatherCard, LV_OPA_60, 0);
  lv_obj_set_style_shadow_width(weatherCard, 14, 0);
  lv_obj_set_style_shadow_ofs_y(weatherCard, 4, 0);

  lv_obj_t *weatherAccent = makePlain(weatherCard);
  lv_obj_set_size(weatherAccent, 150, 2);
  lv_obj_set_style_bg_color(weatherAccent, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(weatherAccent, LV_OPA_COVER, 0);
  lv_obj_set_pos(weatherAccent, 36, 0);

  portraitUi.weatherIconCanvas = lv_canvas_create(weatherCard);
  lv_obj_remove_style_all(portraitUi.weatherIconCanvas);
  lv_obj_set_size(portraitUi.weatherIconCanvas, 24, 24);
  lv_canvas_set_buffer(portraitUi.weatherIconCanvas, portraitIconBuf, 24, 24, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(portraitUi.weatherIconCanvas, 18, 8);

  portraitUi.weatherTemp = makeLabel(weatherCard, "--\xC2\xB0", &lv_font_montserrat_18, COL_INK);
  lv_obj_set_pos(portraitUi.weatherTemp, 58, 8);

  portraitUi.weatherCond = makeLabel(weatherCard, "WAITING", &lv_font_montserrat_12, COL_INK_2);
  lv_label_set_long_mode(portraitUi.weatherCond, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(portraitUi.weatherCond, 86, 16);
  lv_obj_set_pos(portraitUi.weatherCond, 118, 13);

  lv_obj_t *weatherSep = makeDot(weatherCard, 3, COL_INK_4);
  lv_obj_set_pos(weatherSep, 206, 19);

  portraitUi.weatherLoc = makeLabel(weatherCard, "--", &lv_font_montserrat_12, COL_INK_2);
  lv_label_set_long_mode(portraitUi.weatherLoc, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(portraitUi.weatherLoc, 54, 16);
  lv_obj_set_pos(portraitUi.weatherLoc, 218, 13);

  lv_obj_t *np = makeGlass(portraitUi.root, PORTRAIT_WIDTH - 56, 72, 34);
  lv_obj_set_pos(np, 28, PORTRAIT_HEIGHT - 88);
  lv_obj_set_style_bg_opa(np, LV_OPA_70, 0);
  lv_obj_set_style_border_color(np, hex(COL_ACCENT_SOFT), 0);
  lv_obj_set_style_border_opa(np, LV_OPA_60, 0);
  lv_obj_set_style_shadow_width(np, 18, 0);
  lv_obj_set_style_shadow_ofs_y(np, 3, 0);

  lv_obj_t *album = makePlain(np);
  lv_obj_set_size(album, 48, 48);
  lv_obj_set_style_radius(album, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(album, hex(0x153A55), 0);
  lv_obj_set_style_bg_opa(album, LV_OPA_COVER, 0);
  lv_obj_set_pos(album, 16, 12);

  lv_obj_t *musicIcon = makeLabel(album, LV_SYMBOL_AUDIO, &lv_font_montserrat_20, COL_INK_2);
  lv_obj_center(musicIcon);

  portraitUi.nowPlayingTitle = makeLabel(np, "Resonance - Extended Mix", &lv_font_montserrat_14, COL_INK);
  lv_label_set_long_mode(portraitUi.nowPlayingTitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(portraitUi.nowPlayingTitle, 196, 18);
  lv_obj_set_pos(portraitUi.nowPlayingTitle, 88, 17);

  portraitUi.nowPlayingArtist = makeLabel(np, "HOME  \xE2\x80\xA2  Odyssey", &lv_font_montserrat_12, COL_INK_3);
  lv_label_set_long_mode(portraitUi.nowPlayingArtist, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(portraitUi.nowPlayingArtist, 178, 16);
  lv_obj_set_pos(portraitUi.nowPlayingArtist, 88, 40);

  const int ctrlX = PORTRAIT_WIDTH - 56 - 126;
  lv_obj_t *prev = makeLabel(np, LV_SYMBOL_PREV, &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(prev, ctrlX, 28);
  lv_obj_t *play = makeLabel(np, LV_SYMBOL_PLAY, &lv_font_montserrat_12, COL_INK);
  lv_obj_set_pos(play, ctrlX + 40, 29);
  lv_obj_t *next = makeLabel(np, LV_SYMBOL_NEXT, &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(next, ctrlX + 78, 28);
}

// ---------------------------------------------------------------------------
