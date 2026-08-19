/*
 * Custom OLED status screen for Leonel's wireless Sofle.
 *
 * Display: SSD1306 128x32
 * Controller: nice!nano v2
 *
 * Copyright (c) 2026 Leonel Pinheiro Correa
 * SPDX-License-Identifier: MIT
 */

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


/* -------------------------------------------------------------------------- */
/* CONFIGURATION                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Name displayed in the upper-left corner.
 *
 * Keep this text short because the OLED is only 128 pixels wide.
 */
#define OWNER_NAME "LEONEL"

/*
 * Screen refresh interval.
 *
 * 500 ms is fast enough for battery, layer, connection and WPM information
 * without updating the OLED unnecessarily often.
 */
#define SCREEN_REFRESH_INTERVAL_MS 500


/* -------------------------------------------------------------------------- */
/* LVGL OBJECTS                                                               */
/* -------------------------------------------------------------------------- */

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


/* -------------------------------------------------------------------------- */
/* TIMER                                                                      */
/* -------------------------------------------------------------------------- */

static lv_timer_t *screen_update_timer;


/* -------------------------------------------------------------------------- */
/* TEXT BUFFERS                                                               */
/* -------------------------------------------------------------------------- */

static char battery_text[8];

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static char bluetooth_text[12];
static char layer_text[12];
static char wpm_text[12];

#endif


/* -------------------------------------------------------------------------- */
/* LAYER NAMES                                                                */
/* -------------------------------------------------------------------------- */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/*
 * Returns a short display name for the active layer.
 *
 * The numbers correspond to the order in which the layers appear inside
 * the keymap node.
 *
 * Original Sofle keymap:
 *   0 = Base
 *   1 = Lower
 *   2 = Raise
 *   3 = Adjust
 *
 * Additional names have been included for the Selenium keymap.
 * If your layer order changes, update this function.
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

    case 4:
        return "NAV";

    case 5:
        return "NUM";

    case 6:
        return "FN";

    default:
        return "LAYER";
    }
}

#endif


/* -------------------------------------------------------------------------- */
/* LABEL HELPERS                                                              */
/* -------------------------------------------------------------------------- */

static lv_obj_t *create_label(lv_obj_t *parent, lv_coord_t x, lv_coord_t y) {
    lv_obj_t *label = lv_label_create(parent);

    /*
     * Place the label using fixed coordinates.
     *
     * Fixed positioning is useful on a 128x32 display because it gives
     * predictable control over every available pixel.
     */
    lv_obj_set_pos(label, x, y);

    /*
     * Use the default ZMK/LVGL font.
     *
     * This avoids introducing external fonts and reduces flash and RAM usage.
     */
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);

    /*
     * White text is visible on the black SSD1306 background.
     */
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);

    return label;
}


/* -------------------------------------------------------------------------- */
/* SCREEN UPDATE                                                              */
/* -------------------------------------------------------------------------- */

static void update_battery(void) {
    uint8_t battery_level = zmk_battery_state_of_charge();

    snprintf(
        battery_text,
        sizeof(battery_text),
        "%u%%",
        battery_level
    );

    lv_label_set_text(battery_label, battery_text);
}


#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static void update_bluetooth(void) {
    /*
     * ZMK internally numbers Bluetooth profiles from zero:
     *
     *   internal profile 0 = displayed as BT1
     *   internal profile 1 = displayed as BT2
     *   internal profile 2 = displayed as BT3
     *
     * Adding one makes the display easier to understand.
     */
    uint8_t profile = zmk_ble_active_profile_index();
    bool connected = zmk_ble_active_profile_is_connected();

    /*
     * ASCII V and X are used instead of Unicode check-mark glyphs.
     *
     * The default OLED font may not include the Unicode check mark.
     * A missing glyph could appear as another square or block.
     */
    snprintf(
        bluetooth_text,
        sizeof(bluetooth_text),
        "BT%u %s",
        profile + 1,
        connected ? "V" : "X"
    );

    lv_label_set_text(bluetooth_label, bluetooth_text);
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

    lv_label_set_text(layer_label, layer_text);
}


static void update_wpm(void) {
    uint8_t wpm = zmk_wpm_get_state();

    snprintf(
        wpm_text,
        sizeof(wpm_text),
        "WPM %u",
        wpm
    );

    lv_label_set_text(wpm_label, wpm_text);
}

#endif


static void update_screen(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    update_battery();

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

    update_bluetooth();
    update_layer();
    update_wpm();

#endif
}


/* -------------------------------------------------------------------------- */
/* CUSTOM STATUS SCREEN                                                       */
/* -------------------------------------------------------------------------- */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /*
     * Remove borders, padding, corner radius and scrollbars.
     *
     * The default LVGL object style may reserve pixels around the border.
     * Removing those values allows use of the full 128x32 area.
     */
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);


    /* ---------------------------------------------------------------------- */
    /* COMMON INFORMATION                                                     */
    /* ---------------------------------------------------------------------- */

    /*
     * First line:
     *
     *   LEONEL                                  87%
     */
    name_label = create_label(screen, 0, 0);
    lv_label_set_text(name_label, OWNER_NAME);

    battery_label = create_label(screen, 96, 0);


#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

    /* ---------------------------------------------------------------------- */
    /* CENTRAL, NORMALLY THE LEFT SIDE                                        */
    /* ---------------------------------------------------------------------- */

    /*
     * Second line:
     *
     *   BT2 V                                  BASE
     */
    bluetooth_label = create_label(screen, 0, 11);
    layer_label = create_label(screen, 82, 11);

    /*
     * Third line:
     *
     *   WPM 42
     */
    wpm_label = create_label(screen, 0, 22);


#else

    /* ---------------------------------------------------------------------- */
    /* PERIPHERAL, NORMALLY THE RIGHT SIDE                                    */
    /* ---------------------------------------------------------------------- */

    /*
     * The peripheral does not execute the main keymap and does not have
     * the host Bluetooth profile information used by the central.
     */
    side_label = create_label(screen, 0, 11);
    lv_label_set_text(side_label, "SOFLE RIGHT");

    role_label = create_label(screen, 0, 22);
    lv_label_set_text(role_label, "PERIPHERAL");

#endif


    /*
     * Populate the labels immediately.
     */
    update_screen(NULL);

    /*
     * Refresh the information every 500 milliseconds.
     */
    screen_update_timer = lv_timer_create(
        update_screen,
        SCREEN_REFRESH_INTERVAL_MS,
        NULL
    );

    /*
     * Silence an unused-variable warning on configurations where LVGL
     * manages the timer internally after creation.
     */
    ARG_UNUSED(screen_update_timer);

    return screen;
}
