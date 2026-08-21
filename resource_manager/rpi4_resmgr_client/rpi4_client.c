/*
 * rpi4_client.c - client for rpi4_resmgr, showing all three ways to talk to
 *                 a QNX resource manager.
 *
 * The key idea: on QNX the file descriptor returned by open() *is* a
 * connection id.  So once you have opened /dev/rpi4 you can either use the
 * POSIX calls (read/write/devctl, which are themselves MsgSend() wrappers) or
 * bypass them entirely and MsgSend() your own message struct on the same fd.
 * No ConnectAttach(), no name_open(), no second channel.
 *
 * MsgSend() is synchronous and blocking: the caller goes SEND-blocked until
 * the server receives, then REPLY-blocked until it replies, and the reply
 * lands directly in the caller's buffer - one kernel call, no copies through
 * an intermediate buffer.
 *
 * Usage:
 *   rpi4_client                      # run the whole demo
 *   rpi4_client ping
 *   rpi4_client add 20 22
 *   rpi4_client echo "hello pi"
 *   rpi4_client stat
 *   rpi4_client info                 # devctl() path
 *   rpi4_client label "kitchen pi"   # write() path
 *   rpi4_client cat                  # read() path
 *   rpi4_client reset
 *   rpi4_client bench [iterations]   # MsgSend() round-trip timing
 *   rpi4_client -d /dev/rpi4a ping   # non-default device
 */


#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/neutrino.h>

#include "rpi4_msg.h"

static const char *devpath = RPI4_DEV_PATH;

/* ------------------------------------------------------------------ */
/* The MsgSend() wrappers                                              */
/* ------------------------------------------------------------------ */

/*
 * One request, one reply.  `coid` is just the fd from open().
 *
 * MsgSend() returns whatever the server passed as MsgReply()'s status, or -1
 * with errno set to whatever it passed to MsgError().
 */
static int rpi4_send(int coid, const rpi4_msg_t *msg, size_t sndlen,
                     rpi4_reply_t *rep)
{
    if (MsgSend(coid, msg, sndlen, rep, sizeof(*rep)) == -1) {
        perror("MsgSend");
        return -1;
    }
    return 0;
}

static int rpi4_ping(int coid, rpi4_reply_t *rep)
{
    rpi4_msg_t msg = { .hdr = { .type = RPI4_MSG_PING } };

    return rpi4_send(coid, &msg, sizeof(msg.hdr), rep);
}

static int rpi4_add(int coid, int32_t a, int32_t b, rpi4_reply_t *rep)
{
    rpi4_msg_t msg = { .hdr = { .type = RPI4_MSG_ADD, .a = a, .b = b } };

    /* Send only the header - there is no payload, so there is no reason to
     * make the kernel copy the unused tail of the struct. */
    return rpi4_send(coid, &msg, sizeof(msg.hdr), rep);
}

/*
 * The same round trip, but gathered from two buffers with MsgSendv().  The
 * server still sees one contiguous message; the kernel does the stitching, so
 * a payload that already lives elsewhere never has to be memcpy()d into a
 * staging struct first.
 */
static int rpi4_echo(int coid, const char *text, rpi4_reply_t *rep)
{
    rpi4_hdr_t  hdr = { .type = RPI4_MSG_ECHO };
    iov_t       siov[2], riov[1];
    size_t      len = strlen(text);

    if (len > RPI4_PAYLOAD_MAX) {
        fprintf(stderr, "echo: payload too long (max %u)\n", RPI4_PAYLOAD_MAX);
        return -1;
    }
    hdr.len = len;

    SETIOV(&siov[0], &hdr, sizeof(hdr));
    SETIOV(&siov[1], (void *)text, len);   /* SETIOV takes void *  */
    SETIOV(&riov[0], rep, sizeof(*rep));

    if (MsgSendv(coid, siov, 2, riov, 1) == -1) {
        perror("MsgSendv");
        return -1;
    }
    return 0;
}

static int rpi4_stat_msg(int coid, rpi4_reply_t *rep)
{
    rpi4_msg_t msg = { .hdr = { .type = RPI4_MSG_STAT } };

    return rpi4_send(coid, &msg, sizeof(msg.hdr), rep);
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

static int cmd_info(int fd)
{
    rpi4_stat_t st;

    /* devctl() is MsgSend() in a POSIX costume: it packs an io_devctl_t
     * header plus your struct and sends it on the same connection. */
    int err = devctl(fd, DCMD_RPI4_STAT, &st, sizeof(st), NULL);
    if (err != EOK) {
        fprintf(stderr, "devctl(DCMD_RPI4_STAT): %s\n", strerror(err));
        return -1;
    }

    printf("label=%s msgs=%u reads=%u writes=%u devctls=%u accum=%d\n",
           st.label, st.msgs, st.reads, st.writes, st.devctls, st.accum);
    return 0;
}

static int cmd_cat(int fd)
{
    char    buf[256];
    ssize_t n;

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek");
        return -1;
    }
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, (size_t)n, stdout);

    if (n < 0) {
        perror("read");
        return -1;
    }
    return 0;
}

static int cmd_label(int fd, const char *label)
{
    if (write(fd, label, strlen(label)) == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

/* Time N ping round trips to show what a MsgSend()/MsgReply() pair costs. */
static int cmd_bench(int fd, long iterations)
{
    struct timespec t0, t1;
    rpi4_reply_t    rep;

    if (clock_gettime(CLOCK_MONOTONIC, &t0) == -1) {
        perror("clock_gettime");
        return -1;
    }

    for (long i = 0; i < iterations; i++) {
        if (rpi4_ping(fd, &rep) == -1)
            return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) == -1) {
        perror("clock_gettime");
        return -1;
    }

    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9
              + (double)(t1.tv_nsec - t0.tv_nsec);

    printf("%ld round trips in %.3f ms -> %.0f ns each (%.0f msg/s)\n",
           iterations, ns / 1e6, ns / (double)iterations,
           (double)iterations / (ns / 1e9));
    return 0;
}

static int cmd_demo(int fd)
{
    rpi4_reply_t rep;

    printf("-- MsgSend: ping\n");
    if (rpi4_ping(fd, &rep) == -1)
        return -1;
    printf("   status=%d seq=%u\n", rep.hdr.status, rep.hdr.seq);

    printf("-- MsgSend: add 20 + 22\n");
    if (rpi4_add(fd, 20, 22, &rep) == -1)
        return -1;
    printf("   result=%d seq=%u\n", rep.hdr.result, rep.hdr.seq);

    printf("-- MsgSendv: echo\n");
    if (rpi4_echo(fd, "hello from the pi", &rep) == -1)
        return -1;
    printf("   got %u bytes: \"%.*s\"\n", rep.hdr.len, (int)rep.hdr.len,
           rep.data);

    printf("-- MsgSend: stat\n");
    if (rpi4_stat_msg(fd, &rep) == -1)
        return -1;
    printf("   %s\n", rep.data);

    printf("-- write(): set the label\n");
    if (cmd_label(fd, "rpi4-demo") == -1)
        return -1;

    printf("-- devctl(): read the counters back\n");
    if (cmd_info(fd) == -1)
        return -1;

    printf("-- read(): the text status page\n");
    if (cmd_cat(fd) == -1)
        return -1;

    printf("-- MsgSend: 10000 ping round trips\n");
    return cmd_bench(fd, 10000);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-d device] [command]\n"
            "commands:\n"
            "  (none)          run the full demo\n"
            "  ping            MsgSend a ping\n"
            "  add A B         MsgSend an addition request\n"
            "  echo TEXT       MsgSendv text and get it bounced back\n"
            "  stat            MsgSend a stats request\n"
            "  info            same counters, via devctl()\n"
            "  cat             the text status page, via read()\n"
            "  label TEXT      set the label, via write()\n"
            "  reset           zero the counters, via devctl()\n"
            "  bench [N]       time N MsgSend round trips (default 10000)\n",
            prog);
}

int main(int argc, char **argv)
{
    rpi4_reply_t rep;
    int          c;

    while ((c = getopt(argc, argv, "d:h")) != -1) {
        switch (c) {
        case 'd': devpath = optarg; break;
        default:  usage(argv[0]);   return EXIT_FAILURE;
        }
    }
    argc -= optind;
    argv += optind;

    /* This fd is also the connection id we MsgSend() on. */
    int fd = open(devpath, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "open(%s): %s\nhint: is rpi4_resmgr running?\n",
                devpath, strerror(errno));
        return EXIT_FAILURE;
    }

    int rc = 0;

    if (argc == 0) {
        rc = cmd_demo(fd);

    } else if (strcmp(argv[0], "ping") == 0) {
        rc = rpi4_ping(fd, &rep);
        if (rc == 0)
            printf("pong (seq=%u)\n", rep.hdr.seq);

    } else if (strcmp(argv[0], "add") == 0 && argc == 3) {
        rc = rpi4_add(fd, (int32_t)strtol(argv[1], NULL, 0),
                          (int32_t)strtol(argv[2], NULL, 0), &rep);
        if (rc == 0)
            printf("%d\n", rep.hdr.result);

    } else if (strcmp(argv[0], "echo") == 0 && argc == 2) {
        rc = rpi4_echo(fd, argv[1], &rep);
        if (rc == 0)
            printf("%.*s\n", (int)rep.hdr.len, rep.data);

    } else if (strcmp(argv[0], "stat") == 0) {
        rc = rpi4_stat_msg(fd, &rep);
        if (rc == 0)
            printf("%s\n", rep.data);

    } else if (strcmp(argv[0], "info") == 0) {
        rc = cmd_info(fd);

    } else if (strcmp(argv[0], "cat") == 0) {
        rc = cmd_cat(fd);

    } else if (strcmp(argv[0], "label") == 0 && argc == 2) {
        rc = cmd_label(fd, argv[1]);

    } else if (strcmp(argv[0], "reset") == 0) {
        int err = devctl(fd, DCMD_RPI4_RESET, NULL, 0, NULL);
        if (err != EOK) {
            fprintf(stderr, "devctl(DCMD_RPI4_RESET): %s\n", strerror(err));
            rc = -1;
        }

    } else if (strcmp(argv[0], "bench") == 0) {
        rc = cmd_bench(fd, argc == 2 ? strtol(argv[1], NULL, 0) : 10000);

    } else {
        usage("rpi4_client");
        rc = -1;
    }

    close(fd);
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
