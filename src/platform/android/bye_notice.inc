#ifdef __ANDROID__
/* Farewell on exit: the text goes to Java and from there to the notification
 * shade. Everything else (the 50% chance, which line) is decided by the game. */
void bye_notice_show(const char *text) { (void)kb_call_text_method("showByeNotice", text); }
#else
void bye_notice_show(const char *text) { (void)text; }
#endif
