/*
 * gpio_resmgr.c - Example 1: a minimal single-threaded GPIO resource manager
 *                 for the Raspberry Pi 4 (BCM2711) running QNX 8.0.
 *
 * It publishes ONE device, /dev/gpio0, which speaks two languages:
 *
 *   - devctl()      : the typed API declared in gpio_dcmd.h
 *   - read()/write(): a plain-text API so the board can be driven from the
 *                     shell, e.g.  echo "17 out 1" > /dev/gpio0
 *                                  cat /dev/gpio0
 *
 * Everything runs in a single thread: dispatch_block() receives one message,
 * dispatch_handler() dispatches it to one of the io_* callbacks below, repeat.
 * That is the smallest complete resource manager shape; example 2
 * (gpio_pin_resmgr) shows the thread-pool / one-device-per-pin version.
 *
 * Build : QNX 8.0 SDP, aarch64le
 * Run   : must be root (mmap_device_memory() needs the "mem_phys" ability)
 *             gpio_resmgr &
 *             gpio_resmgr -n /dev/mygpio -b 0xFE000000 -v &
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* <sys/iofunc.h> must come first: it defines RESMGR_OCB_T as iofunc_ocb_t,
 * which <sys/resmgr.h> (pulled in by <sys/dispatch.h>) would otherwise
 * default to void, and every ocb->attr reference would fail to compile. */
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>

#include "gpio_dcmd.h"

/* ------------------------------------------------------------------ */
/* BCM2711 GPIO block                                                  */
/* ------------------------------------------------------------------ */

/* QNX boots the Pi 4 in "low peripheral" mode, so the peripherals live at
 * 0xFE000000 (they are at 0xFC000000 in the 35-bit full address map). */
#define PERIPH_BASE_DEFAULT     0xFE000000u
#define GPIO_BLOCK_OFFSET       0x00200000u
#define GPIO_BLOCK_SIZE         0x1000u

/* Register indices, expressed in 32-bit words. */
#define R_GPFSEL0               (0x00 / 4)  /* 6 x function select          */
#define R_GPSET0                (0x1C / 4)  /* write-1-to-set output        */
#define R_GPCLR0                (0x28 / 4)  /* write-1-to-clear output      */
#define R_GPLEV0                (0x34 / 4)  /* pin level                    */
#define R_GPPUD_CNTRL0          (0xE4 / 4)  /* BCM2711 pull up/down control */

static volatile uint32_t   *gpio_regs;      /* mapped register block        */
static bool                 verbose;

/* Device registers are mapped PROT_NOCACHE, but the CPU may still reorder
 * accesses around them; a full barrier keeps the register writes in program
 * order (mandatory on the Pi 4's Cortex-A72). */
static inline void barrier(void)
{
#if defined(__aarch64__)
    __asm__ __volatile__("dsb sy" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

static inline uint32_t reg_rd(unsigned idx)
{
    uint32_t v = gpio_regs[idx];
    barrier();
    return v;
}

static inline void reg_wr(unsigned idx, uint32_t val)
{
    barrier();
    gpio_regs[idx] = val;
    barrier();
}

static void hw_set_func(unsigned pin, unsigned func)
{
    unsigned idx   = R_GPFSEL0 + pin / 10;
    unsigned shift = (pin % 10) * 3;
    uint32_t v     = reg_rd(idx);

    /* Read-modify-write: one GPFSEL register holds 10 pins, so this is only
     * safe because a single-threaded manager serialises every client. */
    v &= ~(0x7u << shift);
    v |= (func & 0x7u) << shift;
    reg_wr(idx, v);
}

static unsigned hw_get_func(unsigned pin)
{
    unsigned idx   = R_GPFSEL0 + pin / 10;
    unsigned shift = (pin % 10) * 3;

    return (reg_rd(idx) >> shift) & 0x7u;
}

static void hw_set_pull(unsigned pin, unsigned pull)
{
    /* BCM2711 dropped the BCM2835 clock-in sequence: pulls are now a plain
     * 2-bit field, 16 pins per register. */
    unsigned idx   = R_GPPUD_CNTRL0 + pin / 16;
    unsigned shift = (pin % 16) * 2;
    uint32_t v     = reg_rd(idx);

    v &= ~(0x3u << shift);
    v |= (pull & 0x3u) << shift;
    reg_wr(idx, v);
}

static unsigned hw_get_pull(unsigned pin)
{
    unsigned idx   = R_GPPUD_CNTRL0 + pin / 16;
    unsigned shift = (pin % 16) * 2;

    return (reg_rd(idx) >> shift) & 0x3u;
}

static void hw_set_level(unsigned pin, unsigned value)
{
    /* GPSET/GPCLR are write-1-to-act, so no read-modify-write and no race
     * with a client touching a neighbouring pin. */
    reg_wr((value ? R_GPSET0 : R_GPCLR0) + pin / 32, 1u << (pin % 32));
}

static unsigned hw_get_level(unsigned pin)
{
    return (reg_rd(R_GPLEV0 + pin / 32) >> (pin % 32)) & 1u;
}

/* ------------------------------------------------------------------ */
/* Resource manager plumbing                                           */
/* ------------------------------------------------------------------ */

static resmgr_connect_funcs_t   connect_funcs;
static resmgr_io_funcs_t        io_funcs;
static iofunc_attr_t            attr;

/*
 * Scratch buffer used by io_read().  The reply IOV handed back with
 * _RESMGR_NPARTS() is used by the library AFTER this function returns, so it
 * must not point at a stack frame that has already been popped.  A file-scope
 * buffer is safe here only because the manager is single-threaded - the
 * thread-pool version in example 2 keeps its buffer in the per-thread
 * resmgr context instead.
 */
static char                     readbuf[2048];

static const char *func_name(unsigned f)
{
    static const char *names[] = { "in", "out", "alt5", "alt4",
                                   "alt0", "alt1", "alt2", "alt3" };
    return names[f & 0x7u];
}

static const char *pull_name(unsigned p)
{
    switch (p) {
    case GPIO_PULL_UP:   return "up";
    case GPIO_PULL_DOWN: return "down";
    case GPIO_PULL_NONE: return "none";
    default:             return "?";
    }
}

/* Render the state of every exported pin into readbuf. */
static size_t snapshot(void)
{
    size_t len = 0;

    len += snprintf(readbuf + len, sizeof(readbuf) - len,
                    "%-4s %-5s %-5s %s\n", "gpio", "func", "pull", "level");

    for (unsigned pin = 0; pin <= GPIO_MAX_PIN; pin++) {
        len += snprintf(readbuf + len, sizeof(readbuf) - len,
                        "%-4u %-5s %-5s %u\n",
                        pin, func_name(hw_get_func(pin)),
                        pull_name(hw_get_pull(pin)), hw_get_level(pin));
        if (len >= sizeof(readbuf))
            return sizeof(readbuf) - 1;
    }
    return len;
}

/*
 * read() - hand back the snapshot as a normal seekable file so that plain
 * `cat /dev/gpio0` works (and terminates, once the offset reaches EOF).
 */
static int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK)
        return status;

    /* We do not implement readcond()/pread()-style extended types. */
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;

    size_t total = (ocb->offset == 0) ? snapshot() : strlen(readbuf);
    size_t left  = (size_t)ocb->offset >= total ? 0 : total - (size_t)ocb->offset;
    size_t nsend = min(msg->i.nbytes, left);

    if (nsend > 0) {
        SETIOV(ctp->iov, readbuf + ocb->offset, nsend);
        ocb->offset += nsend;
        ocb->attr->flags |= IOFUNC_ATTR_ATIME | IOFUNC_ATTR_DIRTY_TIME;
    }
    _IO_SET_READ_NBYTES(ctp, nsend);

    return nsend > 0 ? _RESMGR_NPARTS(1) : _RESMGR_NPARTS(0);
}

/*
 * One text command.  Accepted forms (whitespace or '=' separated):
 *
 *   17 out        17 out 1       17 in        17 in up
 *   17 = 1        17 1           17 toggle
 */
static int apply_command(char *line)
{
    char    *save = NULL;
    char    *tok  = strtok_r(line, " \t=,", &save);

    if (tok == NULL)
        return EOK;                     /* blank line - nothing to do */

    char        *end;
    unsigned long pin = strtoul(tok, &end, 0);
    if (*end != '\0' || pin > GPIO_MAX_PIN)
        return EINVAL;

    tok = strtok_r(NULL, " \t=,", &save);
    if (tok == NULL)
        return EINVAL;

    if (strcmp(tok, "in") == 0) {
        unsigned pull = GPIO_PULL_NONE;
        tok = strtok_r(NULL, " \t=,", &save);
        if (tok != NULL) {
            if (strcmp(tok, "up") == 0)        pull = GPIO_PULL_UP;
            else if (strcmp(tok, "down") == 0) pull = GPIO_PULL_DOWN;
            else if (strcmp(tok, "none") == 0) pull = GPIO_PULL_NONE;
            else                               return EINVAL;
        }
        hw_set_pull(pin, pull);
        hw_set_func(pin, GPIO_FUNC_INPUT);
        return EOK;
    }

    if (strcmp(tok, "out") == 0) {
        tok = strtok_r(NULL, " \t=,", &save);
        if (tok != NULL) {
            if (strcmp(tok, "0") && strcmp(tok, "1"))
                return EINVAL;
            hw_set_level(pin, tok[0] - '0');   /* drive before enabling */
        }
        hw_set_func(pin, GPIO_FUNC_OUTPUT);
        return EOK;
    }

    if (strcmp(tok, "toggle") == 0) {
        hw_set_level(pin, !hw_get_level(pin));
        return EOK;
    }

    if (strcmp(tok, "0") == 0 || strcmp(tok, "1") == 0) {
        if (hw_get_func(pin) != GPIO_FUNC_OUTPUT)
            return EPERM;               /* refuse to drive a non-output */
        hw_set_level(pin, tok[0] - '0');
        return EOK;
    }

    return EINVAL;
}

static int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
    char cmd[256];

    int status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK)
        return status;

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;

    if (msg->i.nbytes >= sizeof(cmd))
        return EINVAL;

    /* The header sits at the front of the received message; the payload may
     * still be in the client's address space, so pull it across explicitly. */
    int nread = resmgr_msgread(ctp, cmd, msg->i.nbytes, sizeof(msg->i));
    if (nread < 0)
        return errno;
    cmd[nread] = '\0';

    for (char *save = NULL, *line = strtok_r(cmd, "\n", &save);
         line != NULL;
         line = strtok_r(NULL, "\n", &save)) {
        status = apply_command(line);
        if (status != EOK)
            return status;
    }

    /* Report the whole buffer as consumed so the shell's echo does not spin. */
    _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);
    ocb->attr->flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_DIRTY_TIME;

    return EOK;
}

static int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb)
{
    int status = iofunc_devctl_default(ctp, msg, ocb);
    if (status != _RESMGR_DEFAULT)
        return status;                  /* handled (or rejected) generically */

    /* Payload of a devctl message always starts right after the header. */
    void *data = _DEVCTL_DATA(msg->i);

    switch (msg->i.dcmd) {

    case DCMD_GPIO_SET_CONFIG: {
        gpio_config_t *cfg = data;

        if (cfg->pin > GPIO_MAX_PIN || cfg->func > 7 || cfg->pull > GPIO_PULL_DOWN)
            return EINVAL;

        hw_set_pull(cfg->pin, cfg->pull);
        if (cfg->func == GPIO_FUNC_OUTPUT)
            hw_set_level(cfg->pin, cfg->value ? 1 : 0);
        hw_set_func(cfg->pin, cfg->func);

        if (verbose)
            printf("gpio_resmgr: pin %u -> func %s pull %s\n",
                   cfg->pin, func_name(cfg->func), pull_name(cfg->pull));

        memset(&msg->o, 0, sizeof(msg->o));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o));
    }

    case DCMD_GPIO_GET_CONFIG: {
        gpio_config_t *cfg = data;

        if (cfg->pin > GPIO_MAX_PIN)
            return EINVAL;

        gpio_config_t out = {
            .pin   = cfg->pin,
            .func  = hw_get_func(cfg->pin),
            .pull  = hw_get_pull(cfg->pin),
            .value = hw_get_level(cfg->pin)
        };

        memset(&msg->o, 0, sizeof(msg->o));
        msg->o.nbytes = sizeof(out);
        memcpy(_DEVCTL_DATA(msg->o), &out, sizeof(out));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + sizeof(out));
    }

    case DCMD_GPIO_WRITE: {
        gpio_value_t *v = data;

        if (v->pin > GPIO_MAX_PIN)
            return EINVAL;
        if (hw_get_func(v->pin) != GPIO_FUNC_OUTPUT)
            return EPERM;

        hw_set_level(v->pin, v->value ? 1 : 0);

        memset(&msg->o, 0, sizeof(msg->o));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o));
    }

    case DCMD_GPIO_TOGGLE: {
        gpio_value_t *v = data;

        if (v->pin > GPIO_MAX_PIN)
            return EINVAL;
        if (hw_get_func(v->pin) != GPIO_FUNC_OUTPUT)
            return EPERM;

        hw_set_level(v->pin, !hw_get_level(v->pin));

        memset(&msg->o, 0, sizeof(msg->o));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o));
    }

    case DCMD_GPIO_READ: {
        gpio_value_t *v = data;

        if (v->pin > GPIO_MAX_PIN)
            return EINVAL;

        gpio_value_t out = { .pin = v->pin, .value = hw_get_level(v->pin) };

        memset(&msg->o, 0, sizeof(msg->o));
        msg->o.nbytes = sizeof(out);
        memcpy(_DEVCTL_DATA(msg->o), &out, sizeof(out));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + sizeof(out));
    }

    default:
        return ENOTTY;                  /* unknown command for this device */
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-n path] [-b periph_base] [-v]\n"
            "  -n path         device to create   (default %s)\n"
            "  -b periph_base  BCM2711 peripheral base (default 0x%08X)\n"
            "  -v              log configuration changes\n",
            prog, GPIO_DEV_PATH, PERIPH_BASE_DEFAULT);
}

int main(int argc, char **argv)
{
    const char *devpath    = GPIO_DEV_PATH;
    uint64_t    periph_base = PERIPH_BASE_DEFAULT;
    int         c;

    while ((c = getopt(argc, argv, "n:b:vh")) != -1) {
        switch (c) {
        case 'n': devpath     = optarg;                     break;
        case 'b': periph_base = strtoull(optarg, NULL, 0);  break;
        case 'v': verbose     = true;                       break;
        default:  usage(argv[0]);                           return EXIT_FAILURE;
        }
    }

    /* Mapping physical memory needs the "mem_phys" ability.  Ask for it
     * explicitly and then drop the right to gain any further abilities, so a
     * compromised manager cannot escalate. */
    if (procmgr_ability(0,
            PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_MEM_PHYS,
            PROCMGR_AID_EOL) != EOK) {
        perror("procmgr_ability(mem_phys)");
        return EXIT_FAILURE;
    }

    gpio_regs = mmap_device_memory(NULL, GPIO_BLOCK_SIZE,
                                   PROT_READ | PROT_WRITE | PROT_NOCACHE, 0,
                                   periph_base + GPIO_BLOCK_OFFSET);
    if (gpio_regs == MAP_FAILED) {
        perror("mmap_device_memory");
        fprintf(stderr, "hint: run as root, and check the -b base address\n");
        return EXIT_FAILURE;
    }

    /* dispatch handle: owns the channel every client connects to. */
    dispatch_t *dpp = dispatch_create();
    if (dpp == NULL) {
        perror("dispatch_create");
        return EXIT_FAILURE;
    }

    /* Start from the POSIX default handlers, then override the three we care
     * about.  Everything we do not override (stat, chmod, lseek, ...) keeps
     * working for free. */
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read   = io_read;
    io_funcs.write  = io_write;
    io_funcs.devctl = io_devctl;

    iofunc_attr_init(&attr, S_IFCHR | 0666, NULL, NULL);

    resmgr_attr_t rattr = {
        .nparts_max   = 1,
        .msg_max_size = 2048
    };

    int id = resmgr_attach(dpp, &rattr, devpath, _FTYPE_ANY, 0,
                           &connect_funcs, &io_funcs, &attr);
    if (id == -1) {
        perror("resmgr_attach");
        return EXIT_FAILURE;
    }

    dispatch_context_t *ctp = dispatch_context_alloc(dpp);
    if (ctp == NULL) {
        perror("dispatch_context_alloc");
        return EXIT_FAILURE;
    }

    printf("gpio_resmgr: %s ready (regs @ 0x%" PRIx64 ", pins 0..%u)\n",
           devpath, periph_base + GPIO_BLOCK_OFFSET, GPIO_MAX_PIN);
    fflush(stdout);

    /* The whole server: block for a message, hand it to the library, repeat. */
    for (;;) {
        dispatch_context_t *next = dispatch_block(ctp);
        if (next == NULL) {
            if (errno == EINTR)
                continue;
            perror("dispatch_block");
            break;
        }
        ctp = next;
        dispatch_handler(ctp);
    }

    dispatch_context_free(ctp);
    munmap_device_memory((void *)gpio_regs, GPIO_BLOCK_SIZE);
    return EXIT_FAILURE;
}
