/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <settings/settings.h>

#include <math.h>
#include <stdlib.h>

#include <logging/log.h>

#include <drivers/led_strip.h>
#include <drivers/ext_power.h>

#include <zmk/rgb_underglow.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_LABEL DT_LABEL(DT_CHOSEN(zmk_underglow))
#define STRIP_NUM_PIXELS DT_PROP(DT_CHOSEN(zmk_underglow), chain_length)

#define HUE_MAX 360
#define SAT_MAX 100
#define BRT_MAX 100

BUILD_ASSERT(CONFIG_ZMK_RGB_UNDERGLOW_BRT_MIN <= CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX,
             "ERROR: RGB underglow maximum brightness is less than minimum brightness");

enum rgb_underglow_effect {
    UNDERGLOW_EFFECT_INDICATOR, // Special effect to demonstrate the indicator concept
    UNDERGLOW_EFFECT_SOLID,
    UNDERGLOW_EFFECT_BREATHE,
    UNDERGLOW_EFFECT_SPECTRUM,
    UNDERGLOW_EFFECT_SWIRL,
    UNDERGLOW_EFFECT_NUMBER // Used to track number of underglow effects
};

struct rgb_underglow_state {
    struct zmk_led_hsb color;
    uint8_t animation_speed;
    uint8_t current_effect;
    uint16_t animation_step;
    bool on;
};

static const struct device *led_strip;

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static struct rgb_underglow_state state;

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER)
static const struct device *ext_power;
#endif

static struct zmk_led_hsb hsb_scale_min_max(struct zmk_led_hsb hsb) {
    hsb.b = CONFIG_ZMK_RGB_UNDERGLOW_BRT_MIN +
            (CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX - CONFIG_ZMK_RGB_UNDERGLOW_BRT_MIN) * hsb.b / BRT_MAX;
    return hsb;
}

static struct zmk_led_hsb hsb_scale_zero_max(struct zmk_led_hsb hsb) {
    hsb.b = hsb.b * CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX / BRT_MAX;
    return hsb;
}

static struct led_rgb hsb_to_rgb(struct zmk_led_hsb hsb) {
    double r, g, b;

    uint8_t i = hsb.h / 60;
    double v = hsb.b / ((float)BRT_MAX);
    double s = hsb.s / ((float)SAT_MAX);
    double f = hsb.h / ((float)HUE_MAX) * 6 - i;
    double p = v * (1 - s);
    double q = v * (1 - f * s);
    double t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
    }

    struct led_rgb rgb = {r : r * 255, g : g * 255, b : b * 255};

    return rgb;
}

/** 
 * Defines the layout of indicator screen */
#define RGB_ID_COL1 4
#define RGB_ID_COL2 9
#define RGB_ID_COL3 15
#define RGB_ID_COL4 21
#define RGB_ID_COL5 27
#define RGB_ID_COL6 33
#define RGB_ID(col, row)  ((RGB_ID_COLcol)+(row))

#define RGB_HOST_ROW 2
#define RGB_ID_HOST_USB RGB_ID(6, RGB_HOST_ROW)
#define RGB_ID_HOST_BLE0 RGB_ID(5, RGB_HOST_ROW)
#define RGB_ID_HOST_BLE1 RGB_ID(4, RGB_HOST_ROW)
#define RGB_ID_HOST_BLE2 RGB_ID(3, RGB_HOST_ROW)
#define RGB_ID_HOST_BLE3 RGB_ID(2, RGB_HOST_ROW)
#define RGB_HOST_BLE_COUNT 4
const uint8_t[RGB_HOST_BLE_COUNT] RGB_HOST_BLE_IDS = {RGB_ID_HOST_BLE0, RGB_ID_HOST_BLE0, RGB_ID_HOST_BLE0, RGB_ID_HOST_BLE0;

#define RGB_LAYER_ROW 3
#define RGB_ID_LAYER0 RGB_ID(6, RGB_LAYER_ROW)
#define RGB_ID_LAYER1 RGB_ID(5, RGB_LAYER_ROW)
#define RGB_ID_LAYER2 RGB_ID(4, RGB_LAYER_ROW)
#define RGB_ID_LAYER3 RGB_ID(3, RGB_LAYER_ROW)
#define RGB_ID_LAYER4 RGB_ID(2, RGB_LAYER_ROW)
#define RGB_ID_LAYER5 RGB_ID(1, RGB_LAYER_ROW)
#define RGB_LAYER_COUNT 6
const uint8_t[RGB_LAYER_COUNT] RGB_LAYER_IDS = {RGB_ID_LAYER0, RGB_ID_LAYER1, RGB_ID_LAYER2, RGB_ID_LAYER3, RGB_ID_LAYER4, RGB_ID_LAYER5};

#define RGB_BAT_ROW 4
#define RGB_ID_BAT0 RGB_ID(6, RGB_BAT_ROW)
#define RGB_ID_BAT1 RGB_ID(5, RGB_BAT_ROW)
#define RGB_ID_BAT2 RGB_ID(4, RGB_BAT_ROW)
#define RGB_ID_BAT3 RGB_ID(3, RGB_BAT_ROW)
#define RGB_ID_BAT4 RGB_ID(2, RGB_BAT_ROW)
#define RGB_ID_BAT5 RGB_ID(1, RGB_BAT_ROW)
#define RGB_BAT_COUNT 6
const uint8_t[RGB_BAT_COUNT] RGB_BAT_IDS = {RGB_ID_BAT0, RGB_ID_BAT1, RGB_ID_BAT2, RGB_ID_BAT3, RGB_ID_BAT4, RGB_ID_BAT6};

// [SC] [WIP] Effect for the indicators
static void zmk_rgb_underglow_effect_indicator() {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
            pixels[i] = 0;
    }
    
#if ZMK_BLE_IS_CENTRAL
    // Caps lock, num lock and scroll lock
    // [SC] Need to bring in PR 999


    // Layer state
    


    // Bluetooth state

    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = hsb_to_rgb(hsb_scale_min_max(state.color));
    }




#endif
    // Battery state

}

static struct k_delayed_work led_update_work;

static void zmk_rgb_underglow_central_send() {
    int err = zmk_split_bt_update_led(&led_data);
    if (err) {
        LOG_ERR("send failed (err %d)", err);
    }
}
#endif

static void zmk_rgb_underglow_effect_kinesis() {
#if ZMK_BLE_IS_CENTRAL
    // leds for central(left) side

    old_led_data.layer = led_data.layer;
    old_led_data.indicators = led_data.indicators;
    led_data.indicators = zmk_led_indicators_get_current_flags();
    led_data.layer = zmk_keymap_highest_layer_active();

    pixels[0].r = (led_data.indicators & BIT(LED_CAPSLOCK)) * LED_BRIGHTNESS;
    pixels[0].g = (led_data.indicators & BIT(LED_CAPSLOCK)) * LED_BRIGHTNESS;
    pixels[0].b = (led_data.indicators & BIT(LED_CAPSLOCK)) * LED_BRIGHTNESS;
    // set second led as bluetooth state
    switch (zmk_ble_active_profile_index()) {
    case 0:
        pixels[1].r = LED_BRIGHTNESS;
        pixels[1].g = LED_BRIGHTNESS;
        pixels[1].b = LED_BRIGHTNESS;
        break;
    case 1:
        pixels[1].r = 0;
        pixels[1].g = 0;
        pixels[1].b = LED_BRIGHTNESS;
        break;
    case 2:
        pixels[1].r = LED_BRIGHTNESS;
        pixels[1].g = 0;
        pixels[1].b = 0;
        break;
    case 3:
        pixels[1].r = 0;
        pixels[1].g = LED_BRIGHTNESS;
        pixels[1].b = 0;
        break;
    }
    // blink second led slowly if bluetooth not paired, quickly if not connected
    if (zmk_ble_active_profile_is_open()) {
        pixels[1].r = pixels[1].r * last_ble_state[0];
        pixels[1].g = pixels[1].g * last_ble_state[0];
        pixels[1].b = pixels[1].b * last_ble_state[0];
        if (state.animation_step > 3) {
            last_ble_state[0] = !last_ble_state[0];
            state.animation_step = 0;
        }
        state.animation_step++;
    } else if (!zmk_ble_active_profile_is_connected()) {
        pixels[1].r = pixels[1].r * last_ble_state[1];
        pixels[1].g = pixels[1].g * last_ble_state[1];
        pixels[1].b = pixels[1].b * last_ble_state[1];
        if (state.animation_step > 14) {
            last_ble_state[1] = !last_ble_state[1];
            state.animation_step = 0;
        }
        state.animation_step++;
    }
    // set third led as layer state
    switch (led_data.layer) {
    case 0:
        pixels[2].r = 0;
        pixels[2].g = 0;
        pixels[2].b = 0;
        break;
    case 1:
        pixels[2].r = LED_BRIGHTNESS;
        pixels[2].g = LED_BRIGHTNESS;
        pixels[2].b = LED_BRIGHTNESS;
        break;
    case 2:
        pixels[2].r = 0;
        pixels[2].g = 0;
        pixels[2].b = LED_BRIGHTNESS;
        break;
    case 3:
        pixels[2].r = 0;
        pixels[2].g = LED_BRIGHTNESS;
        pixels[2].b = 0;
        break;
    case 4:
        pixels[2].r = LED_BRIGHTNESS;
        pixels[2].g = 0;
        pixels[2].b = 0;
        break;
    case 5:
        pixels[2].r = LED_BRIGHTNESS;
        pixels[2].g = 0;
        pixels[2].b = LED_BRIGHTNESS;
        break;
    case 6:
        pixels[2].r = 0;
        pixels[2].g = LED_BRIGHTNESS;
        pixels[2].b = LED_BRIGHTNESS;
        break;
    case 7:
        pixels[2].r = LED_BRIGHTNESS;
        pixels[2].g = LED_BRIGHTNESS;
        pixels[2].b = 0;
        break;
    default:
        pixels[2].r = 0;
        pixels[2].g = 0;
        pixels[2].b = 0;
        break;
    }
    if (old_led_data.layer != led_data.layer || old_led_data.indicators != led_data.indicators) {
        zmk_rgb_underglow_central_send();
    }
#else
    // leds for peripheral(right) side
    if (zmk_ble_active_profile_is_open()) {
        pixels[0].r = LED_BRIGHTNESS * last_ble_state[0];
        pixels[0].g = 0;
        pixels[0].b = 0;
        pixels[1].r = LED_BRIGHTNESS * last_ble_state[0];
        pixels[1].g = 0;
        pixels[1].b = 0;
        pixels[2].r = LED_BRIGHTNESS * last_ble_state[0];
        pixels[2].g = 0;
        pixels[2].b = 0;
        if (state.animation_step > 3) {
            last_ble_state[0] = !last_ble_state[0];
            state.animation_step = 0;
        }
        state.animation_step++;
    } else if (!zmk_ble_active_profile_is_connected()) {
        pixels[0].r = LED_BRIGHTNESS * last_ble_state[1];
        pixels[0].g = 0;
        pixels[0].b = 0;
        pixels[1].r = LED_BRIGHTNESS * last_ble_state[1];
        pixels[1].g = 0;
        pixels[1].b = 0;
        pixels[2].r = LED_BRIGHTNESS * last_ble_state[1];
        pixels[2].g = 0;
        pixels[2].b = 0;
        if (state.animation_step > 14) {
            last_ble_state[1] = !last_ble_state[1];
            state.animation_step = 0;
        }
        state.animation_step++;
    } else {
        // set first led as LED_NUMLOCK
        pixels[2].r = (led_data.indicators & BIT(LED_NUMLOCK)) * LED_BRIGHTNESS;
        pixels[2].g = (led_data.indicators & BIT(LED_NUMLOCK)) * LED_BRIGHTNESS;
        pixels[2].b = (led_data.indicators & BIT(LED_NUMLOCK)) * LED_BRIGHTNESS;
        // set second led as scroll Lock
        pixels[1].r = (led_data.indicators & BIT(LED_SCROLLLOCK)) * LED_BRIGHTNESS;
        pixels[1].g = (led_data.indicators & BIT(LED_SCROLLLOCK)) * LED_BRIGHTNESS;
        pixels[1].b = (led_data.indicators & BIT(LED_SCROLLLOCK)) * LED_BRIGHTNESS;
        // set third led as layer
        switch (led_data.layer) {
        case 0:
            pixels[0].r = 0;
            pixels[0].g = 0;
            pixels[0].b = 0;
            break;
        case 1:
            pixels[0].r = LED_BRIGHTNESS;
            pixels[0].g = LED_BRIGHTNESS;
            pixels[0].b = LED_BRIGHTNESS;
            break;
        case 2:
            pixels[0].r = 0;
            pixels[0].g = 0;
            pixels[0].b = LED_BRIGHTNESS;
            break;
        case 3:
            pixels[0].r = 0;
            pixels[0].g = LED_BRIGHTNESS;
            pixels[0].b = 0;
            break;
        case 4:
            pixels[0].r = LED_BRIGHTNESS;
            pixels[0].g = 0;
            pixels[0].b = 0;
            break;
        case 5:
            pixels[0].r = LED_BRIGHTNESS;
            pixels[0].g = 0;
            pixels[0].b = LED_BRIGHTNESS;
            break;
        case 6:
            pixels[0].r = 0;
            pixels[0].g = LED_BRIGHTNESS;
            pixels[0].b = LED_BRIGHTNESS;
            break;
        case 7:
            pixels[0].r = LED_BRIGHTNESS;
            pixels[0].g = LED_BRIGHTNESS;
            pixels[0].b = 0;
            break;
        default:
            pixels[0].r = 0;
            pixels[0].g = 0;
            pixels[0].b = 0;
            break;
        }
    }
#endif
}

static void zmk_rgb_underglow_effect_battery() {
    uint8_t soc = zmk_battery_state_of_charge();
    struct led_rgb rgb;
    if (soc > 80) {
        rgb.r = 0;
        rgb.g = 255;
        rgb.b = 0;
    } else if (soc > 50 && soc < 80) {
        rgb.r = 255;
        rgb.g = 255;
        rgb.b = 0;
    } else if (soc > 20 && soc < 51) {
        rgb.r = 255;
        rgb.g = 140;
        rgb.b = 0;
    } else {
        rgb.r = 255;
        rgb.g = 0;
        rgb.b = 0;
    }
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = rgb;
    }
}


static void zmk_rgb_underglow_effect_solid() {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = hsb_to_rgb(hsb_scale_min_max(state.color));
    }
}

static void zmk_rgb_underglow_effect_breathe() {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        struct zmk_led_hsb hsb = state.color;
        hsb.b = abs(state.animation_step - 1200) / 12;

        pixels[i] = hsb_to_rgb(hsb_scale_zero_max(hsb));
    }

    state.animation_step += state.animation_speed * 10;

    if (state.animation_step > 2400) {
        state.animation_step = 0;
    }
}

static void zmk_rgb_underglow_effect_spectrum() {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        struct zmk_led_hsb hsb = state.color;
        hsb.h = state.animation_step;

        pixels[i] = hsb_to_rgb(hsb_scale_min_max(hsb));
    }

    state.animation_step += state.animation_speed;
    state.animation_step = state.animation_step % HUE_MAX;
}

static void zmk_rgb_underglow_effect_swirl() {
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        struct zmk_led_hsb hsb = state.color;
        hsb.h = (HUE_MAX / STRIP_NUM_PIXELS * i + state.animation_step) % HUE_MAX;

        pixels[i] = hsb_to_rgb(hsb_scale_min_max(hsb));
    }

    state.animation_step += state.animation_speed * 2;
    state.animation_step = state.animation_step % HUE_MAX;
}

static void zmk_rgb_underglow_tick(struct k_work *work) {
    switch (state.current_effect) {
    case UNDERGLOW_EFFECT_INDICATOR:
        zmk_rgb_underglow_effect_indicator();
        break;
    case UNDERGLOW_EFFECT_SOLID:
        zmk_rgb_underglow_effect_solid();
        break;
    case UNDERGLOW_EFFECT_BREATHE:
        zmk_rgb_underglow_effect_breathe();
        break;
    case UNDERGLOW_EFFECT_SPECTRUM:
        zmk_rgb_underglow_effect_spectrum();
        break;
    case UNDERGLOW_EFFECT_SWIRL:
        zmk_rgb_underglow_effect_swirl();
        break;
    }

    led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
}

K_WORK_DEFINE(underglow_work, zmk_rgb_underglow_tick);

static void zmk_rgb_underglow_tick_handler(struct k_timer *timer) {
    if (!state.on) {
        return;
    }

    k_work_submit(&underglow_work);
}

K_TIMER_DEFINE(underglow_tick, zmk_rgb_underglow_tick_handler, NULL);

#if IS_ENABLED(CONFIG_SETTINGS)
static int rgb_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "state", &next) && !next) {
        if (len != sizeof(state)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &state, sizeof(state));
        if (rc >= 0) {
            return 0;
        }

        return rc;
    }

    return -ENOENT;
}

struct settings_handler rgb_conf = {.name = "rgb/underglow", .h_set = rgb_settings_set};

static void zmk_rgb_underglow_save_state_work() {
    settings_save_one("rgb/underglow/state", &state, sizeof(state));
}

static struct k_work_delayable underglow_save_work;
#endif

static int zmk_rgb_underglow_init(const struct device *_arg) {
    led_strip = device_get_binding(STRIP_LABEL);
    if (led_strip) {
        LOG_INF("Found LED strip device %s", STRIP_LABEL);
    } else {
        LOG_ERR("LED strip device %s not found", STRIP_LABEL);
        return -EINVAL;
    }

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER)
    ext_power = device_get_binding("EXT_POWER");
    if (ext_power == NULL) {
        LOG_ERR("Unable to retrieve ext_power device: EXT_POWER");
    }
#endif

    state = (struct rgb_underglow_state){
        color : {
            h : CONFIG_ZMK_RGB_UNDERGLOW_HUE_START,
            s : CONFIG_ZMK_RGB_UNDERGLOW_SAT_START,
            b : CONFIG_ZMK_RGB_UNDERGLOW_BRT_START,
        },
        animation_speed : CONFIG_ZMK_RGB_UNDERGLOW_SPD_START,
        current_effect : CONFIG_ZMK_RGB_UNDERGLOW_EFF_START,
        animation_step : 0,
        on : IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_ON_START)
    };

#if IS_ENABLED(CONFIG_SETTINGS)
    settings_subsys_init();

    int err = settings_register(&rgb_conf);
    if (err) {
        LOG_ERR("Failed to register the ext_power settings handler (err %d)", err);
        return err;
    }

    k_work_init_delayable(&underglow_save_work, zmk_rgb_underglow_save_state_work);

    settings_load_subtree("rgb/underglow");
#endif

    k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(25));

    return 0;
}

int zmk_rgb_underglow_save_state() {
#if IS_ENABLED(CONFIG_SETTINGS)
    int ret = k_work_reschedule(&underglow_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(ret, 0);
#else
    return 0;
#endif
}

int zmk_rgb_underglow_get_state(bool *on_off) {
    if (!led_strip)
        return -ENODEV;

    *on_off = state.on;
    return 0;
}

int zmk_rgb_underglow_on() {
    if (!led_strip)
        return -ENODEV;

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER)
    if (ext_power != NULL) {
        int rc = ext_power_enable(ext_power);
        if (rc != 0) {
            LOG_ERR("Unable to enable EXT_POWER: %d", rc);
        }
    }
#endif

    state.on = true;
    state.animation_step = 0;
    k_timer_start(&underglow_tick, K_NO_WAIT, K_MSEC(25));

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_off() {
    if (!led_strip)
        return -ENODEV;

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER)
    if (ext_power != NULL) {
        int rc = ext_power_disable(ext_power);
        if (rc != 0) {
            LOG_ERR("Unable to disable EXT_POWER: %d", rc);
        }
    }
#endif

    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i] = (struct led_rgb){r : 0, g : 0, b : 0};
    }

    led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);

    k_timer_stop(&underglow_tick);
    state.on = false;

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_calc_effect(int direction) {
    return (state.current_effect + UNDERGLOW_EFFECT_NUMBER + direction) % UNDERGLOW_EFFECT_NUMBER;
}

int zmk_rgb_underglow_select_effect(int effect) {
    if (!led_strip)
        return -ENODEV;

    if (effect < 0 || effect >= UNDERGLOW_EFFECT_NUMBER) {
        return -EINVAL;
    }

    state.current_effect = effect;
    state.animation_step = 0;

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_cycle_effect(int direction) {
    return zmk_rgb_underglow_select_effect(zmk_rgb_underglow_calc_effect(direction));
}

int zmk_rgb_underglow_toggle() {
    return state.on ? zmk_rgb_underglow_off() : zmk_rgb_underglow_on();
}

int zmk_rgb_underglow_set_hsb(struct zmk_led_hsb color) {
    if (color.h > HUE_MAX || color.s > SAT_MAX || color.b > BRT_MAX) {
        return -ENOTSUP;
    }

    state.color = color;

    return 0;
}

struct zmk_led_hsb zmk_rgb_underglow_calc_hue(int direction) {
    struct zmk_led_hsb color = state.color;

    color.h += HUE_MAX + (direction * CONFIG_ZMK_RGB_UNDERGLOW_HUE_STEP);
    color.h %= HUE_MAX;

    return color;
}

struct zmk_led_hsb zmk_rgb_underglow_calc_sat(int direction) {
    struct zmk_led_hsb color = state.color;

    int s = color.s + (direction * CONFIG_ZMK_RGB_UNDERGLOW_SAT_STEP);
    if (s < 0) {
        s = 0;
    } else if (s > SAT_MAX) {
        s = SAT_MAX;
    }
    color.s = s;

    return color;
}

struct zmk_led_hsb zmk_rgb_underglow_calc_brt(int direction) {
    struct zmk_led_hsb color = state.color;

    int b = color.b + (direction * CONFIG_ZMK_RGB_UNDERGLOW_BRT_STEP);
    color.b = CLAMP(b, 0, BRT_MAX);

    return color;
}

int zmk_rgb_underglow_change_hue(int direction) {
    if (!led_strip)
        return -ENODEV;

    state.color = zmk_rgb_underglow_calc_hue(direction);

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_change_sat(int direction) {
    if (!led_strip)
        return -ENODEV;

    state.color = zmk_rgb_underglow_calc_sat(direction);

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_change_brt(int direction) {
    if (!led_strip)
        return -ENODEV;

    state.color = zmk_rgb_underglow_calc_brt(direction);

    return zmk_rgb_underglow_save_state();
}

int zmk_rgb_underglow_change_spd(int direction) {
    if (!led_strip)
        return -ENODEV;

    if (state.animation_speed == 1 && direction < 0) {
        return 0;
    }

    state.animation_speed += direction;

    if (state.animation_speed > 5) {
        state.animation_speed = 5;
    }

    return zmk_rgb_underglow_save_state();
}

SYS_INIT(zmk_rgb_underglow_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
