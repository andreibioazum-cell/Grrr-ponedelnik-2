/* PC shim: the engine only passes ANativeWindow pointers around; the PC
 * backend owns its framebuffer and never dereferences the window here. */
#ifndef PC_SHIM_ANDROID_NATIVE_WINDOW_H
#define PC_SHIM_ANDROID_NATIVE_WINDOW_H
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
enum { WINDOW_FORMAT_RGBA_8888 = 1 };
#endif
