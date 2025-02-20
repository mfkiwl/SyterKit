/* SPDX-License-Identifier: MIT */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <types.h>

#include <timer.h>

#include <sys-uart.h>

#include "xformat.h"

#ifdef __cplusplus
extern "C" {
#endif// __cplusplus

enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARNING = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_MUTE = 5,
};

#ifndef LOG_LEVEL_DEFAULT

#ifdef DEBUG_MODE
#define LOG_LEVEL_DEFAULT LOG_LEVEL_DEBUG
#else
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif// DEBUG_MODE

#endif// LOG_LEVEL_DEFAULT

void set_timer_count();

void printk(int level, const char *fmt, ...);

void uart_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif// __cplusplus

#endif
