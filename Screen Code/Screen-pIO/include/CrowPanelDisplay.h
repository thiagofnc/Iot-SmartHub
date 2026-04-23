#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <driver/i2c.h>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

enum class CrowPanelModel {
  FiveInch,
  SevenInch,
};

class CrowPanelDisplay : public lgfx::LGFX_Device {
public:
  lgfx::Bus_RGB bus;
  lgfx::Panel_RGB panel;

  explicit CrowPanelDisplay(CrowPanelModel model = CrowPanelModel::FiveInch) {
    {
      auto cfg = panel.config();
      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.rgb_order = true;
      panel.config(cfg);
    }

    {
      auto cfg = panel.config_detail();
      cfg.use_psram = 1;
      panel.config_detail(cfg);
    }

    {
      auto cfg = bus.config();
      cfg.panel = &panel;
      cfg.freq_write = 15000000;
      cfg.pclk_active_neg = 1;
      cfg.de_idle_high = 0;
      cfg.pclk_idle_high = 0;

      if (model == CrowPanelModel::FiveInch) {
        cfg.pin_d0  = GPIO_NUM_8;
        cfg.pin_d1  = GPIO_NUM_3;
        cfg.pin_d2  = GPIO_NUM_46;
        cfg.pin_d3  = GPIO_NUM_9;
        cfg.pin_d4  = GPIO_NUM_1;
        cfg.pin_d5  = GPIO_NUM_5;
        cfg.pin_d6  = GPIO_NUM_6;
        cfg.pin_d7  = GPIO_NUM_7;
        cfg.pin_d8  = GPIO_NUM_15;
        cfg.pin_d9  = GPIO_NUM_16;
        cfg.pin_d10 = GPIO_NUM_4;
        cfg.pin_d11 = GPIO_NUM_45;
        cfg.pin_d12 = GPIO_NUM_48;
        cfg.pin_d13 = GPIO_NUM_47;
        cfg.pin_d14 = GPIO_NUM_21;
        cfg.pin_d15 = GPIO_NUM_14;
        cfg.pin_henable = GPIO_NUM_40;
        cfg.pin_vsync   = GPIO_NUM_41;
        cfg.pin_hsync   = GPIO_NUM_39;
        cfg.pin_pclk    = GPIO_NUM_0;
        cfg.hsync_polarity = 0;
        cfg.hsync_front_porch = 8;
        cfg.hsync_pulse_width = 4;
        cfg.hsync_back_porch = 43;
        cfg.vsync_polarity = 0;
        cfg.vsync_front_porch = 8;
        cfg.vsync_pulse_width = 4;
        cfg.vsync_back_porch = 12;
      } else {
        cfg.pin_d0  = GPIO_NUM_15;
        cfg.pin_d1  = GPIO_NUM_7;
        cfg.pin_d2  = GPIO_NUM_6;
        cfg.pin_d3  = GPIO_NUM_5;
        cfg.pin_d4  = GPIO_NUM_4;
        cfg.pin_d5  = GPIO_NUM_9;
        cfg.pin_d6  = GPIO_NUM_46;
        cfg.pin_d7  = GPIO_NUM_3;
        cfg.pin_d8  = GPIO_NUM_8;
        cfg.pin_d9  = GPIO_NUM_16;
        cfg.pin_d10 = GPIO_NUM_1;
        cfg.pin_d11 = GPIO_NUM_14;
        cfg.pin_d12 = GPIO_NUM_21;
        cfg.pin_d13 = GPIO_NUM_47;
        cfg.pin_d14 = GPIO_NUM_48;
        cfg.pin_d15 = GPIO_NUM_45;
        cfg.pin_henable = GPIO_NUM_41;
        cfg.pin_vsync   = GPIO_NUM_40;
        cfg.pin_hsync   = GPIO_NUM_39;
        cfg.pin_pclk    = GPIO_NUM_0;
        cfg.hsync_polarity = 0;
        cfg.hsync_front_porch = 40;
        cfg.hsync_pulse_width = 48;
        cfg.hsync_back_porch = 40;
        cfg.vsync_polarity = 0;
        cfg.vsync_front_porch = 1;
        cfg.vsync_pulse_width = 31;
        cfg.vsync_back_porch = 13;
      }

      bus.config(cfg);
    }

    panel.setBus(&bus);

    setPanel(&panel);
  }
};
