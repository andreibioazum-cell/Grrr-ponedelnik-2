#ifndef CB4_WEB_SETJMP_SHIM_H
#define CB4_WEB_SETJMP_SHIM_H
typedef int jmp_buf[1];
#define setjmp(env) ((void)(env), 0)
static inline void longjmp(jmp_buf env, int val) {
    (void)env;
    (void)val;
}
#endif
