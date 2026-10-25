/* Lightweight zero-allocation JSON helpers & HTTP wrappers for Firebase RTDB. */

static int geo_http_get(const char *url, char *out, size_t cap) {
    int code = http_ex("GET", url, NULL, out, cap, NULL, NULL, NULL, 0);
    return code >= 200 && code < 300;
}

static int geo_http_put(const char *url, const char *json) {
    char out[256];
    int code = http_ex("PUT", url, json, out, sizeof(out), NULL, NULL, NULL, 0);
    return code >= 200 && code < 300;
}

static int geo_http_patch(const char *url, const char *json) {
    char out[256];
    int code = http_ex("PATCH", url, json, out, sizeof(out), NULL, NULL, NULL, 0);
    return code >= 200 && code < 300;
}

static int geo_http_delete(const char *url) {
    char out[128];
    int code = http_ex("DELETE", url, NULL, out, sizeof(out), NULL, NULL, NULL, 0);
    return code >= 200 && code < 300;
}

static void geo_sanitize_id(const char *src, char *dst, size_t cap, const char *fallback) {
    size_t i = 0, j = 0;
    if (src) {
        while (src[i] && j + 1 < cap) {
            unsigned char c = (unsigned char)src[i++];
            if (isalnum(c) || c == '_' || c == '-')
                dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
    if (j == 0 && fallback)
        snprintf(dst, cap, "%s", fallback);
}

static void geo_json_escape(const char *src, char *dst, size_t cap) {
    size_t i = 0, j = 0;
    if (!src || !cap) {
        if (cap)
            dst[0] = '\0';
        return;
    }
    while (src[i] && j + 2 < cap) {
        unsigned char c = (unsigned char)src[i++];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c >= 0x20) {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

/* Decode JSON string starting right after opening '"' (including \uXXXX UTF-8 surrogates/Cyrillic!) */
static const char *geo_json_read_str(const char *p, char *out, size_t cap) {
    size_t j = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'u' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2]) &&
                isxdigit((unsigned char)p[3]) && isxdigit((unsigned char)p[4])) {
                char hex[5] = {p[1], p[2], p[3], p[4], 0};
                unsigned int cp = (unsigned int)strtoul(hex, NULL, 16);
                p += 5;
                if (cp < 0x80 && j + 1 < cap) {
                    out[j++] = (char)cp;
                } else if (cp < 0x800 && j + 2 < cap) {
                    out[j++] = (char)(0xC0 | (cp >> 6));
                    out[j++] = (char)(0x80 | (cp & 0x3F));
                } else if (j + 3 < cap) {
                    out[j++] = (char)(0xE0 | (cp >> 12));
                    out[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[j++] = (char)(0x80 | (cp & 0x3F));
                }
                continue;
            }
            if (*p == 'n')
                p++;
            else if (j + 1 < cap)
                out[j++] = *p++;
            else
                p++;
        } else {
            if (j + 1 < cap)
                out[j++] = *p;
            p++;
        }
    }
    if (cap)
        out[j] = '\0';
    return (*p == '"') ? (p + 1) : p;
}

static const char *geo_obj_field(const char *obj_start, const char *obj_end, const char *key) {
    char pat[40];
    size_t plen;
    const char *p = obj_start;
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    plen = strlen(pat);
    while (p && p + plen < obj_end) {
        const char *hit = strstr(p, pat);
        if (!hit || hit >= obj_end)
            return NULL;
        hit += plen;
        while (hit < obj_end && (*hit == ' ' || *hit == '\t'))
            hit++;
        return hit;
    }
    return NULL;
}

static double geo_obj_num(const char *s, const char *e, const char *key, double defv) {
    const char *v = geo_obj_field(s, e, key);
    char *endp = NULL;
    double d;
    if (!v)
        return defv;
    d = strtod(v, &endp);
    return (endp && endp != v) ? d : defv;
}

static void geo_obj_str(const char *s, const char *e, const char *key, char *out, size_t cap) {
    const char *v = geo_obj_field(s, e, key);
    if (out && cap)
        out[0] = '\0';
    if (!v || *v != '"')
        return;
    (void)geo_json_read_str(v + 1, out, cap);
}

static const char *geo_skip_balanced(const char *p) {
    int depth = 0, in_str = 0;
    while (*p) {
        if (in_str) {
            if (*p == '\\' && p[1])
                p++;
            else if (*p == '"')
                in_str = 0;
        } else {
            if (*p == '"')
                in_str = 1;
            else if (*p == '{' || *p == '[')
                depth++;
            else if (*p == '}' || *p == ']') {
                depth--;
                if (depth <= 0)
                    return p + 1;
            }
        }
        p++;
    }
    return p;
}
