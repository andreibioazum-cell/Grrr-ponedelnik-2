#ifndef HOST_STUB_ANDROID_ACTIVITY_H
#define HOST_STUB_ANDROID_ACTIVITY_H
#include <jni.h>
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <android/configuration.h>
struct ANativeActivity;
typedef struct ANativeActivityCallbacks ANativeActivityCallbacks;
typedef struct ANativeActivity {
    struct ANativeActivityCallbacks *callbacks;
    JavaVM *vm;
    JNIEnv *env;
    jobject clazz;
    const char *internalDataPath;
    const char *obbPath;
    AAssetManager *assetManager;
    const char *dataPath;
    void *instance;
} ANativeActivity;
enum {
    ANATIVEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT = 0x0001,
    ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED = 0x0002,
    ANATIVEACTIVITY_HIDE_SOFT_INPUT_IMPLICIT_ONLY = 0x0001,
    ANATIVEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS = 0x0002
};
void ANativeActivity_showSoftInput(ANativeActivity *activity, uint32_t flags);
void ANativeActivity_hideSoftInput(ANativeActivity *activity, uint32_t flags);
#endif
