static size_t chat_utf8_step(const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    if (*p < 0x80) return 1;
    if ((*p & 0xE0) == 0xC0 && p[1]) return 2;
    if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) return 3;
    if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) return 4;
    return 1;
}

static void chat_copy_span(char *destination, const char *first, const char *last) {
    size_t length = (size_t)(last - first);
    if (length >= TEXT_CAP) length = TEXT_CAP - 1;
    memcpy(destination, first, length);
    destination[length] = 0;
}

static void wrap_chat_text(ChatLine *line, const char *message) {
    const char *cursor = message ? message : "";
    float scale = u(0.42f);
    float max_width = scale > 0 ? (content_w() * 0.82f - u(24)) / scale : 720.0f;
    unsigned count = 0;
    if (max_width < 64.0f) max_width = 64.0f;
    while (*cursor && count < CHAT_LINES) {
        const char *start = cursor, *scan = cursor, *space = NULL;
        int finished = 0;
        while (*scan) {
            if (*scan == '\n') {
                chat_copy_span(line->lines[count++], start, scan);
                cursor = scan + 1;
                finished = 1;
                break;
            }
            const char *after = scan + chat_utf8_step(scan);
            char candidate[TEXT_CAP];
            chat_copy_span(candidate, start, after);
            if (scan > start && (float)text_ink_width(candidate) > max_width) {
                const char *end = space && space > start ? space : scan;
                chat_copy_span(line->lines[count++], start, end);
                cursor = space && space > start ? space + 1 : scan;
                while (*cursor == ' ') cursor++;
                finished = 1;
                break;
            }
            if (*scan == ' ') space = scan;
            scan = after;
        }
        if (!finished) {
            chat_copy_span(line->lines[count++], start, scan);
            cursor = scan;
        }
    }
    if (count == 0) {
        line->lines[0][0] = 0;
        count = 1;
    } else if (*cursor && count == CHAT_LINES) {
        size_t length = strlen(line->lines[count - 1]);
        if (length + 3 < TEXT_CAP) memcpy(line->lines[count - 1] + length, "…", 4);
    }
    line->line_count = count;
}
