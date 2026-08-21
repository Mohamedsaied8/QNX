/*
 * rpi4_msg.h - the wire contract shared by rpi4_resmgr (server) and
 *              rpi4_client (client).
 *
 * The manager speaks three dialects on the very same connection:
 *
 *   1. POSIX file ops - read()/write() so the shell can drive it.
 *   2. devctl()       - the DCMD_RPI4_* commands below.
 *   3. private messages - the RPI4_MSG_* types below, sent with a raw
 *                       MsgSend()/MsgSendv() straight onto the fd returned
 *                       by open().  On QNX a file descriptor IS a connection
 *                       id, so no ConnectAttach() is needed.
 *
 * Types 0x100..0x1FF (_IO_BASE.._IO_MAX) belong to the resmgr layer, so a
 * private protocol must live outside them.  _IOMGR_PRIVATE_BASE (0xF000) is
 * the range QNX reserves for unregistered, application-private managers.
 */
#ifndef RPI4_MSG_H_
#define RPI4_MSG_H_

#include <devctl.h>
#include <stdint.h>
#include <sys/iomgr.h>      /* _IOMGR_PRIVATE_BASE */

#define RPI4_DEV_PATH       "/dev/rpi4"
#define RPI4_LABEL_MAX      64u
#define RPI4_PAYLOAD_MAX    256u

/* ------------------------------------------------------------------ */
/* 1. Private message protocol (raw MsgSend)                           */
/* ------------------------------------------------------------------ */

#define RPI4_MSG_LOW        (_IOMGR_PRIVATE_BASE + 0x10)
#define RPI4_MSG_PING       (RPI4_MSG_LOW + 0)  /* round trip, no payload   */
#define RPI4_MSG_ADD        (RPI4_MSG_LOW + 1)  /* result = a + b           */
#define RPI4_MSG_ECHO       (RPI4_MSG_LOW + 2)  /* bounce payload back      */
#define RPI4_MSG_STAT       (RPI4_MSG_LOW + 3)  /* server counters          */
#define RPI4_MSG_HIGH       (RPI4_MSG_LOW + 15) /* room to grow             */

/*
 * Every private message starts with this header.  The `type` field must be
 * first and 16 bits wide: that is what the kernel and dispatch layer look at
 * to route the message to our handler.
 */
typedef struct {
    uint16_t    type;                   /* RPI4_MSG_*                       */
    uint16_t    subtype;                /* unused, keeps the header aligned */
    int32_t     a;                      /* RPI4_MSG_ADD operand             */
    int32_t     b;                      /* RPI4_MSG_ADD operand             */
    uint32_t    len;                    /* payload bytes following, ECHO    */
} rpi4_hdr_t;

typedef struct {
    rpi4_hdr_t  hdr;
    char        data[RPI4_PAYLOAD_MAX];
} rpi4_msg_t;

typedef struct {
    int32_t     status;                 /* EOK, or an errno                 */
    int32_t     result;                 /* RPI4_MSG_ADD sum                 */
    uint32_t    seq;                    /* how many messages we have served */
    uint32_t    len;                    /* payload bytes following          */
} rpi4_reply_hdr_t;

typedef struct {
    rpi4_reply_hdr_t hdr;
    char             data[RPI4_PAYLOAD_MAX];
} rpi4_reply_t;

/* ------------------------------------------------------------------ */
/* 2. devctl() interface                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t    msgs;                   /* private messages served          */
    uint32_t    reads;                  /* read() calls served              */
    uint32_t    writes;                 /* write() calls served             */
    uint32_t    devctls;                /* devctl() calls served            */
    int32_t     accum;                  /* running total of every ADD       */
    char        label[RPI4_LABEL_MAX];  /* whatever was last write()n       */
} rpi4_stat_t;

/* __DIOF: data flows manager -> client.  __DIOT would be the other way. */
#define DCMD_RPI4_STAT      __DIOF(_DCMD_MISC, 1, rpi4_stat_t)
#define DCMD_RPI4_RESET     __DION(_DCMD_MISC, 2)

#endif /* RPI4_MSG_H_ */
