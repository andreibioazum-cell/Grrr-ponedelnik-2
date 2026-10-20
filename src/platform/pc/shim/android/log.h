/* PC shim: Android logging becomes plain stderr output (a console window on
 * Windows, the terminal in the Linux test harness). */
#ifndef PC_SHIM_ANDROID_LOG_H
#define PC_SHIM_ANDROID_LOG_H
#include <stdarg.h>
enum { ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
int __android_log_print(int prio, const char *tag, const char *fmt, ...);
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap);
#endif
