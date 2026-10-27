#include <jni.h>
#include <android/native_activity.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "messenger/app.h"

static ANativeActivity *app_activity;

void messenger_android_set_activity(void *activity) { app_activity = (ANativeActivity *)activity; }

static JNIEnv *bridge_env(int *attached) {
    JNIEnv *env = NULL;
    if (attached) *attached = 0;
    if (!app_activity || !app_activity->vm) return NULL;
    if ((*app_activity->vm)->GetEnv(app_activity->vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if ((*app_activity->vm)->AttachCurrentThread(app_activity->vm, &env, NULL) != JNI_OK) return NULL;
        if (attached) *attached = 1;
    }
    return env;
}

static void bridge_done(int attached) {
    if (attached && app_activity && app_activity->vm)
        (*app_activity->vm)->DetachCurrentThread(app_activity->vm);
}

static jstring bridge_string(JNIEnv *env, const char *value) {
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    size_t bytes = strlen((const char *)p);
    if (bytes > (size_t)INT32_MAX / sizeof(jchar) - 1) return NULL;
    jchar *chars = (jchar *)malloc((bytes + 1) * sizeof(*chars));
    if (!chars) return NULL;
    jsize count = 0;
    while (*p) {
        uint32_t cp;
        size_t width;
        if (*p < 0x80) { cp = *p; width = 1; }
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F); width = 2;
            if (cp < 0x80) width = 0;
        } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); width = 3;
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) width = 0;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(p[0] & 7) << 18) | ((uint32_t)(p[1] & 0x3F) << 12)
                    | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); width = 4;
            if (cp < 0x10000 || cp > 0x10FFFF) width = 0;
        } else { cp = 0xFFFD; width = 1; }
        if (!width) { cp = 0xFFFD; width = 1; }
        p += width;
        if (cp < 0x10000) chars[count++] = (jchar)cp;
        else {
            cp -= 0x10000;
            chars[count++] = (jchar)(0xD800 + (cp >> 10));
            chars[count++] = (jchar)(0xDC00 + (cp & 0x3FF));
        }
    }
    jstring result = (*env)->NewString(env, chars, count);
    free(chars);
    return result;
}

static void call0(const char *name) {
    int attached = 0;
    JNIEnv *env = bridge_env(&attached);
    if (!env || !app_activity->clazz) { bridge_done(attached); return; }
    if ((*env)->PushLocalFrame(env, 8) == 0) {
        jclass cls = (*env)->GetObjectClass(env, app_activity->clazz);
        jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "()V") : NULL;
        if (method) (*env)->CallVoidMethod(env, app_activity->clazz, method);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    bridge_done(attached);
}

static void call1(const char *name, const char *a) {
    int attached = 0;
    JNIEnv *env = bridge_env(&attached);
    if (!env || !app_activity->clazz) { bridge_done(attached); return; }
    if ((*env)->PushLocalFrame(env, 8) == 0) {
        jclass cls = (*env)->GetObjectClass(env, app_activity->clazz);
        jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "(Ljava/lang/String;)V") : NULL;
        if (method) {
            jstring ja = bridge_string(env, a);
            if (ja) (*env)->CallVoidMethod(env, app_activity->clazz, method, ja);
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    bridge_done(attached);
}

static void call2(const char *name, const char *a, const char *b) {
    int attached = 0;
    JNIEnv *env = bridge_env(&attached);
    if (!env || !app_activity->clazz) { bridge_done(attached); return; }
    if ((*env)->PushLocalFrame(env, 8) == 0) {
        jclass cls = (*env)->GetObjectClass(env, app_activity->clazz);
        jmethodID method = cls ? (*env)->GetMethodID(env, cls, name,
                          "(Ljava/lang/String;Ljava/lang/String;)V") : NULL;
        if (method) {
            jstring ja = bridge_string(env, a);
            jstring jb = bridge_string(env, b);
            if (ja && jb) (*env)->CallVoidMethod(env, app_activity->clazz, method, ja, jb);
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    bridge_done(attached);
}

void messenger_android_start(const char *url, const char *nickname) {
    int attached = 0;
    JNIEnv *env = bridge_env(&attached);
    if (!env || !app_activity->clazz) { bridge_done(attached); return; }
    if ((*env)->PushLocalFrame(env, 8) == 0) {
        jclass cls = (*env)->GetObjectClass(env, app_activity->clazz);
        jmethodID method = cls ? (*env)->GetMethodID(env, cls, "startMessenger",
                          "(Ljava/lang/String;Ljava/lang/String;)V") : NULL;
        if (method) {
            jstring ju = bridge_string(env, url);
            jstring jn = bridge_string(env, nickname ? nickname : "Guest");
            if (ju && jn) (*env)->CallVoidMethod(env, app_activity->clazz, method, ju, jn);
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    bridge_done(attached);
}

void messenger_android_connect(const char *id) { call1("connectMessenger", id); }
void messenger_android_accept(const char *id) { call1("acceptMessenger", id); }
void messenger_android_verify(const char *id, const char *fingerprint) { call2("verifyMessenger", id, fingerprint); }
void messenger_android_send(const char *id, const char *message) { call2("sendMessenger", id, message); }
void messenger_android_share(const char *id, const char *fingerprint) { call2("shareMessenger", id, fingerprint); }
void messenger_android_stop(void) { call0("stopMessenger"); }
