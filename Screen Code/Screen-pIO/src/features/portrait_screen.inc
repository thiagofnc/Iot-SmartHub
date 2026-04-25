void buildPortraitClockScreen(lv_obj_t *parent) {
  portraitUi.root = makePlain(parent);
  lv_obj_set_size(portraitUi.root, PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
  lv_obj_set_pos(portraitUi.root, 0, 0);
  lv_obj_add_flag(portraitUi.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(portraitUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(portraitUi.root, LV_OPA_COVER, 0);

  buildStatusRow(portraitUi.root, portraitUi.status, "CONNECTING", "");

  lv_obj_t *stage = makeGlass(portraitUi.root, 400, 680, 24);
  lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, 56);

  portraitUi.dayName = makeLabel(stage, "Thursday", &lv_font_montserrat_20, COL_INK);
  lv_obj_align(portraitUi.dayName, LV_ALIGN_TOP_MID, 0, 18);

  portraitUi.dateLong = makeLabel(stage, "23 APRIL 2026", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align_to(portraitUi.dateLong, portraitUi.dayName, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

  lv_obj_t *sep = makePlain(stage);
  lv_obj_set_size(sep, 320, 1);
  lv_obj_set_style_bg_color(sep, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_bg_opa(sep, LV_OPA_70, 0);
  lv_obj_align_to(sep, portraitUi.dateLong, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

  auto stylePortraitHero = [](lv_obj_t *l, int w, int h) {
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  };

  portraitUi.hh = makeLabel(stage, "10", &lv_font_montserrat_48, COL_INK);
  stylePortraitHero(portraitUi.hh, 120, 70);
  lv_obj_align(portraitUi.hh, LV_ALIGN_TOP_MID, -72, 122);

  portraitUi.colon = makeLabel(stage, ":", &lv_font_montserrat_48, COL_ACCENT);
  stylePortraitHero(portraitUi.colon, 24, 70);
  lv_obj_align(portraitUi.colon, LV_ALIGN_TOP_MID, 0, 122);

  portraitUi.mm = makeLabel(stage, "42", &lv_font_montserrat_48, COL_INK);
  stylePortraitHero(portraitUi.mm, 120, 70);
  lv_obj_align(portraitUi.mm, LV_ALIGN_TOP_MID, 72, 122);

  portraitUi.ampm = makeLabel(stage, "AM", &lv_font_montserrat_18, COL_INK_3);
  lv_obj_align(portraitUi.ampm, LV_ALIGN_TOP_MID, 110, 166);

  lv_obj_t *weatherCard = makeGlass(stage, 336, 112, 16);
  lv_obj_align(weatherCard, LV_ALIGN_TOP_MID, 0, 246);

  portraitUi.weatherIconCanvas = lv_canvas_create(weatherCard);
  lv_obj_remove_style_all(portraitUi.weatherIconCanvas);
  lv_obj_set_size(portraitUi.weatherIconCanvas, 56, 56);
  lv_canvas_set_buffer(portraitUi.weatherIconCanvas, portraitIconBuf, 56, 56, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(portraitUi.weatherIconCanvas, 18, 28);

  portraitUi.weatherTemp = makeLabel(weatherCard, "--\xC2\xB0", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(portraitUi.weatherTemp, 96, 18);

  portraitUi.weatherCond = makeLabel(weatherCard, "WAITING FOR DATA", &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_pos(portraitUi.weatherCond, 96, 52);

  portraitUi.weatherLoc = makeLabel(weatherCard, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(portraitUi.weatherLoc, 96, 76);

  lv_obj_t *np = makeGlass(stage, 336, 98, 16);
  lv_obj_align(np, LV_ALIGN_BOTTOM_MID, 0, -24);

  lv_obj_t *album = makePlain(np);
  lv_obj_set_size(album, 56, 56);
  lv_obj_set_style_bg_color(album, hex(0xB85C3A), 0);
  lv_obj_set_style_bg_grad_color(album, hex(0x6B3A7A), 0);
  lv_obj_set_style_bg_grad_dir(album, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(album, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(album, 10, 0);
  lv_obj_set_pos(album, 16, 21);

  lv_obj_t *hole = makePlain(album);
  lv_obj_set_size(hole, 10, 10);
  lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hole, hex(0x000000), 0);
  lv_obj_set_style_bg_opa(hole, LV_OPA_60, 0);
  lv_obj_align(hole, LV_ALIGN_CENTER, 0, 0);

  portraitUi.nowPlayingTitle = makeLabel(np, "Resonance - Extended Mix", &lv_font_montserrat_14, COL_INK);
  lv_obj_set_pos(portraitUi.nowPlayingTitle, 92, 24);

  portraitUi.nowPlayingArtist = makeLabel(np, "HOME  Odyssey", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(portraitUi.nowPlayingArtist, 92, 50);
}

// ---------------------------------------------------------------------------
