/* The message text lives on screen for many frames, while pool strings
 * (ds_concat, ds_num_to_string) are freed after two frames: selling a sticker
 * stored such a pointer in acct_msg, and a couple of frames later draw_acct_msg
 * read freed memory - a crash on the phone. So everything coming from outside is
 * copied into this buffer. */
static char acct_msg_buf[160];
static const char *acct_msg = "";
static void acct_msg_set(const char *text) {
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(acct_msg_buf) - 1)) {
        acct_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    acct_msg_buf[(int)i] = 0;
    acct_msg = acct_msg_buf;
}
