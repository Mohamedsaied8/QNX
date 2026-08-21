/*
 * gpio_pin.h - public interface of the per-pin GPIO resource manager.
 *
 * The manager publishes one device per exported pin, /dev/gpio/<pin>, so a
 * pin can be handed to a process with plain file permissions and used from
 * the shell:
 *
 *      cat /dev/gpio/17            # "1" or "0"
 *      echo 1     > /dev/gpio/17
 *      echo out   > /dev/gpio/17
 *      echo rising > /dev/gpio/22  # arm edge detection
 *
 * Clients that need typed access or edge events use the devctl() commands
 * below; clients that just want to wait for an edge can also select()/poll()
 * on the descriptor.
 */
#ifndef GPIO_PIN_H_
#define GPIO_PIN_H_

#include <devctl.h>
#include <stdint.h>

#define GPIO_DIR            "/dev/gpio"
#define GPIO_PIN_PATH_FMT   GPIO_DIR "/%u"

/* Pin function - raw BCM2711 GPFSEL encoding. */
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

/* Edge detection, OR-able. */
#define GPIO_EDGE_NONE      0x0u
#define GPIO_EDGE_RISING    0x1u
#define GPIO_EDGE_FALLING   0x2u
#define GPIO_EDGE_BOTH      (GPIO_EDGE_RISING | GPIO_EDGE_FALLING)

typedef struct {
    uint32_t    func;       /* GPIO_FUNC_*                                  */
    uint32_t    pull;       /* GPIO_PULL_*                                  */
    uint32_t    edge;       /* GPIO_EDGE_*                                  */
    uint32_t    value;      /* current level; on SET_CONFIG, the level to
                             * drive before an output is enabled            */
} gpio_pin_config_t;

typedef struct {
    uint32_t    pin;
    uint32_t    level;      /* level sampled when the edge was detected     */
    uint64_t    timestamp;  /* CLOCK_MONOTONIC nanoseconds                  */
    uint64_t    count;      /* total edges seen on this pin since start     */
} gpio_event_t;

#define DCMD_GPIO_PIN_SET_CONFIG    __DIOT(_DCMD_MISC,  1, gpio_pin_config_t)
#define DCMD_GPIO_PIN_GET_CONFIG    __DIOF(_DCMD_MISC,  2, gpio_pin_config_t)
#define DCMD_GPIO_PIN_WRITE         __DIOT(_DCMD_MISC,  3, uint32_t)
#define DCMD_GPIO_PIN_READ          __DIOF(_DCMD_MISC,  4, uint32_t)
#define DCMD_GPIO_PIN_TOGGLE        __DION(_DCMD_MISC,  5)

/*
 * Block until the next edge is detected on this pin, then return it.
 * The manager parks the client (no reply) and answers it from its monitor
 * thread, so the client burns no CPU while waiting.  Returns ENODATA if
 * edge detection has not been armed on the pin, EINTR if the client is
 * unblocked (signal, timer_timeout(), close).
 */
#define DCMD_GPIO_PIN_WAIT_EDGE     __DIOF(_DCMD_MISC, 10, gpio_event_t)

/* Non-blocking: most recent edge (count == 0 if none since start). */
#define DCMD_GPIO_PIN_LAST_EDGE     __DIOF(_DCMD_MISC, 11, gpio_event_t)

#endif /* GPIO_PIN_H_ */
