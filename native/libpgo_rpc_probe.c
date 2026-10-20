/*
 * Diagnostic-only IL2CPP metadata probe for Pokémon GO 0.431.0 ARM64.
 * Reports RPC method signatures, code RVAs, field offsets, and static references.
 * It does not invoke managed methods or send game RPCs.
 */
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef void *pthread_t;

extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern ssize_t read(int, void *, size_t);
extern int close(int);
extern int usleep(unsigned int);
extern int pthread_create(pthread_t *, const void *, void *(*)(void *), void *);
extern int pthread_detach(pthread_t);
extern int __android_log_print(int, const char *, const char *, ...);
extern void *memset(void *, int, size_t);
extern int strcmp(const char *, const char *);
extern char *strstr(const char *, const char *);
extern size_t strlen(const char *);

#define O_RDONLY 0
#define RTLD_NOW 2
#define MAPS_CAPACITY (256UL * 1024UL)
#define LOGI(...) __android_log_print(4, "PGORPCPROBE431", __VA_ARGS__)
#define LOGE(...) __android_log_print(6, "PGORPCPROBE431", __VA_ARGS__)

typedef const void il2cpp_domain_t;
typedef const void il2cpp_assembly_t;
typedef const void il2cpp_image_t;
typedef const void il2cpp_class_t;
typedef const void il2cpp_method_t;
typedef const void il2cpp_field_t;
typedef const void il2cpp_type_t;

typedef il2cpp_domain_t *(*domain_get_fn)(void);
typedef il2cpp_assembly_t **(*domain_get_assemblies_fn)(il2cpp_domain_t *, size_t *);
typedef il2cpp_image_t *(*assembly_get_image_fn)(il2cpp_assembly_t *);
typedef const char *(*image_get_name_fn)(il2cpp_image_t *);
typedef size_t (*image_get_class_count_fn)(il2cpp_image_t *);
typedef il2cpp_class_t *(*image_get_class_fn)(il2cpp_image_t *, size_t);
typedef il2cpp_class_t *(*class_from_type_fn)(il2cpp_type_t *);
typedef const char *(*class_get_name_fn)(il2cpp_class_t *);
typedef const char *(*class_get_namespace_fn)(il2cpp_class_t *);
typedef il2cpp_method_t *(*class_get_methods_fn)(il2cpp_class_t *, void **);
typedef il2cpp_field_t *(*class_get_fields_fn)(il2cpp_class_t *, void **);
typedef const char *(*method_get_name_fn)(il2cpp_method_t *);
typedef uint32_t (*method_get_param_count_fn)(il2cpp_method_t *);
typedef const char *(*method_get_param_name_fn)(il2cpp_method_t *, uint32_t);
typedef il2cpp_type_t *(*method_get_param_fn)(il2cpp_method_t *, uint32_t);
typedef il2cpp_type_t *(*method_get_return_type_fn)(il2cpp_method_t *);
typedef char *(*type_get_name_fn)(il2cpp_type_t *);
typedef const char *(*field_get_name_fn)(il2cpp_field_t *);
typedef il2cpp_type_t *(*field_get_type_fn)(il2cpp_field_t *);
typedef int32_t (*field_get_offset_fn)(il2cpp_field_t *);
typedef uint32_t (*field_get_flags_fn)(il2cpp_field_t *);
typedef void (*field_static_get_value_fn)(il2cpp_field_t *, void *);
typedef void (*il2cpp_free_fn)(void *);
typedef void *(*thread_attach_fn)(il2cpp_domain_t *);
typedef void (*thread_detach_fn)(void *);

struct probe_api {
    domain_get_fn domain_get;
    domain_get_assemblies_fn domain_get_assemblies;
    assembly_get_image_fn assembly_get_image;
    image_get_name_fn image_get_name;
    image_get_class_count_fn image_get_class_count;
    image_get_class_fn image_get_class;
    class_from_type_fn class_from_type;
    class_get_name_fn class_get_name;
    class_get_namespace_fn class_get_namespace;
    class_get_methods_fn class_get_methods;
    class_get_fields_fn class_get_fields;
    method_get_name_fn method_get_name;
    method_get_param_count_fn method_get_param_count;
    method_get_param_name_fn method_get_param_name;
    method_get_param_fn method_get_param;
    method_get_return_type_fn method_get_return_type;
    type_get_name_fn type_get_name;
    field_get_name_fn field_get_name;
    field_get_type_fn field_get_type;
    field_get_offset_fn field_get_offset;
    field_get_flags_fn field_get_flags;
    field_static_get_value_fn field_static_get_value;
    il2cpp_free_fn il2cpp_free;
    thread_attach_fn thread_attach;
    thread_detach_fn thread_detach;
};

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

static int line_has(const char *line, const char *end, const char *needle) {
    for (const char *p = line; p < end; ++p) {
        const char *a = p, *b = needle;
        while (a < end && *b && *a == *b) { ++a; ++b; }
        if (*b == 0) return 1;
    }
    return 0;
}

static uintptr_t find_libil2cpp_base(void) {
    static char maps[MAPS_CAPACITY];
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, maps, MAPS_CAPACITY - 1);
    close(fd);
    if (n <= 0) return 0;
    maps[n] = 0;
    for (ssize_t i = 0; i < n;) {
        ssize_t e = i;
        while (e < n && maps[e] != '\n') ++e;
        const char *p = maps + i, *end = maps + e;
        if (line_has(p, end, "/libil2cpp.so")) {
            uintptr_t lo = parse_hex(&p, end);
            while (p < end && *p == '-') ++p;
            (void)parse_hex(&p, end);
            while (p < end && *p == ' ') ++p;
            while (p < end && *p != ' ') ++p;
            while (p < end && *p == ' ') ++p;
            if (parse_hex(&p, end) == 0) return lo;
        }
        i = e + 1;
    }
    return 0;
}

static int load_api(struct probe_api *a, void *h) {
#define LOAD(member, type, symbol) do { a->member = (type)dlsym(h, #symbol); if (!a->member) { LOGE("missing IL2CPP export: %s", #symbol); return 0; } } while (0)
    LOAD(domain_get, domain_get_fn, il2cpp_domain_get);
    LOAD(domain_get_assemblies, domain_get_assemblies_fn, il2cpp_domain_get_assemblies);
    LOAD(assembly_get_image, assembly_get_image_fn, il2cpp_assembly_get_image);
    LOAD(image_get_name, image_get_name_fn, il2cpp_image_get_name);
    LOAD(image_get_class_count, image_get_class_count_fn, il2cpp_image_get_class_count);
    LOAD(image_get_class, image_get_class_fn, il2cpp_image_get_class);
    LOAD(class_from_type, class_from_type_fn, il2cpp_class_from_type);
    LOAD(class_get_name, class_get_name_fn, il2cpp_class_get_name);
    LOAD(class_get_namespace, class_get_namespace_fn, il2cpp_class_get_namespace);
    LOAD(class_get_methods, class_get_methods_fn, il2cpp_class_get_methods);
    LOAD(class_get_fields, class_get_fields_fn, il2cpp_class_get_fields);
    LOAD(method_get_name, method_get_name_fn, il2cpp_method_get_name);
    LOAD(method_get_param_count, method_get_param_count_fn, il2cpp_method_get_param_count);
    LOAD(method_get_param_name, method_get_param_name_fn, il2cpp_method_get_param_name);
    LOAD(method_get_param, method_get_param_fn, il2cpp_method_get_param);
    LOAD(method_get_return_type, method_get_return_type_fn, il2cpp_method_get_return_type);
    LOAD(type_get_name, type_get_name_fn, il2cpp_type_get_name);
    LOAD(field_get_name, field_get_name_fn, il2cpp_field_get_name);
    LOAD(field_get_type, field_get_type_fn, il2cpp_field_get_type);
    LOAD(field_get_offset, field_get_offset_fn, il2cpp_field_get_offset);
    LOAD(field_get_flags, field_get_flags_fn, il2cpp_field_get_flags);
    LOAD(field_static_get_value, field_static_get_value_fn, il2cpp_field_static_get_value);
    LOAD(il2cpp_free, il2cpp_free_fn, il2cpp_free);
    LOAD(thread_attach, thread_attach_fn, il2cpp_thread_attach);
    LOAD(thread_detach, thread_detach_fn, il2cpp_thread_detach);
#undef LOAD
    return 1;
}

static int is_rpc_class(const char *ns, const char *name) {
    if (!ns || !name) return 0;
    if (!strcmp(ns, "Niantic.Holoholo.Rpc")) {
        return !strcmp(name, "RpcHandler") || !strcmp(name, "RpcHandlerExt") ||
               !strcmp(name, "RpcRequest`1") || !strcmp(name, "SideChannel") ||
               !strcmp(name, "SideChannel`2") || !strcmp(name, "IRpcHandler");
    }
    if (!strcmp(ns, "Niantic.Platform.RpcExtensions"))
        return !strcmp(name, "RpcHelper") || !strcmp(name, "IRpcHelper");
    if (!strcmp(ns, "Niantic.Platform.Ditto.Rpc"))
        return !strcmp(name, "RpcManager") || !strcmp(name, "ActionRequest") ||
               !strcmp(name, "RpcResponse") || !strcmp(name, "IRpcManager") ||
               !strcmp(name, "IDittoRpcManager") || !strcmp(name, "DittoRpcManager");
    return 0;
}

static int is_rpc_reference_type(const char *name) {
    return name && (strstr(name, "RpcHandler") || strstr(name, "IRpcHandler") ||
        strstr(name, "RpcManager") || strstr(name, "IRpcManager") ||
        strstr(name, "RpcHelper") || strstr(name, "IRpcHelper") ||
        strstr(name, "DittoRpcManager"));
}

static void dump_rpc_class(struct probe_api *a, il2cpp_class_t *klass,
                           const char *image, uintptr_t base) {
    const char *ns = a->class_get_namespace(klass);
    const char *name = a->class_get_name(klass);
    LOGI("CLASS image=%s type=%s.%s", image ? image : "?", ns ? ns : "", name ? name : "?");
    void *iter = 0;
    il2cpp_method_t *m;
    unsigned int count = 0;
    while ((m = (il2cpp_method_t *)a->class_get_methods(klass, &iter)) != 0 && count++ < 100) {
        const char *mn = a->method_get_name(m);
        il2cpp_type_t *rt = a->method_get_return_type(m);
        char *rtn = rt ? a->type_get_name(rt) : 0;
        uint32_t pc = a->method_get_param_count(m);
        uintptr_t entry = *(volatile uintptr_t *)m;
        if (base && entry >= base && entry < base + 0x20000000UL) {
            LOGI("METHOD %s.%s.%s rva=0x%lx params=%u return=%s", ns ? ns : "", name ? name : "", mn ? mn : "?", (unsigned long)(entry - base), pc, rtn ? rtn : "?");
        } else {
            LOGI("METHOD %s.%s.%s ptr=%p params=%u return=%s", ns ? ns : "", name ? name : "", mn ? mn : "?", (void *)entry, pc, rtn ? rtn : "?");
        }
        if (rtn) a->il2cpp_free(rtn);
        for (uint32_t j = 0; j < pc && j < 12; ++j) {
            const char *pn = a->method_get_param_name(m, j);
            il2cpp_type_t *pt = a->method_get_param(m, j);
            char *ptn = pt ? a->type_get_name(pt) : 0;
            LOGI("  PARAM[%u] %s : %s", j, pn ? pn : "?", ptn ? ptn : "?");
            if (ptn) a->il2cpp_free(ptn);
        }
    }
    iter = 0;
    il2cpp_field_t *f;
    count = 0;
    while ((f = (il2cpp_field_t *)a->class_get_fields(klass, &iter)) != 0 && count++ < 100) {
        const char *fn = a->field_get_name(f);
        il2cpp_type_t *ftype = a->field_get_type(f);
        char *ftn = ftype ? a->type_get_name(ftype) : 0;
        uint32_t flags = a->field_get_flags(f);
        LOGI("FIELD %s.%s.%s offset=0x%x flags=0x%x type=%s", ns ? ns : "", name ? name : "", fn ? fn : "?", (unsigned int)a->field_get_offset(f), flags, ftn ? ftn : "?");
        if ((flags & 0x10U) && is_rpc_reference_type(ftn)) {
            void *value = 0;
            a->field_static_get_value(f, &value);
            LOGI("STATIC_REF %s.%s.%s value=%p type=%s", ns ? ns : "", name ? name : "", fn ? fn : "?", value, ftn ? ftn : "?");
        }
        if (ftn) a->il2cpp_free(ftn);
    }
}

static int scan_owner_fields(struct probe_api *a, il2cpp_assembly_t **assemblies,
                             size_t assembly_count) {
    int hits = 0;
    for (size_t ai = 0; ai < assembly_count; ++ai) {
        il2cpp_image_t *image = a->assembly_get_image(assemblies[ai]);
        if (!image) continue;
        size_t nc = a->image_get_class_count(image);
        for (size_t ci = 0; ci < nc; ++ci) {
            il2cpp_class_t *owner = a->image_get_class(image, ci);
            if (!owner) continue;
            const char *ons = a->class_get_namespace(owner);
            const char *on = a->class_get_name(owner);
            void *iter = 0;
            il2cpp_field_t *f;
            while ((f = (il2cpp_field_t *)a->class_get_fields(owner, &iter)) != 0) {
                il2cpp_type_t *ft = a->field_get_type(f);
                il2cpp_class_t *fc = ft ? a->class_from_type(ft) : 0;
                if (!fc) continue;
                const char *fns = a->class_get_namespace(fc);
                const char *fnm = a->class_get_name(fc);
                if (!is_rpc_class(fns, fnm)) continue;
                const char *field_name = a->field_get_name(f);
                uint32_t flags = a->field_get_flags(f);
                int32_t offset = a->field_get_offset(f);
                LOGI("OWNER_FIELD owner=%s.%s field=%s offset=0x%x flags=0x%x type=%s.%s", ons ? ons : "", on ? on : "", field_name ? field_name : "?", (unsigned int)offset, flags, fns ? fns : "", fnm ? fnm : "?");
                if (flags & 0x10U) {
                    void *value = 0;
                    a->field_static_get_value(f, &value);
                    LOGI("OWNER_STATIC_REF owner=%s.%s field=%s value=%p type=%s.%s", ons ? ons : "", on ? on : "", field_name ? field_name : "?", value, fns ? fns : "", fnm ? fnm : "?");
                }
                ++hits;
            }
        }
    }
    return hits;
}

static void *probe_worker(void *unused) {
    (void)unused;
    struct probe_api a;
    memset(&a, 0, sizeof(a));
    void *h = 0;
    int ready = 0;
    il2cpp_domain_t *domain = 0;
    il2cpp_assembly_t **assemblies = 0;
    size_t nassemblies = 0;
    void *attached_thread = 0;
    for (int i = 0; i < 900; ++i) {
        if (!h) h = dlopen("libil2cpp.so", RTLD_NOW);
        if (h && !ready) {
            ready = load_api(&a, h);
            if (!ready) return 0;
        }
        if (ready) {
            domain = a.domain_get();
            if (domain) {
                if (!attached_thread) {
                    attached_thread = a.thread_attach(domain);
                    if (!attached_thread) {
                        LOGE("IL2CPP thread attach failed");
                        return 0;
                    }
                }
                assemblies = a.domain_get_assemblies(domain, &nassemblies);
                if (assemblies && nassemblies) break;
            }
        }
        usleep(100000);
    }
    if (!ready || !domain || !assemblies || !nassemblies) {
        LOGE("IL2CPP metadata probe unavailable ready=%d domain=%p assemblies=%p count=%lu", ready, domain, assemblies, (unsigned long)nassemblies);
        if (attached_thread) a.thread_detach(attached_thread);
        return 0;
    }
    uintptr_t base = find_libil2cpp_base();
    LOGI("metadata probe ready assemblies=%lu libil2cpp_base=%lx", (unsigned long)nassemblies, (unsigned long)base);
    int matches = 0;
    for (size_t ai = 0; ai < nassemblies; ++ai) {
        il2cpp_image_t *image = a.assembly_get_image(assemblies[ai]);
        if (!image) continue;
        const char *image_name = a.image_get_name(image);
        size_t nc = a.image_get_class_count(image);
        for (size_t ci = 0; ci < nc; ++ci) {
            il2cpp_class_t *klass = a.image_get_class(image, ci);
            if (!klass) continue;
            if (is_rpc_class(a.class_get_namespace(klass), a.class_get_name(klass))) {
                ++matches;
                dump_rpc_class(&a, klass, image_name, base);
            }
        }
    }
    int owners = scan_owner_fields(&a, assemblies, nassemblies);
    LOGI("RPC metadata probe complete matching_classes=%d owner_fields=%d; no managed methods invoked", matches, owners);
    if (attached_thread) a.thread_detach(attached_thread);
    return 0;
}

__attribute__((constructor)) static void pgo_rpc_probe_init(void) {
    LOGI("RPC metadata probe loaded; read-only diagnostics only");
    pthread_t thread;
    if (pthread_create(&thread, 0, probe_worker, 0) == 0) {
        (void)pthread_detach(thread);
    } else {
        LOGE("could not start RPC metadata probe thread");
    }
}
