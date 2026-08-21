/*
 * rpi4_resmgr.c - a minimal QNX 8.0 resource manager for the Raspberry Pi 4.
 *
 * It creates /dev/rpi4 and serves three kinds of traffic on the one channel:
 *
 *   read()/write()  - a plain-text status page, writable "label"
 *   devctl()        - DCMD_RPI4_STAT / DCMD_RPI4_RESET
 *   MsgSend()       - the private RPI4_MSG_* protocol in rpi4_msg.h
 *
 * The first two are handled by the resmgr library (resmgr_attach); the third
 * is handled by our own callback, registered with message_attach() for the
 * type range RPI4_MSG_LOW..RPI4_MSG_HIGH.  dispatch_handler() looks at the
 * 16-bit type at the front of each message and picks the right one.
 *
 * Single-threaded on purpose: receive one message, handle it, reply, repeat.
 * That makes every handler below implicitly serialised, so the shared state
 * needs no locking.  Swap the loop for a thread_pool_*() setup when you want
 * concurrency (see gpio_pin_resmgr for that shape).
 *
 * Needs no special privilege - it touches no hardware, so it runs as any user
 * that is allowed to create a name in /dev.
 *
 * Build : QNX 8.0 SDP, aarch64le (Raspberry Pi 4) or x86_64 (QEMU)
 * Run   : rpi4_resmgr &            # or: rpi4_resmgr -n /dev/rpi4a -v &
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* <sys/iofunc.h> must come first: it defines RESMGR_OCB_T as iofunc_ocb_t,
 * which <sys/dispatch.h> would otherwise leave as void. */
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

#include "rpi4_msg.h"

/* ------------------------------------------------------------------ */
/* Server state                                                        */
/* ------------------------------------------------------------------ */

static rpi4_stat_t  stats = { .label = "rpi4" };
static bool         verbose;

static resmgr_connect_funcs_t   connect_funcs;
static resmgr_io_funcs_t        io_funcs;
static iofunc_attr_t            attr;

/*
 * Scratch buffer for io_read().  The reply IOV we hand back with
 * _RESMGR_NPARTS() is consumed by the library after io_read() returns, so it
 * must not point into a dead stack frame.  File scope is safe here only
 * because the manager is single-threaded.
 */
static char         readbuf[512];

/* ------------------------------------------------------------------ */
/* 1. Private messages - the MsgSend() path                            */
/* ------------------------------------------------------------------ */

/*
 * Called once per private message.  ctp->msg points at the bytes the kernel
 * copied out of the client, ctp->info.msglen says how many arrived, and
 * ctp->rcvid is the blocked client we must unblock exactly once - with
 * MsgReply() on success or MsgError() on failure.
 */
static int msg_handler(message_context_t *ctp, int type, unsigned flags,
                       void *handle)
{
    const rpi4_msg_t   *msg = (const rpi4_msg_t *)ctp->msg;
    rpi4_reply_t        rep;
    size_t              replylen = sizeof(rep.hdr);

    (void)flags;
    (void)handle;

    /* A short message means a buggy or hostile client - never trust msglen. */
    if (ctp->info.msglen < (ssize_t)sizeof(rpi4_hdr_t)) {
        MsgError(ctp->rcvid, EBADMSG);
        return 0;
    }

    memset(&rep, 0, sizeof(rep));
    stats.msgs++;
    rep.hdr.seq = stats.msgs;

    switch (type) {

    case RPI4_MSG_PING:
        break;                          /* status EOK, nothing else to say  */

    case RPI4_MSG_ADD:
        rep.hdr.result = msg->hdr.a + msg->hdr.b;
        stats.accum   += rep.hdr.result;
        break;

    case RPI4_MSG_ECHO: {
        size_t len  = msg->hdr.len;
        size_t have = (size_t)ctp->info.msglen - sizeof(rpi4_hdr_t);

        /* len is client-supplied: clamp it to what we can hold *and* to what
         * actually arrived, or we would reply with our own stack contents. */
        if (len > RPI4_PAYLOAD_MAX || len > have) {
            MsgError(ctp->rcvid, EINVAL);
            return 0;
        }
        memcpy(rep.data, msg->data, len);
        rep.hdr.len = len;
        replylen    = sizeof(rep.hdr) + len;
        break;
    }

    case RPI4_MSG_STAT: {
        int n = snprintf(rep.data, sizeof(rep.data),
                         "msgs=%u reads=%u writes=%u devctls=%u "
                         "accum=%d label=%s",
                         stats.msgs, stats.reads, stats.writes,
                         stats.devctls, stats.accum, stats.label);

        /* snprintf() reports what it *would* have written, so clamp before
         * using it as a length. */
        if (n < 0)
            n = 0;
        else if ((size_t)n >= sizeof(rep.data))
            n = (int)sizeof(rep.data) - 1;

        rep.hdr.result = stats.accum;
        rep.hdr.len    = (uint32_t)n;
        replylen       = sizeof(rep.hdr) + (size_t)n + 1;  /* + '\0' */
        break;
    }

    default:
        /* Inside our range but not a type we know. */
        MsgError(ctp->rcvid, ENOSYS);
        return 0;
    }

    if (verbose)
        printf("rpi4_resmgr: msg type 0x%04x from pid %d -> result %d\n",
               type, ctp->info.pid, rep.hdr.result);

    MsgReply(ctp->rcvid, EOK, &rep, replylen);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 2. read() / write() - the shell-friendly path                       */
/* ------------------------------------------------------------------ */

static size_t snapshot(void)
{
    return (size_t)snprintf(readbuf, sizeof(readbuf),
                            "label   %s\n"
                            "msgs    %u\n"
                            "reads   %u\n"
                            "writes  %u\n"
                            "devctls %u\n"
                            "accum   %d\n",
                            stats.label, stats.msgs, stats.reads,
                            stats.writes, stats.devctls, stats.accum);
}

static int io_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK)
        return status;

    /* We implement plain read() only, not readcond()/pread(). */
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;

    /* Build the page on the first read, then serve it as a seekable file so
     * that `cat` walks to EOF and stops instead of looping forever. */
    size_t total = (ocb->offset == 0) ? snapshot() : strlen(readbuf);
    size_t left  = (size_t)ocb->offset >= total ? 0 : total - (size_t)ocb->offset;
    size_t nsend = min(msg->i.nbytes, left);

    if (nsend > 0) {
        SETIOV(ctp->iov, readbuf + ocb->offset, nsend);
        ocb->offset += nsend;
        ocb->attr->flags |= IOFUNC_ATTR_ATIME | IOFUNC_ATTR_DIRTY_TIME;
    }
    _IO_SET_READ_NBYTES(ctp, nsend);
    stats.reads++;

    return nsend > 0 ? _RESMGR_NPARTS(1) : _RESMGR_NPARTS(0);
}

static int io_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
    char buf[RPI4_LABEL_MAX];

    int status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK)
        return status;

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;
    if (msg->i.nbytes >= sizeof(buf))
        return EINVAL;

    /* Only the header is guaranteed to be in our receive buffer; pull the
     * payload across explicitly, skipping past that header. */
    int nread = resmgr_msgread(ctp, buf, msg->i.nbytes, sizeof(msg->i));
    if (nread < 0)
        return errno;
    buf[nread] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';   /* drop the newline `echo` adds */

    strlcpy(stats.label, buf, sizeof(stats.label));
    stats.writes++;

    /* Claim the whole buffer so the shell's echo does not retry the tail. */
    _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);
    ocb->attr->flags |= IOFUNC_ATTR_MTIME | IOFUNC_ATTR_DIRTY_TIME;

    return EOK;
}

/* ------------------------------------------------------------------ */
/* 3. devctl() - the typed path                                        */
/* ------------------------------------------------------------------ */

static int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb)
{
    int status = iofunc_devctl_default(ctp, msg, ocb);
    if (status != _RESMGR_DEFAULT)
        return status;                  /* the library already answered it  */

    stats.devctls++;

    switch (msg->i.dcmd) {

    case DCMD_RPI4_STAT:
        memset(&msg->o, 0, sizeof(msg->o));
        msg->o.nbytes = sizeof(stats);
        /* The reply payload sits immediately after the reply header. */
        memcpy(_DEVCTL_DATA(msg->o), &stats, sizeof(stats));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + sizeof(stats));

    case DCMD_RPI4_RESET: {
        char label[RPI4_LABEL_MAX];

        strlcpy(label, stats.label, sizeof(label));
        memset(&stats, 0, sizeof(stats));
        strlcpy(stats.label, label, sizeof(stats.label));

        memset(&msg->o, 0, sizeof(msg->o));
        return _RESMGR_PTR(ctp, &msg->o, sizeof(msg->o));
    }

    default:
        return ENOTTY;                  /* not a command this device knows  */
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-n path] [-v]\n"
            "  -n path  device to create (default %s)\n"
            "  -v       log every private message\n",
            prog, RPI4_DEV_PATH);
}

int main(int argc, char **argv)
{
    const char *devpath = RPI4_DEV_PATH;
    int         c;

    while ((c = getopt(argc, argv, "n:vh")) != -1) {
        switch (c) {
        case 'n': devpath = optarg; break;
        case 'v': verbose = true;   break;
        default:  usage(argv[0]);   return EXIT_FAILURE;
        }
    }

    /* The dispatch handle owns the channel every client connects to. */
    dispatch_t *dpp = dispatch_create();
    if (dpp == NULL) {
        perror("dispatch_create");
        return EXIT_FAILURE;
    }

    /* Start from the POSIX defaults, then override just the three we care
     * about; stat, chmod, lseek, close, ... keep working for free. */
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read   = io_read;
    io_funcs.write  = io_write;
    io_funcs.devctl = io_devctl;

    iofunc_attr_init(&attr, S_IFCHR | 0666, NULL, NULL);

    resmgr_attr_t rattr = {
        .nparts_max   = 1,
        .msg_max_size = sizeof(rpi4_msg_t)
    };

    if (resmgr_attach(dpp, &rattr, devpath, _FTYPE_ANY, 0,
                      &connect_funcs, &io_funcs, &attr) == -1) {
        perror("resmgr_attach");
        return EXIT_FAILURE;
    }

    /* Claim our private type range on the same channel.  Anything the client
     * MsgSend()s with a type in [RPI4_MSG_LOW, RPI4_MSG_HIGH] lands in
     * msg_handler() instead of the resmgr I/O layer. */
    message_attr_t mattr = {
        .nparts_max   = 1,
        .msg_max_size = sizeof(rpi4_msg_t)
    };

    if (message_attach(dpp, &mattr, RPI4_MSG_LOW, RPI4_MSG_HIGH,
                       msg_handler, NULL) == -1) {
        perror("message_attach");
        return EXIT_FAILURE;
    }

    /* Allocate after both attaches: the context is sized from the largest
     * msg_max_size registered on the handle. */
    dispatch_context_t *ctp = dispatch_context_alloc(dpp);
    if (ctp == NULL) {
        perror("dispatch_context_alloc");
        return EXIT_FAILURE;
    }

    printf("rpi4_resmgr: %s ready (private msg types 0x%04x-0x%04x)\n",
           devpath, RPI4_MSG_LOW, RPI4_MSG_HIGH);
    fflush(stdout);

    /* The whole server: block for a message, let dispatch route it, repeat. */
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
    return EXIT_FAILURE;
}
