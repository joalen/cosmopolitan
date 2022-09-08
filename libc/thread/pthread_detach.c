/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/intrin/atomic.h"
#include "libc/intrin/pthread.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/spawn.h"

/**
 * Asks POSIX thread to free itself automatically on termination.
 *
 * @return 0 on success, or errno with error
 */
int pthread_detach(pthread_t thread) {
  enum PosixThreadStatus status;
  struct PosixThread *pt = thread;
  for (;;) {
    status = atomic_load_explicit(&pt->status, memory_order_relaxed);
    if (status == kPosixThreadDetached || status == kPosixThreadZombie) {
      // these two states indicate the thread was already detached, in
      // which case it's already listed under _pthread_zombies.
      break;
    } else if (status == kPosixThreadTerminated) {
      // thread was joinable and finished running. since pthread_join
      // won't be called, it's safe to free the thread resources now.
      _pthread_wait(pt);
      _pthread_free(pt);
      break;
    } else if (status == kPosixThreadJoinable) {
      if (atomic_compare_exchange_weak_explicit(
              &pt->status, &status, kPosixThreadDetached, memory_order_acquire,
              memory_order_relaxed)) {
        _pthread_zombies_add(pt);
        break;
      }
    } else {
      notpossible;
    }
  }
  return 0;
}