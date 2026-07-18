#include "kn/platform.h"
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(_MSC_VER)
#define KN_TLS __declspec(thread)
#else
#define KN_TLS __thread
#endif

#if defined(__clang__) || defined(__GNUC__)
#define KN_NO_ADDRESS_SANITIZE __attribute__((no_sanitize("address")))
#else
#define KN_NO_ADDRESS_SANITIZE
#endif

typedef struct KnGcBlock KnGcBlock;
typedef struct KnGcFrame KnGcFrame;
typedef struct KnGcRoot KnGcRoot;

struct KnGcBlock
{
    KnGcBlock *next;
    void *ptr;
    uint64_t size;
    uint8_t marked;
};

struct KnGcRoot
{
    void *addr;
    uint64_t size;
};

struct KnGcFrame
{
    KnGcFrame *prev;
    int count;
    int cap;
    KnGcRoot *roots;
};

void __kn_gc_collect(void);

static KnGcBlock *g_gc_blocks = 0;
static KN_TLS KnGcFrame *g_gc_frame = 0;
static KN_TLS uintptr_t g_gc_stack_base = 0;
static KnGcRoot *g_gc_global_roots = 0;
static int g_gc_global_root_count = 0;
static int g_gc_global_root_cap = 0;
static uint64_t g_gc_bytes = 0;
static uint64_t g_gc_threshold = 8ULL * 1024ULL * 1024ULL;
static int g_gc_collecting = 0;
static volatile int g_gc_lock = 0;
static volatile int g_async_live_tasks = 0;

typedef struct
{
    const char *key;
    uint64_t tag;
    uint64_t payload;
    uint8_t used;
} KnDictEntry;

typedef struct
{
    uint64_t count;
    uint64_t cap;
    KnDictEntry *entries;
} KnDict;

typedef struct
{
    uint64_t count;
    uint64_t cap;
    uint64_t *tags;
    uint64_t *payloads;
} KnList;

typedef struct
{
    const char **data;
    uint64_t len;
} KnStringArray;

typedef struct
{
    const char *key;
    uint8_t used;
} KnSetEntry;

typedef struct
{
    uint64_t count;
    uint64_t cap;
    KnSetEntry *entries;
} KnSet;

typedef struct
{
    const char *name;
    uint64_t tag;
    uint64_t payload;
} KnMetaExportArg;

typedef struct
{
    const char *owner_name;
    uint64_t owner_kind;
    uint64_t owner_param_count;
    void *owner_ptr;
    const char *meta_qname;
    const char *meta_name;
    const KnMetaExportArg *args;
    uint64_t arg_count;
} KnMetaExportInstance;

typedef struct
{
    const char *qname;
    const char *name;
    uint64_t target_mask;
    uint64_t keep_kind;
    uint64_t repeatable;
    const char **param_names;
    uint64_t param_count;
} KnMetaExportType;

typedef struct
{
    const char *module_name;
    const KnMetaExportType *types;
    uint64_t type_count;
    const KnMetaExportInstance *instances;
    uint64_t instance_count;
} KnMetaExportModule;

typedef struct KnMetaModuleReg KnMetaModuleReg;
struct KnMetaModuleReg
{
    void *handle;
    const KnMetaExportModule *module;
    KnMetaModuleReg *next;
};

static KN_TLS void *g_exc_current = 0;
static KN_TLS void *g_exc_last = 0;
static KN_TLS char g_exc_trace[4096];
static KN_TLS uint64_t g_exc_trace_len = 0;
static KN_TLS char g_exc_last_trace[4096];
static KN_TLS uint64_t g_exc_last_trace_len = 0;

typedef struct
{
    uint16_t *ptr;
    uint64_t cap;
} KnWideTempSlot;

static KnWideTempSlot g_wide_temp_ring[8];
static uint32_t g_wide_temp_index = 0;
static char g_sys_last_error[256];
static KnMetaModuleReg *g_meta_modules = 0;

enum
{
    KN_ANY_TAG_NONE = 0,
    KN_ANY_TAG_I64 = 1,
    KN_ANY_TAG_F64 = 2,
    KN_ANY_TAG_BOOL = 3,
    KN_ANY_TAG_CHAR = 4,
    KN_ANY_TAG_STRING = 5,
    KN_ANY_TAG_PTR = 6
};

void *__kn_gc_alloc(uint64_t size);
void *__kn_exc_get(void);
int __kn_exc_has(void);
const char *__kn_exc_trace(void);
static void rt_memcpy(void *dst, const void *src, size_t n);
static uint64_t rt_strlen(const char *s);
static char *rt_strdup_gc(const char *s);
static int rt_is_path_sep(char c);
static void rt_meta_register_main_module(const KnMetaExportModule *module);
static void rt_meta_register_loaded_module(void *handle, const KnMetaExportModule *module);
static void rt_meta_unregister_handle(void *handle);

typedef struct
{
    uint64_t tag;
    uint64_t payload;
} KnAnyValue;

typedef struct KnAsyncTask KnAsyncTask;
typedef void (*KnAsyncTaskEntryFn)(void *env, void *task);

struct KnAsyncTask
{
    volatile int done;
    volatile int faulted;
    volatile int joined;
    void *error;
    uint64_t result_tag;
    uint64_t result_payload;
    void *env;
    KnAsyncTaskEntryFn entry;
    uint64_t error_trace_len;
    char error_trace[4096];
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE thread;
#else
    pthread_t thread;
#endif
};

static void rt_spin_lock(volatile int *lock)
{
    while (__sync_lock_test_and_set(lock, 1))
    {
#if defined(_WIN32) || defined(_WIN64)
        Sleep(0);
#else
        sched_yield();
#endif
    }
}

static void rt_spin_unlock(volatile int *lock)
{
    __sync_lock_release(lock);
}

static int rt_atomic_load_i32(volatile int *value)
{
    return __sync_add_and_fetch(value, 0);
}

static void rt_atomic_store_i32(volatile int *value, int new_value)
{
    __sync_synchronize();
    __sync_lock_test_and_set(value, new_value);
}

static int rt_atomic_inc_i32(volatile int *value)
{
    return __sync_add_and_fetch(value, 1);
}

static int rt_atomic_dec_i32(volatile int *value)
{
    return __sync_sub_and_fetch(value, 1);
}

static int rt_async_task_begin_finish(KnAsyncTask *task)
{
    if (!task) return 0;
    return __sync_bool_compare_and_swap(&task->done, 0, 2);
}

static void rt_async_task_publish_finish(KnAsyncTask *task)
{
    if (!task) return;
    rt_atomic_store_i32(&task->done, 1);
}

static void rt_set_sys_error(const char *text)
{
    uint64_t n = rt_strlen(text);
    if (n >= (uint64_t)sizeof(g_sys_last_error))
        n = (uint64_t)sizeof(g_sys_last_error) - 1;
    if (n > 0)
        rt_memcpy(g_sys_last_error, text, (size_t)n);
    g_sys_last_error[n] = 0;
}

static void rt_clear_sys_error(void)
{
    g_sys_last_error[0] = 0;
}

const char *__kn_sys_last_error(void)
{
    if (!g_sys_last_error[0]) return "";
    return rt_strdup_gc(g_sys_last_error);
}

uint64_t __kn_time_tick(void);
void __kn_time_sleep(int32_t ms);
const char *__kn_path_normalize(const char *p);

#if defined(_WIN32) || defined(_WIN64)
static KN_HANDLE g_heap = 0;
#define KN_PATH_SEP '\\'

static void *rt_alloc(size_t size)
{
    if (!g_heap)
        g_heap = GetProcessHeap();
    return HeapAlloc(g_heap, 0, size);
}

static void rt_free(void *p)
{
    if (!p) return;
    if (!g_heap)
        g_heap = GetProcessHeap();
    HeapFree(g_heap, 0, p);
}

typedef struct
{
    char **items;
    int count;
    int cap;
} RtArgList;

static void rt_arglist_push(RtArgList *list, char *text)
{
    if (!list) return;
    if (list->count >= list->cap)
    {
        int new_cap = list->cap > 0 ? list->cap * 2 : 8;
        char **next = (char **)rt_alloc(sizeof(char *) * (size_t)new_cap);
        if (!next) return;
        if (list->items && list->count > 0)
            rt_memcpy(next, list->items, sizeof(char *) * (size_t)list->count);
        rt_free(list->items);
        list->items = next;
        list->cap = new_cap;
    }
    list->items[list->count++] = text;
}

static KnStringArray rt_string_array_from_items(char **items, int count, int skip)
{
    KnStringArray out = {0, 0};
    if (!items || count <= skip) return out;

    uint64_t len = (uint64_t)(count - skip);
    const char **data = (const char **)__kn_gc_alloc(sizeof(const char *) * (size_t)len);
    if (!data) return out;

    for (uint64_t i = 0; i < len; i++)
        data[i] = rt_strdup_gc(items[(int)i + skip] ? items[(int)i + skip] : "");

    out.data = data;
    out.len = len;
    return out;
}

static KnStringArray rt_parse_windows_args(const char *cmd)
{
    RtArgList args = {0};
    size_t i = 0;

    while (cmd && cmd[i])
    {
        while (cmd[i] == ' ' || cmd[i] == '\t')
            i++;
        if (!cmd[i]) break;

        bool quoted = false;
        if (cmd[i] == '"')
        {
            quoted = true;
            i++;
        }

        size_t start = i;
        if (quoted)
        {
            while (cmd[i] && cmd[i] != '"')
                i++;
        }
        else
        {
            while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t')
                i++;
        }

        size_t len = i - start;
        char *text = (char *)rt_alloc(len + 1);
        if (!text) break;
        if (len)
            rt_memcpy(text, cmd + start, len);
        text[len] = 0;
        rt_arglist_push(&args, text);

        if (quoted && cmd[i] == '"')
            i++;
    }

    KnStringArray out = rt_string_array_from_items(args.items, args.count, 1);
    for (int k = 0; k < args.count; k++)
        rt_free(args.items[k]);
    rt_free(args.items);
    return out;
}

void __kn_sys_exit(int code)
{
    ExitProcess((uint32_t)code);
}

const char *__kn_sys_command_line(void)
{
    return GetCommandLineA();
}

const char *__kn_sys_executable_path(void)
{
    KN_DWORD cap = KN_MAX_PATH;
    for (;;)
    {
        char *buf = (char *)rt_alloc((size_t)cap);
        KN_DWORD written = 0;
        if (!buf) return "";
        written = GetModuleFileNameA(0, buf, cap);
        if (written == 0)
        {
            rt_free(buf);
            return "";
        }
        if (written < cap)
        {
            buf[written] = 0;
            {
                const char *out = __kn_path_normalize(buf);
                rt_free(buf);
                return out;
            }
        }
        rt_free(buf);
        if (cap >= 65535u)
            return "";
        cap *= 2u;
    }
}

static void rt_store_string_array(KnStringArray value, const char ***out_data, uint64_t *out_len)
{
    if (out_data) *out_data = value.data;
    if (out_len) *out_len = value.len;
}

void __kn_sys_args(const char ***out_data, uint64_t *out_len)
{
    rt_store_string_array(rt_parse_windows_args(GetCommandLineA()), out_data, out_len);
}

void __kn_sys_args_from_argv(int argc, const char **argv, const char ***out_data, uint64_t *out_len)
{
    rt_store_string_array(rt_string_array_from_items((char **)argv, argc, 1), out_data, out_len);
}

int __kn_sys_file_exists(const char *path)
{
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ, 0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE)
        return 0;
    CloseHandle(h);
    return 1;
}

int __kn_sys_exec(const char *command_line)
{
    if (!command_line || !command_line[0]) return -1;
    uint64_t n = rt_strlen(command_line);
    char *mutable_cmd = (char *)rt_alloc((size_t)(n + 1));
    if (!mutable_cmd) return -1;
    if (n) rt_memcpy(mutable_cmd, command_line, (size_t)n);
    mutable_cmd[n] = 0;

    KN_STARTUPINFOA si = {0};
    KN_PROCESS_INFORMATION pi = {0};
    si.cb = (uint32_t)sizeof(si);

    KN_BOOL ok = CreateProcessA(0, mutable_cmd, 0, 0, 0, 0, 0, 0, &si, &pi);
    rt_free(mutable_cmd);
    if (!ok) return -1;

    WaitForSingleObject(pi.hProcess, KN_INFINITE);
    KN_DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code))
        exit_code = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exit_code;
}

void *__kn_async_process_spawn(const char *command_line)
{
    if (!command_line || !command_line[0])
    {
        rt_set_sys_error("Async spawn failed: empty command line");
        return 0;
    }

    uint64_t n = rt_strlen(command_line);
    char *mutable_cmd = (char *)rt_alloc((size_t)(n + 1));
    if (!mutable_cmd)
    {
        rt_set_sys_error("Async spawn failed: out of memory");
        return 0;
    }
    if (n) rt_memcpy(mutable_cmd, command_line, (size_t)n);
    mutable_cmd[n] = 0;

    KN_STARTUPINFOA si = {0};
    KN_PROCESS_INFORMATION pi = {0};
    si.cb = (uint32_t)sizeof(si);

    KN_BOOL ok = CreateProcessA(0, mutable_cmd, 0, 0, 0, 0, 0, 0, &si, &pi);
    rt_free(mutable_cmd);
    if (!ok)
    {
        rt_set_sys_error("Async spawn failed");
        return 0;
    }

    if (pi.hThread)
        CloseHandle(pi.hThread);
    rt_clear_sys_error();
    return pi.hProcess;
}

int __kn_async_process_is_completed(void *handle)
{
    KN_DWORD exit_code = 259u;
    if (!handle)
    {
        rt_set_sys_error("Async IsCompleted failed: null handle");
        return 0;
    }
    if (!GetExitCodeProcess((KN_HANDLE)handle, &exit_code))
    {
        rt_set_sys_error("Async IsCompleted failed");
        return 0;
    }
    rt_clear_sys_error();
    return exit_code != 259u;
}

int __kn_async_process_wait(void *handle, int64_t timeout_ms)
{
    uint64_t start = 0;
    if (!handle)
    {
        rt_set_sys_error("Async Wait failed: null handle");
        return 0;
    }
    if (timeout_ms > 0)
        start = GetTickCount64();

    for (;;)
    {
        KN_DWORD exit_code = 259u;
        if (!GetExitCodeProcess((KN_HANDLE)handle, &exit_code))
        {
            rt_set_sys_error("Async Wait failed");
            return 0;
        }
        if (exit_code != 259u)
        {
            rt_clear_sys_error();
            return 1;
        }
        if (timeout_ms == 0)
        {
            rt_clear_sys_error();
            return 0;
        }
        if (timeout_ms > 0)
        {
            uint64_t now = GetTickCount64();
            if (now - start >= (uint64_t)timeout_ms)
            {
                rt_clear_sys_error();
                return 0;
            }
        }
        Sleep(1u);
    }
}

int __kn_async_process_exit_code(void *handle)
{
    KN_DWORD exit_code = 259u;
    if (!handle)
    {
        rt_set_sys_error("Async ExitCode failed: null handle");
        return -1;
    }
    if (!GetExitCodeProcess((KN_HANDLE)handle, &exit_code))
    {
        rt_set_sys_error("Async ExitCode failed");
        return -1;
    }
    rt_clear_sys_error();
    return (int)exit_code;
}

int __kn_async_process_close(void *handle)
{
    if (!handle)
    {
        rt_set_sys_error("Async Close failed: null handle");
        return 0;
    }
    if (!CloseHandle((KN_HANDLE)handle))
    {
        rt_set_sys_error("Async Close failed");
        return 0;
    }
    rt_clear_sys_error();
    return 1;
}

void *__kn_sys_load_library(const char *path)
{
    if (!path || !path[0])
    {
        rt_set_sys_error("LoadLibrary failed: empty path");
        return 0;
    }
    KN_HANDLE h = LoadLibraryA(path);
    if (!h)
    {
        rt_set_sys_error("LoadLibrary failed");
        return 0;
    }
    {
        typedef void *(*KnMetaGetModuleFn)(void);
        KnMetaGetModuleFn get_meta = (KnMetaGetModuleFn)GetProcAddress((KN_HANDLE)h, "__kn_meta_get_module");
        if (get_meta)
        {
            const KnMetaExportModule *module = (const KnMetaExportModule *)(*get_meta)();
            rt_meta_register_loaded_module(h, module);
        }
    }
    rt_clear_sys_error();
    return h;
}

void *__kn_sys_get_symbol(void *handle, const char *name)
{
    if (!handle)
    {
        rt_set_sys_error("GetSymbol failed: null library handle");
        return 0;
    }
    if (!name || !name[0])
    {
        rt_set_sys_error("GetSymbol failed: empty symbol");
        return 0;
    }
    void *sym = GetProcAddress((KN_HANDLE)handle, name);
    if (!sym)
    {
        rt_set_sys_error("GetSymbol failed");
        return 0;
    }
    rt_clear_sys_error();
    return sym;
}

int __kn_sys_close_library(void *handle)
{
    if (!handle)
    {
        rt_set_sys_error("CloseLibrary failed: null library handle");
        return 0;
    }
    if (!FreeLibrary((KN_HANDLE)handle))
    {
        rt_set_sys_error("CloseLibrary failed");
        return 0;
    }
    rt_meta_unregister_handle(handle);
    rt_clear_sys_error();
    return 1;
}
#else
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#define KN_PATH_SEP '/'

KN_HANDLE KN_STDCALL GetStdHandle(KN_DWORD nStdHandle)
{
    if (nStdHandle == KN_STDOUT_HANDLE) return (KN_HANDLE)(intptr_t)1;
    if (nStdHandle == KN_STDERR_HANDLE) return (KN_HANDLE)(intptr_t)2;
    if (nStdHandle == (KN_DWORD)-10) return (KN_HANDLE)(intptr_t)0;
    return (KN_HANDLE)(intptr_t)-1;
}

KN_BOOL KN_STDCALL WriteFile(KN_HANDLE hFile, const void *lpBuffer, KN_DWORD nBytesToWrite, KN_DWORD *lpBytesWritten, void *lpOverlapped)
{
    (void)lpOverlapped;
    if (lpBytesWritten) *lpBytesWritten = 0;
    if (!lpBuffer) return 0;
    intptr_t fd = (intptr_t)hFile;
    ssize_t n = write((int)fd, lpBuffer, (size_t)nBytesToWrite);
    if (n < 0) return 0;
    if (lpBytesWritten) *lpBytesWritten = (KN_DWORD)n;
    return 1;
}

KN_BOOL KN_STDCALL ReadFile(KN_HANDLE hFile, void *lpBuffer, KN_DWORD nNumberOfBytesToRead, KN_DWORD *lpNumberOfBytesRead, void *lpOverlapped)
{
    (void)lpOverlapped;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
    if (!lpBuffer) return 0;
    intptr_t fd = (intptr_t)hFile;
    ssize_t n = read((int)fd, lpBuffer, (size_t)nNumberOfBytesToRead);
    if (n < 0) return 0;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (KN_DWORD)n;
    return 1;
}

void KN_STDCALL ExitProcess(uint32_t uExitCode)
{
    _exit((int)uExitCode);
}

static void *rt_alloc(size_t size)
{
    return malloc(size);
}

static void rt_free(void *p)
{
    free(p);
}

static KnStringArray rt_string_array_from_items(char **items, int count, int skip)
{
    KnStringArray out = {0, 0};
    if (!items || count <= skip) return out;

    uint64_t len = (uint64_t)(count - skip);
    const char **data = (const char **)__kn_gc_alloc(sizeof(const char *) * (size_t)len);
    if (!data) return out;

    for (uint64_t i = 0; i < len; i++)
        data[i] = rt_strdup_gc(items[(int)i + skip] ? items[(int)i + skip] : "");

    out.data = data;
    out.len = len;
    return out;
}

static void rt_store_string_array(KnStringArray value, const char ***out_data, uint64_t *out_len)
{
    if (out_data) *out_data = value.data;
    if (out_len) *out_len = value.len;
}

void __kn_sys_args(const char ***out_data, uint64_t *out_len)
{
    KnStringArray out = {0, 0};
    rt_store_string_array(out, out_data, out_len);
}

void __kn_sys_args_from_argv(int argc, const char **argv, const char ***out_data, uint64_t *out_len)
{
    rt_store_string_array(rt_string_array_from_items((char **)argv, argc, 1), out_data, out_len);
}

void __kn_sys_exit(int code)
{
    _exit(code);
}

const char *__kn_sys_command_line(void)
{
    return "";
}

const char *__kn_sys_executable_path(void)
{
#if defined(__APPLE__)
    uint32_t cap = 0;
    char *buf = 0;
    char *resolved = 0;
    if (_NSGetExecutablePath(0, &cap) != -1 || cap == 0)
        return "";
    buf = (char *)rt_alloc((size_t)cap + 1u);
    if (!buf)
        return "";
    if (_NSGetExecutablePath(buf, &cap) != 0)
    {
        rt_free(buf);
        return "";
    }
    resolved = realpath(buf, 0);
    if (resolved)
    {
        const char *out = __kn_path_normalize(resolved);
        free(resolved);
        rt_free(buf);
        return out;
    }
    {
        const char *out = __kn_path_normalize(buf);
        rt_free(buf);
        return out;
    }
#else
    size_t cap = 256;
    for (;;)
    {
        char *buf = (char *)rt_alloc(cap);
        ssize_t written = 0;
        if (!buf) return "";
        written = readlink("/proc/self/exe", buf, cap - 1u);
        if (written < 0)
        {
            rt_free(buf);
            return "";
        }
        if ((size_t)written < cap - 1u)
        {
            buf[written] = 0;
            {
                const char *out = __kn_path_normalize(buf);
                rt_free(buf);
                return out;
            }
        }
        rt_free(buf);
        if (cap >= 65536u)
            return "";
        cap *= 2u;
    }
#endif
}

int __kn_sys_file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

int __kn_sys_exec(const char *command_line)
{
    if (!command_line || !command_line[0]) return -1;
    int rc = system(command_line);
    if (rc < 0) return -1;
#if defined(WIFEXITED) && defined(WEXITSTATUS)
    if (WIFEXITED(rc))
        return WEXITSTATUS(rc);
#endif
    return rc;
}

void *__kn_async_process_spawn(const char *command_line)
{
    pid_t pid = 0;
    char *shell = 0;
    typedef struct
    {
        pid_t pid;
        int done;
        int exit_code;
    } KnAsyncProc;
    KnAsyncProc *proc = 0;

    if (!command_line || !command_line[0])
    {
        rt_set_sys_error("Async spawn failed: empty command line");
        return 0;
    }

    proc = (KnAsyncProc *)rt_alloc(sizeof(KnAsyncProc));
    if (!proc)
    {
        rt_set_sys_error("Async spawn failed: out of memory");
        return 0;
    }
    proc->pid = 0;
    proc->done = 0;
    proc->exit_code = 259;

    shell = "/bin/sh";
    pid = fork();
    if (pid < 0)
    {
        rt_free(proc);
        rt_set_sys_error("Async spawn failed");
        return 0;
    }

    if (pid == 0)
    {
        execl(shell, "sh", "-c", command_line, (char *)0);
        _exit(127);
    }

    proc->pid = pid;
    rt_clear_sys_error();
    return proc;
}

int __kn_async_process_is_completed(void *handle)
{
    typedef struct
    {
        pid_t pid;
        int done;
        int exit_code;
    } KnAsyncProc;
    KnAsyncProc *proc = (KnAsyncProc *)handle;
    int status = 0;
    pid_t rc = 0;
    if (!handle)
    {
        rt_set_sys_error("Async IsCompleted failed: null handle");
        return 0;
    }
    if (proc->done)
    {
        rt_clear_sys_error();
        return 1;
    }

    rc = waitpid(proc->pid, &status, WNOHANG);
    if (rc < 0)
    {
        rt_set_sys_error("Async IsCompleted failed");
        return 0;
    }
    if (rc == 0)
    {
        rt_clear_sys_error();
        return 0;
    }

    proc->done = 1;
    if (WIFEXITED(status))
        proc->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        proc->exit_code = 128 + WTERMSIG(status);
    else
        proc->exit_code = 1;
    rt_clear_sys_error();
    return 1;
}

int __kn_async_process_wait(void *handle, int64_t timeout_ms)
{
    typedef struct
    {
        pid_t pid;
        int done;
        int exit_code;
    } KnAsyncProc;
    KnAsyncProc *proc = (KnAsyncProc *)handle;
    uint64_t start = 0;
    int status = 0;
    if (!handle)
    {
        rt_set_sys_error("Async Wait failed: null handle");
        return 0;
    }
    if (proc->done)
    {
        rt_clear_sys_error();
        return 1;
    }
    if (timeout_ms > 0 || timeout_ms < 0)
        start = __kn_time_tick();

    for (;;)
    {
        pid_t rc = waitpid(proc->pid, &status, WNOHANG);
        if (rc < 0)
        {
            rt_set_sys_error("Async Wait failed");
            return 0;
        }
        if (rc == proc->pid)
        {
            proc->done = 1;
            if (WIFEXITED(status))
                proc->exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                proc->exit_code = 128 + WTERMSIG(status);
            else
                proc->exit_code = 1;
            rt_clear_sys_error();
            return 1;
        }
        if (timeout_ms == 0)
        {
            rt_clear_sys_error();
            return 0;
        }
        if (timeout_ms > 0)
        {
            uint64_t now = __kn_time_tick();
            if (now - start >= (uint64_t)timeout_ms)
            {
                rt_clear_sys_error();
                return 0;
            }
        }
        else if (timeout_ms < 0)
        {
            /* keep waiting indefinitely */
        }
        __kn_time_sleep(1);
    }
}

int __kn_async_process_exit_code(void *handle)
{
    typedef struct
    {
        pid_t pid;
        int done;
        int exit_code;
    } KnAsyncProc;
    KnAsyncProc *proc = (KnAsyncProc *)handle;
    if (!handle)
    {
        rt_set_sys_error("Async ExitCode failed: null handle");
        return -1;
    }
    if (!proc->done)
    {
        if (!__kn_async_process_is_completed(handle))
        {
            rt_clear_sys_error();
            return 259;
        }
    }
    rt_clear_sys_error();
    return proc->exit_code;
}

int __kn_async_process_close(void *handle)
{
    typedef struct
    {
        pid_t pid;
        int done;
        int exit_code;
    } KnAsyncProc;
    KnAsyncProc *proc = (KnAsyncProc *)handle;
    if (!handle)
    {
        rt_set_sys_error("Async Close failed: null handle");
        return 0;
    }
    if (!proc->done && !__kn_async_process_is_completed(handle))
    {
        rt_set_sys_error("Async Close failed: process still running");
        return 0;
    }
    rt_free(proc);
    rt_clear_sys_error();
    return 1;
}

void *__kn_sys_load_library(const char *path)
{
    if (!path || !path[0])
    {
        rt_set_sys_error("LoadLibrary failed: empty path");
        return 0;
    }
    dlerror();
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h)
    {
        const char *msg = dlerror();
        rt_set_sys_error(msg && msg[0] ? msg : "LoadLibrary failed");
        return 0;
    }
    dlerror();
    {
        typedef void *(*KnMetaGetModuleFn)(void);
        KnMetaGetModuleFn get_meta = (KnMetaGetModuleFn)dlsym(h, "__kn_meta_get_module");
        (void)dlerror();
        if (get_meta)
        {
            const KnMetaExportModule *module = (const KnMetaExportModule *)(*get_meta)();
            rt_meta_register_loaded_module(h, module);
        }
    }
    rt_clear_sys_error();
    return h;
}

void *__kn_sys_get_symbol(void *handle, const char *name)
{
    if (!handle)
    {
        rt_set_sys_error("GetSymbol failed: null library handle");
        return 0;
    }
    if (!name || !name[0])
    {
        rt_set_sys_error("GetSymbol failed: empty symbol");
        return 0;
    }
    dlerror();
    void *sym = dlsym(handle, name);
    {
        const char *msg = dlerror();
        if (msg && msg[0])
        {
            rt_set_sys_error(msg);
            return 0;
        }
    }
    rt_clear_sys_error();
    return sym;
}

int __kn_sys_close_library(void *handle)
{
    if (!handle)
    {
        rt_set_sys_error("CloseLibrary failed: null library handle");
        return 0;
    }
    if (dlclose(handle) != 0)
    {
        const char *msg = dlerror();
        rt_set_sys_error(msg && msg[0] ? msg : "CloseLibrary failed");
        return 0;
    }
    rt_meta_unregister_handle(handle);
    rt_clear_sys_error();
    return 1;
}
#endif

static void rt_exc_set_with_trace(void *err, const char *trace, uint64_t trace_len)
{
    g_exc_current = err;
    if (!trace || trace_len == 0)
    {
        g_exc_trace_len = 0;
        g_exc_trace[0] = 0;
        return;
    }
    if (trace_len >= (uint64_t)sizeof(g_exc_trace))
        trace_len = (uint64_t)sizeof(g_exc_trace) - 1;
    rt_memcpy(g_exc_trace, trace, (size_t)trace_len);
    g_exc_trace_len = trace_len;
    g_exc_trace[g_exc_trace_len] = 0;
}

void __kn_async_task_complete(void *task_ptr, uint64_t tag, uint64_t payload)
{
    KnAsyncTask *task = (KnAsyncTask *)task_ptr;
    if (!rt_async_task_begin_finish(task)) return;
    task->faulted = 0;
    task->error = 0;
    task->error_trace_len = 0;
    task->error_trace[0] = 0;
    task->result_tag = tag;
    task->result_payload = payload;
    rt_async_task_publish_finish(task);
}

void __kn_async_task_fail(void *task_ptr, void *error, const char *trace)
{
    KnAsyncTask *task = (KnAsyncTask *)task_ptr;
    uint64_t trace_len = rt_strlen(trace);
    if (!rt_async_task_begin_finish(task)) return;
    task->faulted = 1;
    task->error = error;
    task->result_tag = KN_ANY_TAG_NONE;
    task->result_payload = 0;
    if (trace_len >= (uint64_t)sizeof(task->error_trace))
        trace_len = (uint64_t)sizeof(task->error_trace) - 1;
    task->error_trace_len = trace_len;
    if (trace_len > 0)
        rt_memcpy(task->error_trace, trace, (size_t)trace_len);
    task->error_trace[task->error_trace_len] = 0;
    rt_async_task_publish_finish(task);
}

static void rt_async_task_wait_idle(void)
{
    __kn_time_sleep(1);
}

static void rt_async_task_yield_now(void)
{
#if defined(_WIN32) || defined(_WIN64)
    Sleep(0);
#else
    sched_yield();
#endif
}

#if defined(_WIN32) || defined(_WIN64)
static KN_DWORD KN_STDCALL rt_async_task_thread_main(void *param)
#else
static void *rt_async_task_thread_main(void *param)
#endif
{
    KnAsyncTask *task = (KnAsyncTask *)param;
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE thread_handle = 0;
#endif

    g_exc_current = 0;
    g_exc_last = 0;
    g_exc_trace_len = 0;
    g_exc_trace[0] = 0;
    g_exc_last_trace_len = 0;
    g_exc_last_trace[0] = 0;

    if (task)
    {
        if (task->entry)
            task->entry(task->env, task);
        if (rt_atomic_load_i32(&task->done) != 1)
        {
            if (__kn_exc_has())
                __kn_async_task_fail(task, __kn_exc_get(), __kn_exc_trace());
            else
                __kn_async_task_complete(task, KN_ANY_TAG_NONE, 0);
        }
#if defined(_WIN32) || defined(_WIN64)
        thread_handle = task->thread;
        task->thread = 0;
#endif
        rt_atomic_dec_i32(&g_async_live_tasks);
    }

#if defined(_WIN32) || defined(_WIN64)
    if (thread_handle)
        CloseHandle(thread_handle);
    return 0;
#else
    return 0;
#endif
}

void *__kn_async_task_spawn(KnAsyncTaskEntryFn entry, void *env)
{
    KnAsyncTask *task;
    rt_atomic_inc_i32(&g_async_live_tasks);
    task = (KnAsyncTask *)__kn_gc_alloc(sizeof(KnAsyncTask));
    if (!task)
    {
        rt_atomic_dec_i32(&g_async_live_tasks);
        return 0;
    }

    task->done = 0;
    task->faulted = 0;
    task->joined = 0;
    task->error = 0;
    task->result_tag = KN_ANY_TAG_NONE;
    task->result_payload = 0;
    task->env = env;
    task->entry = entry;
    task->error_trace_len = 0;
    task->error_trace[0] = 0;
#if defined(_WIN32) || defined(_WIN64)
    task->thread = 0;
    task->thread = CreateThread(0, 0, rt_async_task_thread_main, task, 0, 0);
    if (!task->thread)
    {
        rt_atomic_dec_i32(&g_async_live_tasks);
        return 0;
    }
#else
    if (pthread_create(&task->thread, 0, rt_async_task_thread_main, task) != 0)
    {
        rt_atomic_dec_i32(&g_async_live_tasks);
        return 0;
    }
    pthread_detach(task->thread);
#endif
    return task;
}

void __kn_async_task_run(void *task_ptr, uint64_t *out_tag, uint64_t *out_payload)
{
    KnAsyncTask *task = (KnAsyncTask *)task_ptr;
    if (out_tag) *out_tag = KN_ANY_TAG_NONE;
    if (out_payload) *out_payload = 0;
    if (!task) return;

    while (rt_atomic_load_i32(&task->done) != 1)
        rt_async_task_wait_idle();

    if (task->faulted)
    {
        rt_exc_set_with_trace(task->error, task->error_trace, task->error_trace_len);
        return;
    }

    if (out_tag) *out_tag = task->result_tag;
    if (out_payload) *out_payload = task->result_payload;
}

typedef struct
{
    int32_t ms;
} KnAsyncSleepEnv;

static void rt_async_task_sleep_worker(void *env_ptr, void *task_ptr)
{
    KnAsyncSleepEnv *env = (KnAsyncSleepEnv *)env_ptr;
    if (env && env->ms > 0)
        __kn_time_sleep(env->ms);
    __kn_async_task_complete(task_ptr, KN_ANY_TAG_NONE, 0);
}

static void rt_async_task_yield_worker(void *env_ptr, void *task_ptr)
{
    (void)env_ptr;
    rt_async_task_yield_now();
    __kn_async_task_complete(task_ptr, KN_ANY_TAG_NONE, 0);
}

void *__kn_async_task_sleep(int32_t ms)
{
    KnAsyncSleepEnv *env = (KnAsyncSleepEnv *)__kn_gc_alloc(sizeof(KnAsyncSleepEnv));
    if (!env) return 0;
    env->ms = ms;
    return __kn_async_task_spawn(rt_async_task_sleep_worker, env);
}

void *__kn_async_task_yield(void)
{
    return __kn_async_task_spawn(rt_async_task_yield_worker, 0);
}

void __kn_exc_set(void *err)
{
    g_exc_current = err;
    g_exc_trace_len = 0;
    g_exc_trace[0] = 0;
}

void *__kn_exc_get(void)
{
    return g_exc_current;
}

int __kn_exc_has(void)
{
    return g_exc_current ? 1 : 0;
}

void *__kn_exc_last(void)
{
    if (g_exc_current) return g_exc_current;
    return g_exc_last;
}

void __kn_exc_clear(void)
{
    if (g_exc_current)
    {
        g_exc_last = g_exc_current;
        g_exc_last_trace_len = g_exc_trace_len;
        if (g_exc_last_trace_len >= (uint64_t)sizeof(g_exc_last_trace))
            g_exc_last_trace_len = (uint64_t)sizeof(g_exc_last_trace) - 1;
        if (g_exc_last_trace_len > 0)
            rt_memcpy(g_exc_last_trace, g_exc_trace, (size_t)g_exc_last_trace_len);
        g_exc_last_trace[g_exc_last_trace_len] = 0;
    }
    g_exc_current = 0;
    g_exc_trace_len = 0;
    g_exc_trace[0] = 0;
}

void __kn_exc_push(const char *fn)
{
    if (!fn || !fn[0]) return;
    uint64_t fn_len = rt_strlen(fn);
    if (fn_len == 0) return;
    if (g_exc_trace_len + fn_len + 4 >= (uint64_t)sizeof(g_exc_trace))
        return;

    if (g_exc_trace_len > 0)
        g_exc_trace[g_exc_trace_len++] = '\n';
    g_exc_trace[g_exc_trace_len++] = 'a';
    g_exc_trace[g_exc_trace_len++] = 't';
    g_exc_trace[g_exc_trace_len++] = ' ';
    rt_memcpy(g_exc_trace + g_exc_trace_len, fn, (size_t)fn_len);
    g_exc_trace_len += fn_len;
    g_exc_trace[g_exc_trace_len] = 0;
}

const char *__kn_exc_trace(void)
{
    if (g_exc_current) return g_exc_trace;
    return g_exc_last_trace;
}

static void rt_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
}

static void rt_memset(void *dst, int v, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8_t)v;
}

static int rt_memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++)
    {
        if (x[i] != y[i]) return (x[i] < y[i]) ? -1 : 1;
    }
    return 0;
}

static uint64_t rt_strlen(const char *s)
{
    uint64_t n = 0;
    if (!s) return 0;
    while (s[n] != 0) n++;
    return n;
}

static int rt_streq(const char *a, const char *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    while (*a && *b)
    {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static char *rt_strdup_gc(const char *s)
{
    uint64_t n = rt_strlen(s);
    char *out = (char *)__kn_gc_alloc(n + 1);
    if (!out) return 0;
    if (n) rt_memcpy(out, s, (size_t)n);
    out[n] = 0;
    return out;
}

static char *rt_slice_gc(const char *s, uint64_t begin, uint64_t len)
{
    if (!s) return rt_strdup_gc("");
    uint64_t n = rt_strlen(s);
    if (begin > n) begin = n;
    if (begin + len > n) len = n - begin;
    char *out = (char *)__kn_gc_alloc(len + 1);
    if (!out) return 0;
    if (len) rt_memcpy(out, s + begin, (size_t)len);
    out[len] = 0;
    return out;
}

static char *rt_concat_gc(const char *a, const char *b)
{
    uint64_t an = rt_strlen(a);
    uint64_t bn = rt_strlen(b);
    char *out = (char *)__kn_gc_alloc(an + bn + 1);
    if (!out) return 0;
    if (an) rt_memcpy(out, a, (size_t)an);
    if (bn) rt_memcpy(out + an, b, (size_t)bn);
    out[an + bn] = 0;
    return out;
}

static int rt_str_contains(const char *s, const char *sub)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(sub);
    if (pn == 0) return 1;
    if (sn < pn) return 0;
    for (uint64_t i = 0; i + pn <= sn; i++)
    {
        if (rt_memcmp(s + i, sub, (size_t)pn) == 0)
            return 1;
    }
    return 0;
}

static int64_t rt_str_index_of(const char *s, const char *sub)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(sub);
    if (pn == 0) return 0;
    if (sn < pn) return -1;
    for (uint64_t i = 0; i + pn <= sn; i++)
    {
        if (rt_memcmp(s + i, sub, (size_t)pn) == 0)
            return (int64_t)i;
    }
    return -1;
}

static uint64_t rt_hash(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    if (!s) return h;
    while (*s)
    {
        h ^= (uint8_t)(*s);
        h *= 1099511628211ULL;
        s++;
    }
    return h;
}

static KnDictEntry *dict_find_slot(KnDictEntry *entries, uint64_t cap, const char *key, int *found)
{
    uint64_t mask = cap - 1;
    uint64_t idx = rt_hash(key) & mask;
    for (uint64_t n = 0; n < cap; n++)
    {
        KnDictEntry *e = &entries[(idx + n) & mask];
        if (!e->used)
        {
            if (found) *found = 0;
            return e;
        }
        if (rt_streq(e->key, key))
        {
            if (found) *found = 1;
            return e;
        }
    }
    if (found) *found = 0;
    return &entries[0];
}

static void dict_rehash(KnDict *d, uint64_t new_cap)
{
    if (!d || new_cap < 8) new_cap = 8;
    // keep power-of-two table for cheap masking
    if ((new_cap & (new_cap - 1)) != 0)
    {
        uint64_t p = 8;
        while (p < new_cap) p <<= 1;
        new_cap = p;
    }

    KnDictEntry *new_entries = (KnDictEntry *)__kn_gc_alloc(sizeof(KnDictEntry) * (size_t)new_cap);
    if (!new_entries) return;
    rt_memset(new_entries, 0, sizeof(KnDictEntry) * (size_t)new_cap);

    if (d->entries && d->cap)
    {
        for (uint64_t i = 0; i < d->cap; i++)
        {
            KnDictEntry *old = &d->entries[i];
            if (!old->used) continue;
            int found = 0;
            KnDictEntry *slot = dict_find_slot(new_entries, new_cap, old->key, &found);
            slot->key = old->key;
            slot->tag = old->tag;
            slot->payload = old->payload;
            slot->used = 1;
        }
    }

    d->entries = new_entries;
    d->cap = new_cap;
}

void *__kn_dict_new(void)
{
    KnDict *d = (KnDict *)__kn_gc_alloc(sizeof(KnDict));
    if (!d) return 0;
    d->count = 0;
    d->cap = 0;
    d->entries = 0;
    return d;
}

void __kn_dict_set(void *dict_ptr, const char *key, uint64_t tag, uint64_t payload)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (!d || !key) return;

    if (d->cap == 0)
        dict_rehash(d, 8);
    else if ((d->count + 1) * 10 >= d->cap * 7)
        dict_rehash(d, d->cap << 1);

    if (!d->entries || d->cap == 0) return;

    int found = 0;
    KnDictEntry *slot = dict_find_slot(d->entries, d->cap, key, &found);
    if (!found)
    {
        slot->key = key;
        slot->used = 1;
        d->count++;
    }
    slot->tag = tag;
    slot->payload = payload;
}

void __kn_dict_get(void *dict_ptr, const char *key, uint64_t *out_tag, uint64_t *out_payload)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (out_tag) *out_tag = 0;
    if (out_payload) *out_payload = 0;
    if (!d || !key || !d->entries || d->cap == 0) return;
    int found = 0;
    KnDictEntry *slot = dict_find_slot(d->entries, d->cap, key, &found);
    if (!found) return;
    if (out_tag) *out_tag = slot->tag;
    if (out_payload) *out_payload = slot->payload;
}

int __kn_dict_contains(void *dict_ptr, const char *key)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (!d || !key || !d->entries || d->cap == 0) return 0;
    int found = 0;
    (void)dict_find_slot(d->entries, d->cap, key, &found);
    return found ? 1 : 0;
}

uint64_t __kn_dict_count(void *dict_ptr)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (!d) return 0;
    return d->count;
}

int __kn_dict_remove(void *dict_ptr, const char *key)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (!d || !key || !d->entries || d->cap == 0) return 0;
    int found = 0;
    KnDictEntry *slot = dict_find_slot(d->entries, d->cap, key, &found);
    if (!found || !slot) return 0;
    slot->used = 0;
    slot->key = 0;
    slot->tag = 0;
    slot->payload = 0;
    if (d->count > 0) d->count--;
    dict_rehash(d, d->cap);
    return 1;
}

int __kn_dict_is_empty(void *dict_ptr)
{
    KnDict *d = (KnDict *)dict_ptr;
    if (!d) return 1;
    return d->count == 0 ? 1 : 0;
}

void *__kn_list_new(void)
{
    KnList *l = (KnList *)__kn_gc_alloc(sizeof(KnList));
    if (!l) return 0;
    l->count = 0;
    l->cap = 0;
    l->tags = 0;
    l->payloads = 0;
    return l;
}

static void list_reserve(KnList *l, uint64_t want)
{
    if (!l) return;
    if (l->cap >= want) return;
    uint64_t cap = l->cap ? l->cap : 8;
    while (cap < want) cap <<= 1;
    uint64_t *nt = (uint64_t *)__kn_gc_alloc(sizeof(uint64_t) * (size_t)cap);
    uint64_t *np = (uint64_t *)__kn_gc_alloc(sizeof(uint64_t) * (size_t)cap);
    if (!nt || !np) return;
    if (l->count > 0)
    {
        rt_memcpy(nt, l->tags, sizeof(uint64_t) * (size_t)l->count);
        rt_memcpy(np, l->payloads, sizeof(uint64_t) * (size_t)l->count);
    }
    l->tags = nt;
    l->payloads = np;
    l->cap = cap;
}

void __kn_list_add(void *list_ptr, uint64_t tag, uint64_t payload)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return;
    list_reserve(l, l->count + 1);
    if (!l->tags || !l->payloads || l->cap <= l->count) return;
    l->tags[l->count] = tag;
    l->payloads[l->count] = payload;
    l->count++;
}

void __kn_list_insert(void *list_ptr, int64_t index, uint64_t tag, uint64_t payload)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return;
    if (index < 0) index = 0;
    uint64_t idx = (uint64_t)index;
    if (idx > l->count) idx = l->count;
    list_reserve(l, l->count + 1);
    if (!l->tags || !l->payloads || l->cap <= l->count) return;
    for (uint64_t i = l->count; i > idx; i--)
    {
        l->tags[i] = l->tags[i - 1];
        l->payloads[i] = l->payloads[i - 1];
    }
    l->tags[idx] = tag;
    l->payloads[idx] = payload;
    l->count++;
}

void __kn_list_get(void *list_ptr, uint64_t index, uint64_t *out_tag, uint64_t *out_payload)
{
    KnList *l = (KnList *)list_ptr;
    if (out_tag) *out_tag = 0;
    if (out_payload) *out_payload = 0;
    if (!l || index >= l->count) return;
    if (out_tag) *out_tag = l->tags[index];
    if (out_payload) *out_payload = l->payloads[index];
}

void __kn_list_set(void *list_ptr, uint64_t index, uint64_t tag, uint64_t payload)
{
    KnList *l = (KnList *)list_ptr;
    if (!l || index >= l->count) return;
    l->tags[index] = tag;
    l->payloads[index] = payload;
}

uint64_t __kn_list_count(void *list_ptr)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return 0;
    return l->count;
}

void __kn_list_clear(void *list_ptr)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return;
    l->count = 0;
}

void __kn_list_remove_at(void *list_ptr, uint64_t index, uint64_t *out_tag, uint64_t *out_payload)
{
    KnList *l = (KnList *)list_ptr;
    if (out_tag) *out_tag = 0;
    if (out_payload) *out_payload = 0;
    if (!l || index >= l->count) return;
    if (out_tag) *out_tag = l->tags[index];
    if (out_payload) *out_payload = l->payloads[index];
    for (uint64_t i = index + 1; i < l->count; i++)
    {
        l->tags[i - 1] = l->tags[i];
        l->payloads[i - 1] = l->payloads[i];
    }
    l->count--;
}

void __kn_list_pop(void *list_ptr, uint64_t *out_tag, uint64_t *out_payload)
{
    KnList *l = (KnList *)list_ptr;
    if (out_tag) *out_tag = 0;
    if (out_payload) *out_payload = 0;
    if (!l || l->count == 0) return;
    uint64_t idx = l->count - 1;
    if (out_tag) *out_tag = l->tags[idx];
    if (out_payload) *out_payload = l->payloads[idx];
    l->count--;
}

int __kn_list_is_empty(void *list_ptr)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return 1;
    return l->count == 0 ? 1 : 0;
}

int __kn_list_contains(void *list_ptr, uint64_t tag, uint64_t payload)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return 0;
    for (uint64_t i = 0; i < l->count; i++)
    {
        if (l->tags[i] != tag) continue;
        if (tag == 5)
        {
            const char *a = (const char *)(uintptr_t)l->payloads[i];
            const char *b = (const char *)(uintptr_t)payload;
            if (rt_streq(a, b)) return 1;
            continue;
        }
        if (l->payloads[i] == payload) return 1;
    }
    return 0;
}

int64_t __kn_list_index_of(void *list_ptr, uint64_t tag, uint64_t payload)
{
    KnList *l = (KnList *)list_ptr;
    if (!l) return -1;
    for (uint64_t i = 0; i < l->count; i++)
    {
        if (l->tags[i] != tag) continue;
        if (tag == 5)
        {
            const char *a = (const char *)(uintptr_t)l->payloads[i];
            const char *b = (const char *)(uintptr_t)payload;
            if (rt_streq(a, b)) return (int64_t)i;
            continue;
        }
        if (l->payloads[i] == payload) return (int64_t)i;
    }
    return -1;
}

void *__kn_dict_keys(void *dict_ptr)
{
    KnDict *d = (KnDict *)dict_ptr;
    void *list = __kn_list_new();
    if (!list) return 0;
    if (!d || !d->entries || d->cap == 0) return list;
    for (uint64_t i = 0; i < d->cap; i++)
    {
        KnDictEntry *e = &d->entries[i];
        if (!e->used || !e->key) continue;
        __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)e->key);
    }
    return list;
}

void *__kn_dict_values(void *dict_ptr)
{
    KnDict *d = (KnDict *)dict_ptr;
    void *list = __kn_list_new();
    if (!list) return 0;
    if (!d || !d->entries || d->cap == 0) return list;
    for (uint64_t i = 0; i < d->cap; i++)
    {
        KnDictEntry *e = &d->entries[i];
        if (!e->used) continue;
        __kn_list_add(list, e->tag, e->payload);
    }
    return list;
}

static int rt_meta_name_match(const char *want, const char *qname, const char *short_name)
{
    if (!want || !want[0]) return 0;
    if (qname && rt_streq(want, qname)) return 1;
    if (short_name && rt_streq(want, short_name)) return 1;
    return 0;
}

static void rt_meta_dict_set_string(void *dict_ptr, const char *key, const char *value)
{
    __kn_dict_set(dict_ptr, key, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)(value ? value : ""));
}

static void rt_meta_dict_set_i64(void *dict_ptr, const char *key, uint64_t value)
{
    __kn_dict_set(dict_ptr, key, KN_ANY_TAG_I64, value);
}

static void rt_meta_dict_set_ptr(void *dict_ptr, const char *key, void *value)
{
    __kn_dict_set(dict_ptr, key, KN_ANY_TAG_PTR, (uint64_t)(uintptr_t)value);
}

static const char *rt_meta_arg_key(const char *name)
{
    uint64_t n = rt_strlen(name ? name : "");
    char *out = (char *)__kn_gc_alloc(n + 5);
    if (!out) return "";
    out[0] = 'a';
    out[1] = 'r';
    out[2] = 'g';
    out[3] = '.';
    if (n > 0)
        rt_memcpy(out + 4, name, (size_t)n);
    out[n + 4] = 0;
    return out;
}

static void *rt_meta_build_instance_dict(const KnMetaExportModule *module, const KnMetaExportInstance *inst)
{
    void *item = __kn_dict_new();
    if (!item || !inst) return item;

    rt_meta_dict_set_string(item, "owner", inst->owner_name);
    rt_meta_dict_set_i64(item, "owner_kind", inst->owner_kind);
    rt_meta_dict_set_i64(item, "owner_param_count", inst->owner_param_count);
    rt_meta_dict_set_ptr(item, "owner_ptr", inst->owner_ptr);
    rt_meta_dict_set_string(item, "module", module ? module->module_name : "");
    rt_meta_dict_set_string(item, "meta", inst->meta_qname);
    rt_meta_dict_set_string(item, "name", inst->meta_name);
    rt_meta_dict_set_i64(item, "arg_count", inst->arg_count);
    if (inst->args)
    {
        for (uint64_t i = 0; i < inst->arg_count; i++)
        {
            const KnMetaExportArg *arg = &inst->args[i];
            const char *key = rt_meta_arg_key(arg->name);
            __kn_dict_set(item, key, arg->tag, arg->payload);
        }
    }
    return item;
}

static void rt_meta_list_add_string(void *list, const char *text)
{
    if (!list) return;
    __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)(text ? text : ""));
}

static void rt_meta_unregister_handle(void *handle)
{
    KnMetaModuleReg *prev = 0;
    KnMetaModuleReg *cur = g_meta_modules;
    while (cur)
    {
        if (cur->handle == handle)
        {
            KnMetaModuleReg *next = cur->next;
            if (prev)
                prev->next = next;
            else
                g_meta_modules = next;
            rt_free(cur);
            cur = next;
            continue;
        }
        prev = cur;
        cur = cur->next;
    }
}

static void rt_meta_register_module(void *handle, const KnMetaExportModule *module, int replace)
{
    KnMetaModuleReg *cur = 0;
    if (!module) return;

    for (cur = g_meta_modules; cur; cur = cur->next)
    {
        if (cur->handle == handle)
        {
            cur->module = module;
            return;
        }
        if (cur->module == module)
            return;
    }

    if (replace)
        rt_meta_unregister_handle(handle);

    cur = (KnMetaModuleReg *)rt_alloc(sizeof(KnMetaModuleReg));
    if (!cur) return;
    cur->handle = handle;
    cur->module = module;
    cur->next = g_meta_modules;
    g_meta_modules = cur;
}

static void rt_meta_register_main_module(const KnMetaExportModule *module)
{
    rt_meta_register_module(0, module, 1);
}

static void rt_meta_register_loaded_module(void *handle, const KnMetaExportModule *module)
{
    if (!handle) return;
    rt_meta_register_module(handle, module, 0);
}

void __kn_meta_register_main(void *module_ptr)
{
    rt_meta_register_main_module((const KnMetaExportModule *)module_ptr);
}

void *__kn_meta_query(const char *meta_name)
{
    void *list = __kn_list_new();
    if (!list) return 0;
    if (!meta_name || !meta_name[0]) return list;

    for (KnMetaModuleReg *mod = g_meta_modules; mod; mod = mod->next)
    {
        if (!mod->module || !mod->module->instances) continue;
        for (uint64_t i = 0; i < mod->module->instance_count; i++)
        {
            const KnMetaExportInstance *inst = &mod->module->instances[i];
            if (!rt_meta_name_match(meta_name, inst->meta_qname, inst->meta_name))
                continue;
            rt_meta_list_add_string(list, inst->owner_name);
        }
    }
    return list;
}

void *__kn_meta_of(const char *owner_name)
{
    void *list = __kn_list_new();
    if (!list) return 0;
    if (!owner_name || !owner_name[0]) return list;

    for (KnMetaModuleReg *mod = g_meta_modules; mod; mod = mod->next)
    {
        if (!mod->module || !mod->module->instances) continue;
        for (uint64_t i = 0; i < mod->module->instance_count; i++)
        {
            const KnMetaExportInstance *inst = &mod->module->instances[i];
            if (!inst->owner_name || !rt_streq(owner_name, inst->owner_name))
                continue;
            rt_meta_list_add_string(list, inst->meta_qname);
        }
    }
    return list;
}

int __kn_meta_has(const char *owner_name, const char *meta_name)
{
    if (!owner_name || !owner_name[0] || !meta_name || !meta_name[0])
        return 0;

    for (KnMetaModuleReg *mod = g_meta_modules; mod; mod = mod->next)
    {
        if (!mod->module || !mod->module->instances) continue;
        for (uint64_t i = 0; i < mod->module->instance_count; i++)
        {
            const KnMetaExportInstance *inst = &mod->module->instances[i];
            if (!inst->owner_name || !rt_streq(owner_name, inst->owner_name))
                continue;
            if (rt_meta_name_match(meta_name, inst->meta_qname, inst->meta_name))
                return 1;
        }
    }
    return 0;
}

void *__kn_meta_find(const char *owner_name, const char *meta_name)
{
    if (!owner_name || !owner_name[0] || !meta_name || !meta_name[0])
        return 0;

    for (KnMetaModuleReg *mod = g_meta_modules; mod; mod = mod->next)
    {
        if (!mod->module || !mod->module->instances) continue;
        for (uint64_t i = 0; i < mod->module->instance_count; i++)
        {
            const KnMetaExportInstance *inst = &mod->module->instances[i];
            if (!inst->owner_name || !rt_streq(owner_name, inst->owner_name))
                continue;
            if (rt_meta_name_match(meta_name, inst->meta_qname, inst->meta_name))
                return rt_meta_build_instance_dict(mod->module, inst);
        }
    }
    return 0;
}

static KnSetEntry *set_find_slot(KnSetEntry *entries, uint64_t cap, const char *key, int *found)
{
    uint64_t mask = cap - 1;
    uint64_t idx = rt_hash(key) & mask;
    for (uint64_t n = 0; n < cap; n++)
    {
        KnSetEntry *e = &entries[(idx + n) & mask];
        if (!e->used)
        {
            if (found) *found = 0;
            return e;
        }
        if (rt_streq(e->key, key))
        {
            if (found) *found = 1;
            return e;
        }
    }
    if (found) *found = 0;
    return &entries[0];
}

static void set_rehash(KnSet *s, uint64_t new_cap)
{
    if (!s || new_cap < 8) new_cap = 8;
    if ((new_cap & (new_cap - 1)) != 0)
    {
        uint64_t p = 8;
        while (p < new_cap) p <<= 1;
        new_cap = p;
    }
    KnSetEntry *new_entries = (KnSetEntry *)__kn_gc_alloc(sizeof(KnSetEntry) * (size_t)new_cap);
    if (!new_entries) return;
    rt_memset(new_entries, 0, sizeof(KnSetEntry) * (size_t)new_cap);

    if (s->entries && s->cap)
    {
        for (uint64_t i = 0; i < s->cap; i++)
        {
            KnSetEntry *old = &s->entries[i];
            if (!old->used) continue;
            int found = 0;
            KnSetEntry *slot = set_find_slot(new_entries, new_cap, old->key, &found);
            slot->key = old->key;
            slot->used = 1;
        }
    }
    s->entries = new_entries;
    s->cap = new_cap;
}

void *__kn_set_new(void)
{
    KnSet *s = (KnSet *)__kn_gc_alloc(sizeof(KnSet));
    if (!s) return 0;
    s->count = 0;
    s->cap = 0;
    s->entries = 0;
    return s;
}

void __kn_set_add(void *set_ptr, const char *key)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s || !key) return;
    if (s->cap == 0)
        set_rehash(s, 8);
    else if ((s->count + 1) * 10 >= s->cap * 7)
        set_rehash(s, s->cap << 1);
    if (!s->entries || s->cap == 0) return;

    int found = 0;
    KnSetEntry *slot = set_find_slot(s->entries, s->cap, key, &found);
    if (!found)
    {
        slot->key = key;
        slot->used = 1;
        s->count++;
    }
}

int __kn_set_contains(void *set_ptr, const char *key)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s || !key || !s->entries || s->cap == 0) return 0;
    int found = 0;
    (void)set_find_slot(s->entries, s->cap, key, &found);
    return found ? 1 : 0;
}

uint64_t __kn_set_count(void *set_ptr)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s) return 0;
    return s->count;
}

int __kn_set_remove(void *set_ptr, const char *key)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s || !key || !s->entries || s->cap == 0) return 0;
    int found = 0;
    KnSetEntry *slot = set_find_slot(s->entries, s->cap, key, &found);
    if (!found || !slot) return 0;
    slot->used = 0;
    slot->key = 0;
    if (s->count > 0) s->count--;
    set_rehash(s, s->cap);
    return 1;
}

void __kn_set_clear(void *set_ptr)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s) return;
    s->count = 0;
    if (s->entries && s->cap)
        rt_memset(s->entries, 0, sizeof(KnSetEntry) * (size_t)s->cap);
}

int __kn_set_is_empty(void *set_ptr)
{
    KnSet *s = (KnSet *)set_ptr;
    if (!s) return 1;
    return s->count == 0 ? 1 : 0;
}

void *__kn_set_values(void *set_ptr)
{
    KnSet *s = (KnSet *)set_ptr;
    void *list = __kn_list_new();
    if (!list) return 0;
    if (!s || !s->entries || s->cap == 0) return list;
    for (uint64_t i = 0; i < s->cap; i++)
    {
        KnSetEntry *e = &s->entries[i];
        if (!e->used || !e->key) continue;
        __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)e->key);
    }
    return list;
}

void *__kn_string_to_chars(const char *s, uint64_t *out_len)
{
    uint64_t len = rt_strlen(s);
    if (out_len) *out_len = len;
    if (len == 0) return 0;
    char *buf = (char *)__kn_gc_alloc((uint64_t)len);
    if (!buf) return 0;
    rt_memcpy(buf, s, (size_t)len);
    return buf;
}

const uint16_t *__kn_win_utf16(const char *s)
{
    static const uint16_t empty[1] = {0};
    if (!s) return empty;

    KnWideTempSlot *slot = &g_wide_temp_ring[g_wide_temp_index++ % 8u];

#if defined(_WIN32) || defined(_WIN64)
    int need = MultiByteToWideChar(65001u, 0u, s, -1, 0, 0);
    if (need <= 0)
        need = (int)rt_strlen(s) + 1;
#else
    int need = (int)rt_strlen(s) + 1;
#endif

    if (need <= 0) return empty;
    if (slot->cap < (uint64_t)need)
    {
        if (slot->ptr) rt_free(slot->ptr);
        slot->ptr = (uint16_t *)rt_alloc(sizeof(uint16_t) * (size_t)need);
        slot->cap = slot->ptr ? (uint64_t)need : 0;
    }
    if (!slot->ptr) return empty;

#if defined(_WIN32) || defined(_WIN64)
    if (MultiByteToWideChar(65001u, 0u, s, -1, slot->ptr, need) > 0)
        return slot->ptr;
#endif

    {
        uint64_t i = 0;
        for (; s[i]; i++)
            slot->ptr[i] = (uint8_t)s[i];
        slot->ptr[i] = 0;
    }
    return slot->ptr;
}

const char *__kn_win_get_text(void *hWnd)
{
#if defined(_WIN32) || defined(_WIN64)
    if (!hWnd) return rt_strdup_gc("");

    int wide_len = GetWindowTextLengthW((KN_HANDLE)hWnd);
    if (wide_len < 0)
        wide_len = 0;

    uint16_t *wide = (uint16_t *)rt_alloc(sizeof(uint16_t) * (size_t)(wide_len + 1));
    if (!wide)
        return rt_strdup_gc("");

    wide[0] = 0;
    if (GetWindowTextW((KN_HANDLE)hWnd, wide, wide_len + 1) < 0)
        wide[0] = 0;

    int out_len = WideCharToMultiByte(65001u, 0u, wide, -1, 0, 0, 0, 0);
    if (out_len > 0)
    {
        char *out = (char *)__kn_gc_alloc((uint64_t)out_len);
        if (out)
        {
            if (WideCharToMultiByte(65001u, 0u, wide, -1, out, out_len, 0, 0) > 0)
            {
                rt_free(wide);
                return out;
            }
        }
    }

    {
        int i = 0;
        char *fallback = (char *)__kn_gc_alloc((uint64_t)(wide_len + 1));
        if (!fallback)
        {
            rt_free(wide);
            return rt_strdup_gc("");
        }
        while (wide[i])
        {
            fallback[i] = (char)(wide[i] & 0xFF);
            i++;
        }
        fallback[i] = 0;
        rt_free(wide);
        return fallback;
    }
#else
    (void)hWnd;
    return rt_strdup_gc("");
#endif
}

const char *__kn_path_combine(const char *a, const char *b)
{
    if (!a || !a[0]) return rt_strdup_gc(b ? b : "");
    if (!b || !b[0]) return rt_strdup_gc(a);
    uint64_t an = rt_strlen(a);
    uint64_t bn = rt_strlen(b);
    int need_sep = 1;
    if (an > 0)
    {
        char c = a[an - 1];
        if (c == '\\' || c == '/') need_sep = 0;
    }
    char *out = (char *)__kn_gc_alloc(an + bn + (need_sep ? 2 : 1));
    if (!out) return "";
    rt_memcpy(out, a, (size_t)an);
    uint64_t off = an;
    if (need_sep) out[off++] = KN_PATH_SEP;
    rt_memcpy(out + off, b, (size_t)bn);
    off += bn;
    out[off] = 0;
    return out;
}

const char *__kn_path_join3(const char *a, const char *b, const char *c)
{
    const char *ab = __kn_path_combine(a, b);
    return __kn_path_combine(ab, c);
}

const char *__kn_path_join4(const char *a, const char *b, const char *c, const char *d)
{
    const char *abc = __kn_path_join3(a, b, c);
    return __kn_path_combine(abc, d);
}

const char *__kn_path_file_name(const char *p)
{
    if (!p) return "";
    uint64_t n = rt_strlen(p);
    uint64_t start = 0;
    for (uint64_t i = 0; i < n; i++)
    {
        if (p[i] == '\\' || p[i] == '/')
            start = i + 1;
    }
    return rt_slice_gc(p, start, n - start);
}

const char *__kn_path_extension(const char *p)
{
    if (!p) return "";
    uint64_t n = rt_strlen(p);
    int64_t dot = -1;
    for (uint64_t i = 0; i < n; i++)
    {
        if (p[i] == '\\' || p[i] == '/') dot = -1;
        else if (p[i] == '.') dot = (int64_t)i;
    }
    if (dot < 0 || (uint64_t)dot + 1 >= n) return "";
    return rt_slice_gc(p, (uint64_t)dot + 1, n - (uint64_t)dot - 1);
}

const char *__kn_path_directory(const char *p)
{
    if (!p) return "";
    uint64_t n = rt_strlen(p);
    int64_t cut = -1;
    for (uint64_t i = 0; i < n; i++)
        if (p[i] == '\\' || p[i] == '/') cut = (int64_t)i;
    if (cut < 0) return "";
    return rt_slice_gc(p, 0, (uint64_t)cut);
}

const char *__kn_path_normalize(const char *p)
{
    if (!p) return "";
    uint64_t n = rt_strlen(p);
    char *out = (char *)__kn_gc_alloc(n + 2u);
    uint64_t *segment_starts = (uint64_t *)rt_alloc(sizeof(uint64_t) * (size_t)(n + 1u));
    uint64_t i = 0;
    uint64_t w = 0;
    uint64_t segment_count = 0;
    uint64_t protected_segments = 0;
    int absolute = 0;
    int drive_relative = 0;
    int unc = 0;
    const char separator = KN_PATH_SEP;
    if (!out || !segment_starts)
    {
        rt_free(segment_starts);
        return "";
    }

#if defined(_WIN32) || defined(_WIN64)
    if (n >= 2u && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
    {
        out[w++] = p[0];
        out[w++] = ':';
        i = 2;
        if (i < n && rt_is_path_sep(p[i]))
        {
            out[w++] = separator;
            absolute = 1;
            while (i < n && rt_is_path_sep(p[i])) i++;
        }
        else
            drive_relative = 1;
    }
    else if (n >= 2u && rt_is_path_sep(p[0]) && rt_is_path_sep(p[1]))
    {
        out[w++] = separator;
        out[w++] = separator;
        i = 2;
        absolute = 1;
        unc = 1;
        while (i < n && rt_is_path_sep(p[i])) i++;
    }
    else if (n > 0 && rt_is_path_sep(p[0]))
    {
        out[w++] = separator;
        i = 1;
        absolute = 1;
        while (i < n && rt_is_path_sep(p[i])) i++;
    }
#else
    if (n > 0 && rt_is_path_sep(p[0]))
    {
        out[w++] = separator;
        i = 1;
        absolute = 1;
        while (i < n && rt_is_path_sep(p[i])) i++;
    }
#endif

    while (i < n)
    {
        uint64_t begin = 0;
        uint64_t length = 0;
        uint64_t output_start = 0;
        int previous_is_parent = 0;
        while (i < n && rt_is_path_sep(p[i])) i++;
        if (i >= n) break;
        begin = i;
        while (i < n && !rt_is_path_sep(p[i])) i++;
        length = i - begin;
        if (length == 1u && p[begin] == '.')
            continue;
        if (length == 2u && p[begin] == '.' && p[begin + 1u] == '.')
        {
            if (segment_count > protected_segments)
            {
                uint64_t previous_start = segment_starts[segment_count - 1u];
                uint64_t text_start = previous_start;
                if (text_start < w && out[text_start] == separator) text_start++;
                previous_is_parent = w - text_start == 2u &&
                                     out[text_start] == '.' && out[text_start + 1u] == '.';
                if (!previous_is_parent)
                {
                    w = previous_start;
                    segment_count--;
                    continue;
                }
            }
            if (absolute)
                continue;
        }

        output_start = w;
        if (w > 0 && out[w - 1u] != separator && !(drive_relative && w == 2u))
            out[w++] = separator;
        segment_starts[segment_count++] = output_start;
        rt_memcpy(out + w, p + begin, (size_t)length);
        w += length;
        if (unc && protected_segments < 2u)
            protected_segments++;
    }

    if (w == 0 && n > 0)
        out[w++] = '.';
    out[w] = 0;
    rt_free(segment_starts);
    return out;
}

const char *__kn_path_cwd(void)
{
#if defined(_WIN32) || defined(_WIN64)
    KN_DWORD cap = KN_MAX_PATH;
    for (;;)
    {
        char *buf = (char *)rt_alloc((size_t)cap);
        KN_DWORD written = 0;
        if (!buf) return "";
        written = GetCurrentDirectoryA(cap, buf);
        if (written == 0)
        {
            rt_free(buf);
            return "";
        }
        if (written < cap)
        {
            const char *out = __kn_path_normalize(buf);
            rt_free(buf);
            return out;
        }
        rt_free(buf);
        if (written >= 65535u)
            return "";
        cap = written + 1u;
    }
#else
    size_t cap = 256;
    for (;;)
    {
        char *buf = (char *)rt_alloc(cap);
        if (!buf) return "";
        if (getcwd(buf, cap))
        {
            const char *out = __kn_path_normalize(buf);
            rt_free(buf);
            return out;
        }
        rt_free(buf);
        if (cap >= 65536u)
            return "";
        cap <<= 1u;
    }
#endif
}

const char *__kn_path_change_extension(const char *p, const char *ext)
{
    if (!p) return "";
    if (!ext) ext = "";
    uint64_t n = rt_strlen(p);
    int64_t dot = -1;
    for (uint64_t i = 0; i < n; i++)
    {
        if (p[i] == '\\' || p[i] == '/') dot = -1;
        else if (p[i] == '.') dot = (int64_t)i;
    }
    uint64_t base = (dot < 0) ? n : (uint64_t)dot;
    int has_ext = ext[0] != 0;
    int add_dot = has_ext && ext[0] != '.';
    uint64_t en = rt_strlen(ext);
    uint64_t out_len = base + (has_ext ? (uint64_t)add_dot + en : 0);
    char *out = (char *)__kn_gc_alloc(out_len + 1);
    if (!out) return "";
    if (base) rt_memcpy(out, p, (size_t)base);
    uint64_t off = base;
    if (has_ext)
    {
        if (add_dot) out[off++] = '.';
        if (en) rt_memcpy(out + off, ext, (size_t)en);
        off += en;
    }
    out[off] = 0;
    return out;
}

int __kn_path_is_absolute(const char *p)
{
    if (!p || !p[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    if ((p[0] == '\\' && p[1] == '\\') || (p[0] == '/' && p[1] == '/'))
        return 1;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
        p[1] == ':' &&
        (p[2] == '\\' || p[2] == '/'))
        return 1;
    return 0;
#else
    return p[0] == '/' ? 1 : 0;
#endif
}

const char *__kn_path_base_name(const char *p)
{
    const char *name = __kn_path_file_name(p);
    uint64_t n = rt_strlen(name);
    int64_t dot = -1;
    for (uint64_t i = 0; i < n; i++)
    {
        if (name[i] == '.')
            dot = (int64_t)i;
    }
    if (dot <= 0) return rt_strdup_gc(name);
    return rt_slice_gc(name, 0, (uint64_t)dot);
}

const char *__kn_path_ensure_trailing_separator(const char *p)
{
    if (!p || !p[0])
    {
        char *out = (char *)__kn_gc_alloc(2);
        if (!out) return "";
        out[0] = KN_PATH_SEP;
        out[1] = 0;
        return out;
    }

    {
        const char *norm = __kn_path_normalize(p);
        uint64_t n = rt_strlen(norm);
        if (n > 0)
        {
            char last = norm[n - 1];
            if (last == '\\' || last == '/')
                return rt_strdup_gc(norm);
        }

        {
            char *out = (char *)__kn_gc_alloc(n + 2);
            if (!out) return "";
            if (n) rt_memcpy(out, norm, (size_t)n);
            out[n] = KN_PATH_SEP;
            out[n + 1] = 0;
            return out;
        }
    }
}

int __kn_text_contains(const char *s, const char *sub)
{
    return rt_str_contains(s, sub);
}

int __kn_text_starts_with(const char *s, const char *prefix)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(prefix);
    if (pn > sn) return 0;
    return rt_memcmp(s, prefix, (size_t)pn) == 0;
}

int __kn_text_ends_with(const char *s, const char *suffix)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(suffix);
    if (pn > sn) return 0;
    return rt_memcmp(s + (sn - pn), suffix, (size_t)pn) == 0;
}

int64_t __kn_text_index_of(const char *s, const char *sub)
{
    return rt_str_index_of(s, sub);
}

int64_t __kn_text_last_index_of(const char *s, const char *sub)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(sub);
    if (pn == 0) return (int64_t)sn;
    if (sn < pn) return -1;
    uint64_t i = sn - pn + 1;
    while (i > 0)
    {
        i--;
        if (rt_memcmp(s + i, sub, (size_t)pn) == 0)
            return (int64_t)i;
    }
    return -1;
}

const char *__kn_text_trim(const char *s)
{
    if (!s) return "";
    uint64_t n = rt_strlen(s);
    uint64_t b = 0;
    uint64_t e = n;
    while (b < n)
    {
        char c = s[b];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        b++;
    }
    while (e > b)
    {
        char c = s[e - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        e--;
    }
    return rt_slice_gc(s, b, e - b);
}

const char *__kn_text_replace(const char *s, const char *from, const char *to)
{
    if (!s) return "";
    uint64_t sn = rt_strlen(s);
    uint64_t fn = rt_strlen(from);
    uint64_t tn = rt_strlen(to);
    uint64_t matches = 0;
    uint64_t scan = 0;
    uint64_t out_len = 0;
    uint64_t write = 0;
    char *out = 0;
    if (fn == 0) return rt_strdup_gc(s);

    while (scan + fn <= sn)
    {
        if (rt_memcmp(s + scan, from, (size_t)fn) == 0)
        {
            matches++;
            scan += fn;
        }
        else
            scan++;
    }
    if (matches == 0) return rt_strdup_gc(s);
    if (tn >= fn)
    {
        uint64_t growth = tn - fn;
        if (growth != 0 && matches > (UINT64_MAX - sn) / growth) return "";
        out_len = sn + matches * growth;
    }
    else
        out_len = sn - matches * (fn - tn);
    out = (char *)__kn_gc_alloc(out_len + 1u);
    if (!out) return "";
    scan = 0;
    while (scan < sn)
    {
        if (scan + fn <= sn && rt_memcmp(s + scan, from, (size_t)fn) == 0)
        {
            if (tn) rt_memcpy(out + write, to, (size_t)tn);
            write += tn;
            scan += fn;
        }
        else
            out[write++] = s[scan++];
    }
    out[write] = 0;
    return out;
}

const char *__kn_text_to_upper(const char *s)
{
    if (!s) return "";
    uint64_t n = rt_strlen(s);
    char *out = (char *)__kn_gc_alloc(n + 1);
    if (!out) return "";
    for (uint64_t i = 0; i < n; i++)
    {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
        out[i] = c;
    }
    out[n] = 0;
    return out;
}

const char *__kn_text_to_lower(const char *s)
{
    if (!s) return "";
    uint64_t n = rt_strlen(s);
    char *out = (char *)__kn_gc_alloc(n + 1);
    if (!out) return "";
    for (uint64_t i = 0; i < n; i++)
    {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        out[i] = c;
    }
    out[n] = 0;
    return out;
}

const char *__kn_text_slice(const char *s, int64_t start, int64_t len)
{
    if (!s) return "";
    if (start < 0) start = 0;
    if (len < 0) len = 0;
    return rt_slice_gc(s, (uint64_t)start, (uint64_t)len);
}

int64_t __kn_text_count(const char *s, const char *sub)
{
    uint64_t sn = rt_strlen(s);
    uint64_t pn = rt_strlen(sub);
    if (pn == 0 || sn < pn) return 0;
    int64_t count = 0;
    for (uint64_t i = 0; i + pn <= sn; )
    {
        if (rt_memcmp(s + i, sub, (size_t)pn) == 0)
        {
            count++;
            i += pn;
            continue;
        }
        i++;
    }
    return count;
}

void *__kn_text_split(const char *s, const char *sep)
{
    const char *src = s ? s : "";
    const char *needle = sep ? sep : "";
    void *list = __kn_list_new();
    if (!list) return 0;

    uint64_t sn = rt_strlen(src);
    uint64_t pn = rt_strlen(needle);
    if (pn == 0)
    {
        const char *one = rt_strdup_gc(src);
        __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)one);
        return list;
    }

    uint64_t start = 0;
    for (uint64_t i = 0; i + pn <= sn; )
    {
        if (rt_memcmp(src + i, needle, (size_t)pn) == 0)
        {
            const char *part = rt_slice_gc(src, start, i - start);
            __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)part);
            i += pn;
            start = i;
            continue;
        }
        i++;
    }

    const char *tail = rt_slice_gc(src, start, sn - start);
    __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)tail);
    return list;
}

void *__kn_text_lines(const char *s)
{
    const char *src = s ? s : "";
    void *list = __kn_list_new();
    if (!list) return 0;

    uint64_t n = rt_strlen(src);
    uint64_t start = 0;
    for (uint64_t i = 0; i < n; i++)
    {
        if (src[i] != '\n') continue;
        uint64_t len = i - start;
        if (len > 0 && src[start + len - 1] == '\r')
            len--;
        const char *part = rt_slice_gc(src, start, len);
        __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)part);
        start = i + 1;
    }

    if (start <= n)
    {
        uint64_t len = n - start;
        if (len > 0 && src[start + len - 1] == '\r')
            len--;
        const char *part = rt_slice_gc(src, start, len);
        __kn_list_add(list, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)part);
    }

    return list;
}

const char *__kn_text_join(void *list_ptr, const char *sep)
{
    KnList *l = (KnList *)list_ptr;
    if (!l || l->count == 0) return "";
    const char *glue = sep ? sep : "";
    uint64_t glue_len = rt_strlen(glue);
    uint64_t total = 0;

    for (uint64_t i = 0; i < l->count; i++)
    {
        const char *part = "<any>";
        if (l->tags[i] == KN_ANY_TAG_STRING)
        {
            const char *p = (const char *)(uintptr_t)l->payloads[i];
            part = p ? p : "";
        }
        total += rt_strlen(part);
        if (i > 0) total += glue_len;
    }

    char *out = (char *)__kn_gc_alloc(total + 1);
    if (!out) return "";
    uint64_t off = 0;
    for (uint64_t i = 0; i < l->count; i++)
    {
        const char *part = "<any>";
        if (l->tags[i] == KN_ANY_TAG_STRING)
        {
            const char *p = (const char *)(uintptr_t)l->payloads[i];
            part = p ? p : "";
        }
        if (i > 0 && glue_len)
        {
            rt_memcpy(out + off, glue, (size_t)glue_len);
            off += glue_len;
        }
        uint64_t pn = rt_strlen(part);
        if (pn)
        {
            rt_memcpy(out + off, part, (size_t)pn);
            off += pn;
        }
    }
    out[off] = 0;
    return out;
}

static int rt_is_path_sep(char c)
{
    return c == '\\' || c == '/';
}

static uint64_t rt_path_root_len(const char *p)
{
    if (!p || !p[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
        p[1] == ':' &&
        rt_is_path_sep(p[2]))
        return 3;
    if (rt_is_path_sep(p[0]) && rt_is_path_sep(p[1]))
    {
        uint64_t i = 2;
        int parts = 0;
        while (p[i])
        {
            while (rt_is_path_sep(p[i]))
                i++;
            while (p[i] && !rt_is_path_sep(p[i]))
                i++;
            parts++;
            if (parts >= 2)
                break;
        }
        return i;
    }
    if (rt_is_path_sep(p[0]))
        return 1;
    return 0;
#else
    return p[0] == '/' ? 1 : 0;
#endif
}

static char *rt_norm_path_alloc(const char *path)
{
    const char *norm = __kn_path_normalize(path ? path : "");
    uint64_t n = rt_strlen(norm);
    char *buf = (char *)rt_alloc((size_t)n + 1u);
    uint64_t root = 0;
    if (!buf) return 0;
    if (n > 0)
        rt_memcpy(buf, norm, (size_t)n);
    buf[n] = 0;
    root = rt_path_root_len(buf);
    while (n > root && rt_is_path_sep(buf[n - 1]))
        n--;
    buf[n] = 0;
    return buf;
}

int __kn_dir_exists(const char *path)
{
    char *norm = rt_norm_path_alloc(path);
    int ok = 0;
    if (!norm || !norm[0])
    {
        rt_free(norm);
        return 0;
    }
#if defined(_WIN32) || defined(_WIN64)
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(norm, &data);
    if (h != KN_INVALID_HANDLE_VALUE)
    {
        ok = (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY) != 0 ? 1 : 0;
        FindClose(h);
    }
#else
    struct stat st;
    if (stat(norm, &st) == 0 && S_ISDIR(st.st_mode))
        ok = 1;
#endif
    rt_free(norm);
    return ok;
}

int __kn_dir_create(const char *path)
{
    char *norm = rt_norm_path_alloc(path);
    int ok = 0;
    if (!norm || !norm[0])
    {
        rt_free(norm);
        return 0;
    }
#if defined(_WIN32) || defined(_WIN64)
    ok = CreateDirectoryA(norm, 0) ? 1 : 0;
#else
    ok = mkdir(norm, 0755) == 0 ? 1 : 0;
#endif
    rt_free(norm);
    return ok;
}

int __kn_dir_ensure(const char *path)
{
    char *norm = rt_norm_path_alloc(path);
    uint64_t n = 0;
    uint64_t root = 0;
    uint64_t i = 0;

    if (!norm || !norm[0])
    {
        rt_free(norm);
        return 0;
    }
    if (__kn_dir_exists(norm))
    {
        rt_free(norm);
        return 1;
    }

    n = rt_strlen(norm);
    root = rt_path_root_len(norm);
    i = root;
    while (i < n)
    {
        while (i < n && rt_is_path_sep(norm[i]))
            i++;
        while (i < n && !rt_is_path_sep(norm[i]))
            i++;
        if (i == 0)
            break;
        {
            char saved = norm[i];
            norm[i] = 0;
            if (norm[0] && !__kn_dir_exists(norm) && !__kn_dir_create(norm))
            {
                norm[i] = saved;
                rt_free(norm);
                return 0;
            }
            norm[i] = saved;
        }
    }

    i = __kn_dir_exists(norm);
    rt_free(norm);
    return (int)i;
}

int __kn_dir_delete(const char *path)
{
    char *norm = rt_norm_path_alloc(path);
    int ok = 0;
    if (!norm || !norm[0])
    {
        rt_free(norm);
        return 0;
    }
#if defined(_WIN32) || defined(_WIN64)
    ok = RemoveDirectoryA(norm) ? 1 : 0;
#else
    ok = rmdir(norm) == 0 ? 1 : 0;
#endif
    rt_free(norm);
    return ok;
}

int __kn_dir_delete_if_exists(const char *path)
{
    if (!path || !path[0]) return 0;
    if (!__kn_dir_exists(path))
        return 1;
    return __kn_dir_delete(path);
}

typedef struct
{
    char **items;
    uint64_t count;
    uint64_t cap;
} KnNativePathList;

static char *rt_path_join_alloc(const char *dir, const char *name)
{
    uint64_t dn = rt_strlen(dir);
    uint64_t nn = rt_strlen(name);
    int needs_sep = dn > 0 && !rt_is_path_sep(dir[dn - 1]);
    char *out = (char *)rt_alloc((size_t)(dn + nn + (needs_sep ? 2u : 1u)));
    uint64_t offset = 0;
    if (!out) return 0;
    if (dn)
    {
        rt_memcpy(out, dir, (size_t)dn);
        offset = dn;
    }
    if (needs_sep)
        out[offset++] = KN_PATH_SEP;
    if (nn)
    {
        rt_memcpy(out + offset, name, (size_t)nn);
        offset += nn;
    }
    out[offset] = 0;
    return out;
}

static int rt_path_compare(const char *a, const char *b)
{
    uint64_t i = 0;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (a[i] && b[i])
    {
        unsigned char ac = (unsigned char)a[i];
        unsigned char bc = (unsigned char)b[i];
        if (ac != bc) return ac < bc ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return a[i] ? 1 : -1;
}

static void rt_path_list_dispose(KnNativePathList *paths)
{
    uint64_t i = 0;
    if (!paths) return;
    for (i = 0; i < paths->count; i++)
        rt_free(paths->items[i]);
    rt_free(paths->items);
    paths->items = 0;
    paths->count = 0;
    paths->cap = 0;
}

static int rt_path_list_push(KnNativePathList *paths, char *path)
{
    if (!paths || !path) return 0;
    if (paths->count == paths->cap)
    {
        uint64_t next_cap = paths->cap ? paths->cap << 1u : 32u;
        char **next = 0;
        if (next_cap < paths->cap || next_cap > (uint64_t)(SIZE_MAX / sizeof(char *)))
            return 0;
        next = (char **)rt_alloc(sizeof(char *) * (size_t)next_cap);
        if (!next) return 0;
        if (paths->count)
            rt_memcpy(next, paths->items, sizeof(char *) * (size_t)paths->count);
        rt_free(paths->items);
        paths->items = next;
        paths->cap = next_cap;
    }
    paths->items[paths->count++] = path;
    return 1;
}

static void rt_path_list_sift_down(KnNativePathList *paths, uint64_t root, uint64_t count)
{
    for (;;)
    {
        uint64_t child = root * 2u + 1u;
        uint64_t largest = root;
        char *tmp = 0;
        if (child >= count) return;
        if (rt_path_compare(paths->items[largest], paths->items[child]) < 0)
            largest = child;
        if (child + 1u < count && rt_path_compare(paths->items[largest], paths->items[child + 1u]) < 0)
            largest = child + 1u;
        if (largest == root) return;
        tmp = paths->items[root];
        paths->items[root] = paths->items[largest];
        paths->items[largest] = tmp;
        root = largest;
    }
}

static void rt_path_list_sort(KnNativePathList *paths)
{
    uint64_t start = 0;
    uint64_t end = 0;
    if (!paths || paths->count < 2) return;
    start = paths->count / 2u;
    while (start > 0)
    {
        start--;
        rt_path_list_sift_down(paths, start, paths->count);
    }
    end = paths->count;
    while (end > 1)
    {
        char *tmp = paths->items[0];
        paths->items[0] = paths->items[end - 1u];
        paths->items[end - 1u] = tmp;
        end--;
        rt_path_list_sift_down(paths, 0, end);
    }
}

static void rt_collect_files(const char *directory, int recursive, KnNativePathList *paths)
{
#if defined(_WIN32) || defined(_WIN64)
    char *pattern = rt_path_join_alloc(directory, "*");
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE handle = KN_INVALID_HANDLE_VALUE;
    if (!pattern) return;
    handle = FindFirstFileA(pattern, &data);
    rt_free(pattern);
    if (handle == KN_INVALID_HANDLE_VALUE) return;
    do
    {
        const char *name = data.cFileName;
        char *full = 0;
        int is_directory = 0;
        if (rt_streq(name, ".") || rt_streq(name, ".."))
            continue;
        full = rt_path_join_alloc(directory, name);
        if (!full) continue;
        is_directory = (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory)
        {
            if (recursive && (data.dwFileAttributes & KN_FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                rt_collect_files(full, recursive, paths);
            rt_free(full);
        }
        else if (!rt_path_list_push(paths, full))
            rt_free(full);
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
#else
    DIR *dir = opendir(directory);
    struct dirent *entry = 0;
    if (!dir) return;
    while ((entry = readdir(dir)) != 0)
    {
        const char *name = entry->d_name;
        char *full = 0;
        struct stat st;
        if (rt_streq(name, ".") || rt_streq(name, ".."))
            continue;
        full = rt_path_join_alloc(directory, name);
        if (!full) continue;
        if (lstat(full, &st) != 0)
        {
            rt_free(full);
            continue;
        }
        if (S_ISDIR(st.st_mode))
        {
            if (recursive)
                rt_collect_files(full, recursive, paths);
            rt_free(full);
        }
        else if (S_ISREG(st.st_mode))
        {
            if (!rt_path_list_push(paths, full))
                rt_free(full);
        }
        else
            rt_free(full);
    }
    closedir(dir);
#endif
}

void *__kn_dir_files(const char *path, int recursive)
{
    KnNativePathList paths = { 0 };
    char *root = rt_norm_path_alloc(path);
    void *result = __kn_list_new();
    uint64_t i = 0;
    if (!result)
    {
        rt_free(root);
        return 0;
    }
    if (!root || !root[0])
    {
        rt_free(root);
        return result;
    }

    rt_collect_files(root, recursive != 0, &paths);
    rt_path_list_sort(&paths);
    for (i = 0; i < paths.count; i++)
    {
        char *item = rt_strdup_gc(paths.items[i]);
        if (item)
            __kn_list_add(result, KN_ANY_TAG_STRING, (uint64_t)(uintptr_t)item);
    }
    rt_path_list_dispose(&paths);
    rt_free(root);
    return result;
}

int __kn_file_create(const char *path)
{
    if (!path || !path[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_WRITE, KN_FILE_SHARE_READ, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    close(fd);
    return 1;
#endif
}

int __kn_file_touch(const char *path)
{
    if (!path || !path[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_WRITE, KN_FILE_SHARE_READ, 0, KN_OPEN_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE) return 0;
    CloseHandle(h);
    return 1;
#else
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) return 0;
    close(fd);
    return 1;
#endif
}

const char *__kn_file_read_all_text(const char *path)
{
    if (!path || !path[0]) return "";
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ, 0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE) return "";
    int64_t fsz = 0;
    if (!GetFileSizeEx(h, &fsz) || fsz < 0)
    {
        CloseHandle(h);
        return "";
    }
    uint64_t n = (uint64_t)fsz;
    char *buf = (char *)__kn_gc_alloc(n + 1);
    if (!buf)
    {
        CloseHandle(h);
        return "";
    }
    KN_DWORD rd = 0;
    if (n > 0)
    {
        if (!ReadFile(h, buf, (KN_DWORD)n, &rd, 0))
        {
            CloseHandle(h);
            return "";
        }
    }
    buf[(uint64_t)rd] = 0;
    CloseHandle(h);
    return buf;
#else
    int fd = open(path, O_RDONLY);
    if (fd < 0) return "";
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 0)
    {
        close(fd);
        return "";
    }
    uint64_t n = (uint64_t)st.st_size;
    char *buf = (char *)__kn_gc_alloc(n + 1);
    if (!buf)
    {
        close(fd);
        return "";
    }
    uint64_t off = 0;
    while (off < n)
    {
        ssize_t r = read(fd, buf + off, (size_t)(n - off));
        if (r <= 0)
        {
            close(fd);
            return "";
        }
        off += (uint64_t)r;
    }
    close(fd);
    buf[off] = 0;
    return buf;
#endif
}

const char *__kn_file_read_first_line(const char *path)
{
    const char *text = __kn_file_read_all_text(path);
    if (!text || !text[0]) return "";
    {
        uint64_t len = 0;
        while (text[len] && text[len] != '\n')
            len++;
        if (len > 0 && text[len - 1] == '\r')
            len--;
        return rt_slice_gc(text, 0, len);
    }
}

int __kn_file_write_all_text(const char *path, const char *text)
{
    if (!path || !path[0]) return 0;
    if (!text) text = "";
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_WRITE, KN_FILE_SHARE_READ, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE) return 0;
    uint64_t n = rt_strlen(text);
    KN_DWORD wr = 0;
    KN_BOOL ok = 1;
    if (n > 0)
        ok = WriteFile(h, text, (KN_DWORD)n, &wr, 0);
    CloseHandle(h);
    return ok ? 1 : 0;
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    uint64_t n = rt_strlen(text);
    uint64_t off = 0;
    while (off < n)
    {
        ssize_t w = write(fd, text + off, (size_t)(n - off));
        if (w <= 0)
        {
            close(fd);
            return 0;
        }
        off += (uint64_t)w;
    }
    close(fd);
    return 1;
#endif
}

int __kn_file_append_all_text(const char *path, const char *text)
{
    const char *old = __kn_file_read_all_text(path);
    const char *add = text ? text : "";
    const char *merged = rt_concat_gc(old ? old : "", add);
    return __kn_file_write_all_text(path, merged);
}

int __kn_file_append_line(const char *path, const char *text)
{
    const char *line = rt_concat_gc(text ? text : "", "\n");
    return __kn_file_append_all_text(path, line);
}

int __kn_file_delete(const char *path)
{
    if (!path || !path[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    return DeleteFileA(path) ? 1 : 0;
#else
    return unlink(path) == 0 ? 1 : 0;
#endif
}

int __kn_file_delete_if_exists(const char *path)
{
    if (!path || !path[0]) return 0;
    if (!__kn_sys_file_exists(path))
        return 1;
    return __kn_file_delete(path);
}

int64_t __kn_file_size(const char *path)
{
    if (!path || !path[0]) return -1;
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ, 0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (h == KN_INVALID_HANDLE_VALUE) return -1;
    int64_t fsz = -1;
    if (!GetFileSizeEx(h, &fsz))
        fsz = -1;
    CloseHandle(h);
    return fsz;
#else
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
#endif
}

int __kn_file_is_empty(const char *path)
{
    return __kn_file_size(path) == 0 ? 1 : 0;
}

int __kn_file_copy(const char *src, const char *dst)
{
    if (!src || !dst || !src[0] || !dst[0]) return 0;
#if defined(_WIN32) || defined(_WIN64)
    KN_HANDLE in = CreateFileA(src, KN_GENERIC_READ, KN_FILE_SHARE_READ, 0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    KN_HANDLE out = 0;
    if (in == KN_INVALID_HANDLE_VALUE) return 0;
    out = CreateFileA(dst, KN_GENERIC_WRITE, KN_FILE_SHARE_READ, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (out == KN_INVALID_HANDLE_VALUE)
    {
        CloseHandle(in);
        return 0;
    }
    for (;;)
    {
        char buf[8192];
        KN_DWORD rd = 0;
        if (!ReadFile(in, buf, (KN_DWORD)sizeof(buf), &rd, 0))
        {
            CloseHandle(out);
            CloseHandle(in);
            return 0;
        }
        if (rd == 0)
            break;
        {
            KN_DWORD off = 0;
            while (off < rd)
            {
                KN_DWORD wr = 0;
                if (!WriteFile(out, buf + off, rd - off, &wr, 0) || wr == 0)
                {
                    CloseHandle(out);
                    CloseHandle(in);
                    return 0;
                }
                off += wr;
            }
        }
    }
    CloseHandle(out);
    CloseHandle(in);
    return 1;
#else
    int in = open(src, O_RDONLY);
    int out = 0;
    if (in < 0) return 0;
    out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0)
    {
        close(in);
        return 0;
    }
    for (;;)
    {
        char buf[8192];
        ssize_t rd = read(in, buf, sizeof(buf));
        if (rd < 0)
        {
            close(out);
            close(in);
            return 0;
        }
        if (rd == 0)
            break;
        {
            ssize_t off = 0;
            while (off < rd)
            {
                ssize_t wr = write(out, buf + off, (size_t)(rd - off));
                if (wr <= 0)
                {
                    close(out);
                    close(in);
                    return 0;
                }
                off += wr;
            }
        }
    }
    close(out);
    close(in);
    return 1;
#endif
}

int __kn_file_move(const char *src, const char *dst)
{
    if (!src || !dst || !src[0] || !dst[0]) return 0;
#if !defined(_WIN32) && !defined(_WIN64)
    if (rename(src, dst) == 0)
        return 1;
#endif
    if (!__kn_file_copy(src, dst)) return 0;
    return __kn_file_delete(src);
}

void *__kn_file_read_lines(const char *path)
{
    const char *text = __kn_file_read_all_text(path);
    return __kn_text_lines(text);
}

int __kn_file_write_lines(const char *path, void *lines)
{
    const char *text = __kn_text_join(lines, "\n");
    return __kn_file_write_all_text(path, text);
}

int __kn_file_replace_text(const char *path, const char *from, const char *to)
{
    const char *text = 0;
    const char *next = 0;
    if (!path || !path[0]) return 0;
    if (!__kn_sys_file_exists(path))
        return 0;
    text = __kn_file_read_all_text(path);
    next = __kn_text_replace(text, from ? from : "", to ? to : "");
    return __kn_file_write_all_text(path, next);
}

void __kn_gc_init(void)
{
    uintptr_t marker = 0;
    // Generated Kinal frames register typed roots explicitly. The runtime and
    // FFI bridges are C code, so retain their in-flight GC pointers by scanning
    // the used native-stack range from this stable entry marker.
    g_gc_stack_base = (uintptr_t)&marker;
}

KnGcFrame *__kn_gc_push_frame(void)
{
    KnGcFrame *f = (KnGcFrame *)rt_alloc(sizeof(KnGcFrame));
    if (!f) return 0;
    f->prev = g_gc_frame;
    f->count = 0;
    f->cap = 0;
    f->roots = 0;
    g_gc_frame = f;
    return f;
}

void __kn_gc_add_root(KnGcFrame *f, void *addr, uint64_t size)
{
    if (!f || !addr || size == 0) return;
    if (f->count + 1 > f->cap)
    {
        int new_cap = f->cap ? f->cap * 2 : 8;
        KnGcRoot *n = (KnGcRoot *)rt_alloc(sizeof(KnGcRoot) * (size_t)new_cap);
        if (!n) return;
        if (f->roots)
        {
            rt_memcpy(n, f->roots, sizeof(KnGcRoot) * (size_t)f->count);
            rt_free(f->roots);
        }
        f->roots = n;
        f->cap = new_cap;
    }
    f->roots[f->count].addr = addr;
    f->roots[f->count].size = size;
    f->count++;
}

void __kn_gc_add_global_root(void *addr, uint64_t size)
{
    if (!addr || size == 0) return;
    rt_spin_lock(&g_gc_lock);
    if (g_gc_global_root_count + 1 > g_gc_global_root_cap)
    {
        int new_cap = g_gc_global_root_cap ? g_gc_global_root_cap * 2 : 16;
        KnGcRoot *next = (KnGcRoot *)rt_alloc(sizeof(KnGcRoot) * (size_t)new_cap);
        if (!next)
        {
            rt_spin_unlock(&g_gc_lock);
            return;
        }
        if (g_gc_global_roots && g_gc_global_root_count > 0)
        {
            rt_memcpy(next, g_gc_global_roots, sizeof(KnGcRoot) * (size_t)g_gc_global_root_count);
            rt_free(g_gc_global_roots);
        }
        g_gc_global_roots = next;
        g_gc_global_root_cap = new_cap;
    }
    g_gc_global_roots[g_gc_global_root_count].addr = addr;
    g_gc_global_roots[g_gc_global_root_count].size = size;
    g_gc_global_root_count++;
    rt_spin_unlock(&g_gc_lock);
}

void __kn_gc_pop_frame(KnGcFrame *f)
{
    if (!f) return;
    g_gc_frame = f->prev;
    if (f->roots) rt_free(f->roots);
    rt_free(f);
}

void *__kn_gc_alloc(uint64_t size)
{
    uintptr_t marker = 0;
    if (!g_gc_stack_base)
        g_gc_stack_base = (uintptr_t)&marker;
    if (size == 0) size = 1;
    rt_spin_lock(&g_gc_lock);
    if (!g_gc_collecting && rt_atomic_load_i32(&g_async_live_tasks) == 0 && g_gc_bytes > g_gc_threshold)
    {
        rt_spin_unlock(&g_gc_lock);
        __kn_gc_collect();
        rt_spin_lock(&g_gc_lock);
    }
    void *mem = rt_alloc((size_t)size);
    if (!mem)
    {
        rt_spin_unlock(&g_gc_lock);
        return 0;
    }
    rt_memset(mem, 0, (size_t)size);
    KnGcBlock *b = (KnGcBlock *)rt_alloc(sizeof(KnGcBlock));
    if (!b)
    {
        rt_spin_unlock(&g_gc_lock);
        return mem;
    }
    b->ptr = mem;
    b->size = size;
    b->marked = 0;
    b->next = g_gc_blocks;
    g_gc_blocks = b;
    g_gc_bytes += size;
    rt_spin_unlock(&g_gc_lock);
    return mem;
}

static void gc_index_sift_down(KnGcBlock **items, int count, int root)
{
    for (;;)
    {
        int child = root * 2 + 1;
        if (child >= count)
            return;
        if (child + 1 < count &&
            (uintptr_t)items[child]->ptr < (uintptr_t)items[child + 1]->ptr)
            child++;
        if ((uintptr_t)items[root]->ptr >= (uintptr_t)items[child]->ptr)
            return;
        KnGcBlock *swap = items[root];
        items[root] = items[child];
        items[child] = swap;
        root = child;
    }
}

static void gc_index_sort(KnGcBlock **items, int count)
{
    if (!items || count < 2)
        return;
    for (int i = count / 2; i > 0; i--)
        gc_index_sift_down(items, count, i - 1);
    for (int end = count - 1; end > 0; end--)
    {
        KnGcBlock *swap = items[0];
        items[0] = items[end];
        items[end] = swap;
        gc_index_sift_down(items, end, 0);
    }
}

static KnGcBlock *gc_find_indexed_block(void *p, KnGcBlock **index, int count)
{
    if (!p || !index || count <= 0)
        return 0;
    uintptr_t value = (uintptr_t)p;
    int low = 0;
    int high = count;
    // Find the first block whose start is greater than the candidate pointer.
    while (low < high)
    {
        int middle = low + (high - low) / 2;
        if ((uintptr_t)index[middle]->ptr <= value)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == 0)
        return 0;
    KnGcBlock *candidate = index[low - 1];
    uintptr_t start = (uintptr_t)candidate->ptr;
    if (value >= start && value - start < candidate->size)
        return candidate;
    return 0;
}

static void gc_mark_ptr(void *p, KnGcBlock **index, int index_count,
                        KnGcBlock **stack, int *sp, int cap)
{
    KnGcBlock *b = gc_find_indexed_block(p, index, index_count);
    if (!b || b->marked) return;
    b->marked = 1;
    if (*sp < cap)
        stack[(*sp)++] = b;
}

static void gc_scan_region(void *addr, uint64_t size, KnGcBlock **index, int index_count,
                           KnGcBlock **stack, int *sp, int cap)
{
    if (!addr || size == 0) return;
    uintptr_t *cur = (uintptr_t *)addr;
    uintptr_t *end = (uintptr_t *)((uint8_t *)addr + size);
    while (cur < end)
    {
        gc_mark_ptr((void *)(*cur), index, index_count, stack, sp, cap);
        cur++;
    }
}

// Conservative native-stack scanning intentionally crosses compiler-created
// stack redzones. Disable ASan instrumentation for this narrow collector hook;
// heap/object scans remain fully instrumented.
static KN_NO_ADDRESS_SANITIZE void gc_scan_native_stack(uintptr_t base,
                                                        KnGcBlock **index, int index_count,
                                                        KnGcBlock **stack, int *sp, int cap)
{
    uintptr_t marker = 0;
    uintptr_t current = (uintptr_t)&marker;
    uintptr_t low = current < base ? current : base;
    uintptr_t high = current < base ? base : current;
    if (!base || high < low || high - low > 128ULL * 1024ULL * 1024ULL)
        return;
    uintptr_t *cursor = (uintptr_t *)low;
    uintptr_t *end = (uintptr_t *)(high + sizeof(uintptr_t));
    while (cursor < end)
    {
        gc_mark_ptr((void *)(*cursor), index, index_count, stack, sp, cap);
        cursor++;
    }
}

void __kn_gc_collect(void)
{
    if (rt_atomic_load_i32(&g_async_live_tasks) > 0) return;
    rt_spin_lock(&g_gc_lock);
    if (g_gc_collecting || rt_atomic_load_i32(&g_async_live_tasks) > 0)
    {
        rt_spin_unlock(&g_gc_lock);
        return;
    }
    g_gc_collecting = 1;

    int block_count = 0;
    for (KnGcBlock *b = g_gc_blocks; b; b = b->next)
        block_count++;

    if (block_count == 0)
    {
        g_gc_collecting = 0;
        rt_spin_unlock(&g_gc_lock);
        return;
    }

    KnGcBlock **index = (KnGcBlock **)rt_alloc(sizeof(KnGcBlock *) * (size_t)block_count);
    KnGcBlock **stack = (KnGcBlock **)rt_alloc(sizeof(KnGcBlock *) * (size_t)block_count);
    if (!index || !stack)
    {
        if (index) rt_free(index);
        if (stack) rt_free(stack);
        g_gc_collecting = 0;
        rt_spin_unlock(&g_gc_lock);
        return;
    }
    int index_count = 0;
    for (KnGcBlock *b = g_gc_blocks; b; b = b->next)
        index[index_count++] = b;
    gc_index_sort(index, index_count);
    int sp = 0;

    // clear marks
    for (KnGcBlock *b = g_gc_blocks; b; b = b->next)
        b->marked = 0;

    // roots
    gc_scan_native_stack(g_gc_stack_base, index, index_count, stack, &sp, block_count);

    for (KnGcFrame *f = g_gc_frame; f; f = f->prev)
    {
        for (int i = 0; i < f->count; i++)
            gc_scan_region(f->roots[i].addr, f->roots[i].size,
                           index, index_count, stack, &sp, block_count);
    }

    for (int i = 0; i < g_gc_global_root_count; i++)
        gc_scan_region(g_gc_global_roots[i].addr, g_gc_global_roots[i].size,
                       index, index_count, stack, &sp, block_count);

    // Pending/last exception objects live in runtime globals, not stack frames.
    // Keep them alive so uncaught exceptions do not become dangling pointers
    // before the entry point reports them or before a catch block inspects them.
    if (g_exc_current)
        gc_mark_ptr(g_exc_current, index, index_count, stack, &sp, block_count);
    if (g_exc_last)
        gc_mark_ptr(g_exc_last, index, index_count, stack, &sp, block_count);

    // traverse heap graph
    while (sp > 0)
    {
        KnGcBlock *b = stack[--sp];
        gc_scan_region(b->ptr, b->size, index, index_count, stack, &sp, block_count);
    }

    // sweep
    KnGcBlock *prev = 0;
    KnGcBlock *cur = g_gc_blocks;
    g_gc_bytes = 0;
    while (cur)
    {
        KnGcBlock *next = cur->next;
        if (!cur->marked)
        {
            rt_free(cur->ptr);
            rt_free(cur);
            if (prev)
                prev->next = next;
            else
                g_gc_blocks = next;
        }
        else
        {
            g_gc_bytes += cur->size;
            prev = cur;
        }
        cur = next;
    }

    rt_free(index);
    rt_free(stack);

    g_gc_threshold = g_gc_bytes * 2;
    if (g_gc_threshold < 8ULL * 1024ULL * 1024ULL)
        g_gc_threshold = 8ULL * 1024ULL * 1024ULL;
    g_gc_collecting = 0;
    rt_spin_unlock(&g_gc_lock);
}

static int32_t clamp_i32(uint64_t v)
{
    return v > 0x7fffffffULL ? 0x7fffffff : (int32_t)v;
}

uint64_t __kn_time_tick(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
#endif
}

void __kn_time_sleep(int32_t ms)
{
    if (ms <= 0) return;
#if defined(_WIN32) || defined(_WIN64)
    Sleep((KN_DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&req, 0);
#endif
}

#if defined(_WIN32) || defined(_WIN64)
static uint64_t filetime_to_unix_ms(KN_FILETIME ft)
{
    uint64_t raw = ((uint64_t)ft.dwHighDateTime << 32) | (uint64_t)ft.dwLowDateTime;
    if (raw <= 116444736000000000ULL)
        return 0;
    return (raw - 116444736000000000ULL) / 10000ULL;
}

static KN_FILETIME unix_ms_to_filetime(uint64_t ms)
{
    uint64_t raw = ms * 10000ULL + 116444736000000000ULL;
    KN_FILETIME ft;
    ft.dwLowDateTime = (uint32_t)(raw & 0xffffffffULL);
    ft.dwHighDateTime = (uint32_t)(raw >> 32);
    return ft;
}
#endif

void __kn_time_now_parts(int32_t *year, int32_t *month, int32_t *day,
                         int32_t *hour, int32_t *minute, int32_t *second,
                         int32_t *millisecond, int64_t *tick_ms)
{
#if defined(_WIN32) || defined(_WIN64)
    KN_FILETIME ft;
    KN_SYSTEMTIME st;
    GetSystemTimeAsFileTime(&ft);
    FileTimeToSystemTime(&ft, &st);
    uint64_t ms = filetime_to_unix_ms(ft);
    if (year) *year = (int32_t)st.wYear;
    if (month) *month = (int32_t)st.wMonth;
    if (day) *day = (int32_t)st.wDay;
    if (hour) *hour = (int32_t)st.wHour;
    if (minute) *minute = (int32_t)st.wMinute;
    if (second) *second = (int32_t)st.wSecond;
    if (millisecond) *millisecond = (int32_t)st.wMilliseconds;
    if (tick_ms) *tick_ms = (int64_t)ms;
#else
    struct timespec ts;
    struct tm tmv;
    int32_t y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, ms = 0;
    int64_t tick = 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
    {
        time_t sec = ts.tv_sec;
        gmtime_r(&sec, &tmv);
        y = (int32_t)(tmv.tm_year + 1900);
        mo = (int32_t)(tmv.tm_mon + 1);
        d = (int32_t)tmv.tm_mday;
        h = (int32_t)tmv.tm_hour;
        mi = (int32_t)tmv.tm_min;
        s = (int32_t)tmv.tm_sec;
        ms = (int32_t)(ts.tv_nsec / 1000000L);
        tick = (int64_t)((uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ms);
    }
    if (year) *year = y;
    if (month) *month = mo;
    if (day) *day = d;
    if (hour) *hour = h;
    if (minute) *minute = mi;
    if (second) *second = s;
    if (millisecond) *millisecond = ms;
    if (tick_ms) *tick_ms = tick;
#endif
}

static void fmt_append_char(char *buf, size_t cap, size_t *off, char c)
{
    if (*off + 1 >= cap) return;
    buf[(*off)++] = c;
    buf[*off] = 0;
}

static void fmt_append_uint(char *buf, size_t cap, size_t *off, int32_t v, int width)
{
    char tmp[16];
    int n = 0;
    uint32_t x = v < 0 ? (uint32_t)(-v) : (uint32_t)v;
    if (x == 0)
        tmp[n++] = '0';
    else
    {
        while (x && n < (int)sizeof(tmp))
        {
            tmp[n++] = (char)('0' + (x % 10U));
            x /= 10U;
        }
    }

    while (n < width && n < (int)sizeof(tmp))
        tmp[n++] = '0';

    if (v < 0)
        fmt_append_char(buf, cap, off, '-');
    for (int i = n - 1; i >= 0; i--)
        fmt_append_char(buf, cap, off, tmp[i]);
}

static void tick_to_parts(uint64_t tick_ms, int32_t *year, int32_t *month, int32_t *day,
                          int32_t *hour, int32_t *minute, int32_t *second, int32_t *millisecond)
{
#if defined(_WIN32) || defined(_WIN64)
    KN_FILETIME ft = unix_ms_to_filetime(tick_ms);
    KN_SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    if (year) *year = (int32_t)st.wYear;
    if (month) *month = (int32_t)st.wMonth;
    if (day) *day = (int32_t)st.wDay;
    if (hour) *hour = (int32_t)st.wHour;
    if (minute) *minute = (int32_t)st.wMinute;
    if (second) *second = (int32_t)st.wSecond;
    if (millisecond) *millisecond = (int32_t)st.wMilliseconds;
#else
    time_t sec = (time_t)(tick_ms / 1000ULL);
    struct tm tmv;
    gmtime_r(&sec, &tmv);
    if (year) *year = (int32_t)(tmv.tm_year + 1900);
    if (month) *month = (int32_t)(tmv.tm_mon + 1);
    if (day) *day = (int32_t)tmv.tm_mday;
    if (hour) *hour = (int32_t)tmv.tm_hour;
    if (minute) *minute = (int32_t)tmv.tm_min;
    if (second) *second = (int32_t)tmv.tm_sec;
    if (millisecond) *millisecond = clamp_i32(tick_ms % 1000ULL);
#endif
}

const char *__kn_time_format(uint64_t tick_ms, const char *pattern)
{
    int32_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, millis = 0;
    tick_to_parts(tick_ms, &year, &month, &day, &hour, &minute, &second, &millis);

    const char *fmt = pattern && pattern[0] ? pattern : "yyyy-MM-dd HH:mm:ss";
    char *out = (char *)__kn_gc_alloc(128);
    if (!out) return "";
    out[0] = 0;

    size_t off = 0;
    for (size_t i = 0; fmt[i] != 0; )
    {
        if (fmt[i] == 'y' && fmt[i + 1] == 'y' && fmt[i + 2] == 'y' && fmt[i + 3] == 'y')
        {
            fmt_append_uint(out, 128, &off, year, 4);
            i += 4;
            continue;
        }
        if (fmt[i] == 'M' && fmt[i + 1] == 'M')
        {
            fmt_append_uint(out, 128, &off, month, 2);
            i += 2;
            continue;
        }
        if (fmt[i] == 'd' && fmt[i + 1] == 'd')
        {
            fmt_append_uint(out, 128, &off, day, 2);
            i += 2;
            continue;
        }
        if (fmt[i] == 'H' && fmt[i + 1] == 'H')
        {
            fmt_append_uint(out, 128, &off, hour, 2);
            i += 2;
            continue;
        }
        if (fmt[i] == 'm' && fmt[i + 1] == 'm')
        {
            fmt_append_uint(out, 128, &off, minute, 2);
            i += 2;
            continue;
        }
        if (fmt[i] == 's' && fmt[i + 1] == 's')
        {
            fmt_append_uint(out, 128, &off, second, 2);
            i += 2;
            continue;
        }
        if (fmt[i] == 'f' && fmt[i + 1] == 'f' && fmt[i + 2] == 'f')
        {
            fmt_append_uint(out, 128, &off, millis, 3);
            i += 3;
            continue;
        }
        fmt_append_char(out, 128, &off, fmt[i]);
        i++;
    }
    return out;
}

uint64_t __kn_time_nano(void)
{
#if defined(_WIN32) || defined(_WIN64)
    int64_t freq = 0;
    int64_t counter = 0;
    if (!QueryPerformanceFrequency(&freq) || freq <= 0)
        return 0;
    if (!QueryPerformanceCounter(&counter))
        return 0;
    long double nanos = ((long double)counter * 1000000000.0L) / (long double)freq;
    if (nanos < 0.0L) nanos = 0.0L;
    return (uint64_t)nanos;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}
