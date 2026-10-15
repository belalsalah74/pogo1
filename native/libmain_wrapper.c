typedef struct JavaVM JavaVM;
typedef int jint;

extern void *dlopen(const char *filename, int flags);
extern void *dlsym(void *handle, const char *symbol);
extern int __android_log_print(int, const char *, const char *, ...);

#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define JNI_VERSION_1_6 0x00010006
#define LOGE(...) __android_log_print(6, "PGOLOADER431", __VA_ARGS__)
#define LOGI(...) __android_log_print(4, "PGOLOADER431", __VA_ARGS__)

typedef jint (*jni_onload_t)(JavaVM *, void *);

__attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("wrapper JNI_OnLoad entered");
    void *orig = dlopen("libmain_orig.so", RTLD_NOW | RTLD_LOCAL);
    if (!orig) {
        LOGE("failed to load libmain_orig.so");
        return -1;
    }

    jni_onload_t fn = (jni_onload_t)dlsym(orig, "JNI_OnLoad");
    if (!fn) {
        // JNI_OnLoad is optional for a native library. Do not reject an otherwise
        // loadable original library just because it does not export this symbol.
        LOGI("original library has no JNI_OnLoad; continuing with JNI 1.6");
        return JNI_VERSION_1_6;
    }

    // Let the original library initialize first. The hook's constructor starts
    // an asynchronous worker, so loading it before Unity's JNI initialization
    // can introduce an avoidable startup race.
    jint version = fn(vm, reserved);
    if (version < 0) {
        LOGE("original JNI_OnLoad failed: %d", version);
        return version;
    }

    void *hook = dlopen("libpgo_hook.so", RTLD_NOW | RTLD_LOCAL);
    if (!hook) {
        LOGE("failed to load libpgo_hook.so; continuing without hook");
    } else {
        LOGI("native hook payload loaded after original JNI_OnLoad");
    }

    return version;
}
