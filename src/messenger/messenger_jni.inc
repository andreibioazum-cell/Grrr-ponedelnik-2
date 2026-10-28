#ifdef __ANDROID__
#include <jni.h>
static void java_string_to_utf8(JNIEnv *env, jstring value, char *output, size_t capacity) {
    if (!output || capacity == 0) return;
    output[0] = 0;
    if (!value) return;
    jsize length = (*env)->GetStringLength(env, value);
    const jchar *chars = (*env)->GetStringChars(env, value, NULL);
    if (!chars) return;
    size_t used = 0;
    for (jsize i = 0; i < length; i++) {
        uint32_t cp = chars[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < length
                && chars[i + 1] >= 0xDC00 && chars[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (chars[++i] - 0xDC00);
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        unsigned char encoded[4];
        size_t count;
        if (cp < 0x80) { encoded[0] = (unsigned char)cp; count = 1; }
        else if (cp < 0x800) {
            encoded[0] = (unsigned char)(0xC0 | (cp >> 6));
            encoded[1] = (unsigned char)(0x80 | (cp & 0x3F)); count = 2;
        } else if (cp < 0x10000) {
            encoded[0] = (unsigned char)(0xE0 | (cp >> 12));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | (cp & 0x3F)); count = 3;
        } else {
            encoded[0] = (unsigned char)(0xF0 | (cp >> 18));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[3] = (unsigned char)(0x80 | (cp & 0x3F)); count = 4;
        }
        if (used + count >= capacity) break;
        memcpy(output + used, encoded, count);
        used += count;
    }
    output[used] = 0;
    (*env)->ReleaseStringChars(env, value, chars);
}
JNIEXPORT void JNICALL Java_com_ponedelnik_messenger_MessengerActivity_nativeMessengerEvent(JNIEnv *env, jobject obj, jint type,
                                                                       jstring peer, jstring data) {
    char peer_utf8[40], data_utf8[TEXT_CAP];
    (void)obj;
    java_string_to_utf8(env, peer, peer_utf8, sizeof(peer_utf8));
    java_string_to_utf8(env, data, data_utf8, sizeof(data_utf8));
    messenger_app_on_java_event((int)type, peer_utf8, data_utf8);
}
#endif
