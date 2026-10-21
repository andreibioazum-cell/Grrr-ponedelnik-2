#ifndef PC_INPUT_H
#define PC_INPUT_H
/* PC-only extras around the shared keyboard API (runtime.h): the physical
 * Enter key has no Android key code on the PC path. */
void pc_keyboard_enter(void);
#endif
