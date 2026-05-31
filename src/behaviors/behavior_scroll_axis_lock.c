#define DT_DRV_COMPAT zmk_behavior_scroll_axis_lock

#include <stdint.h>

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/scroll_axis_lock.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_scroll_axis_lock_config {
    enum zmk_scroll_axis_lock_mode axis;
};

static enum zmk_scroll_axis_lock_mode active_axis = ZMK_SCROLL_AXIS_LOCK_NONE;

void zmk_scroll_axis_lock_set(enum zmk_scroll_axis_lock_mode mode) { active_axis = mode; }

enum zmk_scroll_axis_lock_mode zmk_scroll_axis_lock_get(void) { return active_axis; }

static int behavior_scroll_axis_lock_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_scroll_axis_lock_config *cfg = dev->config;

    zmk_scroll_axis_lock_set(cfg->axis);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_scroll_axis_lock_config *cfg = dev->config;

    if (zmk_scroll_axis_lock_get() == cfg->axis) {
        zmk_scroll_axis_lock_set(ZMK_SCROLL_AXIS_LOCK_NONE);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_scroll_axis_lock_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define SCROLL_AXIS_LOCK_INST(n)                                                                  \
    static const struct behavior_scroll_axis_lock_config                                           \
        behavior_scroll_axis_lock_config_##n = {                                                   \
            .axis = DT_INST_PROP(n, axis),                                                         \
        };                                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_scroll_axis_lock_init, NULL, NULL,                         \
                            &behavior_scroll_axis_lock_config_##n, POST_KERNEL,                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_scroll_axis_lock_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_AXIS_LOCK_INST)
