#pragma once

#include <stdint.h>

enum zmk_scroll_axis_lock_mode {
    ZMK_SCROLL_AXIS_LOCK_NONE = 0,
    ZMK_SCROLL_AXIS_LOCK_VERTICAL = 1,
    ZMK_SCROLL_AXIS_LOCK_HORIZONTAL = 2,
};

void zmk_scroll_axis_lock_set(enum zmk_scroll_axis_lock_mode mode);
enum zmk_scroll_axis_lock_mode zmk_scroll_axis_lock_get(void);
