# rpi4_resmgr_client

A minimal QNX 8.0 resource manager for the Raspberry Pi 4, plus a client that
drives it with a raw `MsgSend()`.

| file | what it is |
|---|---|
| `rpi4_msg.h`    | the wire contract: private message types, structs, devctl commands |
| `rpi4_resmgr.c` | the server - creates `/dev/rpi4` |
| `rpi4_client.c` | the client - `MsgSend()`, `MsgSendv()`, `devctl()`, `read()`, `write()` |

## The idea

`resmgr_attach()` claims the POSIX I/O message range (`_IO_BASE.._IO_MAX`) on
the manager's channel, so `open()`, `read()`, `write()`, `devctl()` and friends
work with no extra code. `message_attach()` then claims a *second*, private
range on the **same** channel — `RPI4_MSG_LOW..RPI4_MSG_HIGH`, inside the
`_IOMGR_PRIVATE_BASE` (0xF000) block QNX reserves for unregistered managers.
`dispatch_handler()` reads the 16-bit type at the front of each message and
routes it to whichever handler owns that range.

On the client side the trick is that **a QNX file descriptor is a connection
id**. After `open("/dev/rpi4")` you can `MsgSend(fd, ...)` your own struct
directly — no `ConnectAttach()`, no `name_open()`, no second channel:

```c
int        fd  = open(RPI4_DEV_PATH, O_RDWR);
rpi4_msg_t msg = { .hdr = { .type = RPI4_MSG_ADD, .a = 20, .b = 22 } };
rpi4_reply_t rep;

MsgSend(fd, &msg, sizeof(msg.hdr), &rep, sizeof(rep));   /* rep.hdr.result == 42 */
```

`MsgSend()` is synchronous: the caller goes SEND-blocked until the server
receives, then REPLY-blocked until it replies, and the reply is copied straight
into the caller's buffer. `MsgSendv()` (used by the `echo` command) gathers the
header and the payload from two separate buffers into one message, so a payload
that already lives somewhere else never has to be staged into a struct first.

The server is single-threaded — `dispatch_block()` / `dispatch_handler()` in a
loop — which is the smallest complete resource manager shape and makes the
shared state implicitly race-free. For a concurrent version, swap the loop for
`thread_pool_create()` and give each thread its own scratch buffer.

## Build

Configure with the QNX CMake toolchain (the QNX VS Code extension ships them),
or use the bundled tasks: **QNX: CMake Configure (debug)** → **QNX: CMake Build**.

```sh
source ~/qnx800/qnxsdp-env.sh
cmake -S . -B build-aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=~/.vscode/extensions/qnx.qnx-vscode-1.1.0/cmake-toolchains/aarch64le.cmake \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build build-aarch64
```

Direct compile, if you prefer:

```sh
qcc -Vgcc_ntoaarch64le -Wall -Wextra -std=gnu11 rpi4_resmgr.c -o rpi4_resmgr
qcc -Vgcc_ntoaarch64le -Wall -Wextra -std=gnu11 rpi4_client.c -o rpi4_client
```

`gcc_ntox86_64` builds the same sources for the QEMU target. Nothing here
touches hardware, so neither program needs root.

## Run

On the target:

```sh
rpi4_resmgr &            # or: rpi4_resmgr -n /dev/rpi4a -v &
rpi4_client              # full demo: every path, then a round-trip benchmark
```

Individual commands:

```sh
rpi4_client ping                 # MsgSend
rpi4_client add 20 22            # MsgSend  -> 42
rpi4_client echo "hello pi"      # MsgSendv -> bounced back
rpi4_client stat                 # MsgSend  -> server counters
rpi4_client info                 # devctl(DCMD_RPI4_STAT)
rpi4_client label "kitchen pi"   # write()
rpi4_client cat                  # read()
rpi4_client reset                # devctl(DCMD_RPI4_RESET)
rpi4_client bench 100000         # time N MsgSend round trips
rpi4_client -d /dev/rpi4a ping   # non-default device
```

And from the shell, because it is a real device:

```sh
cat /dev/rpi4
echo "kitchen pi" > /dev/rpi4
```

## Protocol

| message | direction | meaning |
|---|---|---|
| `RPI4_MSG_PING` | → | round trip, no payload |
| `RPI4_MSG_ADD`  | ↔ | `result = a + b`, accumulated server-side |
| `RPI4_MSG_ECHO` | ↔ | bounce up to `RPI4_PAYLOAD_MAX` bytes back |
| `RPI4_MSG_STAT` | ↔ | counters, as text |

Every reply carries `status`, `result`, `seq` (messages served so far) and
`len`. A rejected request unblocks the client through `MsgError()` instead, so
it surfaces as `MsgSend()` returning `-1` with `errno` set.

The server clamps `hdr.len` against both `RPI4_PAYLOAD_MAX` and the number of
bytes that actually arrived (`ctp->info.msglen`) — a client-supplied length is
never trusted, or a short send would make the reply leak the server's stack.
