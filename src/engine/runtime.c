/* Application runtime and Android keyboard integration. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <pthread.h>
#include <time.h>
/* Shared C runtime services used by the native 2D engine. */
#include "engine/runtime/core.inc"
#include "engine/runtime/values.inc"

#include "platform/android/keyboard.inc"
