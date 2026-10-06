typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef struct { unsigned long opaque[8]; } pthread_t;
extern int open(const char*, int, ...);
extern ssize_t read(int, void*, size_t);
extern int close(int);
extern int usleep(unsigned int);
extern int pthread_create(pthread_t*, const void*, void*(*)(void*), void*);
extern int pthread_detach(pthread_t);
extern int __android_log_print(int,const char*,const char*,...);
#define O_RDONLY 0
#define LOGI(...) __android_log_print(4,"PGOHOOK",__VA_ARGS__)

#define IL2CPP_RVA_CURVE_SET 0x86e53e0UL
#define IL2CPP_RVA_HIT_SET   0x86e7144UL
#define IL2CPP_RVA_LAUNCH    0x86e8bb4UL
#define IL2CPP_SLOT_CURVE_SET 0xb0fee68UL
#define IL2CPP_SLOT_HIT_SET   0xb0fef50UL
#define IL2CPP_SLOT_LAUNCH    0xb0fefc8UL

typedef void (*curve_set_t)(void*, float, float, float, void*);
typedef void (*hit_set_t)(void*, void*, void*);
typedef void (*launch_t)(void*, float, float, void*);
static curve_set_t orig_curve_set;
static hit_set_t orig_hit_set;
static launch_t orig_launch;

static uintptr_t find_base(void) {
  int fd=open("/proc/self/maps",O_RDONLY); if(fd<0) return 0;
  char buf[32768]; ssize_t n=read(fd,buf,sizeof(buf)-1); close(fd); if(n<=0)return 0; buf[n]=0;
  const char *needle="/libil2cpp.so";
  for(ssize_t i=0;i<n;i++){
    int match=1; for(int j=0;needle[j];j++){ if(i+j>=n || buf[i+j]!=needle[j]){match=0;break;} }
    if(!match)continue;
    ssize_t s=i; while(s>0 && buf[s-1]!='\n')s--; uintptr_t v=0; int k=0;
    while(s<n && buf[s]>='0'&&buf[s]<='9' || s<n && buf[s]>='a'&&buf[s]<='f') { v=v*16+(buf[s]<='9'?buf[s]-'0':buf[s]-'a'+10); s++; k++; }
    if(k) return v;
  }
  return 0;
}

static void curve_set_hook(void *self,float x,float y,float z,void *mi){
  /* PGSharp's Curve option is a client-side throw modifier. We preserve the
     caller's vector when it is already non-zero; otherwise inject a minimal
     non-zero curve vector. This is deliberately conservative for testing. */
  float mag=x*x+y*y+z*z;
  if(mag < 0.000001f) x=1.0f;
  orig_curve_set(self,x,y,z,mi);
}

static void hit_set_hook(void *self,void *collision,void *mi){
  /* 100% Hit needs the game's collision object, so do not fabricate one. */
  orig_hit_set(self,collision,mi);
}

static void launch_hook(void *self,float a,float b,void *mi){
  /* ABI-verified Launch interception point. Behavior remains unchanged until
     throw geometry is independently validated on-device. */
  orig_launch(self,a,b,mi);
}

static void *worker(void *unused){
  (void)unused;
  uintptr_t base=0;
  for(int i=0;i<300 && !base;i++){ base=find_base(); if(!base)usleep(100000); }
  if(!base){LOGI("libil2cpp not found");return 0;}
  uintptr_t *curve_slot=(uintptr_t*)(base+IL2CPP_SLOT_CURVE_SET);
  uintptr_t *hit_slot=(uintptr_t*)(base+IL2CPP_SLOT_HIT_SET);
  uintptr_t *launch_slot=(uintptr_t*)(base+IL2CPP_SLOT_LAUNCH);
  orig_curve_set=(curve_set_t)*curve_slot;
  orig_hit_set=(hit_set_t)*hit_slot;
  orig_launch=(launch_t)*launch_slot;
  if((uintptr_t)orig_curve_set != base+IL2CPP_RVA_CURVE_SET ||
     (uintptr_t)orig_hit_set != base+IL2CPP_RVA_HIT_SET ||
     (uintptr_t)orig_launch != base+IL2CPP_RVA_LAUNCH){
    LOGI("method-table validation failed: %lx %lx %lx",(unsigned long)orig_curve_set,(unsigned long)orig_hit_set,(unsigned long)orig_launch); return 0;
  }
  *curve_slot=(uintptr_t)curve_set_hook;
  *hit_slot=(uintptr_t)hit_set_hook;
  *launch_slot=(uintptr_t)launch_hook;
  LOGI("hooks installed at libil2cpp base %lx",(unsigned long)base);
  return 0;
}

__attribute__((constructor)) static void init(void){
  pthread_t t; if(pthread_create(&t,0,worker,0)==0) pthread_detach(t);
}
