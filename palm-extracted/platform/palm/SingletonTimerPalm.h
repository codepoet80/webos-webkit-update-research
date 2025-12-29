/* ============================================================
 * Date  : 2008-02-21
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef SINGLETONTIMER_H
#define SINGLETONTIMER_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void  (*SingletonTimerCallback)(void* userArg);
typedef struct SingletonTimer SingletonTimer;

void SingletonTimerInit(GMainLoop* loop);

SingletonTimer* SingletonTimerCreate(void (*callback)(void*), void* userArg);

void SingletonTimerFire(SingletonTimer* timer, uint64_t timeInMs);

void SingletonTimerDelete(SingletonTimer* timer);

void SingletonTimerFreeze();

void SingletonTimerThaw();

#ifdef __cplusplus
}
#endif

#endif /* SINGLETONTIMER_H */
