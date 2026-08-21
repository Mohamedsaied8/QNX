/*
 * gpio_client.c - exercises /dev/gpio0 through the typed devctl() API.
 *
 *   gpio_client blink 17 20        blink GPIO17, 20 times
 *   gpio_client get   17           print func/pull/level of GPIO17
 *   gpio_client set   17 out 1     configure GPIO17 as output, drive high
 *   gpio_client set   22 in  up    configure GPIO22 as input, pull-up
 *   gpio_client watch 22           poll GPIO22 and print level changes
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/neutrino.h>

#include "gpio_dcmd.h"

static int fd = -1;

static int gpio_set(unsigned pin, unsigned func, unsigned pull, unsigned value)
{
    gpio_config_t cfg = { .pin = pin, .func = func, .pull = pull, .value = value };

    return devctl(fd, DCMD_GPIO_SET_CONFIG, &cfg, sizeof(cfg), NULL);
}

static int gpio_get(unsigned pin, gpio_config_t *out)
{
    out->pin = pin;

    return devctl(fd, DCMD_GPIO_GET_CONFIG, out, sizeof(*out), NULL);
}

static int gpio_put(unsigned pin, unsigned value)
{
    gpio_value_t v = { .pin = pin, .value = value };

    return devctl(fd, DCMD_GPIO_WRITE, &v, sizeof(v), NULL);
}

static int gpio_level(unsigned pin, unsigned *value)
{
    gpio_value_t v = { .pin = pin };
    int rc = devctl(fd, DCMD_GPIO_READ, &v, sizeof(v), NULL);

    if (rc == EOK)
        *value = v.value;
    return rc;
}

static const char *func_name(unsigned f)
{
    static const char *names[] = { "in", "out", "alt5", "alt4",
                                   "alt0", "alt1", "alt2", "alt3" };
    return names[f & 0x7u];
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s blink <pin> [count] | get <pin> | "
                "set <pin> in|out [up|down|none|0|1] | watch <pin>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *cmd = argv[1];
    unsigned    pin = (unsigned)strtoul(argv[2], NULL, 0);
    int         rc;

    fd = open("/dev/gpio", O_RDWR);
    if (fd == -1) {
        perror("open " GPIO_DEV_PATH);
        return EXIT_FAILURE;
    }

    if (strcmp(cmd, "blink") == 0) {
        unsigned count = (argc > 3) ? (unsigned)strtoul(argv[3], NULL, 0) : 10;

        rc = gpio_set(pin, GPIO_FUNC_OUTPUT, GPIO_PULL_NONE, 0);
        if (rc != EOK) {
            fprintf(stderr, "set output: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
        for (unsigned i = 0; i < count; i++) {
            gpio_put(pin, i & 1);
            delay(250);
        }
        gpio_put(pin, 0);

    } else if (strcmp(cmd, "get") == 0) {
        gpio_config_t cfg;

        rc = gpio_get(pin, &cfg);
        if (rc != EOK) {
            fprintf(stderr, "get: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
        printf("gpio%-2u func=%s pull=%u level=%u\n",
               cfg.pin, func_name(cfg.func), cfg.pull, cfg.value);

    } else if (strcmp(cmd, "set") == 0) {
        unsigned func  = GPIO_FUNC_INPUT;
        unsigned pull  = GPIO_PULL_NONE;
        unsigned value = 0;

        if (argc < 4) {
            fprintf(stderr, "set needs a direction\n");
            return EXIT_FAILURE;
        }
        func = (strcmp(argv[3], "out") == 0) ? GPIO_FUNC_OUTPUT : GPIO_FUNC_INPUT;

        if (argc > 4) {
            if      (strcmp(argv[4], "up") == 0)   pull  = GPIO_PULL_UP;
            else if (strcmp(argv[4], "down") == 0) pull  = GPIO_PULL_DOWN;
            else                                   value = (unsigned)strtoul(argv[4], NULL, 0);
        }
        rc = gpio_set(pin, func, pull, value);
        if (rc != EOK) {
            fprintf(stderr, "set: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }

    } else if (strcmp(cmd, "watch") == 0) {
        unsigned last = 2;              /* impossible level, forces first print */

        rc = gpio_set(pin, GPIO_FUNC_INPUT, GPIO_PULL_UP, 0);
        if (rc != EOK) {
            fprintf(stderr, "set input: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
        printf("watching gpio%u (Ctrl-C to stop)\n", pin);
        for (;;) {
            unsigned level;

            if (gpio_level(pin, &level) != EOK)
                break;
            if (level != last) {
                printf("gpio%u -> %u\n", pin, level);
                fflush(stdout);
                last = level;
            }
            delay(20);                  /* client-side polling; example 2 does
                                         * this properly with edge events    */
        }

    } else {
        fprintf(stderr, "unknown command '%s'\n", cmd);
        return EXIT_FAILURE;
    }

    close(fd);
    return EXIT_SUCCESS;
}
