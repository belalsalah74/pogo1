typedef struct JavaVM JavaVM;
typedef int jint;
extern void *dlopen(const char*, int);
extern void *dlsym(void*, const char*);
extern char *dlerror(void);
#define RTLD_NOW 2

typedef jint (*jni_onload_t)(JavaVM*, void*);
__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM *vm, void *reserved){
  void *orig=dlopen("libmain_orig.so",RTLD_NOW);
  if(!orig) return -1;
  jni_onload_t fn=(jni_onload_t)dlsym(orig,"JNI_OnLoad");
  if(!fn) return -1;
  void *hook=dlopen("libpgo_hook.so",RTLD_NOW);
  (void)hook;
  return fn(vm,reserved);
}
