typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned char uint8_t;
typedef void *pthread_t;

/* Minimal bionic declarations: avoid relying on system headers in this payload. */
extern int open(const char*, int, ...);
extern ssize_t read(int, void*, size_t);
extern int close(int);
extern int usleep(unsigned int);
extern int pthread_create(pthread_t*, const void*, void*(*)(void*), void*);
extern int pthread_detach(pthread_t);
extern int __android_log_print(int, const char*, const char*, ...);
extern int mprotect(void*, size_t, int);

#define O_RDONLY 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define LOGI(...) __android_log_print(4, "PGOHOOK431", __VA_ARGS__)
#define LOGE(...) __android_log_print(6, "PGOHOOK431", __VA_ARGS__)
#define PAGE_SIZE 4096UL
#define MAPS_CAPACITY (1024UL * 1024UL)

/*
 * Verified from the exact Pokémon GO 0.431.0 ARM64 binary and metadata:
 * Holoholo.Rpc.CatchPokemonProto.WriteTo(CodedOutputStream)
 *   Method token 0x06002B93, entry RVA 0x0B00C95C, codegen pointer slot RVA 0x0E8422C8
 * Holoholo.Rpc.CatchPokemonProto.khg(ref WriteContext)
 *   Method token 0x06002B94, entry RVA 0x0B00C97C, codegen pointer slot RVA 0x0E8422D0
 *
 * Both entries have the same IL2CPP ARM64 argument layout for our purposes:
 * instance, output/writer pointer, hidden MethodInfo pointer.
 */
#define RVA_CATCH_WRITE_TO 0x0B00C95CUL
#define SLOT_CATCH_WRITE_TO 0x0E8422C8UL
#define RVA_CATCH_WRITE_CONTEXT 0x0B00C97CUL
#define SLOT_CATCH_WRITE_CONTEXT 0x0E8422D0UL
#define RVA_CATCH_CALCULATE_SIZE 0x0B00CE7CUL
#define SLOT_CATCH_CALCULATE_SIZE 0x0E8422D8UL

static char g_maps[MAPS_CAPACITY];
typedef void (*catch_write_t)(void*, void*, void*);
typedef int (*catch_calculate_size_t)(void*, void*);
static catch_write_t orig_catch_write_to;
static catch_write_t orig_catch_write_context;
static catch_calculate_size_t orig_catch_calculate_size;
static volatile unsigned int g_hook_calls;

static uintptr_t parse_hex(const char **p, const char *end) {
    uintptr_t v = 0;
    while (*p < end) {
        char c = **p;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = (v << 4) | (uintptr_t)d;
        ++*p;
    }
    return v;
}

static int line_has_path(const char *line, const char *end, const char *needle) {
    for (const char *p = line; p < end; ++p) {
        const char *a = p;
        const char *b = needle;
        while (a < end && *b && *a == *b) { ++a; ++b; }
        if (*b == 0) return 1;
    }
    return 0;
}

/* The ELF load bias is the start of the /libil2cpp.so mapping with file offset 0. */
static uintptr_t find_base(char *maps, ssize_t n) {
    const char *needle = "/libil2cpp.so";
    for (ssize_t i = 0; i < n;) {
        ssize_t e = i;
        while (e < n && maps[e] != '\n') ++e;
        const char *line = maps + i;
        const char *end = maps + e;

        if (line_has_path(line, end, needle)) {
            const char *p = line;
            uintptr_t lo = parse_hex(&p, end);
            while (p < end && *p == '-') ++p;
            (void)parse_hex(&p, end);
            while (p < end && *p == ' ') ++p;
            while (p < end && *p != ' ') ++p; /* permissions */
            while (p < end && *p == ' ') ++p;
            uintptr_t offset = parse_hex(&p, end);
            if (offset == 0) return lo;
        }
        i = e + 1;
    }
    return 0;
}

static int mapping_protection(char *maps, ssize_t n, uintptr_t address) {
    for (ssize_t i = 0; i < n;) {
        ssize_t e = i;
        while (e < n && maps[e] != '\n') ++e;
        const char *p = maps + i;
        const char *end = maps + e;
        uintptr_t lo = parse_hex(&p, end);
        while (p < end && *p == '-') ++p;
        uintptr_t hi = parse_hex(&p, end);
        while (p < end && *p == ' ') ++p;

        if (address >= lo && address < hi) {
            int prot = 0;
            if (p < end && p[0] == 'r') prot |= PROT_READ;
            if (p + 1 < end && p[1] == 'w') prot |= PROT_WRITE;
            if (p + 2 < end && p[2] == 'x') prot |= PROT_EXEC;
            return prot;
        }
        i = e + 1;
    }
    return -1;
}

static int install_exact_slot(uintptr_t slot_address, uintptr_t target,
                              uintptr_t replacement, char *maps, ssize_t n) {
    volatile uintptr_t *slot = (volatile uintptr_t*)slot_address;
    uintptr_t current = *slot;

    if (current == replacement) return 1;
    if (current != target) {
        LOGE("refusing to patch unexpected slot=%lx current=%lx expected=%lx",
             (unsigned long)slot_address, (unsigned long)current, (unsigned long)target);
        return 0;
    }

    int prot = mapping_protection(maps, n, slot_address);
    if (prot < 0 || !(prot & PROT_READ) || (prot & PROT_EXEC)) {
        LOGE("refusing to patch slot=%lx with unknown/unsafe protection=%d",
             (unsigned long)slot_address, prot);
        return 0;
    }

    uintptr_t page = slot_address & ~(PAGE_SIZE - 1UL);
    if (mprotect((void*)page, PAGE_SIZE, prot | PROT_WRITE) != 0) {
        LOGE("mprotect failed for method pointer slot=%lx", (unsigned long)slot_address);
        return 0;
    }

    *slot = replacement;
    int restore_result = mprotect((void*)page, PAGE_SIZE, prot);
    if (restore_result != 0) {
        LOGE("warning: failed to restore memory protection for slot=%lx",
             (unsigned long)slot_address);
    }

    LOGI("installed method pointer hook slot=%lx target=%lx replacement=%lx",
         (unsigned long)slot_address, (unsigned long)target, (unsigned long)replacement);
    return 1;
}

static void rewrite_catch_request(void *self) {
    if (!self) {
        LOGE("CatchPokemonProto hook received null self");
        return;
    }

    /*
     * CatchPokemonProto object offsets are confirmed by its generated serializer:
     * +0x20 NormalizedReticleSize (double)
     * +0x30 HitPokemon (bool)
     * +0x38 SpinModifier (double)
     * +0x40 NormalizedHitPosition (double)
     *
     * 1.95 lies in the Excellent range [1.7, 2.0). 0.850 is a documented
     * curveball request example; normalized hit position 1.0 represents the
     * centered/full hit value used in that same established protocol example.
     */
    double old_nrs = *(double*)((char*)self + 0x20);
    uint8_t old_hit = *((uint8_t*)((char*)self + 0x30));
    double old_spin = *(double*)((char*)self + 0x38);
    double old_hit_position = *(double*)((char*)self + 0x40);

    *(double*)((char*)self + 0x20) = 1.95;
    *((uint8_t*)((char*)self + 0x30)) = 1;
    *(double*)((char*)self + 0x38) = 0.850;
    *(double*)((char*)self + 0x40) = 1.0;

    unsigned int call = __sync_add_and_fetch(&g_hook_calls, 1);
    if (call <= 20) {
        LOGI("catch request #%u NRS %f->1.95 hit %u->1 spin %f->0.850 position %f->1.0",
             call, old_nrs, (unsigned int)old_hit, old_spin, old_hit_position);
    }
}

static void catch_write_to_hook(void *self, void *output, void *mi) {
    rewrite_catch_request(self);
    if (orig_catch_write_to) {
        orig_catch_write_to(self, output, mi);
    } else {
        LOGE("original CatchPokemonProto.WriteTo is null; serialization dropped");
    }
}

static void catch_write_context_hook(void *self, void *writer, void *mi) {
    rewrite_catch_request(self);
    if (orig_catch_write_context) {
        orig_catch_write_context(self, writer, mi);
    } else {
        LOGE("original CatchPokemonProto.khg is null; serialization dropped");
    }
}

/*
 * CalculateSize is also intercepted so the same fields are present when the
 * protobuf message length is computed. Changing optional zero fields only in
 * WriteTo could otherwise make the later serialized message longer than the
 * reserved output buffer.
 */
static int catch_calculate_size_hook(void *self, void *mi) {
    rewrite_catch_request(self);
    if (orig_catch_calculate_size) {
        return orig_catch_calculate_size(self, mi);
    }
    LOGE("original CatchPokemonProto.CalculateSize is null");
    return 0;
}

static void *worker(void *unused) {
    (void)unused;
    uintptr_t base = 0;
    ssize_t n = 0;

    for (int i = 0; i < 300 && !base; i++) {
        int fd = open("/proc/self/maps", O_RDONLY);
        if (fd >= 0) {
            n = read(fd, g_maps, MAPS_CAPACITY - 1);
            close(fd);
            if (n > 0) {
                g_maps[n] = 0;
                base = find_base(g_maps, n);
            }
        }
        if (!base) usleep(100000);
    }

    if (!base) {
        LOGE("libil2cpp base not found; enhanced throw hook not installed");
        return 0;
    }

    uintptr_t target_write_to = base + RVA_CATCH_WRITE_TO;
    uintptr_t target_write_context = base + RVA_CATCH_WRITE_CONTEXT;
    uintptr_t target_calculate_size = base + RVA_CATCH_CALCULATE_SIZE;
    uintptr_t slot_write_to = base + SLOT_CATCH_WRITE_TO;
    uintptr_t slot_write_context = base + SLOT_CATCH_WRITE_CONTEXT;
    uintptr_t slot_calculate_size = base + SLOT_CATCH_CALCULATE_SIZE;

    orig_catch_write_to = (catch_write_t)target_write_to;
    orig_catch_write_context = (catch_write_t)target_write_context;
    orig_catch_calculate_size = (catch_calculate_size_t)target_calculate_size;

    LOGI("libil2cpp base=%lx WriteTo=%lx slot=%lx khg=%lx slot=%lx CalculateSize=%lx slot=%lx",
         (unsigned long)base,
         (unsigned long)target_write_to, (unsigned long)slot_write_to,
         (unsigned long)target_write_context, (unsigned long)slot_write_context,
         (unsigned long)target_calculate_size, (unsigned long)slot_calculate_size);

    /* The library mapping exists only after relocation; read current maps once more. */
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        LOGE("unable to re-read maps; enhanced throw hook not installed");
        return 0;
    }
    n = read(fd, g_maps, MAPS_CAPACITY - 1);
    close(fd);
    if (n <= 0) {
        LOGE("unable to read maps; enhanced throw hook not installed");
        return 0;
    }
    g_maps[n] = 0;

    int calculate_size_ok = install_exact_slot(slot_calculate_size, target_calculate_size,
                                                (uintptr_t)catch_calculate_size_hook, g_maps, n);
    int write_to_ok = install_exact_slot(slot_write_to, target_write_to,
                                         (uintptr_t)catch_write_to_hook, g_maps, n);
    int write_context_ok = install_exact_slot(slot_write_context, target_write_context,
                                              (uintptr_t)catch_write_context_hook, g_maps, n);

    if (calculate_size_ok || write_to_ok || write_context_ok) {
        LOGI("Enhanced Throw hooks installed: CalculateSize=%d WriteTo=%d khg=%d",
             calculate_size_ok, write_to_ok, write_context_ok);
    } else {
        LOGE("no serializer hook installed; binary did not match verified 0.431.0 layout");
    }
    return 0;
}

__attribute__((constructor)) static void init(void) {
    LOGI("native enhanced-throw payload loaded");
    pthread_t thread;
    if (pthread_create(&thread, 0, worker, 0) == 0) {
        (void)pthread_detach(thread);
    } else {
        LOGE("failed to start enhanced-throw worker");
    }
}
