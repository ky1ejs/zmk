/*
 * led_driver.c — TornBlue onboard LED indicators
 *
 * Two modes, selected via Kconfig:
 *
 * CONFIG_TORNBLUE_LED_BT_PROFILE=y (Bluetooth profile indicators):
 *   LED1 = BT0, LED2 = BT1, LED3 = BT2, all three = BT3
 *
 * CONFIG_TORNBLUE_LED_BT_PROFILE=n (Layer indicators, default):
 *   LED1 = NAV (layer 1), LED2 = NUM (layer 2), LED3 = SYM (layer 3)
 *
 * Only actively driven on the central (left) half. The peripheral
 * (right) compiles this file but registers no event subscription,
 * so its LEDs remain off.
 */

#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>

#ifdef CONFIG_TORNBLUE_LED_BT_PROFILE
#include <zmk/ble.h>
#include <zmk/events/ble_active_profile_changed.h>
#else
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>
#endif

#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

#ifdef CONFIG_TORNBLUE_LED_BT_PROFILE

static void update_leds_for_profile(uint8_t profile_index) {
    /* One-hot: LED1=BT0, LED2=BT1, LED3=BT2, all three=BT3 */
    gpio_pin_set_dt(&led1, profile_index == 0 || profile_index >= 3);
    gpio_pin_set_dt(&led2, profile_index == 1 || profile_index >= 3);
    gpio_pin_set_dt(&led3, profile_index == 2 || profile_index >= 3);
}

static int led_event_handler(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev) {
        update_leds_for_profile(ev->index);
    }
    return 0;
}

#else /* Layer indicators */

static int led_event_handler(const zmk_event_t *eh) {
    const uint8_t layer = zmk_keymap_highest_layer_active();
    gpio_pin_set_dt(&led1, layer == 1); /* NAV */
    gpio_pin_set_dt(&led2, layer == 2); /* NUM */
    gpio_pin_set_dt(&led3, layer == 3); /* SYM */
    return 0;
}

#endif /* CONFIG_TORNBLUE_LED_BT_PROFILE */

static int led_init(void) {
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);

#if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && defined(CONFIG_TORNBLUE_LED_BT_PROFILE)
    /* Show the current BT profile at boot before any event fires */
    update_leds_for_profile(zmk_ble_active_profile_index());
#endif

    return 0;
}

ZMK_LISTENER(led, led_event_handler);

#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL
#ifdef CONFIG_TORNBLUE_LED_BT_PROFILE
ZMK_SUBSCRIPTION(led, zmk_ble_active_profile_changed);
#else
ZMK_SUBSCRIPTION(led, zmk_layer_state_changed);
#endif
#endif

SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
