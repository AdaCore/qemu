/*
 * os-win32.c
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 * Copyright (c) 2010 Red Hat, Inc.
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
#include <windows.h>
#include <mmsystem.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include "config-host.h"
#include "sysemu/sysemu.h"
#include "qemu-options.h"

/***********************************************************/
/* Functions missing in mingw */

int setenv(const char *name, const char *value, int overwrite)
{
    int result = 0;
    if (overwrite || !getenv(name)) {
        size_t length = strlen(name) + strlen(value) + 2;
        char *string = g_malloc(length);
        snprintf(string, length, "%s=%s", name, value);
        result = putenv(string);

        /* Windows takes a copy and does not continue to use our string.
         * Therefore it can be safely freed on this platform.  POSIX code
         * typically has to leak the string because according to the spec it
         * becomes part of the environment.
         */
        g_free(string);
    }
    return result;
}

static BOOL WINAPI qemu_ctrl_handler(DWORD type)
{
    qemu_system_shutdown_request();
    /* Windows 7 kills application when the function returns.
       Sleep here to give QEMU a try for closing.
       Sleep period is 10000ms because Windows kills the program
       after 10 seconds anyway. */
    Sleep(10000);

    return TRUE;
}

static TIMECAPS mm_tc;

static void os_undo_timer_resolution(void)
{
    timeEndPeriod(mm_tc.wPeriodMin);
}

void os_setup_early_signal_handling(void)
{
    SetConsoleCtrlHandler(qemu_ctrl_handler, TRUE);
    timeGetDevCaps(&mm_tc, sizeof(mm_tc));
    timeBeginPeriod(mm_tc.wPeriodMin);
    atexit(os_undo_timer_resolution);
}

/* Look for support files in the same directory as the executable.  */
char *os_find_datadir(void)
{
    return qemu_get_exec_dir();
}

void os_set_line_buffering(void)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
}

/*
 * Parse OS specific command line options.
 * return 0 if option handled, -1 otherwise
 */
void os_parse_cmd_args(int index, const char *optarg)
{
    return;
}

void os_pidfile_error(void)
{
    fprintf(stderr, "Could not acquire pid file: %s\n", strerror(errno));
}

int qemu_create_pidfile(const char *filename)
{
    char buffer[128];
    int len;
    HANDLE file;
    OVERLAPPED overlap;
    BOOL ret;
    memset(&overlap, 0, sizeof(overlap));

    file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        return -1;
    }
    len = snprintf(buffer, sizeof(buffer), "%d\n", getpid());
    ret = WriteFile(file, (LPCVOID)buffer, (DWORD)len,
                    NULL, &overlap);
    CloseHandle(file);
    if (ret == 0) {
        return -1;
    }
    return 0;
}

void set_cpu_affinity(const char *optarg)
{
    HANDLE     h;
    DWORD_PTR  mask;
    char      *endptr = NULL;

    mask = strtol(optarg, &endptr, 16);
    if (endptr != optarg) {
        h = GetCurrentProcess();
        SetProcessAffinityMask(h, mask);
    } else {
        fprintf(stderr, "Invalid CPU mask '%s'\n", optarg);
        abort();
    }
}
