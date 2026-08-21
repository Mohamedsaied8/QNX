/*
 * gpio_dcmd.h - public devctl() interface of the single-device GPIO
 *               resource manager (/dev/gpio0) for the Raspberry Pi 4.
 *
 * Include this from any client that wants programmatic access.  The same
 * functionality is also reachable from the shell through read()/write()
 * on the device, see README.md.
 */
#ifndef GPIO_DCMD_H_
#define GPIO_DCMD_H_

#include <devctl.h>
#include <stdint.h>

#define GPIO_DEV_PATH       "/dev/gpio0"

/* Highest GPIO the manager exposes.  BCM2711 has 58 GPIOs, but only 0..27
 * are routed to the 40-pin header; 28..57 drive on-board peripherals and
 * are refused so a stray write cannot kill the SD card or the Ethernet PHY. */
#define GPIO_MAX_PIN        27u

/* Pin function - the values are the raw BCM2711 GPFSEL encoding. */
enum {
    GPIO_FUNC_INPUT  = 0,
    GPIO_FUNC_OUTPUT = 1,
    GPIO_FUNC_ALT5   = 2,
    GPIO_FUNC_ALT4   = 3,
    GPIO_FUNC_ALT0   = 4,
    GPIO_FUNC_ALT1   = 5,
    GPIO_FUNC_ALT2   = 6,
    GPIO_FUNC_ALT3   = 7
};

/* Internal pull resistor - raw BCM2711 GPIO_PUP_PDN_CNTRL encoding. */
enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP   = 1,
    GPIO_PULL_DOWN = 2
};

typedef struct {
    uint32_t    pin;    /* 0 .. GPIO_MAX_PIN                                */
    uint32_t    func;   /* GPIO_FUNC_*                                      */
    uint32_t    pull;   /* GPIO_PULL_*                                      */
    uint32_t    value;  /* initial output level; ignored for inputs         */
} gpio_config_t;

typedef struct {
    uint32_t    pin;    /* 0 .. GPIO_MAX_PIN                                */
    uint32_t    value;  /* 0 or 1                                           */
} gpio_value_t;

/*
 * __DIOT  : data travels client -> manager
 * __DIOF  : data travels manager -> client
 * __DIOTF : both directions (we send the pin, we get the answer back)
 *
 * _DCMD_MISC is the class reserved for private, device-specific commands.
 */
#define DCMD_GPIO_SET_CONFIG    __DIOT(_DCMD_MISC,  1, gpio_config_t)
#define DCMD_GPIO_GET_CONFIG    __DIOTF(_DCMD_MISC, 2, gpio_config_t)
#define DCMD_GPIO_WRITE         __DIOT(_DCMD_MISC,  3, gpio_value_t)
#define DCMD_GPIO_READ          __DIOTF(_DCMD_MISC, 4, gpio_value_t)
#define DCMD_GPIO_TOGGLE        __DIOT(_DCMD_MISC,  5, gpio_value_t)

#endif /* GPIO_DCMD_H_ */
