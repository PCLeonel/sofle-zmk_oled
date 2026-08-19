/*
 * Custom OLED status screen for Leonel's wireless Sofle.
 *
 * Display: SSD1306 128x32
 * Controller: nice!nano v2
 *
 * Copyright (c) 2026 Leonel Pinheiro Correa
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <lvgl.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <zmk/battery.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/ble.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>
#endif


/* =============================================================================
 * PERSONALIZATION
 * =============================================================================
 */

/*
 * Name displayed in the upper-left corner.
 *
 * Keep the name reasonably short because the OLED is only 128 pixels wide.
 */
#define OWNER_NAME "LEONEL"

/*
 * Refresh interval for dynamic information.
 *
 * 500 milliseconds is sufficient for:
 *   - battery percentage;
 *   - Bluetooth connection;
 *   - active Bluetooth profile;
 *   - active layer;
 *   - WPM.
 */
#define SCREEN_REFRESH_INTERVAL_MS 500


/* =============================================================================
 * LVGL OBJECTS
 * =============================================================================
 */

static lv_obj_t *name_label;
static lv_obj_t *battery_label;

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static lv_obj_t *bluetooth_label;
static lv_obj_t *layer_label;
static lv_obj_t *wpm_label;

#else

static lv_obj_t *side_label;
static lv_obj_t *role_label;

#endif


/* =============================================================================
 * TIMER
 * =============================================================================
 */

static lv_timer_t *screen_update_timer;


/* =============================================================================
 * TEXT BUFFERS
 * =============================================================================
 */

static char battery_text[8];

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/*
 * This buffer is larger because ✓ and ✕ are multibyte UTF-8 characters.
 */
static char bluetooth_text[20];

static char layer_text[16];
static char wpm_text[12];

#endif


/* =============================================================================
 * ORIGINAL SOFLE LAYER NAMES
 * =============================================================================
 */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/*
 * Layer order from the original Sofle keymap:
 *
 *   0 = BASE
 *   1 = LOWER
 *   2 = RAISE
 *   3 = ADJUST
 *
 * LOWER + RAISE activate ADJUST through the conditional layer configured
 * in sofle.keymap.
 */
static const char *get_layer_name(uint8_t layer) {
    switch (layer) {
    case 0:
        return "BASE";

    case 1:
        return "LOWER";

    case 2:
        return "RAISE";

    case 3:
        return "ADJUST";

    default:
        return "LAYER";
    }
}

#endif


/* =============================================================================
 * LABEL CREATION
 * =============================================================================
 */

static lv_obj_t *create_label(
    lv_obj_t *parent,
    lv_coord_t x,
    lv_coord_t y
) {
    lv_obj_t *label = lv_label_create(parent);

    /*
     * Fixed positioning gives predictable control over the limited
     * 128x32 OLED area.
     */
    lv_obj_set_pos(label, x, y);

    /*
     * Use the default font already compiled into the firmware.
     *
     * The current build already renders ✓ and ✕ correctly, so no additional
     * font configuration is required.
     */
    lv_obj_set_style_text_font(
        label,
        LV_FONT_DEFAULT,
        LV_PART_MAIN
    );

    /*
     * White text on the black OLED background.
     */
    lv_obj_set_style_text_color(
        label,
        lv_color_white(),
        LV_PART_MAIN
    );

    return label;
}


/* =============================================================================
 * BATTERY
 * =============================================================================
 */

static void update_battery(void) {
    uint8_t battery_level = zmk_battery_state_of_charge();

    snprintf(
        battery_text,
        sizeof(battery_text),
        "%u%%",
        battery_level
    );

    lv_label_set_text(
        battery_label,
        battery_text
    );
}


/* =============================================================================
 * CENTRAL SIDE INFORMATION
 * =============================================================================
 */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static void update_bluetooth(void) {
    /*
     * ZMK numbers the Bluetooth profiles internally from zero:
     *
     *   internal 0 = BT1
     *   internal 1 = BT2
     *   internal 2 = BT3
     *   internal 3 = BT4
     *   internal 4 = BT5
     */
    uint8_t profile = zmk_ble_active_profile_index();

    /*
     * Returns true when the currently selected Bluetooth profile
     * is connected to a host.
     */
    bool connected = zmk_ble_active_profile_is_connected();

    /*
     * Display examples:
     *
     *   BT1 ✓
     *   BT2 ✕
     */
    snprintf(
        bluetooth_text,
        sizeof(bluetooth_text),
        "BT%u %s",
        profile + 1,
        connected ? "✓" : "✕"
    );

    lv_label_set_text(
        bluetooth_label,
        bluetooth_text
    );
}


static void update_layer(void) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    const char *name = get_layer_name(layer);

    snprintf(
        layer_text,
        sizeof(layer_text),
        "%s",
        name
    );

    lv_label_set_text(
        layer_label,
        layer_text
    );
}


static void update_wpm(void) {
    uint8_t wpm = zmk_wpm_get_state();

    snprintf(
        wpm_text,
        sizeof(wpm_text),
        "WPM %u",
        wpm
    );

    lv_label_set_text(
        wpm_label,
        wpm_text
    );
}

#endif


/* =============================================================================
 * PERIODIC SCREEN UPDATE
 * =============================================================================
 */

static void update_screen(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    /*
     * The battery is local to each nice!nano.
     *
     * On the left screen, this is the left-side battery.
     * On the right screen, this is the right-side battery.
     */
    update_battery();

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

    /*
     * These values belong to the central side:
     *
     *   - host Bluetooth profile;
     *   - host connection status;
     *   - active keymap layer;
     *   - words per minute.
     */
    update_bluetooth();
    update_layer();
    update_wpm();

#endif
}


/* =============================================================================
 * CUSTOM ZMK STATUS SCREEN
 * =============================================================================
 */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /*
     * Configure a clean black screen using the full 128x32 area.
     */
    lv_obj_set_style_bg_color(
        screen,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        screen,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        screen,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        screen,
        0,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        screen,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /* =========================================================================
     * FIRST LINE, BOTH SIDES
     * =========================================================================
     *
     * Expected display:
     *
     *   LEONEL                                  87%
     */

    name_label = create_label(
        screen,
        0,
        0
    );

    lv_label_set_text(
        name_label,
        OWNER_NAME
    );

    battery_label = create_label(
        screen,
        96,
        0
    );


#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

    /* =========================================================================
     * CENTRAL SIDE, NORMALLY LEFT
     * =========================================================================
     *
     * Complete expected screen:
     *
     *   LEONEL      87%
     *   BT2 ✓      BASE
     *   WPM 42
     */

    bluetooth_label = create_label(
        screen,
        0,
        11
    );

    layer_label = create_label(
        screen,
        82,
        11
    );

    wpm_label = create_label(
        screen,
        0,
        22
    );


#else

    /* =========================================================================
     * PERIPHERAL SIDE, NORMALLY RIGHT
     * =========================================================================
     *
     * Complete expected screen:
     *
     *   LEONEL      91%
     *   SOFLE RIGHT
     *   PERIPHERAL
     */

    side_label = create_label(
        screen,
        0,
        11
    );

    lv_label_set_text(
        side_label,
        "SOFLE RIGHT"
    );

    role_label = create_label(
        screen,
        0,
        22
    );

    lv_label_set_text(
        role_label,
        "PERIPHERAL"
    );

#endif


    /*
     * Populate all labels before presenting the screen.
     */
    update_screen(NULL);

    /*
     * Update dynamic information every 500 milliseconds.
     */
    screen_update_timer = lv_timer_create(
        update_screen,
        SCREEN_REFRESH_INTERVAL_MS,
        NULL
    );

    ARG_UNUSED(screen_update_timer);

    return screen;
}
