#pragma once

typedef enum
{
    KN_ENV_HOSTED = 0,
    KN_ENV_FREESTANDING = 1
} KnEnvKind;

typedef enum
{
    KN_RUNTIME_NONE = 0,
    KN_RUNTIME_ALLOC = 1,
    KN_RUNTIME_GC = 2
} KnRuntimeMode;

typedef enum
{
    KN_PANIC_TRAP = 0,
    KN_PANIC_LOOP = 1
} KnPanicMode;
