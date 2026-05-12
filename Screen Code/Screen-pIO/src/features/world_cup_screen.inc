// World Cup screen (landscape + portrait)
// ---------------------------------------------------------------------------
// Hero panel: flag + team labels + score + live-status pill. Details panel:
// four label/value cells covering kickoff, venue, stage, referee. Same data
// drives both orientations; one set of flag PNG buffers is shared.
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t kWcAccent = 0x7CD4F2;
constexpr uint32_t kWcAmber = 0xFFB86B;
constexpr uint32_t kWcLive = 0xE94E77;
constexpr uint32_t kWcCardFill = 0x141A2E;

// Build a label-over-value column inside the details panel.
struct WcDetailCell {
  lv_obj_t *label;
  lv_obj_t *value;
};

WcDetailCell makeDetailCell(lv_obj_t *parent, const char *labelText, int x, int y,
                            int width, bool landscape) {
  WcDetailCell cell;
  cell.label = makeLabel(parent, labelText, &lv_font_montserrat_12, COL_INK_3);
  lv_obj_set_style_text_letter_space(cell.label, 2, 0);
  lv_obj_set_size(cell.label, width, 14);
  lv_obj_set_pos(cell.label, x, y);

  cell.value = makeLabel(parent, "--",
                         landscape ? &lv_font_montserrat_18 : &lv_font_montserrat_16, COL_INK);
  lv_label_set_long_mode(cell.value, LV_LABEL_LONG_DOT);
  lv_obj_set_size(cell.value, width, landscape ? 24 : 22);
  lv_obj_set_pos(cell.value, x, y + 20);
  return cell;
}

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

  // ---- Hero panel ----------------------------------------------------------
  const int panelX = landscape ? 40 : 28;
  const int heroY = landscape ? 92 : 92;
  const int heroW = screenW - 2 * panelX;
  const int heroH = landscape ? 230 : 470;
  ui.heroPanel = makeGlass(ui.root, heroW, heroH, 22);
  lv_obj_set_pos(ui.heroPanel, panelX, heroY);

  ui.stageLabel = makeLabel(ui.heroPanel, "AWAITING MATCH",
                            &lv_font_montserrat_14, COL_INK_2);
  lv_obj_set_style_text_letter_space(ui.stageLabel, 2, 0);
  lv_obj_set_style_text_align(ui.stageLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_size(ui.stageLabel, heroW - 40, 18);
  lv_obj_set_pos(ui.stageLabel, 20, 16);

  const int flagW = 160;
  const int flagH = 104;
  const int teamShortFont = landscape ? 22 : 22;

  if (landscape) {
    // Three columns: home flag | score | away flag
    const int homeX = 44;
    const int awayX = heroW - 44 - flagW;
    const int flagsY = 48;

    ui.flagHome = lv_img_create(ui.heroPanel);
    lv_obj_set_size(ui.flagHome, flagW, flagH);
    lv_obj_set_pos(ui.flagHome, homeX, flagsY);
    lv_obj_set_style_radius(ui.flagHome, 6, 0);
    lv_obj_set_style_clip_corner(ui.flagHome, true, 0);
    lv_obj_add_flag(ui.flagHome, LV_OBJ_FLAG_HIDDEN);

    ui.flagHomeFallback = makeFlagFallback(ui.heroPanel, flagW, flagH);
    lv_obj_set_pos(ui.flagHomeFallback, homeX, flagsY);

    ui.flagAway = lv_img_create(ui.heroPanel);
    lv_obj_set_size(ui.flagAway, flagW, flagH);
    lv_obj_set_pos(ui.flagAway, awayX, flagsY);
    lv_obj_set_style_radius(ui.flagAway, 6, 0);
    lv_obj_set_style_clip_corner(ui.flagAway, true, 0);
    lv_obj_add_flag(ui.flagAway, LV_OBJ_FLAG_HIDDEN);

    ui.flagAwayFallback = makeFlagFallback(ui.heroPanel, flagW, flagH);
    lv_obj_set_pos(ui.flagAwayFallback, awayX, flagsY);

    auto makeTeamLabels = [&](lv_obj_t *&shortLbl, lv_obj_t *&fullLbl, int x) {
      shortLbl = makeLabel(ui.heroPanel, "TBD",
                           &lv_font_montserrat_22, COL_INK);
      lv_obj_set_style_text_align(shortLbl, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_letter_space(shortLbl, 2, 0);
      lv_obj_set_size(shortLbl, flagW, 26);
      lv_obj_set_pos(shortLbl, x, 160);

      fullLbl = makeLabel(ui.heroPanel, "",
                          &lv_font_montserrat_12, COL_INK_3);
      lv_label_set_long_mode(fullLbl, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_align(fullLbl, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_size(fullLbl, flagW, 16);
      lv_obj_set_pos(fullLbl, x, 190);
    };
    makeTeamLabels(ui.teamHomeShort, ui.teamHomeFull, homeX);
    makeTeamLabels(ui.teamAwayShort, ui.teamAwayFull, awayX);

    // Score column in the middle.
    const int scoreCenterX = heroW / 2;
    ui.scoreHome = makeLabel(ui.heroPanel, "-", &lv_font_montserrat_22, COL_INK);
    lv_obj_set_style_text_align(ui.scoreHome, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_size(ui.scoreHome, 60, 30);
    lv_obj_set_pos(ui.scoreHome, scoreCenterX - 70, 90);

    ui.scoreSep = makeLabel(ui.heroPanel, "vs", &lv_font_montserrat_14, COL_INK_3);
    lv_obj_set_style_text_align(ui.scoreSep, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.scoreSep, 24, 18);
    lv_obj_set_pos(ui.scoreSep, scoreCenterX - 12, 96);

    ui.scoreAway = makeLabel(ui.heroPanel, "-", &lv_font_montserrat_22, COL_INK);
    lv_obj_set_size(ui.scoreAway, 60, 30);
    lv_obj_set_pos(ui.scoreAway, scoreCenterX + 10, 90);

    // Status pill anchored below the score area.
    ui.statusPill = makePlain(ui.heroPanel);
    lv_obj_set_size(ui.statusPill, 180, 26);
    lv_obj_set_pos(ui.statusPill, scoreCenterX - 90, 140);
    lv_obj_set_style_radius(ui.statusPill, 13, 0);
    lv_obj_set_style_bg_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_bg_opa(ui.statusPill, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.statusPill, 1, 0);
    lv_obj_set_style_border_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_border_opa(ui.statusPill, LV_OPA_60, 0);

    ui.statusPillLabel = makeLabel(ui.statusPill, "SCHEDULED",
                                   &lv_font_montserrat_12, kWcAccent);
    lv_obj_set_style_text_letter_space(ui.statusPillLabel, 2, 0);
    lv_obj_set_style_text_align(ui.statusPillLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui.statusPillLabel, 180);
    lv_obj_align(ui.statusPillLabel, LV_ALIGN_CENTER, 0, 0);
  } else {
    // Portrait: stack flag-A / score / flag-B.
    const int flagCenterX = (heroW - flagW) / 2;

    ui.flagHome = lv_img_create(ui.heroPanel);
    lv_obj_set_size(ui.flagHome, flagW, flagH);
    lv_obj_set_pos(ui.flagHome, flagCenterX, 50);
    lv_obj_set_style_radius(ui.flagHome, 6, 0);
    lv_obj_set_style_clip_corner(ui.flagHome, true, 0);
    lv_obj_add_flag(ui.flagHome, LV_OBJ_FLAG_HIDDEN);

    ui.flagHomeFallback = makeFlagFallback(ui.heroPanel, flagW, flagH);
    lv_obj_set_pos(ui.flagHomeFallback, flagCenterX, 50);

    ui.teamHomeShort = makeLabel(ui.heroPanel, "TBD",
                                 &lv_font_montserrat_22, COL_INK);
    lv_obj_set_style_text_align(ui.teamHomeShort, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ui.teamHomeShort, 2, 0);
    lv_obj_set_size(ui.teamHomeShort, heroW - 40, 26);
    lv_obj_set_pos(ui.teamHomeShort, 20, 162);

    ui.teamHomeFull = makeLabel(ui.heroPanel, "", &lv_font_montserrat_12, COL_INK_3);
    lv_label_set_long_mode(ui.teamHomeFull, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui.teamHomeFull, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.teamHomeFull, heroW - 40, 16);
    lv_obj_set_pos(ui.teamHomeFull, 20, 192);

    // Score row in the middle.
    ui.scoreHome = makeLabel(ui.heroPanel, "-", &lv_font_montserrat_22, COL_INK);
    lv_obj_set_style_text_align(ui.scoreHome, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_size(ui.scoreHome, 60, 30);
    lv_obj_set_pos(ui.scoreHome, heroW / 2 - 70, 222);

    ui.scoreSep = makeLabel(ui.heroPanel, "vs", &lv_font_montserrat_14, COL_INK_3);
    lv_obj_set_style_text_align(ui.scoreSep, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.scoreSep, 24, 18);
    lv_obj_set_pos(ui.scoreSep, heroW / 2 - 12, 228);

    ui.scoreAway = makeLabel(ui.heroPanel, "-", &lv_font_montserrat_22, COL_INK);
    lv_obj_set_size(ui.scoreAway, 60, 30);
    lv_obj_set_pos(ui.scoreAway, heroW / 2 + 10, 222);

    ui.statusPill = makePlain(ui.heroPanel);
    lv_obj_set_size(ui.statusPill, 180, 26);
    lv_obj_set_pos(ui.statusPill, (heroW - 180) / 2, 268);
    lv_obj_set_style_radius(ui.statusPill, 13, 0);
    lv_obj_set_style_bg_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_bg_opa(ui.statusPill, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.statusPill, 1, 0);
    lv_obj_set_style_border_color(ui.statusPill, hex(kWcAccent), 0);
    lv_obj_set_style_border_opa(ui.statusPill, LV_OPA_60, 0);

    ui.statusPillLabel = makeLabel(ui.statusPill, "SCHEDULED",
                                   &lv_font_montserrat_12, kWcAccent);
    lv_obj_set_style_text_letter_space(ui.statusPillLabel, 2, 0);
    lv_obj_set_style_text_align(ui.statusPillLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui.statusPillLabel, 180);
    lv_obj_align(ui.statusPillLabel, LV_ALIGN_CENTER, 0, 0);

    ui.flagAway = lv_img_create(ui.heroPanel);
    lv_obj_set_size(ui.flagAway, flagW, flagH);
    lv_obj_set_pos(ui.flagAway, flagCenterX, 306);
    lv_obj_set_style_radius(ui.flagAway, 6, 0);
    lv_obj_set_style_clip_corner(ui.flagAway, true, 0);
    lv_obj_add_flag(ui.flagAway, LV_OBJ_FLAG_HIDDEN);

    ui.flagAwayFallback = makeFlagFallback(ui.heroPanel, flagW, flagH);
    lv_obj_set_pos(ui.flagAwayFallback, flagCenterX, 306);

    ui.teamAwayShort = makeLabel(ui.heroPanel, "TBD",
                                 &lv_font_montserrat_22, COL_INK);
    lv_obj_set_style_text_align(ui.teamAwayShort, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ui.teamAwayShort, 2, 0);
    lv_obj_set_size(ui.teamAwayShort, heroW - 40, 26);
    lv_obj_set_pos(ui.teamAwayShort, 20, 416);

    ui.teamAwayFull = makeLabel(ui.heroPanel, "", &lv_font_montserrat_12, COL_INK_3);
    lv_label_set_long_mode(ui.teamAwayFull, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui.teamAwayFull, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(ui.teamAwayFull, heroW - 40, 16);
    lv_obj_set_pos(ui.teamAwayFull, 20, 446);
  }

  // ---- Details panel ------------------------------------------------------
  const int detailsY = heroY + heroH + 16;
  const int detailsW = heroW;
  const int detailsH = landscape ? 120 : 200;
  lv_obj_t *details = makeGlass(ui.root, detailsW, detailsH, 22);
  lv_obj_set_pos(details, panelX, detailsY);

  if (landscape) {
    const int colW = (detailsW - 32) / 4;
    auto kickoff  = makeDetailCell(details, "KICKOFF",  16 + 0 * colW, 20, colW - 8, true);
    auto venue    = makeDetailCell(details, "VENUE",    16 + 1 * colW, 20, colW - 8, true);
    auto stage    = makeDetailCell(details, "STAGE",    16 + 2 * colW, 20, colW - 8, true);
    auto referee  = makeDetailCell(details, "REFEREE",  16 + 3 * colW, 20, colW - 8, true);
    ui.kickoffValue     = kickoff.value;
    ui.venueValue       = venue.value;
    ui.stageDetailValue = stage.value;
    ui.refereeValue     = referee.value;

    // Second row could carry richer info later (matchday, attendance). For
    // now leave room for a future expansion line below each value.
  } else {
    const int rowH = 44;
    auto kickoff  = makeDetailCell(details, "KICKOFF",  20, 18 + 0 * rowH, detailsW - 40, false);
    auto venue    = makeDetailCell(details, "VENUE",    20, 18 + 1 * rowH, detailsW - 40, false);
    auto stage    = makeDetailCell(details, "STAGE",    20, 18 + 2 * rowH, detailsW - 40, false);
    auto referee  = makeDetailCell(details, "REFEREE",  20, 18 + 3 * rowH, detailsW - 40, false);
    ui.kickoffValue     = kickoff.value;
    ui.venueValue       = venue.value;
    ui.stageDetailValue = stage.value;
    ui.refereeValue     = referee.value;
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

// Format the "stage / matchday / group" header line.
String wcStageHeader(const WorldCupMatch &m) {
  String s;
  if (!m.group.isEmpty()) {
    s = m.group;
    s.toUpperCase();
  } else if (!m.stage.isEmpty()) {
    String stg = m.stage;
    stg.replace('_', ' ');
    stg.toUpperCase();
    s = stg;
  }
  if (m.matchday > 0) {
    if (!s.isEmpty()) s += "  /  ";
    s += "MATCHDAY ";
    s += String(m.matchday);
  }
  if (s.isEmpty()) s = "FIXTURE";
  return s;
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
  const bool hasScore = (m.home.score >= 0 && m.away.score >= 0);
  char hBuf[8], aBuf[8];
  if (hasScore) {
    snprintf(hBuf, sizeof(hBuf), "%d", m.home.score);
    snprintf(aBuf, sizeof(aBuf), "%d", m.away.score);
  } else {
    snprintf(hBuf, sizeof(hBuf), "-");
    snprintf(aBuf, sizeof(aBuf), "-");
  }
  lv_label_set_text(ui.scoreHome, hBuf);
  lv_label_set_text(ui.scoreAway, aBuf);
  lv_label_set_text(ui.scoreSep, hasScore ? "-" : "vs");

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
  lv_label_set_text(ui.stageDetailValue, wcStageHeader(m).c_str());
  lv_label_set_text(ui.refereeValue, m.referee.isEmpty() ? "--" : m.referee.c_str());

  String header = wcStageHeader(m);
  if (!header.isEmpty()) lv_label_set_text(ui.stageLabel, header.c_str());
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
    lv_label_set_text(ui.scoreHome, "-");
    lv_label_set_text(ui.scoreAway, "-");
    lv_label_set_text(ui.scoreSep, "vs");
    lv_label_set_text(ui.statusPillLabel, "--");
    lv_label_set_text(ui.kickoffValue, "--");
    lv_label_set_text(ui.venueValue, "--");
    lv_label_set_text(ui.stageDetailValue, "--");
    lv_label_set_text(ui.refereeValue, "--");
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
