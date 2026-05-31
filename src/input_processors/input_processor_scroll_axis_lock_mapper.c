#define DT_DRV_COMPAT zmk_input_processor_scroll_axis_lock_mapper

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>

#include <zmk/input_processor.h>
#include <zmk/scroll_axis_lock.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int scroll_axis_lock_mapper_handle_event(const struct device *dev, struct input_event *event,
                                                uint32_t param1, uint32_t param2,
                                                struct zmk_input_processor_state *state) {
    enum zmk_scroll_axis_lock_mode mode = zmk_scroll_axis_lock_get();

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    switch (event->code) {
    case INPUT_REL_X:
        if (mode == ZMK_SCROLL_AXIS_LOCK_VERTICAL) {
            event->value = 0;
        } else {
            event->code = INPUT_REL_HWHEEL;
        }
        break;
    case INPUT_REL_Y:
        if (mode == ZMK_SCROLL_AXIS_LOCK_HORIZONTAL) {
            event->value = 0;
        } else {
            event->code = INPUT_REL_WHEEL;
        }
        break;
    default:
        break;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static int scroll_axis_lock_mapper_init(const struct device *dev) { return 0; }

static struct zmk_input_processor_driver_api scroll_axis_lock_mapper_driver_api = {
    .handle_event = scroll_axis_lock_mapper_handle_event,
};

#define SCROLL_AXIS_LOCK_MAPPER_INST(n)                                                           \
    DEVICE_DT_INST_DEFINE(n, scroll_axis_lock_mapper_init, NULL, NULL, NULL, POST_KERNEL,         \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &scroll_axis_lock_mapper_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_AXIS_LOCK_MAPPER_INST)
