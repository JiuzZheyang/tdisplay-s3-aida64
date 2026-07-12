#ifndef LV_PORT_H
#define LV_PORT_H

#include "esp_lcd_types.h"

#define LCD_WIDTH  170
#define LCD_HEIGHT 320

void lv_port_init(esp_lcd_panel_handle_t panel_handle);

#endif
