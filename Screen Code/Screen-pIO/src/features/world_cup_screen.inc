// World Cup screen (landscape + portrait)
// ---------------------------------------------------------------------------
// Single hero panel: stage header, two flags + team labels, score (always
// shown as digits, 0-0 before kickoff), and a status pill with kickoff time
// + venue beneath it. Same data drives both orientations.
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t kWcAccent = 0x7CD4F2;
constexpr uint32_t kWcAmber = 0xFFB86B;
constexpr uint32_t kWcLive = 0xE94E77;
constexpr uint32_t kWcCardFill = 0x141A2E;

// Tinted fallback used when a flag image isn't (yet) available — a rounded
// rectangle with the TLA centered in it.
lv_obj_t *makeFlagFallback(lv_obj_t *parent, int w, int h) {
  lv_obj_t *box = makePlain(parent);
  lv_obj_set_size(box, w, h);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_bg_color(box, hex(0x223052), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, hex(COL_GLASS_STROKE_STRONG), 0);
  lv_obj_set_style_border_opa(box, LV_OPA_70, 0);
  lv_obj_t *txt = makeLabel(box, "?", &lv_font_montserrat_22, COL_INK);
  lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(txt, w);
  lv_obj_align(txt, LV_ALIGN_CENTER, 0, 0);
  return box;
}

}  // namespace

// Sync the lv_img widget to whichever flag buffer is currently loaded.
// Shows the fallback box (with TLA text) instead when the buffer is empty.
void applyFlagToImg(lv_obj_t *img, lv_obj_t *fallback, lv_obj_t *fallbackText,
                    WorldCupFlagBuffer &buf, const String &tla) {
  if (buf.data && buf.size > 0) {
    buf.dsc.header.always_zero = 0;
    buf.dsc.header.w = 0;          // PNG decoder fills these
    buf.dsc.header.h = 0;
    buf.dsc.header.cf = LV_IMG_CF_RAW_ALPHA;
    buf.dsc.data = buf.data;
    buf.dsc.data_size = buf.size;
    lv_img_set_src(img, &buf.dsc);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    if (fallback) lv_obj_add_flag(fallback, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (fallback) {
      if (fallbackText) {
        lv_label_set_text(fallbackText, tla.isEmpty() ? "?" : tla.c_str());
      }
      lv_obj_clear_flag(fallback, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  }
}

void buildWorldCupScreenCommon(WorldCupUi &ui, lv_obj_t *parent, bool landscape,
                               int screenW, int screenH) {
  ui.root = makePlain(parent);
  lv_obj_set_size(ui.root, screenW, screenH);
  lv_obj_set_pos(ui.root, 0, 0);
  lv_obj_add_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(ui.root, hex(COL_BG_1), 0);
  lv_obj_set_style_bg_opa(ui.root, LV_OPA_COVER, 0);

  buildStatusRow(ui.root, ui.status, "CONNECTING", "FOOTBALL-DATA");
  if (!landscape) {
    lv_obj_t *statusRow = lv_obj_get_parent(ui.status.dot);
    lv_obj_set_size(statusRow, screenW - 56, 22);
    lv_obj_set_pos(statusRow, 28, 20);
  }

  const int eyebrowX = landscape ? 40 : 36;
  lv_obj_t *eyebrowBar = makePlain(ui.root);
  lv_obj_set_size(eyebrowBar, 22, 2);
  lv_obj_set_pos(eyebrowBar, eyebrowX, 58);
  lv_obj_set_style_bg_color(eyebrowBar, hex(kWcAccent), 0);
  lv_obj_set_style_bg_opa(eyebrowBar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(eyebrowBar, 1, 0);

  ui.headerLabel = makeLabel(ui.root, "WORLD CUP", &lv_font_montserrat_12, kWcAccent);
  lv_obj_set_style_text_letter_space(ui.headerLabel, 2, 0);
  lv_obj_set_pos(ui.headerLabel, eyebrowX + 32, 51);

  ui.updated = makeLabel(ui.root, "AWAITING DATA", &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_letter_space(ui.updated, 1, 0);
  lv_obj_set_size(ui.updated, 300, 14);
  lv_obj_set_style_text_align(ui.updated, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(ui.updated, screenW - 300 - (landscape ? 40 : 28), 53);

  // ---- Single hero panel (fills the available area) ----------------------
  const int panelX = landscape ? 40 : 28;
  const int heroY = 92;
  const int heroW = screenW - 2 * panelX;
  const int heroH = screenH - heroY - 24;
  ui.heroPanel = makeGlass(ui.root, heroW, heroH, 22);
  lv_obj_set_pos(ui.heroPanel, panelX, heroY);

  ui.stageLabel = makeLabel(ui.heroPanel, "AWAITING MATCH",
                            &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_style_text_letter_space(ui.stageLabel, 2, 0);
  lv_obj_set_style_text_align(ui.stageLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_size(ui.stageLabel, heroW - 40, 18);
  lv_obj_set_pos(ui.stageLabel, 20, 22);

  const int flagW = 160;
  const int flagH = 104;

  // Helper for creating + styling the flag widget pair (image + fallback box).
  auto makeFlagPair = [&](lv_obj_t *&img, lv_obj_t *&fallback, int x, int y) {
    img = lv_img_create(ui.heroPanel);
    lv_obj_set_size(img, flagW, flagH);
    lv_obj_set_pos(img, x, y);
    lv_obj_set_style_radius(img, 6, 0);
    lv_obj_set_style_clip_corner(img, true, 0);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    fallback = makeFlagFallback(ui.heroPanel, flagW, flagH);
    lv_obj_set_pos(fallback, x, y);
  };

  auto makeTeamLabels = [&](lv_obj_t *&shortLbl, lv_obj_t *&fullLbl,
                            int x, int y, int width) {
    shortLbl = makeLabel(ui.heroPanel, "TBD", &lv_font_montserrat_22, COL_INK);
    lv_obj_set_style_text_align(shortLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(shortLbl, 2, 0);
    lv_obj_set_size(shortLbl, width, 26);
    lv_obj_set_pos(shortLbl, x, y);

    fullLbl = makeLabel(ui.heroPanel, "", &lv_font_montserrat_12, COL_INK_3);
    lv_label_set_long_mode(fullLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(fullLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(fullLbl, width, 16);
    lv_obj_set_pos(fullLbl, x, y + 28);
  };

  auto makeStatusPillAndDetails = [&](int pillY, int kickoffY, int venueY) {
    ui.statusPill = makePlain(ui.heroPanel);
    lv_obj_set_size(ui.statusPill, 200, 28);
    lv_obj_set_pos(ui.statusPill, (heroW - 200) / 2, pillY);
    lv_obj_set_style_radius(ui.statusPill, 14, 0);
    lv_obj_set_style_bg_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_bg_opa(ui.statusPill, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.statusPill, 1, 0);
    lv_obj_set_style_border_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_border_opa(ui.statusPill, LV_OPA_60, 0);

    ui.statusPillLabel = makeLabel(ui.statusPill, "SCHEDULED",
                                   &lv_font_montserrat_14, kWcAccent);
    lv_obj_set_style_text_letter_space(ui.statusPillLabel, 2, 0);
    lv_obj_set_style_text_align(ui.statusPillLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui.statusPillLabel, 200);
    lv_obj_align(ui.statusPillLabel, LV_ALIGN_CENTER, 0, 0);

    ui.kickoffValue = makeLabel(ui.heroPanel, "--",
                                &lv_font_montserrat_16, COL_INK);
    lv_obj_set_style_text_align(ui.kickoffValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ui.kickoffValue, 1, 0);
    lv_obj_set_size(ui.kickoffValue, heroW - 40, 22);
    lv_obj_set_pos(ui.kickoffValue, 20, kickoffY);

    ui.venueValue = makeLabel(ui.heroPanel, "--",
                              &lv_font_montserrat_14, COL_INK_2);
    lv_label_set_long_mode(ui.venueValue, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui.venueValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.venueValue, heroW - 40, 20);
    lv_obj_set_pos(ui.venueValue, 20, venueY);
  };

  auto makeScoreRow = [&](int y) {
    // Big "0 - 0" digits in 48pt so the score reads from across the room.
    const int cx = heroW / 2;
    const int digitW = 64;
    const int digitH = 64;
    ui.scoreHome = makeLabel(ui.heroPanel, "0", &lv_font_montserrat_48, COL_INK);
    lv_obj_set_style_text_align(ui.scoreHome, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_size(ui.scoreHome, digitW, digitH);
    lv_obj_set_pos(ui.scoreHome, cx - digitW - 18, y);

    ui.scoreSep = makeLabel(ui.heroPanel, "-", &lv_font_montserrat_48, COL_INK_3);
    lv_obj_set_style_text_align(ui.scoreSep, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.scoreSep, 36, digitH);
    lv_obj_set_pos(ui.scoreSep, cx - 18, y);

    ui.scoreAway = makeLabel(ui.heroPanel, "0", &lv_font_montserrat_48, COL_INK);
    lv_obj_set_style_text_align(ui.scoreAway, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_size(ui.scoreAway, digitW, digitH);
    lv_obj_set_pos(ui.scoreAway, cx + 18, y);
  };

  if (landscape) {
    // Three columns: home flag | score+status+details | away flag
    const int homeX = 44;
    const int awayX = heroW - 44 - flagW;
    const int flagsY = 64;

    makeFlagPair(ui.flagHome, ui.flagHomeFallback, homeX, flagsY);
    makeFlagPair(ui.flagAway, ui.flagAwayFallback, awayX, flagsY);
    makeTeamLabels(ui.teamHomeShort, ui.teamHomeFull, homeX, 180, flagW);
    makeTeamLabels(ui.teamAwayShort, ui.teamAwayFull, awayX, 180, flagW);
    makeScoreRow(58);
    makeStatusPillAndDetails(/*pillY=*/156, /*kickoffY=*/200, /*venueY=*/230);
  } else {
    // Portrait: vertical flow A flag → score+status → B flag.
    const int flagCenterX = (heroW - flagW) / 2;

    makeFlagPair(ui.flagHome, ui.flagHomeFallback, flagCenterX, 60);
    makeTeamLabels(ui.teamHomeShort, ui.teamHomeFull, 20, 178, heroW - 40);

    makeScoreRow(228);
    makeStatusPillAndDetails(/*pillY=*/308, /*kickoffY=*/348, /*venueY=*/376);

    makeFlagPair(ui.flagAway, ui.flagAwayFallback, flagCenterX, 414);
    makeTeamLabels(ui.teamAwayShort, ui.teamAwayFull, 20, 532, heroW - 40);
  }
}

void buildWorldCupScreen(lv_obj_t *parent) {
  buildWorldCupScreenCommon(wcUi, parent, /*landscape=*/true,
                            SCREEN_WIDTH, SCREEN_HEIGHT);
}

void buildWorldCupPortraitScreen(lv_obj_t *parent) {
  buildWorldCupScreenCommon(wcPortraitUi, parent, /*landscape=*/false,
                            PORTRAIT_WIDTH, PORTRAIT_HEIGHT);
}

// Header line showing only the tournament stage (group / KO round).
String wcStageHeader(const WorldCupMatch &m) {
  if (m.stage.isEmpty()) return "FIXTURE";
  String stg = m.stage;
  stg.replace('_', ' ');
  stg.toUpperCase();
  return stg;
}

void wcApplyTeamToUi(WorldCupUi &ui, const WorldCupMatch &m) {
  lv_label_set_text(ui.teamHomeShort,
                    m.home.tla.isEmpty() ? (m.home.shortName.isEmpty() ? "TBD" : m.home.shortName.c_str())
                                         : m.home.tla.c_str());
  lv_label_set_text(ui.teamHomeFull,
                    m.home.name.isEmpty() ? "" : m.home.name.c_str());
  lv_label_set_text(ui.teamAwayShort,
                    m.away.tla.isEmpty() ? (m.away.shortName.isEmpty() ? "TBD" : m.away.shortName.c_str())
                                         : m.away.tla.c_str());
  lv_label_set_text(ui.teamAwayFull,
                    m.away.name.isEmpty() ? "" : m.away.name.c_str());

  applyFlagToImg(ui.flagHome, ui.flagHomeFallback,
                 lv_obj_get_child(ui.flagHomeFallback, 0), wcFlagHome, m.home.tla);
  applyFlagToImg(ui.flagAway, ui.flagAwayFallback,
                 lv_obj_get_child(ui.flagAwayFallback, 0), wcFlagAway, m.away.tla);
}

void wcApplyScoreAndStatus(WorldCupUi &ui, const WorldCupMatch &m) {
  // Always show a numeric score — pre-kickoff matches read as 0 - 0.
  const int home = (m.home.score >= 0) ? m.home.score : 0;
  const int away = (m.away.score >= 0) ? m.away.score : 0;
  char hBuf[8], aBuf[8];
  snprintf(hBuf, sizeof(hBuf), "%d", home);
  snprintf(aBuf, sizeof(aBuf), "%d", away);
  lv_label_set_text(ui.scoreHome, hBuf);
  lv_label_set_text(ui.scoreAway, aBuf);
  lv_label_set_text(ui.scoreSep, "-");

  String pillText;
  uint32_t pillColor = kWcAccent;
  if (m.status == "IN_PLAY") {
    pillText = m.minute > 0 ? (String("LIVE  /  ") + m.minute + "'") : String("LIVE");
    pillColor = kWcLive;
  } else if (m.status == "PAUSED") {
    pillText = "HALF TIME";
    pillColor = kWcAmber;
  } else if (m.status == "FINISHED") {
    pillText = "FULL TIME";
    pillColor = COL_INK_3;
  } else if (m.status == "POSTPONED" || m.status == "SUSPENDED") {
    pillText = m.status;
    pillColor = kWcAmber;
  } else {
    pillText = "SCHEDULED";
    pillColor = kWcAccent;
  }
  pillText.toUpperCase();
  lv_label_set_text(ui.statusPillLabel, pillText.c_str());
  lv_obj_set_style_bg_color(ui.statusPill, hex(pillColor), 0);
  lv_obj_set_style_border_color(ui.statusPill, hex(pillColor), 0);
  lv_obj_set_style_text_color(ui.statusPillLabel, hex(pillColor), 0);
}

void wcApplyDetailsToUi(WorldCupUi &ui, const WorldCupMatch &m) {
  // Kickoff: local time using weatherData.timezoneOffset (which we already
  // sync via the weather fetch) so the dashboard reads in your wall clock.
  if (m.utcDate > 0) {
    char tBuf[24];
    const time_t t = static_cast<time_t>(m.utcDate + weatherData.timezoneOffset);
    struct tm tm; gmtime_r(&t, &tm);
    int h = tm.tm_hour;
    const char *am = (h >= 12) ? "PM" : "AM";
    int h12 = h % 12; if (h12 == 0) h12 = 12;
    const long localNow = static_cast<long>(time(nullptr)) + weatherData.timezoneOffset;
    const long localKick = m.utcDate + weatherData.timezoneOffset;
    const long daysAhead = (localKick / 86400L) - (localNow / 86400L);
    if (daysAhead == 0) {
      snprintf(tBuf, sizeof(tBuf), "TODAY %d:%02d %s", h12, tm.tm_min, am);
    } else if (daysAhead == 1) {
      snprintf(tBuf, sizeof(tBuf), "TMRW %d:%02d %s", h12, tm.tm_min, am);
    } else {
      static const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
      snprintf(tBuf, sizeof(tBuf), "%s %d:%02d %s", days[tm.tm_wday], h12, tm.tm_min, am);
    }
    lv_label_set_text(ui.kickoffValue, tBuf);
  } else {
    lv_label_set_text(ui.kickoffValue, "--");
  }
  lv_label_set_text(ui.venueValue, m.venue.isEmpty() ? "--" : m.venue.c_str());
  lv_label_set_text(ui.stageLabel, wcStageHeader(m).c_str());
}

void updateWorldCupUiOne(WorldCupUi &ui) {
  if (!ui.root) return;
  if (!wcMatch.valid) {
    const char *msg = wcMatch.lastError.isEmpty() ? "AWAITING DATA" : wcMatch.lastError.c_str();
    lv_label_set_text(ui.stageLabel, msg);
    lv_label_set_text(ui.teamHomeShort, "--");
    lv_label_set_text(ui.teamHomeFull, "");
    lv_label_set_text(ui.teamAwayShort, "--");
    lv_label_set_text(ui.teamAwayFull, "");
    lv_label_set_text(ui.scoreHome, "0");
    lv_label_set_text(ui.scoreAway, "0");
    lv_label_set_text(ui.scoreSep, "-");
    lv_label_set_text(ui.statusPillLabel, "--");
    lv_label_set_text(ui.kickoffValue, "--");
    lv_label_set_text(ui.venueValue, "--");
    lv_label_set_text(ui.updated, msg);
    applyFlagToImg(ui.flagHome, ui.flagHomeFallback,
                   lv_obj_get_child(ui.flagHomeFallback, 0), wcFlagHome, "");
    applyFlagToImg(ui.flagAway, ui.flagAwayFallback,
                   lv_obj_get_child(ui.flagAwayFallback, 0), wcFlagAway, "");
    return;
  }
  wcApplyTeamToUi(ui, wcMatch);
  wcApplyScoreAndStatus(ui, wcMatch);
  wcApplyDetailsToUi(ui, wcMatch);

  unsigned long elapsed = (millis() - wcMatch.lastFetchMs) / 1000UL;
  char uBuf[24];
  if (elapsed < 60) snprintf(uBuf, sizeof(uBuf), "UPDATED JUST NOW");
  else snprintf(uBuf, sizeof(uBuf), "UPDATED %luM AGO", elapsed / 60);
  lv_label_set_text(ui.updated, uBuf);
}

void updateWorldCupUi() {
  updateWorldCupUiOne(wcUi);
  updateWorldCupUiOne(wcPortraitUi);
}

// Move the pending data produced by the worker task into the live UI state.
// Runs on the main thread, called when wcUiDirty is set. When the pending
// match isn't valid (i.e. the fetch failed) we only refresh the UI so the
// new error message becomes visible, without clobbering any prior good data.
void applyWcUiUpdate() {
  if (wcPendingMatch.valid) {
    wcMatch = wcPendingMatch;
  }

  // Swap in new flag PNG buffers, freeing the old ones.
  auto swap = [](WorldCupFlagBuffer &dst, uint8_t *&srcData, size_t &srcSize, String &srcIso) {
    if (srcData) {
      if (dst.data) heap_caps_free(dst.data);
      dst.data = srcData;
      dst.size = srcSize;
      dst.loadedIso = srcIso;
      srcData = nullptr;
      srcSize = 0;
    } else if (!srcIso.isEmpty() && srcIso == dst.loadedIso) {
      // Worker chose to keep the existing flag — nothing to do.
    } else if (srcIso.isEmpty()) {
      if (dst.data) heap_caps_free(dst.data);
      dst.data = nullptr;
      dst.size = 0;
      dst.loadedIso = "";
    }
  };
  swap(wcFlagHome, wcPendingFlagHomeData, wcPendingFlagHomeSize, wcPendingFlagHomeIso);
  swap(wcFlagAway, wcPendingFlagAwayData, wcPendingFlagAwaySize, wcPendingFlagAwayIso);

  wcUiDirty = false;
  updateWorldCupUi();
}

// ---------------------------------------------------------------------------
