#define DT_DRV_COMPAT zmk_behavior_lang_toggle

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_lang_toggle_config {
    uint32_t first_key;
    uint32_t second_key;
};

struct active_lang_toggle {
    bool pressed;
    uint32_t position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    uint8_t source;
#endif
    uint32_t key;
};

struct behavior_lang_toggle_data {
    bool send_second_next;
    struct active_lang_toggle active[CONFIG_ZMK_BEHAVIOR_LANG_TOGGLE_MAX_HELD];
};

static int behavior_lang_toggle_init(const struct device *dev) { return 0; }

static bool active_matches_event(const struct active_lang_toggle *active,
                                 const struct zmk_behavior_binding_event *event) {
    if (!active->pressed || active->position != event->position) {
        return false;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    if (active->source != event->source) {
        return false;
    }
#endif

    return true;
}

static struct active_lang_toggle *
find_active_for_event(struct behavior_lang_toggle_data *data,
                      const struct zmk_behavior_binding_event *event) {
    for (int i = 0; i < CONFIG_ZMK_BEHAVIOR_LANG_TOGGLE_MAX_HELD; i++) {
        if (active_matches_event(&data->active[i], event)) {
            return &data->active[i];
        }
    }

    return NULL;
}

static struct active_lang_toggle *find_free_active(struct behavior_lang_toggle_data *data) {
    for (int i = 0; i < CONFIG_ZMK_BEHAVIOR_LANG_TOGGLE_MAX_HELD; i++) {
        if (!data->active[i].pressed) {
            return &data->active[i];
        }
    }

    return NULL;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_lang_toggle_config *cfg = dev->config;
    struct behavior_lang_toggle_data *data = dev->data;
    struct active_lang_toggle *active = find_active_for_event(data, &event);
    uint32_t key = data->send_second_next ? cfg->second_key : cfg->first_key;
    int ret;

    if (active != NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    active = find_free_active(data);
    if (active == NULL) {
        LOG_ERR("Unable to store active language toggle, increase "
                "CONFIG_ZMK_BEHAVIOR_LANG_TOGGLE_MAX_HELD");
        return -ENOSPC;
    }

    ret = raise_zmk_keycode_state_changed_from_encoded(key, true, event.timestamp);
    if (ret < 0) {
        return ret;
    }

    active->pressed = true;
    active->position = event.position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    active->source = event.source;
#endif
    active->key = key;
    data->send_second_next = !data->send_second_next;

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_lang_toggle_data *data = dev->data;
    struct active_lang_toggle *active = find_active_for_event(data, &event);
    uint32_t key;

    if (active == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    key = active->key;
    active->pressed = false;

    return raise_zmk_keycode_state_changed_from_encoded(key, false, event.timestamp);
}

static const struct behavior_driver_api behavior_lang_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define LANG_TOGGLE_INST(n)                                                                        \
    static const struct behavior_lang_toggle_config behavior_lang_toggle_config_##n = {            \
        .first_key = DT_INST_PROP(n, first_key),                                                   \
        .second_key = DT_INST_PROP(n, second_key),                                                 \
    };                                                                                             \
    static struct behavior_lang_toggle_data behavior_lang_toggle_data_##n = {};                    \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_lang_toggle_init, NULL, &behavior_lang_toggle_data_##n,    \
                            &behavior_lang_toggle_config_##n, POST_KERNEL,                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_lang_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LANG_TOGGLE_INST)
