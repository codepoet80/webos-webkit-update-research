
// Copyright 2010 Palm Inc.

#include "config.h"
#include "SystemTime.h"

#include <sys/time.h>
#include <time.h>
#include <stdio.h>

// This is to be used as a reference time for instrumentation.
unsigned long palm_time_ms() {
	struct timespec tm;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tm);
	return tm.tv_sec * 1000LL + tm.tv_nsec / 1000000;
}

// This is to be used as a reference time for instrumentation.
unsigned long palm_monotonic_time_ms() {
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	return tm.tv_sec * 1000LL + tm.tv_nsec / 1000000;
}

namespace WebCore {

// -> returns seconds
double currentTime()
{
	struct timeval aTimeval;
	struct timezone aTimezone;
	
	gettimeofday( &aTimeval, &aTimezone );
	return (double)aTimeval.tv_sec + (double)(aTimeval.tv_usec / 1000000.0 );
}

double currentMonotonicTime()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) (ts.tv_nsec / 1000000000.0);
}

}
