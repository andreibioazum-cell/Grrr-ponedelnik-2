/* Browser HTTP transport for real live Firebase Auth & Realtime Database. */
typedef int (*DSStreamChunkFn)(void *ctx, const char *data, size_t len);

__attribute__((import_module("env"), import_name("js_http_ex"))) extern int
js_http_ex(const char *method, const char *url, const char *body, char *out, int cap, const char *header,
           const char *value, char *etag, int etag_cap);

static int http_ex(const char *method, const char *url, const char *body, char *out, size_t cap, const char *header,
                   const char *value, char *etag, size_t etag_cap) {
    if (!url || !*url)
        return 0;
    if (out && cap > 0)
        out[0] = '\0';
    if (etag && etag_cap > 0)
        etag[0] = '\0';
    return js_http_ex(method ? method : "GET", url, body ? body : "", out, (int)cap, header ? header : "",
                      value ? value : "", etag, (int)etag_cap);
}

static int http_stream(const char *url, const char *accept, DSStreamChunkFn cb, void *ctx) {
    (void)url;
    (void)accept;
    (void)cb;
    (void)ctx;
    return 0;
}
