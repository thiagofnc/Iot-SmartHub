// Weather screen
// ---------------------------------------------------------------------------
void buildWeatherScreen(lv_obj_t *parent) {
  weatherUi.root = makePlain(parent);
  lv_obj_set_size(weatherUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(weatherUi.root, 0, 0);
  lv_obj_add_flag(weatherUi.root, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_style_bg_color(weatherUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(weatherUi.root, LV_OPA_COVER, 0);

  buildStatusRow(weatherUi.root, weatherUi.status, "CONNECTING", "--");
  weatherUi.updated = weatherUi.status.rightInfo;

  // ---------- HERO CARD (left) ----------
  const int heroX = 28;
  const int heroY = 50;
  const int heroW = 420;
  const int heroH = SCREEN_HEIGHT - heroY - 46;

  lv_obj_t *hero = makeGlass(weatherUi.root, heroW, heroH, 22);
  lv_obj_set_pos(hero, heroX, heroY);

  // location row
  weatherUi.locLabel = makeLabel(hero, LV_SYMBOL_GPS " --", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_set_pos(weatherUi.locLabel, 22, 18);

  weatherUi.coordLabel = makeLabel(hero, "", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(weatherUi.coordLabel, LV_ALIGN_TOP_RIGHT, -22, 18);

  constexpr int kWeatherHeroIconSize = 144;
  constexpr int kWeatherHeroTextW = 220;

  // Big temperature number
  weatherUi.bigTemp = makeLabel(hero, "--\xC2\xB0", &lv_font_montserrat_48, COL_INK);
  lv_obj_set_style_text_letter_space(weatherUi.bigTemp, -2, 0);
  lv_obj_set_pos(weatherUi.bigTemp, 22, 78);

  // Condition icon
  weatherUi.condIconCanvas = lv_canvas_create(hero);
  lv_obj_remove_style_all(weatherUi.condIconCanvas);
  lv_obj_set_size(weatherUi.condIconCanvas, kWeatherHeroIconSize, kWeatherHeroIconSize);
  lv_canvas_set_buffer(weatherUi.condIconCanvas, condIconBuf,
                       kWeatherHeroIconSize, kWeatherHeroIconSize, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(weatherUi.condIconCanvas, heroW - kWeatherHeroIconSize - 18, 58);

  weatherUi.condLabel = makeLabel(hero, "--", &lv_font_montserrat_20, COL_INK);
  lv_obj_set_width(weatherUi.condLabel, kWeatherHeroTextW);
  lv_label_set_long_mode(weatherUi.condLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(weatherUi.condLabel, 22, 160);

  weatherUi.feelsLabel = makeLabel(hero, "FEELS LIKE --", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_width(weatherUi.feelsLabel, kWeatherHeroTextW);
  lv_obj_set_pos(weatherUi.feelsLabel, 22, 196);

  // Divider line
  lv_obj_t *div = makePlain(hero);
  lv_obj_set_size(div, heroW - 44, 1);
  lv_obj_set_pos(div, 22, heroH - 88);
  lv_obj_set_style_bg_color(div, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_bg_opa(div, LV_OPA_70, 0);

  // Hi / Lo / Wind / Rain row
  struct Pair { const char *label; const char *val; lv_obj_t **out; };
  Pair pairs[4] = {
      {"HIGH", "--", &weatherUi.hiVal},
      {"LOW",  "--", &weatherUi.loVal},
      {"WIND", "--", &weatherUi.windVal},
      {"RAIN", "--", &weatherUi.rainVal},
  };
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *col = makePlain(hero);
    lv_obj_set_size(col, (heroW - 44) / 4, 60);
    lv_obj_set_pos(col, 22 + i * ((heroW - 44) / 4), heroH - 76);

    lv_obj_t *lbl = makeLabel(col, pairs[i].label, &lv_font_montserrat_12, COL_INK_3);
    lv_obj_set_pos(lbl, 0, 0);

    lv_obj_t *v = makeLabel(col, pairs[i].val, &lv_font_montserrat_22, COL_INK);
    lv_obj_set_pos(v, 0, 22);
    *pairs[i].out = v;
  }

  // ---------- RIGHT-HAND TILES ----------
  const int tileX = heroX + heroW + 14;
  const int tileRightEdge = SCREEN_WIDTH - 28;
  const int tileCol1Y = heroY;
  const int colW = (tileRightEdge - tileX - 10) / 2;
  const int rowH = 92;
  const int rowH2 = 108;

  // Humidity (top-left tile)
  lv_obj_t *hum = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(hum, tileX, tileCol1Y);
  makeLabel(hum, LV_SYMBOL_TINT " HUMIDITY", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(lv_obj_get_child(hum, 0), LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.humVal = makeLabel(hum, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.humVal, 14, 34);
  lv_obj_t *humBar = makePlain(hum);
  lv_obj_set_size(humBar, colW - 28, 4);
  lv_obj_set_pos(humBar, 14, rowH - 16);
  lv_obj_set_style_bg_color(humBar, hex(COL_RAIL_BG), 0);
  lv_obj_set_style_bg_opa(humBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(humBar, 2, 0);
  weatherUi.humBarFill = makePlain(humBar);
  lv_obj_set_size(weatherUi.humBarFill, 0, 4);
  lv_obj_set_pos(weatherUi.humBarFill, 0, 0);
  lv_obj_set_style_bg_color(weatherUi.humBarFill, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(weatherUi.humBarFill, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(weatherUi.humBarFill, 2, 0);

  // Pressure (top-right tile)
  lv_obj_t *press = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(press, tileX + colW + 10, tileCol1Y);
  lv_obj_t *pl = makeLabel(press, LV_SYMBOL_REFRESH " PRESSURE", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(pl, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.pressVal = makeLabel(press, "--", &lv_font_montserrat_22, COL_INK);
  lv_obj_set_pos(weatherUi.pressVal, 14, 34);
  weatherUi.pressSub = makeLabel(press, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.pressSub, 14, rowH - 22);

  // Wind tile (middle-left)
  lv_obj_t *wind = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(wind, tileX, tileCol1Y + rowH + 10);
  lv_obj_t *wl = makeLabel(wind, LV_SYMBOL_SHUFFLE " WIND", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(wl, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.windTileVal = makeLabel(wind, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.windTileVal, 14, 34);
  weatherUi.windTileSub = makeLabel(wind, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.windTileSub, 14, rowH - 22);

  weatherUi.windArrow = makeLabel(wind, LV_SYMBOL_UP, &lv_font_montserrat_16, COL_ACCENT);
  lv_obj_set_pos(weatherUi.windArrow, colW - 28, rowH - 34);
  lv_obj_set_style_transform_angle(weatherUi.windArrow, 2250, 0);  // 225 deg (SW)
  lv_obj_set_style_transform_pivot_x(weatherUi.windArrow, 6, 0);
  lv_obj_set_style_transform_pivot_y(weatherUi.windArrow, 10, 0);

  // AQI tile (middle-right)
  lv_obj_t *aqi = makeGlass(weatherUi.root, colW, rowH, 16);
  lv_obj_set_pos(aqi, tileX + colW + 10, tileCol1Y + rowH + 10);
  lv_obj_t *al = makeLabel(aqi, LV_SYMBOL_CHARGE " AIR  UV", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(al, LV_ALIGN_TOP_LEFT, 14, 12);
  weatherUi.aqiVal = makeLabel(aqi, "--", &lv_font_montserrat_28, COL_INK);
  lv_obj_set_pos(weatherUi.aqiVal, 14, 34);
  weatherUi.aqiSub = makeLabel(aqi, "--", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_pos(weatherUi.aqiSub, 14, rowH - 22);

  weatherUi.aqiArc = lv_arc_create(aqi);
  lv_obj_remove_style(weatherUi.aqiArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(weatherUi.aqiArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(weatherUi.aqiArc, 46, 46);
  lv_obj_align(weatherUi.aqiArc, LV_ALIGN_TOP_RIGHT, -10, 10);
  lv_arc_set_rotation(weatherUi.aqiArc, 135);
  lv_arc_set_bg_angles(weatherUi.aqiArc, 0, 270);
  lv_arc_set_value(weatherUi.aqiArc, 0);
  lv_obj_set_style_arc_width(weatherUi.aqiArc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_color(weatherUi.aqiArc, hex(COL_RAIL_BG), LV_PART_MAIN);
  lv_obj_set_style_arc_width(weatherUi.aqiArc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(weatherUi.aqiArc, hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(weatherUi.aqiArc, true, LV_PART_INDICATOR);

  // Solar arc tile (spans both cols)
  const int solarW = colW * 2 + 10;
  const int solarY = tileCol1Y + rowH + 10 + rowH + 10;
  const int solarH = heroH - (rowH * 2 + 20);
  lv_obj_t *sun = makeGlass(weatherUi.root, solarW, solarH, 16);
  lv_obj_set_pos(sun, tileX, solarY);
  lv_obj_t *sl = makeLabel(sun, LV_SYMBOL_IMAGE " SOLAR ARC", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_align(sl, LV_ALIGN_TOP_LEFT, 14, 12);

  // Arc
  weatherUi.sunArc = lv_arc_create(sun);
  lv_obj_remove_style(weatherUi.sunArc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(weatherUi.sunArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(weatherUi.sunArc, solarW - 40, (solarH - 60) * 2);  // bottom half visible
  lv_obj_align(weatherUi.sunArc, LV_ALIGN_TOP_MID, 0, 26);
  lv_arc_set_rotation(weatherUi.sunArc, 180);
  lv_arc_set_bg_angles(weatherUi.sunArc, 0, 180);
  lv_arc_set_value(weatherUi.sunArc, 55);
  lv_obj_set_style_arc_width(weatherUi.sunArc, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_color(weatherUi.sunArc, hex(COL_RAIL_BG), LV_PART_MAIN);
  lv_obj_set_style_arc_width(weatherUi.sunArc, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(weatherUi.sunArc, hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(weatherUi.sunArc, true, LV_PART_INDICATOR);

  // Sun dot
  weatherUi.sunDot = makePlain(sun);
  lv_obj_set_size(weatherUi.sunDot, 10, 10);
  lv_obj_set_style_radius(weatherUi.sunDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(weatherUi.sunDot, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(weatherUi.sunDot, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(weatherUi.sunDot, 14, 0);
  lv_obj_set_style_shadow_color(weatherUi.sunDot, hex(COL_ACCENT), 0);
  lv_obj_set_style_shadow_opa(weatherUi.sunDot, LV_OPA_50, 0);
  // Place dot at the top of the arc (approximate — aligned to arc apex)
  lv_obj_align(weatherUi.sunDot, LV_ALIGN_TOP_MID, -40, 28);

  // Sunrise / Daylight / Sunset row
  weatherUi.sunriseText = makeLabel(sun, "SUNRISE\n--:--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.sunriseText, LV_ALIGN_BOTTOM_LEFT, 14, -10);
  lv_obj_set_style_text_line_space(weatherUi.sunriseText, 2, 0);

  weatherUi.daylightText = makeLabel(sun, "DAYLIGHT\n--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.daylightText, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_align(weatherUi.daylightText, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(weatherUi.daylightText, 2, 0);

  weatherUi.sunsetText = makeLabel(sun, "SUNSET\n--:--", &lv_font_montserrat_12, COL_INK_2);
  lv_obj_align(weatherUi.sunsetText, LV_ALIGN_BOTTOM_RIGHT, -14, -10);
  lv_obj_set_style_text_align(weatherUi.sunsetText, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_line_space(weatherUi.sunsetText, 2, 0);
}
