#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef KN_LLVM_BIN
#define KN_LLVM_BIN ""
#endif

#ifndef KN_KERNEL32_LIB
#define KN_KERNEL32_LIB ""
#endif

#ifndef KN_USER32_LIB
#define KN_USER32_LIB ""
#endif

#ifndef KN_GDI32_LIB
#define KN_GDI32_LIB ""
#endif

#ifndef KN_RUNTIME_OBJ
#define KN_RUNTIME_OBJ ""
#endif

#define KN_STDIN_HANDLE  ((uint32_t)-10)
#define KN_STDOUT_HANDLE ((uint32_t)-11)
#define KN_STDERR_HANDLE ((uint32_t)-12)
#define KN_STARTF_USESTDHANDLES 0x00000100u

#define KN_GENERIC_READ 0x80000000u
#define KN_GENERIC_WRITE 0x40000000u
#define KN_FILE_SHARE_READ 0x00000001u
#define KN_FILE_SHARE_WRITE 0x00000002u
#define KN_CREATE_ALWAYS 2u
#define KN_OPEN_ALWAYS 4u
#define KN_OPEN_EXISTING 3u
#define KN_FILE_ATTRIBUTE_NORMAL 0x00000080u
#define KN_FILE_ATTRIBUTE_DIRECTORY 0x00000010u

#define KN_INFINITE 0xFFFFFFFFu

#define KN_MAX_PATH 260
#define KN_INVALID_HANDLE_VALUE ((KN_HANDLE)(intptr_t)-1)

#if defined(_WIN32) || defined(_WIN64)
#if defined(_MSC_VER)
#define KN_DLLIMPORT __declspec(dllimport)
#define KN_STDCALL __stdcall
#else
#define KN_DLLIMPORT __attribute__((dllimport))
#define KN_STDCALL __attribute__((stdcall))
#endif
#else
#define KN_DLLIMPORT
#define KN_STDCALL
#endif

typedef void* KN_HANDLE;
typedef uint32_t KN_DWORD;
typedef int32_t KN_BOOL;

typedef struct
{
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} KN_FILETIME;

typedef struct
{
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
} KN_SYSTEMTIME;

typedef struct
{
    uint32_t dwFileAttributes;
    KN_FILETIME ftCreationTime;
    KN_FILETIME ftLastAccessTime;
    KN_FILETIME ftLastWriteTime;
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    char cFileName[KN_MAX_PATH];
    char cAlternateFileName[14];
} KN_WIN32_FIND_DATAA;

typedef struct
{
    uint32_t nLength;
    void *lpSecurityDescriptor;
    KN_BOOL bInheritHandle;
} KN_SECURITY_ATTRIBUTES;

typedef struct
{
    uint32_t cb;
    const char *lpReserved;
    const char *lpDesktop;
    const char *lpTitle;
    uint32_t dwX;
    uint32_t dwY;
    uint32_t dwXSize;
    uint32_t dwYSize;
    uint32_t dwXCountChars;
    uint32_t dwYCountChars;
    uint32_t dwFillAttribute;
    uint32_t dwFlags;
    uint16_t wShowWindow;
    uint16_t cbReserved2;
    uint8_t *lpReserved2;
    KN_HANDLE hStdInput;
    KN_HANDLE hStdOutput;
    KN_HANDLE hStdError;
} KN_STARTUPINFOA;

typedef struct
{
    KN_HANDLE hProcess;
    KN_HANDLE hThread;
    uint32_t dwProcessId;
    uint32_t dwThreadId;
} KN_PROCESS_INFORMATION;

typedef KN_DWORD (KN_STDCALL *KN_THREAD_START_ROUTINE)(void *lpThreadParameter);

KN_DLLIMPORT KN_HANDLE KN_STDCALL GetStdHandle(KN_DWORD nStdHandle);
KN_DLLIMPORT KN_BOOL KN_STDCALL GetConsoleMode(KN_HANDLE hConsoleHandle, KN_DWORD *lpMode);
KN_DLLIMPORT KN_BOOL KN_STDCALL SetConsoleMode(KN_HANDLE hConsoleHandle, KN_DWORD dwMode);
KN_DLLIMPORT KN_BOOL KN_STDCALL SetConsoleOutputCP(KN_DWORD wCodePageID);
KN_DLLIMPORT KN_BOOL KN_STDCALL SetConsoleCP(KN_DWORD wCodePageID);
KN_DLLIMPORT KN_BOOL KN_STDCALL WriteFile(KN_HANDLE hFile, const void *lpBuffer, KN_DWORD nBytesToWrite, KN_DWORD *lpBytesWritten, void *lpOverlapped);
KN_DLLIMPORT void KN_STDCALL ExitProcess(uint32_t uExitCode);
KN_DLLIMPORT KN_HANDLE KN_STDCALL GetProcessHeap(void);
KN_DLLIMPORT void *KN_STDCALL HeapAlloc(KN_HANDLE hHeap, KN_DWORD dwFlags, size_t dwBytes);
KN_DLLIMPORT KN_BOOL KN_STDCALL HeapFree(KN_HANDLE hHeap, KN_DWORD dwFlags, void *lpMem);
KN_DLLIMPORT KN_HANDLE KN_STDCALL CreateFileA(const char *lpFileName, KN_DWORD dwDesiredAccess, KN_DWORD dwShareMode, KN_SECURITY_ATTRIBUTES *lpSecurityAttributes, KN_DWORD dwCreationDisposition, KN_DWORD dwFlagsAndAttributes, KN_HANDLE hTemplateFile);
KN_DLLIMPORT KN_BOOL KN_STDCALL ReadFile(KN_HANDLE hFile, void *lpBuffer, KN_DWORD nNumberOfBytesToRead, KN_DWORD *lpNumberOfBytesRead, void *lpOverlapped);
KN_DLLIMPORT KN_BOOL KN_STDCALL CloseHandle(KN_HANDLE hObject);
KN_DLLIMPORT KN_BOOL KN_STDCALL GetFileSizeEx(KN_HANDLE hFile, int64_t *lpFileSize);
KN_DLLIMPORT const char *KN_STDCALL GetCommandLineA(void);
KN_DLLIMPORT KN_BOOL KN_STDCALL CreateProcessA(const char *lpApplicationName, char *lpCommandLine, KN_SECURITY_ATTRIBUTES *lpProcessAttributes, KN_SECURITY_ATTRIBUTES *lpThreadAttributes, KN_BOOL bInheritHandles, KN_DWORD dwCreationFlags, void *lpEnvironment, const char *lpCurrentDirectory, KN_STARTUPINFOA *lpStartupInfo, KN_PROCESS_INFORMATION *lpProcessInformation);
KN_DLLIMPORT KN_HANDLE KN_STDCALL CreateThread(KN_SECURITY_ATTRIBUTES *lpThreadAttributes, size_t dwStackSize, KN_THREAD_START_ROUTINE lpStartAddress, void *lpParameter, KN_DWORD dwCreationFlags, KN_DWORD *lpThreadId);
KN_DLLIMPORT KN_DWORD KN_STDCALL WaitForSingleObject(KN_HANDLE hHandle, KN_DWORD dwMilliseconds);
KN_DLLIMPORT KN_BOOL KN_STDCALL GetExitCodeProcess(KN_HANDLE hProcess, KN_DWORD *lpExitCode);
KN_DLLIMPORT void KN_STDCALL Sleep(KN_DWORD dwMilliseconds);
KN_DLLIMPORT uint64_t KN_STDCALL GetTickCount64(void);
KN_DLLIMPORT KN_HANDLE KN_STDCALL LoadLibraryA(const char *lpLibFileName);
KN_DLLIMPORT void *KN_STDCALL GetProcAddress(KN_HANDLE hModule, const char *lpProcName);
KN_DLLIMPORT KN_BOOL KN_STDCALL FreeLibrary(KN_HANDLE hModule);
KN_DLLIMPORT KN_BOOL KN_STDCALL QueryPerformanceCounter(int64_t *lpPerformanceCount);
KN_DLLIMPORT KN_BOOL KN_STDCALL QueryPerformanceFrequency(int64_t *lpFrequency);
KN_DLLIMPORT void KN_STDCALL GetSystemTimeAsFileTime(KN_FILETIME *lpSystemTimeAsFileTime);
KN_DLLIMPORT KN_BOOL KN_STDCALL FileTimeToSystemTime(const KN_FILETIME *lpFileTime, KN_SYSTEMTIME *lpSystemTime);
KN_DLLIMPORT KN_HANDLE KN_STDCALL FindFirstFileA(const char *lpFileName, KN_WIN32_FIND_DATAA *lpFindFileData);
KN_DLLIMPORT KN_BOOL KN_STDCALL FindNextFileA(KN_HANDLE hFindFile, KN_WIN32_FIND_DATAA *lpFindFileData);
KN_DLLIMPORT KN_BOOL KN_STDCALL FindClose(KN_HANDLE hFindFile);
KN_DLLIMPORT KN_BOOL KN_STDCALL CreateDirectoryA(const char *lpPathName, KN_SECURITY_ATTRIBUTES *lpSecurityAttributes);
KN_DLLIMPORT KN_BOOL KN_STDCALL RemoveDirectoryA(const char *lpPathName);
KN_DLLIMPORT KN_BOOL KN_STDCALL DeleteFileA(const char *lpFileName);
KN_DLLIMPORT KN_DWORD KN_STDCALL GetEnvironmentVariableA(const char *lpName, char *lpBuffer, KN_DWORD nSize);
KN_DLLIMPORT KN_DWORD KN_STDCALL GetCurrentDirectoryA(KN_DWORD nBufferLength, char *lpBuffer);
KN_DLLIMPORT KN_DWORD KN_STDCALL GetModuleFileNameA(KN_HANDLE hModule, char *lpFilename, KN_DWORD nSize);
KN_DLLIMPORT KN_DWORD KN_STDCALL GetFullPathNameA(const char *lpFileName, KN_DWORD nBufferLength, char *lpBuffer, char **lpFilePart);
KN_DLLIMPORT int KN_STDCALL MultiByteToWideChar(KN_DWORD CodePage, KN_DWORD dwFlags, const char *lpMultiByteStr, int cbMultiByte, uint16_t *lpWideCharStr, int cchWideChar);
KN_DLLIMPORT int KN_STDCALL WideCharToMultiByte(KN_DWORD CodePage, KN_DWORD dwFlags, const uint16_t *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cbMultiByte, const char *lpDefaultChar, KN_BOOL *lpUsedDefaultChar);
KN_DLLIMPORT int KN_STDCALL GetWindowTextLengthW(KN_HANDLE hWnd);
KN_DLLIMPORT int KN_STDCALL GetWindowTextW(KN_HANDLE hWnd, uint16_t *lpString, int nMaxCount);
