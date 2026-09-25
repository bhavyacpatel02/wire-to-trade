# wire-to-trade

A synthetic market-data pipeline built to measure wire-to-trade latency, one
phase at a time. Each phase boundary is a working system; each later phase
replaces one stage and measures the difference against the one before it.

```
UDP producer -> receive thread -> mutex-guarded std::queue -> consumer thread -> counters
```

**Status: Phase 0 complete (2026-09-19).** This is the deliberately bad
baseline. Every stage is a placeholder. Its job is to exist end to end, to have
honest accounting, and to be the number every later phase is compared against.

## Build

```
make          # optimised          -> build/release/
make asan     # ASan + UBSan       -> build/asan/
make tsan     # ThreadSanitizer    -> build/tsan/
make all      # release + asan
make test     # tests/test_*.cpp, GoogleTest, built under ASan + UBSan
make clean
```

Toolchain: Apple clang 16, `-std=c++20`, macOS on Apple Silicon (M1). The
only external dependency is GoogleTest, for `make test` only:
`brew install googletest`.

## Run

Start the consumer first. It binds UDP port 31337 on localhost. Then run the
producer. Both take N, the number of messages, as their only argument.

```
./build/release/consumer 200000     # terminal 1
./build/release/producer 200000     # terminal 2
```

The producer sends sequence numbers 0 through N-1 as fast as `sendto` allows,
with no pacing and no waiting on the consumer (open loop). The consumer arms a
one-second receive timeout after the first packet arrives and exits on its own
once the socket has been quiet for a second. Ctrl-C stops it early; the report
still prints.

Only one consumer can own the port at a time. A second one fails with
`Bind failed, errno=48` (`EADDRINUSE`).

## What the report means

```
Expected 200000 messages: received 200000, out of order 0, highest sequence number 199999
Missing: 0
```

- **received**: messages whose sequence number was higher than any seen before.
- **out of order**: sequence number less than or equal to the highest seen.
  Late, duplicate, or a producer that was run twice. Not counted as received.
- **missing**: expected minus received, computed once at exit. No running gap
  count, because unsigned subtraction wraps when a sequence number goes
  backwards and a late packet can fill a gap already counted.

The reconciliation identity is `received + missing == expected`. If received
exceeds expected, the report prints a warning instead of a wrapped number.

## Phase 0 baseline numbers

Single runs on 2026-09-19, loopback, producer and consumer on the same M1.
Sanitizer drop counts vary run to run; the point is that the identity holds,
not the exact figure.

| Build   | N       | Received | Out of order | Missing |
|---------|---------|----------|--------------|---------|
| release | 10      | 10       | 0            | 0       |
| release | 1,000   | 1,000    | 0            | 0       |
| release | 200,000 | 200,000  | 0            | 0       |
| release, producer run twice | 1,000 | 1,000 | 1,000 | 0 |
| tsan    | 200,000 | 20,607   | 0            | 179,393 |
| asan    | 200,000 | 191,976  | 0            | 8,024   |

TSan and ASan/UBSan both report nothing on every run above.

Two observations worth keeping:

- The release build keeps up with 200,000 messages on loopback with zero loss.
  The kernel receive buffer here is 786,896 bytes, about 65,000 messages, so the
  consumer is draining faster than that fills.
- Under sanitizer overhead the consumer falls behind, the receive buffer
  overflows, and the kernel drops packets silently. The accounting still
  balances. That is the first real packet loss this system has produced.

## What is deliberately bad, and which phase fixes it

| Placeholder | Cost | Replaced in |
|---|---|---|
| Consumer busy-spins and takes the mutex on every iteration, even when the queue is empty | Burns one core at 100% while idle; the receiver competes for the lock on every push | Phase 4 (SPSC ring buffer) |
| `std::queue` grows without bound and allocates as it grows | Allocation on the receive path; no back-pressure; memory grows if the consumer falls behind | Phase 4 |
| One 12-byte struct per datagram, `memcpy` out of the socket buffer, then copied again into the queue | Two copies per message | Phase 2 (ITCH framing, parse in place), Phase 4 (recv straight into the ring slot) |
| Blocking `recvmsg`, one datagram per call | Syscall and sleep/wake per packet | Phase 6 (busy-poll, `receive_batch()`) |
| No timestamps anywhere | Nothing is measured yet | Phase 1 |
| Message is 12 bytes, not ITCH-shaped | Baseline numbers will shift when the wire format changes size | Phase 2 |
| No thread pinning | Threads migrate between P and E cores; tails past p90 are not trustworthy on this platform | Linux port (extension) |

Also known and accepted for now: the producer prints `sendto` failures but does
not count them, so if `ENOBUFS` ever appears the identity has to be checked by
hand. Sequence numbers start at 0. Out-of-order and duplicate are not
distinguished; that needs a memory of which sequence numbers have been seen and
is real protocol work for Phase 2.

## Layout

```
src/consumer.cpp   receive thread + consumer thread + report
src/producer.cpp   open-loop sender
src/message.h      the wire struct (12 bytes, native byte order, host padding)
src/constants.h    port, address, buffer size
Makefile           three configurations, one directory each
```

## Regenerating the numbers

```
make all && make tsan
for B in release tsan asan; do
  ./build/$B/consumer 200000 & sleep 0.5; ./build/release/producer 200000; wait
done
```
