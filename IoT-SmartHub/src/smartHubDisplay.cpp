#define LGFX_USE_V1

#include "smartHubDisplay.h"

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lvgl.h>

SmartHubDisplay smartHubDisplay;

namespace {
constexpr uint16_t kScreenWidth = 800;
constexpr uint16_t kScreenHeight = 480;
constexpr unsigned long kLvglTickMs = 5;
constexpr uint8_t kDisplayBacklightPin = 2;
constexpr uint8_t kDisplayEnablePin = 38;

class LGFX : public lgfx::LGFX_Device {
public:
  LGFX() {
    {
      auto cfg = bus.config();
      cfg.panel = &panel;
      cfg.pin_d0 = GPIO_NUM_15;
      cfg.pin_d1 = GPIO_NUM_7;
      cfg.pin_d2 = GPIO_NUM_6;
      cfg.pin_d3 = GPIO_NUM_5;
      cfg.pin_d4 = GPIO_NUM_4;
      cfg.pin_d5 = GPIO_NUM_9;
      cfg.pin_d6 = GPIO_NUM_46;
      cfg.pin_d7 = GPIO_NUM_3;
      cfg.pin_d8 = GPIO_NUM_8;
      cfg.pin_d9 = GPIO_NUM_16;
      cfg.pin_d10 = GPIO_NUM_1;
      cfg.pin_d11 = GPIO_NUM_14;
      cfg.pin_d12 = GPIO_NUM_21;
      cfg.pin_d13 = GPIO_NUM_47;
      cfg.pin_d14 = GPIO_NUM_48;
      cfg.pin_d15 = GPIO_NUM_45;
      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync = GPIO_NUM_40;
      cfg.pin_hsync = GPIO_NUM_39;
      cfg.pin_pclk = GPIO_NUM_0;
      cfg.freq_write = 15000000;
      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch = 40;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch = 13;
      cfg.pclk_active_neg = 1;
      cfg.de_idle_high = 0;
      cfg.pclk_idle_high = 0;
      bus.config(cfg);
      panel.setBus(&bus);
    }

    {
      auto cfg = panel.config();
      cfg.memory_width = kScreenWidth;
      cfg.memory_height = kScreenHeight;
      cfg.panel_width = kScreenWidth;
      cfg.panel_height = kScreenHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      panel.config(cfg);
    }

    setPanel(&panel);
  }

private:
  lgfx::Bus_RGB bus;
  lgfx::Panel_RGB panel;
};

LGFX *display = nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_color_t lvglBuffer[kScreenWidth * 40];

lv_obj_t *watchStatusLabel = nullptr;
lv_obj_t *watchNameLabel = nullptr;
lv_obj_t *watchModelLabel = nullptr;
lv_obj_t *phoneBatteryLabel = nullptr;
lv_obj_t *ipadBatteryLabel = nullptr;
lv_obj_t *nearbyDeviceLabels[BluetoothConnection::kMaxNearbyDevices] = {};

void flushDisplay(lv_disp_drv_t *disp, const lv_area_t *area,
                  lv_color_t *colorP) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;

  display->startWrite();
  display->setAddrWindow(area->x1, area->y1, width, height);
  display->writePixels(reinterpret_cast<lgfx::rgb565_t *>(&colorP->full),
                       width * height);
  display->endWrite();

  lv_disp_flush_ready(disp);
}

void createStatusCard(lv_obj_t *parent, const char *title, lv_obj_t **valueLabel,
                      lv_coord_t x, lv_coord_t y) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, 340, 68);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0xD6DEE8), 0);
  lv_obj_set_style_pad_all(card, 14, 0);

  lv_obj_t *titleLabel = lv_label_create(card);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x5E6B7A), 0);
  lv_obj_set_style_text_font(titleLabel, LV_FONT_DEFAULT, 0);

  *valueLabel = lv_label_create(card);
  lv_obj_align(*valueLabel, LV_ALIGN_LEFT_MID, 0, 14);
  lv_obj_set_width(*valueLabel, 300);
  lv_obj_set_style_text_color(*valueLabel, lv_color_hex(0x172033), 0);
  lv_obj_set_style_text_font(*valueLabel, LV_FONT_DEFAULT, 0);
}

void createNearbyPanel(lv_obj_t *parent) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, 350, 360);
  lv_obj_set_pos(panel, 410, 95);
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0xD6DEE8), 0);
  lv_obj_set_style_pad_all(panel, 14, 0);

  lv_obj_t *titleLabel = lv_label_create(panel);
  lv_label_set_text(titleLabel, "Nearby BLE devices");
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x172033), 0);
  lv_obj_set_style_text_font(titleLabel, LV_FONT_DEFAULT, 0);

  for (size_t i = 0; i < BluetoothConnection::kMaxNearbyDevices; ++i) {
    nearbyDeviceLabels[i] = lv_label_create(panel);
    lv_obj_set_pos(nearbyDeviceLabels[i], 0, 42 + (i * 58));
    lv_obj_set_width(nearbyDeviceLabels[i], 310);
    lv_label_set_long_mode(nearbyDeviceLabels[i], LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(nearbyDeviceLabels[i], lv_color_hex(0x172033), 0);
    lv_obj_set_style_text_font(nearbyDeviceLabels[i], LV_FONT_DEFAULT, 0);
    lv_label_set_text(nearbyDeviceLabels[i], "--");
  }
}
} // namespace

void SmartHubDisplay::begin() {
  Serial.println("Creating display driver...");
  display = new LGFX();
  Serial.println("Display driver created");

  pinMode(kDisplayBacklightPin, OUTPUT);
  digitalWrite(kDisplayBacklightPin, HIGH);
  pinMode(kDisplayEnablePin, OUTPUT);
  digitalWrite(kDisplayEnablePin, HIGH);
  delay(200);

  Serial.println("Initializing display panel...");
  display->init();
  Serial.println("Display panel initialized");

  display->setRotation(0);
  display->fillScreen(TFT_BLUE);
  delay(1000);
  display->fillScreen(TFT_WHITE);

  lv_init();
  lv_disp_draw_buf_init(&drawBuffer, lvglBuffer, nullptr, kScreenWidth * 40);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = kScreenWidth;
  dispDrv.ver_res = kScreenHeight;
  dispDrv.flush_cb = flushDisplay;
  dispDrv.draw_buf = &drawBuffer;
  lv_disp_drv_register(&dispDrv);

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xEEF3F8), 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "ESP32 SmartHub");
  lv_obj_set_style_text_color(title, lv_color_hex(0x172033), 0);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
  lv_obj_set_pos(title, 50, 34);

  createStatusCard(screen, "Watch", &watchStatusLabel, 50, 95);
  createStatusCard(screen, "Device name", &watchNameLabel, 50, 170);
  createStatusCard(screen, "Model", &watchModelLabel, 50, 245);
  createStatusCard(screen, "iPhone battery", &phoneBatteryLabel, 50, 320);
  createStatusCard(screen, "iPad battery", &ipadBatteryLabel, 50, 395);
  createNearbyPanel(screen);

  refresh(false, "--", "--", -1, -1, nullptr, 0);
  lv_timer_handler();
}

void SmartHubDisplay::loop() {
  static unsigned long lastLvglTick = 0;
  const unsigned long now = millis();

  if (now - lastLvglTick >= kLvglTickMs) {
    lv_tick_inc(now - lastLvglTick);
    lastLvglTick = now;
  }

  lv_timer_handler();
}

void SmartHubDisplay::refresh(bool watchConnected, const String &watchName,
                              const String &watchModel, int phoneBattery,
                              int ipadBattery,
                              const BluetoothConnection::NearbyDevice *nearbyDevices,
                              size_t nearbyDeviceCount) {
  lv_label_set_text(watchStatusLabel, watchConnected ? "Connected" : "Scanning");
  lv_label_set_text(watchNameLabel, watchName.c_str());
  lv_label_set_text(watchModelLabel, watchModel.c_str());
  lv_label_set_text(phoneBatteryLabel,
                    phoneBattery >= 0 ? (String(phoneBattery) + "%").c_str()
                                      : "unknown");
  lv_label_set_text(ipadBatteryLabel,
                    ipadBattery >= 0 ? (String(ipadBattery) + "%").c_str()
                                     : "unknown");

  for (size_t i = 0; i < BluetoothConnection::kMaxNearbyDevices; ++i) {
    if (nearbyDevices != nullptr && i < nearbyDeviceCount) {
      String displayName = nearbyDevices[i].name;
      if (displayName == "Unknown") {
        displayName = nearbyDevices[i].address;
      }

      String line = String(i + 1) + ". " + displayName + "\n";
      line += String(nearbyDevices[i].rssi) + " dBm  ";
      line += nearbyDevices[i].address;
      lv_label_set_text(nearbyDeviceLabels[i], line.c_str());
    } else {
      lv_label_set_text(nearbyDeviceLabels[i], "--");
    }
  }

  lv_obj_invalidate(lv_scr_act());
}
