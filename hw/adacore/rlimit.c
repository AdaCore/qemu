/*
 * QEMU rlimit
 *
 * Copyright (c) 2016-2021 AdaCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/adacore/rlimit.h"
#include "qemu/timer.h"

void qemu_exit_with_debug(const char *fmt, ...);
static uint64_t rlimit;

static void rlimit_timer_tick(void *opaque)
{
    qemu_exit_with_debug("\nQEMU rlimit exceeded (%"PRId64"s)\n", rlimit);
}

void rlimit_set_value(const char *optarg)
{
    char *endptr = NULL;

    rlimit = strtol(optarg, &endptr, 10);

    if (endptr == optarg) {
        fprintf(stderr, "Invalid rlimit value '%s'\n", optarg);
        abort();
    }
}

void rlimit_init(void)
{
    uint64_t now;
    QEMUTimer *timer;

    if (rlimit != 0) {
        timer = timer_new_ns(QEMU_CLOCK_HOST, rlimit_timer_tick, NULL);
        if (timer == NULL) {
            fprintf(stderr, "%s: Cannot allocate timer\n", __func__);
            abort();
        }
        now = qemu_clock_get_ns(QEMU_CLOCK_HOST);
        timer_mod_ns(timer, now + rlimit * 1000000000ULL);
    }
}
