/* Command line shared by the window and the headless harness:
 *   --size 540x960     framebuffer size (window default 540x960, harness too)
 *   --assets DIR       folder with textures/, audio/, fonts/ (default assets)
 *   --data DIR         folder for local saves (default cb4_data)
 *   --frames N         harness: how many steps to run
 *   --headless         force the harness even when a window can be opened
 *   --touch F:X:Y:A[:ID]   harness: inject a touch (A: 0 down, 1 up, 2 move)
 *   --goto F:STATE         harness: start_transition(STATE) at frame F
 *   --type F:TEXT          harness: type text into the open field at frame F
 *   --enter F / --back F   harness: press Enter / Back at frame F
 *   --dump F:PATH          harness: write a BMP of the frame F picture
 *   --report F             harness: print state/hp after frame F
 *   --stick F:DX:DY        harness: hold the virtual stick (0..1, y down)
 *   --atk F:0|1            harness: press/release the punch pad
 *   --blk F:0|1            harness: press/release the block pad
 *   --super F:0|1          harness: press/release the super pad
 * Script events are kept as raw strings and interpreted by pc_headless.inc. */
#ifndef PC_ARGS_H
#define PC_ARGS_H

/* Room for every event of a long run: a --report per frame in a two-second
 * stretch already needs dozens, and a dropped event is invisible in the log. */
#define PC_MAX_EVENTS 192
#define PC_EVENT_LEN 200
/* Paths are copied out of the command line: the parser tokenizes into its own
 * stack buffer, so keeping a pointer there would dangle the moment parsing
 * returns (the assets folder then read as garbage and the game failed to
 * start - on Windows only when the stack happened to be reused). */
#define PC_PATH_LEN 1024

typedef struct {
    int frames;
    int warmup;
    int width, height;
    int headless;
    char assets[PC_PATH_LEN];
    char data[PC_PATH_LEN];
    int eventc;
    char events[PC_MAX_EVENTS][PC_EVENT_LEN];
} PcArgs;

static void pc_args_default(PcArgs *a) {
    memset(a, 0, sizeof(*a));
    a->frames = 180;
    a->warmup = 0;
    a->width = 540;
    a->height = 960;
    a->headless = 0;
    snprintf(a->assets, sizeof(a->assets), "assets");
    snprintf(a->data, sizeof(a->data), "cb4_data");
}

static void pc_args_push_event(PcArgs *a, const char *name, const char *value) {
    if (a->eventc >= PC_MAX_EVENTS)
        return;
    snprintf(a->events[a->eventc], PC_EVENT_LEN, "%s %s", name, value ? value : "");
    a->eventc++;
}

/* Splits on spaces, honours "double quotes"; the first token (the program
 * itself) is skipped. */
/* The token table has to hold the whole command line: with a 128-slot table a
 * long harness run (a --report per frame) silently lost every event past the
 * cutoff, so the log just stopped in the middle. 1024 pointers are cheap. */
#define PC_MAX_TOKENS 1024

static void pc_args_parse(const char *cmdline, PcArgs *a) {
    char buf[4096];
    char *p, *tok[PC_MAX_TOKENS];
    int n = 0, i;
    if (!cmdline)
        return;
    snprintf(buf, sizeof(buf), "%s", cmdline);
    p = buf;
    while (*p && n < (int)(sizeof(tok) / sizeof(tok[0]))) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (*p == '"') {
            p++;
            tok[n++] = p;
            while (*p && *p != '"')
                p++;
            if (*p == '"')
                *p++ = '\0';
        } else {
            tok[n++] = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';
        }
    }
    for (i = 1; i < n; i++) {
        const char *k = tok[i];
        const char *v = i + 1 < n ? tok[i + 1] : "";
        if (strcmp(k, "--headless") == 0) {
            a->headless = 1;
            continue;
        }
        if (!*v)
            continue;
        if (strcmp(k, "--frames") == 0) {
            a->frames = atoi(v);
            if (a->frames < 1)
                a->frames = 1;
            i++;
        } else if (strcmp(k, "--warmup") == 0) {
            a->warmup = atoi(v);
            if (a->warmup < 0)
                a->warmup = 0;
            i++;
        } else if (strcmp(k, "--size") == 0) {
            int w = 0, h = 0;
            if (sscanf(v, "%dx%d", &w, &h) == 2 && w >= 200 && h >= 200 && w <= 4096 && h <= 4096) {
                a->width = w;
                a->height = h;
            }
            i++;
        } else if (strcmp(k, "--assets") == 0) {
            snprintf(a->assets, sizeof(a->assets), "%s", v);
            i++;
        } else if (strcmp(k, "--data") == 0) {
            snprintf(a->data, sizeof(a->data), "%s", v);
            i++;
        } else if (strcmp(k, "--touch") == 0 || strcmp(k, "--goto") == 0 || strcmp(k, "--type") == 0 ||
                   strcmp(k, "--enter") == 0 || strcmp(k, "--back") == 0 || strcmp(k, "--dump") == 0 ||
                   strcmp(k, "--report") == 0 || strcmp(k, "--stick") == 0 || strcmp(k, "--atk") == 0 ||
                   strcmp(k, "--blk") == 0 || strcmp(k, "--super") == 0 || strcmp(k, "--login") == 0 ||
                   strcmp(k, "--logininfo") == 0) {
            pc_args_push_event(a, k + 2, v);
            i++;
        }
    }
}

#endif

/* Склеивает argv в одну строку для pc_args_parse. strncat считает
 * терминатор из своего лимита, поэтому старый "- 2" не оставлял места для
 * разделителя, и длинная командная строка (много --touch) уезжала за конец
 * буфера. Аргумент добавляется, только если помещается вместе с пробелом. */
static void pc_args_join(int argc, char **argv, char *dst, size_t cap) {
    size_t used = 0;
    int i;
    if (!dst || cap < 2)
        return;
    dst[0] = '\0';
    snprintf(dst, cap, "cb4_headless ");
    used = strlen(dst);
    for (i = 1; i < argc; i++) {
        size_t need = strlen(argv[i]) + 1;
        if (need + 1 > cap - used)
            break;
        memcpy(dst + used, argv[i], need - 1);
        used += need - 1;
        dst[used++] = ' ';
        dst[used] = '\0';
    }
}

/* Путь для --gfx-test из строки аргументов (по умолчанию gfx_test.bmp). */
static void pc_args_dump_path(const char *cmdline, char *dst, size_t cap) {
    const char *dump = "gfx_test.bmp";
    const char *p = cmdline ? strstr(cmdline, "--dump ") : NULL;
    size_t n;
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon)
            dump = colon + 1;
    }
    n = strcspn(dump, " \t\n");
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, dump, n);
    dst[n] = '\0';
}
