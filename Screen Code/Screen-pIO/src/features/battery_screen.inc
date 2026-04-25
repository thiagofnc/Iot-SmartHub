// Battery screen
// ---------------------------------------------------------------------------
uint32_t batteryColorForPercent(int percent) {
  if (percent <= 20) return 0xE06060;
  if (percent <= 50) return 0xE0A040;
  return 0x38EF7D;
}

void updateBatteryGauge(int index) {
  if (index < 0 || index >= kBatteryDeviceCount) return;
  BatteryGaugeUi &ui = batteryUi.gauges[index];
  if (!ui.arc) return;

  const int percent = constrain(batteryDevices[index].percent, 0, 100);
  lv_arc_set_value(ui.arc, percent);
  lv_obj_set_style_arc_color(ui.arc, hex(batteryColorForPercent(percent)), LV_PART_INDICATOR);
  lv_obj_set_style_text_color(ui.icon, hex(percent <= 20 ? 0xFFB3B3 : COL_INK), 0);

  char valueBuf[8];
  snprintf(valueBuf, sizeof(valueBuf), "%d", percent);
  lv_label_set_text(ui.value, valueBuf);
  lv_label_set_text(ui.name, batteryDevices[index].name);
  lv_label_set_text(ui.icon, batteryDevices[index].icon);
}

void updateBatteryUi() {
  if (!batteryUi.root) return;
  for (int i = 0; i < kBatteryDeviceCount; ++i) updateBatteryGauge(i);
  lv_label_set_text(batteryUi.updated, "SERIAL INPUT");
}

bool setDeviceBattery(int deviceNumber, int percent) {
  if (deviceNumber < 1 || deviceNumber > kBatteryDeviceCount) return false;
  if (percent < 0 || percent > 100) return false;
  batteryDevices[deviceNumber - 1].percent = percent;
  updateBatteryGauge(deviceNumber - 1);
  lv_label_set_text(batteryUi.updated, "UPDATED FROM SERIAL");
  return true;
}

lv_obj_t *makeBatteryGauge(lv_obj_t *parent, int index, int x, int y) {
  lv_obj_t *tile = makePlain(parent);
  lv_obj_set_size(tile, 304, 132);
  lv_obj_set_pos(tile, x, y);

  BatteryGaugeUi &ui = batteryUi.gauges[index];

  ui.name = makeLabel(tile, batteryDevices[index].name, &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_align(ui.name, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(ui.name, 2, 0);
  lv_obj_set_width(ui.name, 100);
  lv_obj_align(ui.name, LV_ALIGN_TOP_MID, 0, 0);

  ui.arc = lv_arc_create(tile);
  lv_obj_remove_style(ui.arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(ui.arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(ui.arc, 104, 104);
  lv_obj_align(ui.arc, LV_ALIGN_TOP_MID, 0, 20);
  lv_arc_set_range(ui.arc, 0, 100);
  lv_arc_set_rotation(ui.arc, 135);
  lv_arc_set_bg_angles(ui.arc, 0, 270);
  lv_obj_set_style_arc_width(ui.arc, 9, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ui.arc, hex(COL_RAIL_BG), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(ui.arc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ui.arc, 9, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ui.arc, true, LV_PART_INDICATOR);

  ui.icon = makeLabel(tile, batteryDevices[index].icon, &lv_font_montserrat_20, COL_INK);
  lv_obj_align(ui.icon, LV_ALIGN_TOP_MID, 0, 62);

  ui.value = makeLabel(tile, "0", &lv_font_montserrat_22, COL_INK);
  lv_obj_set_style_text_align(ui.value, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(ui.value, 82);
  lv_obj_align(ui.value, LV_ALIGN_TOP_MID, 0, 110);

  updateBatteryGauge(index);
  return tile;
}

void buildBatteryScreen(lv_obj_t *parent) {
  batteryUi.root = makePlain(parent);
  lv_obj_set_size(batteryUi.root, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(batteryUi.root, 0, 0);
  lv_obj_add_flag(batteryUi.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(batteryUi.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(batteryUi.root, LV_OPA_COVER, 0);

  buildStatusRow(batteryUi.root, batteryUi.status, "CONNECTING", "--");
  batteryUi.updated = batteryUi.status.rightInfo;

  lv_obj_t *eyebrowBar = makePlain(batteryUi.root);
  lv_obj_set_size(eyebrowBar, 22, 2);
  lv_obj_set_pos(eyebrowBar, 40, 58);
  lv_obj_set_style_bg_color(eyebrowBar, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(eyebrowBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(eyebrowBar, 1, 0);

  lv_obj_t *eyebrow = makeLabel(batteryUi.root, "BATTERY LEVELS", &lv_font_montserrat_12, COL_ACCENT);
  lv_obj_set_style_text_letter_space(eyebrow, 2, 0);
  lv_obj_set_pos(eyebrow, 72, 51);

  const int panelW = SCREEN_WIDTH - 80;
  const int panelH = 352;
  lv_obj_t *panel = makeGlass(batteryUi.root, panelW, panelH, 22);
  lv_obj_set_pos(panel, 40, 92);

  makeBatteryGauge(panel, 0, 48, 32);
  makeBatteryGauge(panel, 1, 368, 32);
  makeBatteryGauge(panel, 2, 48, 184);
  makeBatteryGauge(panel, 3, 368, 184);
}

// ---------------------------------------------------------------------------
