#include "kn/platform.h"

#if !defined(_WIN32) && !defined(_WIN64)

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

typedef enum
{
    KN_HOST_HANDLE_FD = 1,
    KN_HOST_HANDLE_PROCESS = 2,
    KN_HOST_HANDLE_THREAD = 3,
    KN_HOST_HANDLE_FIND = 4
} KnHostHandleKind;

typedef struct
{
    int kind;
    int owned;
    union
    {
        int fd;
        struct
        {
            pid_t pid;
            int waited;
            int exit_code;
        } process;
        struct
        {
            glob_t matches;
            size_t index;
        } find;
    } u;
} KnHostHandle;

static KnHostHandle g_stdin_handle = { KN_HOST_HANDLE_FD, 0, { .fd = 0 } };
static KnHostHandle g_stdout_handle = { KN_HOST_HANDLE_FD, 0, { .fd = 1 } };
static KnHostHandle g_stderr_handle = { KN_HOST_HANDLE_FD, 0, { .fd = 2 } };
static int g_heap_token = 0;
static char g_cmdline_cache[8192];
static int g_cmdline_ready = 0;

static KnHostHandle *kn_host_new_handle(int kind)
{
    KnHostHandle *h = (KnHostHandle *)malloc(sizeof(KnHostHandle));
    if (!h) return 0;
    memset(h, 0, sizeof(*h));
    h->kind = kind;
    h->owned = 1;
    return h;
}

static KnHostHandle *kn_host_as_handle(KN_HANDLE h)
{
    return (KnHostHandle *)h;
}

static const char *kn_host_basename(const char *path)
{
    const char *base = path;
    const char *p = path;
    while (p && *p)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
        p++;
    }
    return base;
}

static void kn_host_fill_find_data(const char *path, KN_WIN32_FIND_DATAA *data)
{
    struct stat st;
    size_t n = 0;
    const char *base = kn_host_basename(path);
    if (!data) return;
    memset(data, 0, sizeof(*data));
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        data->dwFileAttributes = KN_FILE_ATTRIBUTE_DIRECTORY;
    n = strlen(base);
    if (n >= sizeof(data->cFileName))
        n = sizeof(data->cFileName) - 1;
    memcpy(data->cFileName, base, n);
    data->cFileName[n] = 0;
}

static int kn_host_is_absolute(const char *path)
{
    return path && path[0] == '/';
}

static void kn_host_norm_copy(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = 0;
        return;
    }
    while (src[i] && i + 1 < cap)
    {
        dst[i] = (src[i] == '\\') ? '/' : src[i];
        i++;
    }
    dst[i] = 0;
}

static int kn_host_norm_path(const char *src, char *dst, size_t cap)
{
    if (!src || !dst || cap == 0)
        return 0;
    kn_host_norm_copy(dst, cap, src);
    return dst[0] != 0;
}

static int kn_host_make_abs(const char *path, char *out, size_t cap)
{
    char cwd[PATH_MAX];
    size_t n = 0;
    if (!path || !out || cap == 0)
        return 0;
    if (kn_host_is_absolute(path))
    {
        kn_host_norm_copy(out, cap, path);
        return 1;
    }
    if (!getcwd(cwd, sizeof(cwd)))
        return 0;
    n = strlen(cwd);
    if (n + 1 >= cap)
        return 0;
    memcpy(out, cwd, n);
    if (n > 0 && out[n - 1] != '/')
        out[n++] = '/';
    out[n] = 0;
    kn_host_norm_copy(out + n, cap - n, path);
    return 1;
}

static void kn_host_normalize_abs_path(char *path)
{
    char parts[PATH_MAX];
    size_t len = 0;
    size_t write_off = 0;
    const char *src = path;
    if (!path || !path[0])
        return;

    len = strlen(path);
    if (len >= sizeof(parts))
        len = sizeof(parts) - 1;
    memcpy(parts, path, len);
    parts[len] = 0;

    if (parts[0] != '/')
        return;

    write_off = 1;
    path[0] = '/';
    path[1] = 0;

    {
        char *save = 0;
        char *seg = strtok_r(parts + 1, "/", &save);
        size_t stack[PATH_MAX / 2];
        size_t depth = 0;

        while (seg)
        {
            if (seg[0] == 0 || strcmp(seg, ".") == 0)
            {
                seg = strtok_r(0, "/", &save);
                continue;
            }
            if (strcmp(seg, "..") == 0)
            {
                if (depth > 0)
                {
                    write_off = stack[--depth];
                    path[write_off] = 0;
                }
                seg = strtok_r(0, "/", &save);
                continue;
            }
            if (write_off > 1 && write_off + 1 < PATH_MAX)
                path[write_off++] = '/';
            stack[depth++] = write_off;
            while (*seg && write_off + 1 < PATH_MAX)
                path[write_off++] = *seg++;
            path[write_off] = 0;
            seg = strtok_r(0, "/", &save);
        }
    }

    if (write_off == 0)
    {
        path[0] = '/';
        path[1] = 0;
    }
}

static void kn_host_cache_cmdline(void)
{
    int fd = -1;
    ssize_t n = 0;
    if (g_cmdline_ready)
        return;
    g_cmdline_ready = 1;
    g_cmdline_cache[0] = 0;
    fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0)
        return;
    n = read(fd, g_cmdline_cache, (sizeof(g_cmdline_cache) - 1));
    close(fd);
    if (n <= 0)
    {
        g_cmdline_cache[0] = 0;
        return;
    }
    for (ssize_t i = 0; i < n - 1; i++)
    {
        if (g_cmdline_cache[i] == 0)
            g_cmdline_cache[i] = ' ';
    }
    g_cmdline_cache[n] = 0;
}

KN_HANDLE KN_STDCALL GetStdHandle(KN_DWORD nStdHandle)
{
    if (nStdHandle == KN_STDOUT_HANDLE) return &g_stdout_handle;
    if (nStdHandle == KN_STDERR_HANDLE) return &g_stderr_handle;
    if (nStdHandle == (KN_DWORD)-10) return &g_stdin_handle;
    return KN_INVALID_HANDLE_VALUE;
}

KN_BOOL KN_STDCALL GetConsoleMode(KN_HANDLE hConsoleHandle, KN_DWORD *lpMode)
{
    KnHostHandle *h = kn_host_as_handle(hConsoleHandle);
    if (!h || h->kind != KN_HOST_HANDLE_FD)
        return 0;
    if (lpMode) *lpMode = isatty(h->u.fd) ? 1u : 0u;
    return 1;
}

KN_BOOL KN_STDCALL SetConsoleMode(KN_HANDLE hConsoleHandle, KN_DWORD dwMode)
{
    (void)hConsoleHandle;
    (void)dwMode;
    return 1;
}

KN_BOOL KN_STDCALL SetConsoleOutputCP(KN_DWORD wCodePageID)
{
    (void)wCodePageID;
    return 1;
}

KN_BOOL KN_STDCALL SetConsoleCP(KN_DWORD wCodePageID)
{
    (void)wCodePageID;
    return 1;
}

KN_BOOL KN_STDCALL WriteFile(KN_HANDLE hFile, const void *lpBuffer, KN_DWORD nBytesToWrite, KN_DWORD *lpBytesWritten, void *lpOverlapped)
{
    KnHostHandle *h = kn_host_as_handle(hFile);
    ssize_t n = 0;
    (void)lpOverlapped;
    if (lpBytesWritten) *lpBytesWritten = 0;
    if (!h || h->kind != KN_HOST_HANDLE_FD || !lpBuffer)
        return 0;
    n = write(h->u.fd, lpBuffer, (size_t)nBytesToWrite);
    if (n < 0)
        return 0;
    if (lpBytesWritten) *lpBytesWritten = (KN_DWORD)n;
    return 1;
}

KN_BOOL KN_STDCALL ReadFile(KN_HANDLE hFile, void *lpBuffer, KN_DWORD nNumberOfBytesToRead, KN_DWORD *lpNumberOfBytesRead, void *lpOverlapped)
{
    KnHostHandle *h = kn_host_as_handle(hFile);
    ssize_t n = 0;
    (void)lpOverlapped;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
    if (!h || h->kind != KN_HOST_HANDLE_FD || !lpBuffer)
        return 0;
    n = read(h->u.fd, lpBuffer, (size_t)nNumberOfBytesToRead);
    if (n < 0)
        return 0;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (KN_DWORD)n;
    return 1;
}

void KN_STDCALL ExitProcess(uint32_t uExitCode)
{
    _exit((int)uExitCode);
}

KN_HANDLE KN_STDCALL GetProcessHeap(void)
{
    return &g_heap_token;
}

void *KN_STDCALL HeapAlloc(KN_HANDLE hHeap, KN_DWORD dwFlags, size_t dwBytes)
{
    (void)hHeap;
    (void)dwFlags;
    return malloc(dwBytes ? dwBytes : 1);
}

KN_BOOL KN_STDCALL HeapFree(KN_HANDLE hHeap, KN_DWORD dwFlags, void *lpMem)
{
    (void)hHeap;
    (void)dwFlags;
    free(lpMem);
    return 1;
}

KN_HANDLE KN_STDCALL CreateFileA(const char *lpFileName, KN_DWORD dwDesiredAccess, KN_DWORD dwShareMode, KN_SECURITY_ATTRIBUTES *lpSecurityAttributes, KN_DWORD dwCreationDisposition, KN_DWORD dwFlagsAndAttributes, KN_HANDLE hTemplateFile)
{
    KnHostHandle *h = 0;
    int flags = 0;
    int fd = -1;
    char path[PATH_MAX];
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;
    if (!lpFileName || !lpFileName[0])
        return KN_INVALID_HANDLE_VALUE;
    if (!kn_host_norm_path(lpFileName, path, sizeof(path)))
        return KN_INVALID_HANDLE_VALUE;
    if ((dwDesiredAccess & KN_GENERIC_WRITE) && (dwDesiredAccess & KN_GENERIC_READ))
        flags = O_RDWR;
    else if (dwDesiredAccess & KN_GENERIC_WRITE)
        flags = O_WRONLY;
    else
        flags = O_RDONLY;

    if (dwCreationDisposition == KN_CREATE_ALWAYS)
        flags |= O_CREAT | O_TRUNC;
    else if (dwCreationDisposition == KN_OPEN_ALWAYS)
        flags |= O_CREAT;

    fd = open(path, flags, 0644);
    if (fd < 0)
        return KN_INVALID_HANDLE_VALUE;
    h = kn_host_new_handle(KN_HOST_HANDLE_FD);
    if (!h)
    {
        close(fd);
        return KN_INVALID_HANDLE_VALUE;
    }
    h->u.fd = fd;
    return h;
}

KN_BOOL KN_STDCALL CloseHandle(KN_HANDLE hObject)
{
    KnHostHandle *h = kn_host_as_handle(hObject);
    if (!h || h == &g_stdin_handle || h == &g_stdout_handle || h == &g_stderr_handle)
        return 1;
    if (h->kind == KN_HOST_HANDLE_FD)
    {
        if (h->owned)
            close(h->u.fd);
        free(h);
        return 1;
    }
    if (h->kind == KN_HOST_HANDLE_PROCESS || h->kind == KN_HOST_HANDLE_THREAD)
    {
        free(h);
        return 1;
    }
    return 0;
}

KN_BOOL KN_STDCALL GetFileSizeEx(KN_HANDLE hFile, int64_t *lpFileSize)
{
    KnHostHandle *h = kn_host_as_handle(hFile);
    struct stat st;
    if (!h || h->kind != KN_HOST_HANDLE_FD || !lpFileSize)
        return 0;
    if (fstat(h->u.fd, &st) != 0)
        return 0;
    *lpFileSize = (int64_t)st.st_size;
    return 1;
}

const char *KN_STDCALL GetCommandLineA(void)
{
    kn_host_cache_cmdline();
    return g_cmdline_cache;
}

KN_BOOL KN_STDCALL CreateProcessA(const char *lpApplicationName, char *lpCommandLine, KN_SECURITY_ATTRIBUTES *lpProcessAttributes, KN_SECURITY_ATTRIBUTES *lpThreadAttributes, KN_BOOL bInheritHandles, KN_DWORD dwCreationFlags, void *lpEnvironment, const char *lpCurrentDirectory, KN_STARTUPINFOA *lpStartupInfo, KN_PROCESS_INFORMATION *lpProcessInformation)
{
    pid_t pid = 0;
    KnHostHandle *proc = 0;
    KnHostHandle *thr = 0;
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    if (!lpProcessInformation)
        return 0;
    pid = fork();
    if (pid < 0)
        return 0;
    if (pid == 0)
    {
        /* Redirect stdin/stdout/stderr if requested. */
        if (lpStartupInfo && (lpStartupInfo->dwFlags & KN_STARTF_USESTDHANDLES))
        {
            KnHostHandle *h_in  = kn_host_as_handle(lpStartupInfo->hStdInput);
            KnHostHandle *h_out = kn_host_as_handle(lpStartupInfo->hStdOutput);
            KnHostHandle *h_err = kn_host_as_handle(lpStartupInfo->hStdError);
            if (h_in  && h_in->kind  == KN_HOST_HANDLE_FD && h_in->u.fd  != 0) dup2(h_in->u.fd,  0);
            if (h_out && h_out->kind == KN_HOST_HANDLE_FD && h_out->u.fd != 1) dup2(h_out->u.fd, 1);
            if (h_err && h_err->kind == KN_HOST_HANDLE_FD && h_err->u.fd != 2) dup2(h_err->u.fd, 2);
        }
        if (lpCurrentDirectory && lpCurrentDirectory[0])
            chdir(lpCurrentDirectory);
        if (lpApplicationName && lpApplicationName[0] && (!lpCommandLine || !lpCommandLine[0]))
        {
            execlp(lpApplicationName, lpApplicationName, (char *)0);
        }
        else if (lpCommandLine && lpCommandLine[0])
        {
            execl("/bin/sh", "sh", "-c", lpCommandLine, (char *)0);
        }
        else
        {
            _exit(127);
        }
        _exit(127);
    }
    proc = kn_host_new_handle(KN_HOST_HANDLE_PROCESS);
    thr = kn_host_new_handle(KN_HOST_HANDLE_THREAD);
    if (!proc || !thr)
    {
        if (proc) free(proc);
        if (thr) free(thr);
        kill(pid, SIGKILL);
        waitpid(pid, 0, 0);
        return 0;
    }
    proc->u.process.pid = pid;
    proc->u.process.waited = 0;
    proc->u.process.exit_code = 1;
    lpProcessInformation->hProcess = proc;
    lpProcessInformation->hThread = thr;
    lpProcessInformation->dwProcessId = (uint32_t)pid;
    lpProcessInformation->dwThreadId = 0;
    return 1;
}

KN_DWORD KN_STDCALL WaitForSingleObject(KN_HANDLE hHandle, KN_DWORD dwMilliseconds)
{
    KnHostHandle *h = kn_host_as_handle(hHandle);
    int status = 0;
    (void)dwMilliseconds;
    if (!h || h->kind != KN_HOST_HANDLE_PROCESS)
        return 1;
    if (!h->u.process.waited)
    {
        if (waitpid(h->u.process.pid, &status, 0) < 0)
            return 1;
        h->u.process.waited = 1;
        if (WIFEXITED(status))
            h->u.process.exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            h->u.process.exit_code = 128 + WTERMSIG(status);
        else
            h->u.process.exit_code = 1;
    }
    return 0;
}

KN_BOOL KN_STDCALL GetExitCodeProcess(KN_HANDLE hProcess, KN_DWORD *lpExitCode)
{
    KnHostHandle *h = kn_host_as_handle(hProcess);
    if (!h || h->kind != KN_HOST_HANDLE_PROCESS || !lpExitCode)
        return 0;
    if (!h->u.process.waited)
    {
        int status = 0;
        pid_t rc = waitpid(h->u.process.pid, &status, WNOHANG);
        if (rc == 0)
        {
            *lpExitCode = 259u;
            return 1;
        }
        if (rc < 0)
            return 0;
        h->u.process.waited = 1;
        if (WIFEXITED(status))
            h->u.process.exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            h->u.process.exit_code = 128 + WTERMSIG(status);
        else
            h->u.process.exit_code = 1;
    }
    *lpExitCode = (KN_DWORD)h->u.process.exit_code;
    return 1;
}

void KN_STDCALL Sleep(KN_DWORD dwMilliseconds)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(dwMilliseconds / 1000u);
    ts.tv_nsec = (long)((dwMilliseconds % 1000u) * 1000000u);
    nanosleep(&ts, 0);
}

uint64_t KN_STDCALL GetTickCount64(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

KN_HANDLE KN_STDCALL LoadLibraryA(const char *lpLibFileName)
{
    char path[PATH_MAX];
    if (!lpLibFileName || !lpLibFileName[0])
        return 0;
    if (!kn_host_norm_path(lpLibFileName, path, sizeof(path)))
        return 0;
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *KN_STDCALL GetProcAddress(KN_HANDLE hModule, const char *lpProcName)
{
    if (!hModule || !lpProcName || !lpProcName[0])
        return 0;
    return dlsym(hModule, lpProcName);
}

KN_BOOL KN_STDCALL FreeLibrary(KN_HANDLE hModule)
{
    if (!hModule)
        return 0;
    return dlclose(hModule) == 0 ? 1 : 0;
}

KN_BOOL KN_STDCALL QueryPerformanceCounter(int64_t *lpPerformanceCount)
{
    struct timespec ts;
    if (!lpPerformanceCount)
        return 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    *lpPerformanceCount = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
    return 1;
}

KN_BOOL KN_STDCALL QueryPerformanceFrequency(int64_t *lpFrequency)
{
    if (!lpFrequency)
        return 0;
    *lpFrequency = 1000000000LL;
    return 1;
}

void KN_STDCALL GetSystemTimeAsFileTime(KN_FILETIME *lpSystemTimeAsFileTime)
{
    struct timespec ts;
    uint64_t unix_100ns = 0;
    uint64_t win_100ns = 0;
    if (!lpSystemTimeAsFileTime)
        return;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        lpSystemTimeAsFileTime->dwLowDateTime = 0;
        lpSystemTimeAsFileTime->dwHighDateTime = 0;
        return;
    }
    unix_100ns = (uint64_t)ts.tv_sec * 10000000ULL + (uint64_t)(ts.tv_nsec / 100ULL);
    win_100ns = unix_100ns + 116444736000000000ULL;
    lpSystemTimeAsFileTime->dwLowDateTime = (uint32_t)(win_100ns & 0xFFFFFFFFu);
    lpSystemTimeAsFileTime->dwHighDateTime = (uint32_t)(win_100ns >> 32);
}

KN_BOOL KN_STDCALL FileTimeToSystemTime(const KN_FILETIME *lpFileTime, KN_SYSTEMTIME *lpSystemTime)
{
    uint64_t win_100ns = 0;
    uint64_t unix_100ns = 0;
    time_t secs = 0;
    struct tm tmv;
    if (!lpFileTime || !lpSystemTime)
        return 0;
    win_100ns = ((uint64_t)lpFileTime->dwHighDateTime << 32) | (uint64_t)lpFileTime->dwLowDateTime;
    if (win_100ns < 116444736000000000ULL)
        return 0;
    unix_100ns = win_100ns - 116444736000000000ULL;
    secs = (time_t)(unix_100ns / 10000000ULL);
    if (!gmtime_r(&secs, &tmv))
        return 0;
    lpSystemTime->wYear = (uint16_t)(tmv.tm_year + 1900);
    lpSystemTime->wMonth = (uint16_t)(tmv.tm_mon + 1);
    lpSystemTime->wDay = (uint16_t)tmv.tm_mday;
    lpSystemTime->wDayOfWeek = (uint16_t)tmv.tm_wday;
    lpSystemTime->wHour = (uint16_t)tmv.tm_hour;
    lpSystemTime->wMinute = (uint16_t)tmv.tm_min;
    lpSystemTime->wSecond = (uint16_t)tmv.tm_sec;
    lpSystemTime->wMilliseconds = (uint16_t)((unix_100ns % 10000000ULL) / 10000ULL);
    return 1;
}

KN_HANDLE KN_STDCALL FindFirstFileA(const char *lpFileName, KN_WIN32_FIND_DATAA *lpFindFileData)
{
    KnHostHandle *h = 0;
    int rc = 0;
    char pattern[PATH_MAX];
    if (!lpFileName || !lpFileName[0])
        return KN_INVALID_HANDLE_VALUE;
    if (!kn_host_norm_path(lpFileName, pattern, sizeof(pattern)))
        return KN_INVALID_HANDLE_VALUE;
    h = kn_host_new_handle(KN_HOST_HANDLE_FIND);
    if (!h)
        return KN_INVALID_HANDLE_VALUE;
    rc = glob(pattern, 0, 0, &h->u.find.matches);
    if (rc != 0 || h->u.find.matches.gl_pathc == 0)
    {
        globfree(&h->u.find.matches);
        free(h);
        return KN_INVALID_HANDLE_VALUE;
    }
    h->u.find.index = 1;
    kn_host_fill_find_data(h->u.find.matches.gl_pathv[0], lpFindFileData);
    return h;
}

KN_BOOL KN_STDCALL FindNextFileA(KN_HANDLE hFindFile, KN_WIN32_FIND_DATAA *lpFindFileData)
{
    KnHostHandle *h = kn_host_as_handle(hFindFile);
    if (!h || h->kind != KN_HOST_HANDLE_FIND)
        return 0;
    if (h->u.find.index >= h->u.find.matches.gl_pathc)
        return 0;
    kn_host_fill_find_data(h->u.find.matches.gl_pathv[h->u.find.index], lpFindFileData);
    h->u.find.index++;
    return 1;
}

KN_BOOL KN_STDCALL FindClose(KN_HANDLE hFindFile)
{
    KnHostHandle *h = kn_host_as_handle(hFindFile);
    if (!h || h->kind != KN_HOST_HANDLE_FIND)
        return 0;
    globfree(&h->u.find.matches);
    free(h);
    return 1;
}

KN_BOOL KN_STDCALL CreateDirectoryA(const char *lpPathName, KN_SECURITY_ATTRIBUTES *lpSecurityAttributes)
{
    char path[PATH_MAX];
    (void)lpSecurityAttributes;
    if (!lpPathName || !lpPathName[0])
        return 0;
    if (!kn_host_norm_path(lpPathName, path, sizeof(path)))
        return 0;
    if (mkdir(path, 0777) == 0)
        return 1;
    return errno == EEXIST ? 1 : 0;
}

KN_BOOL KN_STDCALL RemoveDirectoryA(const char *lpPathName)
{
    char path[PATH_MAX];
    if (!lpPathName || !lpPathName[0])
        return 0;
    if (!kn_host_norm_path(lpPathName, path, sizeof(path)))
        return 0;
    return rmdir(path) == 0 ? 1 : 0;
}

KN_BOOL KN_STDCALL DeleteFileA(const char *lpFileName)
{
    char path[PATH_MAX];
    if (!lpFileName || !lpFileName[0])
        return 0;
    if (!kn_host_norm_path(lpFileName, path, sizeof(path)))
        return 0;
    return unlink(path) == 0 ? 1 : 0;
}

KN_DWORD KN_STDCALL GetEnvironmentVariableA(const char *lpName, char *lpBuffer, KN_DWORD nSize)
{
    const char *value = getenv(lpName ? lpName : "");
    size_t len = value ? strlen(value) : 0;
    if (!value)
        return 0;
    if (lpBuffer && nSize > 0)
    {
        size_t copy = len < (size_t)(nSize - 1) ? len : (size_t)(nSize - 1);
        memcpy(lpBuffer, value, copy);
        lpBuffer[copy] = 0;
    }
    return (KN_DWORD)len;
}

KN_DWORD KN_STDCALL GetCurrentDirectoryA(KN_DWORD nBufferLength, char *lpBuffer)
{
    if (!lpBuffer || nBufferLength == 0)
        return 0;
    if (!getcwd(lpBuffer, (size_t)nBufferLength))
        return 0;
    return (KN_DWORD)strlen(lpBuffer);
}

KN_DWORD KN_STDCALL GetFullPathNameA(const char *lpFileName, KN_DWORD nBufferLength, char *lpBuffer, char **lpFilePart)
{
    size_t len = 0;
    const char *base = 0;
    if (!lpBuffer || nBufferLength == 0 || !kn_host_make_abs(lpFileName, lpBuffer, (size_t)nBufferLength))
        return 0;
    kn_host_normalize_abs_path(lpBuffer);
    len = strlen(lpBuffer);
    if (lpFilePart)
    {
        base = kn_host_basename(lpBuffer);
        *lpFilePart = (char *)base;
    }
    return (KN_DWORD)len;
}

int KN_STDCALL MultiByteToWideChar(KN_DWORD CodePage, KN_DWORD dwFlags, const char *lpMultiByteStr, int cbMultiByte, uint16_t *lpWideCharStr, int cchWideChar)
{
    int count = 0;
    (void)CodePage;
    (void)dwFlags;
    if (!lpMultiByteStr)
        return 0;
    if (cbMultiByte < 0)
        cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
    while (count < cbMultiByte && (!lpWideCharStr || count < cchWideChar))
    {
        if (lpWideCharStr)
            lpWideCharStr[count] = (uint8_t)lpMultiByteStr[count];
        count++;
    }
    return count;
}

int KN_STDCALL WideCharToMultiByte(KN_DWORD CodePage, KN_DWORD dwFlags, const uint16_t *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cbMultiByte, const char *lpDefaultChar, KN_BOOL *lpUsedDefaultChar)
{
    int count = 0;
    (void)CodePage;
    (void)dwFlags;
    (void)lpDefaultChar;
    if (lpUsedDefaultChar) *lpUsedDefaultChar = 0;
    if (!lpWideCharStr)
        return 0;
    if (cchWideChar < 0)
    {
        while (lpWideCharStr[count] != 0)
            count++;
        count++;
    }
    else
    {
        count = cchWideChar;
    }
    if (lpMultiByteStr && cbMultiByte > 0)
    {
        int limit = count < cbMultiByte ? count : cbMultiByte;
        for (int i = 0; i < limit; i++)
            lpMultiByteStr[i] = (char)(lpWideCharStr[i] & 0xFF);
        if (limit > 0 && limit == cbMultiByte)
            lpMultiByteStr[cbMultiByte - 1] = 0;
    }
    return count;
}

int KN_STDCALL GetWindowTextLengthW(KN_HANDLE hWnd)
{
    (void)hWnd;
    return 0;
}

int KN_STDCALL GetWindowTextW(KN_HANDLE hWnd, uint16_t *lpString, int nMaxCount)
{
    (void)hWnd;
    if (lpString && nMaxCount > 0)
        lpString[0] = 0;
    return 0;
}

#endif
