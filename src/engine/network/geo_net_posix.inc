/* Host POSIX HTTP/1.1 client for automated live multi-player Firebase RTDB E2E tests. */
#include <netdb.h>
#include <sys/socket.h>

typedef int (*DSStreamChunkFn)(void *ctx, const char *data, size_t len);

static int http_ex(const char *method, const char *url, const char *body, char *out, size_t cap,
                   const char *header, const char *value, char *etag, size_t etag_cap) {
    char host[128] = {0}, port[16] = "80", path[384] = "/";
    const char *p, *slash, *colon;
    struct addrinfo hints, *res = NULL;
    int fd = -1, code = 0;
    char req[4096];
    int req_len;
    size_t body_len = body ? strlen(body) : 0;

    if (out && cap)
        out[0] = '\0';
    if (etag && etag_cap)
        etag[0] = '\0';
    if (!url || strncmp(url, "http://", 7) != 0)
        return 0;

    p = url + 7;
    slash = strchr(p, '/');
    colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hl = (size_t)(colon - p);
        size_t pl = slash ? (size_t)(slash - colon - 1) : strlen(colon + 1);
        if (hl >= sizeof(host) || pl >= sizeof(port))
            return 0;
        memcpy(host, p, hl);
        memcpy(port, colon + 1, pl);
        port[pl] = '\0';
    } else {
        size_t hl = slash ? (size_t)(slash - p) : strlen(p);
        if (hl >= sizeof(host))
            return 0;
        memcpy(host, p, hl);
    }
    if (slash)
        snprintf(path, sizeof(path), "%s", slash);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res)
        return 0;

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        if (fd >= 0)
            close(fd);
        freeaddrinfo(res);
        return 0;
    }
    freeaddrinfo(res);

    req_len = snprintf(req, sizeof(req),
                       "%s %s HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\n"
                       "Content-Type: application/json\r\nContent-Length: %zu\r\n"
                       "%s%s%s%s\r\n",
                       method, path, host, port, body_len, header ? header : "", header ? ": " : "",
                       (header && value) ? value : "", header ? "\r\n" : "");
    if (send(fd, req, (size_t)req_len, 0) < 0 || (body_len > 0 && send(fd, body, body_len, 0) < 0)) {
        close(fd);
        return 0;
    }

    {
        size_t cap_raw = (cap > 65536 ? cap : 65536) + 2048;
        char *raw = (char *)malloc(cap_raw);
        size_t total = 0;
        ssize_t n;
        char *hdr_end;
        if (!raw) {
            close(fd);
            return 0;
        }
        while (total + 1 < cap_raw && (n = recv(fd, raw + total, cap_raw - 1 - total, 0)) > 0)
            total += (size_t)n;
        raw[total] = '\0';
        close(fd);
        code = (strncmp(raw, "HTTP/", 5) == 0 && strchr(raw, ' ')) ? atoi(strchr(raw, ' ') + 1) : 0;
        if (code <= 0) {
            free(raw);
            return 0;
        }
        hdr_end = strstr(raw, "\r\n\r\n");
        if (hdr_end && out && cap > 0) {
            const char *bstart = hdr_end + 4;
            size_t bl = total - (size_t)(bstart - raw);
            if (bl >= cap)
                bl = cap - 1;
            memcpy(out, bstart, bl);
            out[bl] = '\0';
        }
        free(raw);
    }
    return code;
}

static int http_stream(const char *url, const char *accept, DSStreamChunkFn cb, void *ctx) {
    (void)url;
    (void)accept;
    (void)cb;
    (void)ctx;
    return 0;
}
