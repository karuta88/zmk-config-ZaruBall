/*
 * Temporary pointer layer with a post-keypress rearm delay.
 *
 * Based on ZMK's temp-layer input processor.
 */

#define DT_DRV_COMPAT zmk_input_processor_aml_temp_layer

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_LAYERS ZMK_KEYMAP_LAYERS_LEN

struct aml_temp_layer_config {
    int16_t require_prior_idle_ms;
    int16_t rearm_after_pointer_idle_ms;
    const uint16_t *excluded_positions;
    size_t num_positions;
    const uint16_t *rearm_positions;
    size_t num_rearm_positions;
};

struct aml_temp_layer_state {
    uint8_t toggle_layer;
    bool is_active;
    bool waiting_for_pointer_idle;
    int64_t last_keycode_timestamp;
    int64_t last_pointer_timestamp;
};

struct aml_temp_layer_data {
    struct k_mutex lock;
    struct aml_temp_layer_state state;
};

struct layer_state_action {
    uint8_t layer;
    bool activate;
};

K_MSGQ_DEFINE(aml_temp_layer_action_msgq, sizeof(struct layer_state_action), MAX_LAYERS, 4);

static struct k_work_delayable layer_disable_works[MAX_LAYERS];

static bool position_is_listed(const uint16_t *positions, size_t num_positions, uint32_t position) {
    for (size_t i = 0; i < num_positions; i++) {
        if (positions[i] == position) {
            return true;
        }
    }

    return false;
}

static bool elapsed_at_least(int64_t since, int16_t duration_ms, int64_t now) {
    return now - since >= duration_ms;
}

static void update_layer_state(struct aml_temp_layer_state *state, bool activate) {
    if (state->is_active == activate) {
        return;
    }

    state->is_active = activate;
    if (activate) {
        zmk_keymap_layer_activate(state->toggle_layer);
    } else {
        zmk_keymap_layer_deactivate(state->toggle_layer);
    }
}

static void start_pointer_idle_rearm(struct aml_temp_layer_state *state) {
    state->waiting_for_pointer_idle = true;
    state->last_pointer_timestamp = k_uptime_get();
}

static void layer_action_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    const struct device *dev = DEVICE_DT_INST_GET(0);
    struct aml_temp_layer_data *data = dev->data;
    struct layer_state_action action;

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return;
    }

    while (k_msgq_get(&aml_temp_layer_action_msgq, &action, K_NO_WAIT) == 0) {
        if (!action.activate && zmk_keymap_layer_active(action.layer)) {
            update_layer_state(&data->state, false);
        } else if (action.activate) {
            update_layer_state(&data->state, true);
        }
    }

    k_mutex_unlock(&data->lock);
}

static K_WORK_DEFINE(layer_action_work, layer_action_work_cb);

static void layer_disable_callback(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    int layer = ARRAY_INDEX(layer_disable_works, delayable);
    struct layer_state_action action = {.layer = layer, .activate = false};

    if (k_msgq_put(&aml_temp_layer_action_msgq, &action, K_NO_WAIT) == 0) {
        k_work_submit(&layer_action_work);
    }
}

static int handle_layer_state_changed(const struct device *dev, const zmk_event_t *event) {
    ARG_UNUSED(event);

    struct aml_temp_layer_data *data = dev->data;
    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (!zmk_keymap_layer_active(zmk_keymap_layer_index_to_id(data->state.toggle_layer))) {
        data->state.is_active = false;
        k_work_cancel_delayable(&layer_disable_works[data->state.toggle_layer]);
    }

    k_mutex_unlock(&data->lock);
    return ZMK_EV_EVENT_BUBBLE;
}

static int handle_position_state_changed(const struct device *dev, const zmk_event_t *event) {
    const struct zmk_position_state_changed *position_event = as_zmk_position_state_changed(event);
    if (!position_event->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct aml_temp_layer_data *data = dev->data;
    const struct aml_temp_layer_config *config = dev->config;
    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const bool should_rearm =
        position_is_listed(config->rearm_positions, config->num_rearm_positions,
                           position_event->position);
    const bool is_excluded =
        position_is_listed(config->excluded_positions, config->num_positions,
                           position_event->position);

    if (data->state.is_active && should_rearm) {
        start_pointer_idle_rearm(&data->state);
    }

    if (data->state.is_active && !is_excluded) {
        update_layer_state(&data->state, false);
    }

    k_mutex_unlock(&data->lock);
    return ZMK_EV_EVENT_BUBBLE;
}

static int handle_keycode_state_changed(const struct device *dev, const zmk_event_t *event) {
    const struct zmk_keycode_state_changed *keycode_event = as_zmk_keycode_state_changed(event);
    if (!keycode_event->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct aml_temp_layer_data *data = dev->data;
    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    data->state.last_keycode_timestamp = keycode_event->timestamp;
    k_mutex_unlock(&data->lock);
    return ZMK_EV_EVENT_BUBBLE;
}

#define DISPATCH_EVENT(inst)                                                                       \
    {                                                                                              \
        int ret = handle_layer_state_changed(DEVICE_DT_INST_GET(inst), event);                     \
        if (ret < 0) {                                                                             \
            return ret;                                                                            \
        }                                                                                          \
    }

static int handle_layer_event_dispatcher(const zmk_event_t *event) {
    DT_INST_FOREACH_STATUS_OKAY(DISPATCH_EVENT)
    return ZMK_EV_EVENT_BUBBLE;
}

#undef DISPATCH_EVENT
#define DISPATCH_EVENT(inst)                                                                       \
    {                                                                                              \
        int ret = handle_position_state_changed(DEVICE_DT_INST_GET(inst), event);                  \
        if (ret < 0) {                                                                             \
            return ret;                                                                            \
        }                                                                                          \
    }

static int handle_position_event_dispatcher(const zmk_event_t *event) {
    DT_INST_FOREACH_STATUS_OKAY(DISPATCH_EVENT)
    return ZMK_EV_EVENT_BUBBLE;
}

#undef DISPATCH_EVENT
#define DISPATCH_EVENT(inst)                                                                       \
    {                                                                                              \
        int ret = handle_keycode_state_changed(DEVICE_DT_INST_GET(inst), event);                   \
        if (ret < 0) {                                                                             \
            return ret;                                                                            \
        }                                                                                          \
    }

static int handle_keycode_event_dispatcher(const zmk_event_t *event) {
    DT_INST_FOREACH_STATUS_OKAY(DISPATCH_EVENT)
    return ZMK_EV_EVENT_BUBBLE;
}

#undef DISPATCH_EVENT

static int aml_temp_layer_handle_event(const struct device *dev, struct input_event *event,
                                       uint32_t param1, uint32_t param2,
                                       struct zmk_input_processor_state *state) {
    ARG_UNUSED(event);
    ARG_UNUSED(state);

    if (param1 >= MAX_LAYERS) {
        return -EINVAL;
    }

    struct aml_temp_layer_data *data = dev->data;
    const struct aml_temp_layer_config *config = dev->config;
    const int64_t now = k_uptime_get();

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return -EAGAIN;
    }

    data->state.toggle_layer = param1;

    if (data->state.waiting_for_pointer_idle) {
        if (!elapsed_at_least(data->state.last_pointer_timestamp,
                              config->rearm_after_pointer_idle_ms, now)) {
            data->state.last_pointer_timestamp = now;
            k_mutex_unlock(&data->lock);
            return ZMK_INPUT_PROC_CONTINUE;
        }

        data->state.waiting_for_pointer_idle = false;
    }

    data->state.last_pointer_timestamp = now;
    if (!data->state.is_active &&
        elapsed_at_least(data->state.last_keycode_timestamp, config->require_prior_idle_ms, now)) {
        struct layer_state_action action = {.layer = param1, .activate = true};

        if (k_msgq_put(&aml_temp_layer_action_msgq, &action, K_NO_WAIT) == 0) {
            k_work_submit(&layer_action_work);
        }
    }

    if (param2 > 0) {
        k_work_reschedule(&layer_disable_works[param1], K_MSEC(param2));
    }

    k_mutex_unlock(&data->lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

static int aml_temp_layer_init(const struct device *dev) {
    struct aml_temp_layer_data *data = dev->data;
    k_mutex_init(&data->lock);

    for (int i = 0; i < MAX_LAYERS; i++) {
        k_work_init_delayable(&layer_disable_works[i], layer_disable_callback);
    }

    return 0;
}

static const struct zmk_input_processor_driver_api aml_temp_layer_driver_api = {
    .handle_event = aml_temp_layer_handle_event,
};

#define AML_TEMP_LAYER_INST(n)                                                                     \
    static struct aml_temp_layer_data aml_temp_layer_data_##n = {};                                \
    static const uint16_t excluded_positions_##n[] = DT_INST_PROP(n, excluded_positions);          \
    static const uint16_t rearm_positions_##n[] = DT_INST_PROP(n, rearm_positions);                \
    static const struct aml_temp_layer_config aml_temp_layer_config_##n = {                        \
        .require_prior_idle_ms = DT_INST_PROP_OR(n, require_prior_idle_ms, 0),                     \
        .rearm_after_pointer_idle_ms = DT_INST_PROP(n, rearm_after_pointer_idle_ms),               \
        .excluded_positions = excluded_positions_##n,                                              \
        .num_positions = DT_INST_PROP_LEN(n, excluded_positions),                                  \
        .rearm_positions = rearm_positions_##n,                                                    \
        .num_rearm_positions = DT_INST_PROP_LEN(n, rearm_positions),                               \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, aml_temp_layer_init, NULL, &aml_temp_layer_data_##n,                  \
                          &aml_temp_layer_config_##n, POST_KERNEL,                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &aml_temp_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AML_TEMP_LAYER_INST)

ZMK_LISTENER(aml_temp_layer_layer, handle_layer_event_dispatcher);
ZMK_SUBSCRIPTION(aml_temp_layer_layer, zmk_layer_state_changed);
ZMK_LISTENER(aml_temp_layer_position, handle_position_event_dispatcher);
ZMK_SUBSCRIPTION(aml_temp_layer_position, zmk_position_state_changed);
ZMK_LISTENER(aml_temp_layer_keycode, handle_keycode_event_dispatcher);
ZMK_SUBSCRIPTION(aml_temp_layer_keycode, zmk_keycode_state_changed);
