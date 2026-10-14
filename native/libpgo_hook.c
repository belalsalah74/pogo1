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
extern int mprotect(void*, size_t, int);
#define O_RDONLY 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define LOGI(...) __android_log_print(4,"PGOHOOK431",__VA_ARGS__)
#define IL2CPP_RVA_CURVE_SET 0xAC16314UL
#define PAGE_SIZE 4096UL
#define MAPS_CAPACITY (1024UL * 1024UL)
static char g_maps[MAPS_CAPACITY];

typedef void (*curve_set_t)(void*, float, float, float, void*);
static curve_set_t orig_curve_set;
static volatile unsigned int g_hook_calls;

static void curve_set_hook(void *self, float x, float y, float z, void *mi) {
    unsigned int call = __sync_add_and_fetch(&g_hook_calls, 1);
    if (call <= 10) LOGI("curve setter invoked #%u self=%p vector=(%f,%f,%f) method=%p",
                         call, self, x, y, z, mi);
    if (orig_curve_set) {
        float mag = x*x + y*y + z*z;
        if (mag < 0.000001f) x = 1.0f;
        orig_curve_set(self, x, y, z, mi);
    } else {
        LOGI("ERROR: original curve setter is null; call dropped");
    }
}

static uintptr_t parse_hex(const char **p, const char *end) {
    uintptr_t v=0; while(*p<end) { char c=**p; int d;
        if(c>='0'&&c<='9') d=c-'0'; else if(c>='a'&&c<='f') d=c-'a'+10; else if(c>='A'&&c<='F') d=c-'A'+10; else break;
        v=(v<<4)|(uintptr_t)d; ++*p;
    } return v;
}
static uintptr_t find_base(char *maps, ssize_t n) {
    const char *needle="/libil2cpp.so";
    for(ssize_t i=0;i<n;i++) {
        int m=1; for(int j=0;needle[j];j++) if(i+j>=n||maps[i+j]!=needle[j]){m=0;break;}
        if(!m) continue;
        ssize_t s=i; while(s>0&&maps[s-1]!='\n')--s;
        const char *p=maps+s, *end=maps+i;
        uintptr_t lo=parse_hex(&p,end);
        while(p<end&&*p!='-')++p; if(p<end)++p;
        (void)parse_hex(&p,end);
        while(p<end&&*p==' ')++p;
        const char *q=p; while(q<end&&*q!=' ')++q;
        while(q<end&&*q==' ')++q;
        uintptr_t off=parse_hex(&q,end);
        if(off==0) return lo;
    }
    return 0;
}
static int install(uintptr_t base, char *maps, ssize_t n) {
    uintptr_t target=base+IL2CPP_RVA_CURVE_SET; int found=0;
    const char *needle="/libil2cpp.so";
    for(ssize_t i=0;i<n;) {
        ssize_t e=i; while(e<n&&maps[e]!='\n')++e;
        int il2cpp=0; for(ssize_t q=i;q+(ssize_t)12<e;q++){int m=1;for(int j=0;needle[j];j++)if(maps[q+j]!=needle[j]){m=0;break;}if(m){il2cpp=1;break;}}
        if(il2cpp) {
            const char *p=maps+i,*end=maps+e;
            uintptr_t lo=parse_hex(&p,end); if(p<end)++p; uintptr_t hi=parse_hex(&p,end);
            while(p<end&&*p==' ')++p;
            int prot=0; if(p<end&&p[0]=='r')prot|=PROT_READ; if(p+1<end&&p[1]=='w')prot|=PROT_WRITE; if(p+2<end&&p[2]=='x')prot|=PROT_EXEC;
            if((prot&PROT_READ)&&!(prot&PROT_EXEC)&&hi>lo) {
                uintptr_t start=(lo+7UL)&~7UL;
                for(uintptr_t a=start;a+sizeof(uintptr_t)<=hi;a+=sizeof(uintptr_t)) {
                    volatile uintptr_t *slot=(volatile uintptr_t*)a;
                    if(*slot!=target) continue;
                    uintptr_t page=a&~(PAGE_SIZE-1UL);
                    if(mprotect((void*)page,PAGE_SIZE,prot|PROT_WRITE)!=0) { LOGI("mprotect failed slot=%lx", (unsigned long)a); continue; }
                    orig_curve_set=(curve_set_t)target;
                    *slot=(uintptr_t)curve_set_hook;
                    (void)mprotect((void*)page,PAGE_SIZE,prot);
                    ++found;
                }
            }
        }
        i=e+1;
    }
    return found;
}
static void *worker(void *unused) {
    (void)unused;
    char *maps=g_maps; uintptr_t base=0; ssize_t n=0;
    for(int i=0;i<300&&!base;i++) {
        int fd=open("/proc/self/maps",O_RDONLY); if(fd>=0){n=read(fd,maps,MAPS_CAPACITY-1);close(fd);if(n>0){maps[n]=0;base=find_base(maps,n);}}
        if(!base)usleep(100000);
    }
    if(!base){LOGI("libil2cpp base not found");return 0;}
    LOGI("libil2cpp base=%lx target=%lx", (unsigned long)base, (unsigned long)(base+IL2CPP_RVA_CURVE_SET));
    for(int i=0;i<8;i++) {
        int fd=open("/proc/self/maps",O_RDONLY); if(fd<0){usleep(250000);continue;}
        n=read(fd,maps,MAPS_CAPACITY-1);close(fd);if(n<=0){usleep(250000);continue;}maps[n]=0;
        int matches=install(base,maps,n);
        if(matches){LOGI("curve setter pointer slot installed matches=%d; awaiting invocation",matches);return 0;}
        usleep(250000);
    }
    LOGI("curve setter pointer slot not found target=%lx", (unsigned long)(base+IL2CPP_RVA_CURVE_SET));
    return 0;
}
__attribute__((constructor)) static void init(void) {
    LOGI("native hook payload loaded");
    pthread_t t;
    if(pthread_create(&t,0,worker,0)==0) pthread_detach(t);
    else LOGI("failed to start hook worker");
}
