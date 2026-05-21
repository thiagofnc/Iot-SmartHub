// Nearby BLE devices screen (landscape + portrait)
// ---------------------------------------------------------------------------
// Visual catalogue of devices the ESP would see on a BLE scan. Real scanning
// is intentionally not wired up yet — the cards are populated from the static
// kNearbyDevices table in app_state.inc so we can tune the UI in isolation.
//
// Up / down in the serial monitor scrolls the list; "rotate" swaps between
// landscape and portrait layouts.
// ---------------------------------------------------------------------------

namespace {

constexpr int kNearbyLandscapeCardW = 688;
constexpr int kNearbyLandscapeCardH = 85;
constexpr int kNearbyPortraitCardW = 392;
constexpr int kNearbyPortraitCardH = 105;

uint32_t rssiColor(int rssi) {
  if (rssi >= -55) return 0x38EF7D;   // strong — green
  if (rssi >= -70) return 0x7CD4F2;   // good — cyan
  if (rssi >= -82) return 0xFFB86B;   // fair — amber
  return 0xE06060;                    // weak — red
}

int rssiBarsLit(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -70) return 3;
  if (rssi >= -82) return 2;
  return 1;
}

// Rough BLE distance estimate via the log-distance path-loss model.
// When the device advertises TX power we treat (TX - 41 dB) as the calibrated
// signal at 1 m (2.4 GHz free-space loss). Otherwise we fall back to the
// common -59 dBm BLE assumption. n = 2.5 is a reasonable indoor exponent.
// This is rough — RSSI varies wildly with antenna orientation and obstacles.
float estimateDistanceMeters(int rssi, int txPower) {
  if (rssi >= 0) return -1.0f;
  const float refPower = (txPower == INT_MIN) ? -59.0f : ((float)txPower - 41.0f);
  const float n = 2.5f;
  return powf(10.0f, (refPower - (float)rssi) / (10.0f * n));
}

void formatDistance(char *out, size_t outSize, float meters) {
  if (meters < 0.0f) { snprintf(out, outSize, "--"); return; }
  if (meters < 10.0f) {
    snprintf(out, outSize, "~%.1f m", meters);
  } else if (meters < 100.0f) {
    snprintf(out, outSize, "~%.0f m", meters);
  } else {
    snprintf(out, outSize, "far");
  }
}

void styleChip(lv_obj_t *chip, const char *text, uint32_t accent) {
  lv_label_set_text(chip, text);
  lv_obj_set_style_text_font(chip, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(chip, hex(accent), 0);
  lv_obj_set_style_bg_color(chip, hex(accent), 0);
  lv_obj_set_style_bg_opa(chip, LV_OPA_20, 0);
  lv_obj_set_style_border_width(chip, 1, 0);
  lv_obj_set_style_border_color(chip, hex(accent), 0);
  lv_obj_set_style_border_opa(chip, LV_OPA_50, 0);
  lv_obj_set_style_radius(chip, 9, 0);
  lv_obj_set_style_pad_left(chip, 8, 0);
  lv_obj_set_style_pad_right(chip, 8, 0);
  lv_obj_set_style_pad_top(chip, 2, 0);
  lv_obj_set_style_pad_bottom(chip, 2, 0);
}

// Build a single device card. The card is a child of the scrollable list,
// flex-layout-positioned by the parent so we only set its size here.
void makeNearbyCard(NearbyCardUi &ui, lv_obj_t *parent, const NearbyDevice &dev,
                    int cardW, int cardH, bool landscape) {
  ui.card = lv_obj_create(parent);
  lv_obj_remove_style_all(ui.card);
  lv_obj_set_size(ui.card, cardW, cardH);
  lv_obj_clear_flag(ui.card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui.card, hex(0x141A2E), 0);
  lv_obj_set_style_bg_opa(ui.card, LV_OPA_80, 0);
  lv_obj_set_style_radius(ui.card, 14, 0);
  lv_obj_set_style_border_width(ui.card, 1, 0);
  lv_obj_set_style_border_color(ui.card, hex(COL_GLASS_STROKE), 0);
  lv_obj_set_style_border_opa(ui.card, LV_OPA_80, 0);

  // Tiny accent strip on the left edge — a quick visual hook tying the card
  // to its vendor accent colour without flooding the whole tile.
  lv_obj_t *accent = makePlain(ui.card);
  lv_obj_set_size(accent, 3, cardH - 24);
  lv_obj_set_pos(accent, 0, 12);
  lv_obj_set_style_bg_color(accent, hex(dev.accent), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(accent, 2, 0);

  // Vendor avatar (tinted circle + icon)
  const int avatarSize = landscape ? 52 : 48;
  const int avatarX = landscape ? 14 : 12;
  const int avatarY = landscape ? 14 : 14;
  ui.avatar = makePlain(ui.card);
  lv_obj_set_size(ui.avatar, avatarSize, avatarSize);
  lv_obj_set_pos(ui.avatar, avatarX, avatarY);
  lv_obj_set_style_radius(ui.avatar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui.avatar, hex(dev.accent), 0);
  lv_obj_set_style_bg_opa(ui.avatar, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.avatar, 1, 0);
  lv_obj_set_style_border_color(ui.avatar, hex(dev.accent), 0);
  lv_obj_set_style_border_opa(ui.avatar, LV_OPA_60, 0);

  ui.avatarIcon = makeLabel(ui.avatar, dev.icon, &lv_font_montserrat_20, dev.accent);
  lv_obj_center(ui.avatarIcon);

  // Body column (name + meta + chip row)
  const int bodyX = avatarX + avatarSize + 14;
  const int rightColW = landscape ? 168 : 110;
  const int bodyW = cardW - bodyX - rightColW - 6;

  ui.name = makeLabel(ui.card, dev.name.c_str(), landscape ? &lv_font_montserrat_18 : &lv_font_montserrat_16, COL_INK);
  lv_label_set_long_mode(ui.name, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(ui.name, bodyW, landscape ? 24 : 22);
  lv_obj_set_pos(ui.name, bodyX, landscape ? 6 : 8);

  char metaBuf[80];
  if (dev.companyId != 0x0000) {
    snprintf(metaBuf, sizeof(metaBuf), "%s  /  %s  /  0x%04X",
             dev.mac.c_str(), dev.vendor.c_str(), dev.companyId);
  } else {
    snprintf(metaBuf, sizeof(metaBuf), "%s  /  %s  /  no CID",
             dev.mac.c_str(), dev.vendor.c_str());
  }
  ui.meta = makeLabel(ui.card, metaBuf, &lv_font_montserrat_12, COL_INK_3);
  lv_label_set_long_mode(ui.meta, LV_LABEL_LONG_DOT);
  lv_obj_set_size(ui.meta, bodyW, 16);
  lv_obj_set_style_text_letter_space(ui.meta, 0, 0);
  lv_obj_set_pos(ui.meta, bodyX, landscape ? 34 : 36);

  // Chip row (flex layout, gap 6)
  ui.chipRow = makePlain(ui.card);
  lv_obj_set_size(ui.chipRow, bodyW, 20);
  lv_obj_set_pos(ui.chipRow, bodyX, landscape ? 56 : 60);
  lv_obj_set_flex_flow(ui.chipRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(ui.chipRow, 6, 0);
  lv_obj_set_style_pad_all(ui.chipRow, 0, 0);
  lv_obj_clear_flag(ui.chipRow, LV_OBJ_FLAG_SCROLLABLE);

  if (dev.serviceCount == 0) {
    lv_obj_t *empty = lv_label_create(ui.chipRow);
    lv_label_set_text(empty, "no advertised services");
    lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(empty, hex(COL_INK_4), 0);
  }
  for (int i = 0; i < dev.serviceCount && i < kNearbyMaxServices; ++i) {
    if (dev.services[i].isEmpty()) continue;
    ui.chips[i] = lv_label_create(ui.chipRow);
    styleChip(ui.chips[i], dev.services[i].c_str(), dev.accent);
  }

  // Right column: estimated distance + signal-strength bars.
  const int rightX = cardW - rightColW + 4;

  char distBuf[16];
  formatDistance(distBuf, sizeof(distBuf),
                 estimateDistanceMeters(dev.rssi, dev.txPower));
  ui.distance = makeLabel(ui.card, distBuf,
                          landscape ? &lv_font_montserrat_18 : &lv_font_montserrat_16,
                          rssiColor(dev.rssi));
  lv_obj_set_size(ui.distance, rightColW - 14, landscape ? 24 : 22);
  lv_obj_set_style_text_align(ui.distance, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(ui.distance, rightX, landscape ? 10 : 12);

  // 4 vertical signal bars, ascending heights, anchored to a common baseline
  // so they look like a phone-style signal indicator.
  const int barW = 5;
  const int barGap = 3;
  const int barsBaseY = landscape ? 60 : 64;
  const int barHeights[4] = {6, 10, 14, 18};
  const int litCount = rssiBarsLit(dev.rssi);
  const uint32_t litCol = rssiColor(dev.rssi);
  const int barsRightX = cardW - 14;
  const int barsTotalW = 4 * barW + 3 * barGap;
  for (int i = 0; i < 4; ++i) {
    ui.rssiBars[i] = makePlain(ui.card);
    lv_obj_set_size(ui.rssiBars[i], barW, barHeights[i]);
    lv_obj_set_pos(ui.rssiBars[i],
                   barsRightX - barsTotalW + i * (barW + barGap),
                   barsBaseY - barHeights[i]);
    lv_obj_set_style_radius(ui.rssiBars[i], 2, 0);
    lv_obj_set_style_bg_color(ui.rssiBars[i], hex(litCol), 0);
    lv_obj_set_style_bg_opa(ui.rssiBars[i], i < litCount ? LV_OPA_COVER : LV_OPA_30, 0);
  }
}

void buildNearbyScreenCommon(NearbyUi &ui, lv_obj_t *parent, bool landscape,
                             int screenW, int screenH) {
  ui.root = makePlain(parent);
  lv_obj_set_size(ui.root, screenW, screenH);
  lv_obj_set_pos(ui.root, 0, 0);
  lv_obj_add_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(ui.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);

  buildStatusRow(ui.root, ui.status, "CONNECTING", "BLE / SCAN");
  if (!landscape) {
    lv_obj_t *statusRow = lv_obj_get_parent(ui.status.dot);
    lv_obj_set_size(statusRow, screenW - 56, 22);
    lv_obj_set_pos(statusRow, 28, 20);
  }

  // Eyebrow header — short accent bar + label, matching the battery screen.
  const int eyebrowX = landscape ? 40 : 36;
  lv_obj_t *eyebrowBar = makePlain(ui.root);
  lv_obj_set_size(eyebrowBar, 22, 2);
  lv_obj_set_pos(eyebrowBar, eyebrowX, 58);
  lv_obj_set_style_bg_color(eyebrowBar, hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(eyebrowBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(eyebrowBar, 1, 0);

  lv_obj_t *eyebrow = makeLabel(ui.root, "NEARBY DEVICES", &lv_font_montserrat_12, COL_ACCENT);
  lv_obj_set_style_text_letter_space(eyebrow, 2, 0);
  lv_obj_set_pos(eyebrow, eyebrowX + 32, 51);

  char countBuf[40];
  snprintf(countBuf, sizeof(countBuf), "%d DEVICES  /  BLE SCAN",
           nearbyDeviceCount);
  ui.headerCount = makeLabel(ui.root, countBuf, &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_letter_space(ui.headerCount, 1, 0);
  if (landscape) {
    lv_obj_set_size(ui.headerCount, 280, 14);
    lv_obj_set_style_text_align(ui.headerCount, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(ui.headerCount, screenW - 280 - 40, 53);
  } else {
    lv_obj_set_size(ui.headerCount, screenW - 80, 14);
    lv_obj_set_style_text_align(ui.headerCount, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(ui.headerCount, 40, 74);
  }

  // Outer glass card + inner scrollable list.
  const int panelX = landscape ? 40 : 28;
  const int panelY = landscape ? 92 : 102;
  const int panelW = screenW - 2 * panelX;
  const int panelH = screenH - panelY - 24;
  lv_obj_t *panel = makeGlass(ui.root, panelW, panelH, 22);
  lv_obj_set_pos(panel, panelX, panelY);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  ui.list = lv_obj_create(panel);
  lv_obj_remove_style_all(ui.list);
  lv_obj_set_size(ui.list, panelW - 4, panelH - 4);
  lv_obj_set_pos(ui.list, 2, 2);
  lv_obj_set_style_radius(ui.list, 20, 0);
  lv_obj_set_style_clip_corner(ui.list, true, 0);
  lv_obj_add_flag(ui.list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(ui.list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(ui.list, LV_SCROLLBAR_MODE_ON);
  lv_obj_set_style_bg_color(ui.list, hex(COL_ACCENT), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(ui.list, LV_OPA_60, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(ui.list, 2, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(ui.list, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_pad_right(ui.list, 2, LV_PART_SCROLLBAR);
  lv_obj_set_flex_flow(ui.list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(ui.list, 16, 0);
  lv_obj_set_style_pad_row(ui.list, 10, 0);

  const int cardW = landscape ? kNearbyLandscapeCardW : kNearbyPortraitCardW;
  const int cardH = landscape ? kNearbyLandscapeCardH : kNearbyPortraitCardH;
  for (int i = 0; i < nearbyDeviceCount && i < kNearbyMaxDeviceCount; ++i) {
    makeNearbyCard(ui.cards[i], ui.list, nearbyDevices[i], cardW, cardH, landscape);
  }
}

void refreshNearbyList(NearbyUi &ui) {
  if (!ui.list) return;
  lv_obj_clean(ui.list);

  const bool landscape = (&ui == &nearbyUi);
  const int cardW = landscape ? kNearbyLandscapeCardW : kNearbyPortraitCardW;
  const int cardH = landscape ? kNearbyLandscapeCardH : kNearbyPortraitCardH;
  for (int i = 0; i < nearbyDeviceCount && i < kNearbyMaxDeviceCount; ++i) {
    makeNearbyCard(ui.cards[i], ui.list, nearbyDevices[i], cardW, cardH, landscape);
  }

  char countBuf[40];
  snprintf(countBuf, sizeof(countBuf), "%d DEVICES  /  BLE SCAN", nearbyDeviceCount);
  if (ui.headerCount) lv_label_set_text(ui.headerCount, countBuf);
}

void refreshNearbyScreens() {
  refreshNearbyList(nearbyUi);
  refreshNearbyList(nearbyPortraitUi);
}

}  // namespace

void buildNearbyScreen(lv_obj_t *parent) {
  buildNearbyScreenCommon(nearbyUi, parent, /*landscape=*/true,
                          SCREEN_WIDTH, SCREEN_HEIGHT);
}

void buildNearbyPortraitScreen(lv_obj_t *parent) {
  buildNearbyScreenCommon(nearbyPortraitUi, parent, /*landscape=*/false,
                          PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
}

// Scroll the active nearby list. dir = -1 scrolls up (toward earlier devices),
// dir = +1 scrolls down. No-op when not on a nearby screen.
bool nearbyScrollBy(int dir) {
  lv_obj_t *list = nullptr;
  int step = 0;
  if (currentScreen == ScreenIndex::NearbyDevices) {
    list = nearbyUi.list;
    step = kNearbyLandscapeCardH + 10;
  } else if (currentScreen == ScreenIndex::NearbyDevicesPortrait) {
    list = nearbyPortraitUi.list;
    step = kNearbyPortraitCardH + 10;
  }
  if (!list) return false;
  lv_obj_scroll_by(list, 0, -dir * step, LV_ANIM_ON);
  return true;
}

// ---------------------------------------------------------------------------
