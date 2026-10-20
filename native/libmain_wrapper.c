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

static void load_optional_payload(const char *soname, const char *description) {
    void *handle = dlopen(soname, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        LOGI("optional payload not loaded: %s (%s)", description, soname);
        return;
    }
    LOGI("optional payload loaded: %s (%s)", description, soname);
}

__attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("wrapper JNI_OnLoad entered");
    void *orig = dlopen("libmain_orig.so", RTLD_NOW | RTLD_LOCAL);
    if (!orig) {
        LOGE("failed to load libmain_orig.so");
        return -1;
    }

    jni_onload_t fn = (jni_onload_t)dlsym(orig, "JNI_OnLoad");
    jint version = JNI_VERSION_1_6;
    if (fn) {
        version = fn(vm, reserved);
        if (version < 0) {
            LOGE("original JNI_OnLoad failed: %d", version);
            return version;
        }
    } else {
        LOGI("original library has no JNI_OnLoad; continuing with JNI 1.6");
    }

    /* Feature payloads are optional; each Morphe patch adds only its own .so. */
    load_optional_payload("libpgo_hook.so", "Enhanced Throw");
    load_optional_payload("libpgo_rpc_probe.so", "RPC Runtime Probe");
    return version;
}
