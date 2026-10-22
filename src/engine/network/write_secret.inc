/* Database secrets: the "write password" and the password suffix of an
 * account.
 *
  * Authentication alone is not enough. An account password is "player password
  * + suffix", so anyone who knows the suffix and the public API key can create
  * an account with someone else's nick and write to the database as them. The
  * suffix used to be "|cb4v1", guessed in a second. Now it is 32 random chars.
 *
  * The second barrier is the "w" field the client adds to EVERY write body:
  * firebase.rules.json rejects any write where newData.child('w').val() is not
  * the write secret. A script with only the API key gets nowhere, even signed
  * in.
 *
  * Both values live in engine/network.h in pieces, each byte xored with 0x5a,
  * so a grep over the built apk does not find the string. New values come from
  * tools/rotate_db_secret.py, which also edits firebase.rules.json. */

static const char *net_secret(char *buf, size_t cap, const char *const *chunks, size_t count) {
    size_t i, at = 0;
    for (i = 0; i < count && at + 1 < cap; i++) {
        const char *p = chunks[i];
        for (; *p && at + 1 < cap; p++) {
            buf[at++] = (char)((unsigned char)*p ^ NET_SECRET_XOR);
            buf[at] = 0;
        }
    }
    buf[at] = 0;
    return buf;
}

/* The "w" field: what the database rules check. */
static const char *net_write_secret(void) {
    static const char *chunks[] = {NET_WRITE_SECRET_0, NET_WRITE_SECRET_1, NET_WRITE_SECRET_2};
    static char secret[NET_WRITE_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Account password suffix. It never goes to the database. */
static const char *net_pass_suffix(void) {
    static const char *chunks[] = {NET_PASS_SECRET_0, NET_PASS_SECRET_1, NET_PASS_SECRET_2};
    static char secret[NET_PASS_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Old password suffix: every account created before NET_PASS_SECRET was
 * hashed with it, and it has leaked into the public repository. New accounts do
 * not use it; old ones are let in with it exactly once and the password is
 * immediately rewritten to the new suffix (fb_update_password). Once players
 * are moved, delete this string, the fb_sign_in_legacy() call and the macro. */
#define FB_LEGACY_PASS_SUFFIX "|cb4v1"

/* Append the "w" field to a write body. The body arrives as a JSON object from
 * the engine, so dropping the closing brace and appending our field is enough.
 * The secret is letters and digits only, no escaping needed. */
static int net_seal_body(char *body, size_t cap) {
    size_t n;
    const char *secret = net_write_secret();
    if (!body || cap < 4)
        return 0;
    n = strlen(body);
    if (n < 2 || body[n - 1] != '}')
        return 0;
        /* ,"w":" secret "} and the terminator; if it does not fit, leave the body */
    if (n + strlen(secret) + 8 > cap)
        return 0;
    body[--n] = 0;
    snprintf(body + n, cap - n, ",\"w\":\"%s\"}", secret);
    return 1;
}

/* Build both secrets up front, while the game is still single threaded. */
static void net_secret_prepare(void) {
    (void)net_write_secret();
    (void)net_pass_suffix();
}
