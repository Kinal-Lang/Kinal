#include "kn/std.h"
#include "kn/util.h"

#define KN_STD_MASK_HOSTED              (1u << KN_STD_PROFILE_HOSTED)
#define KN_STD_MASK_FREESTANDING_CORE   (1u << KN_STD_PROFILE_FREESTANDING_CORE)
#define KN_STD_MASK_FREESTANDING_ALLOC  (1u << KN_STD_PROFILE_FREESTANDING_ALLOC)
#define KN_STD_MASK_FREESTANDING_GC     (1u << KN_STD_PROFILE_FREESTANDING_GC)

static int g_std_profile = KN_STD_PROFILE_HOSTED;

static const KnStdFunc g_console_funcs[] = {
    { "IO.Console", "PrintLine", KN_BUILTIN_IO_CONSOLE_PRINTLINE, { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Console", "Print",     KN_BUILTIN_IO_CONSOLE_PRINT,     { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Console", "ReadLine",  KN_BUILTIN_IO_CONSOLE_READLINE,  { TY_STRING, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_time_funcs[] = {
    { "IO.Time", "Now",         KN_BUILTIN_IO_TIME_NOW,      { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Time.DateTime" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Time", "GetTick",     KN_BUILTIN_IO_TIME_GETTICK,  { TY_INT, TY_UNKNOWN, 0 },  0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Time", "Sleep",       KN_BUILTIN_IO_TIME_SLEEP,    { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Time", "GetNanoTick", KN_BUILTIN_IO_TIME_NANOTICK, { TY_INT, TY_UNKNOWN, 0 },  0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Time", "Format",      KN_BUILTIN_IO_TIME_FORMAT,   { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Time.DateTime" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_string_funcs[] = {
    { "IO.Type.string", "Length",  KN_BUILTIN_IO_STRING_LENGTH,  { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.string", "Concat",  KN_BUILTIN_IO_STRING_CONCAT,  { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.string", "Equals",  KN_BUILTIN_IO_STRING_EQUALS,  { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.string", "ToChars", KN_BUILTIN_IO_STRING_TOCHARS, { TY_ARRAY, TY_CHAR, -1 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_dict_funcs[] = {
    { "IO.Collection.dict", "Create",   KN_BUILTIN_IO_DICT_NEW,      { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Set",      KN_BUILTIN_IO_DICT_SET,      { TY_VOID, TY_UNKNOWN, 0 }, 3, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Fetch",    KN_BUILTIN_IO_DICT_GET,      { TY_ANY, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "TryFetch", KN_BUILTIN_IO_DICT_TRY_FETCH, { TY_ANY, TY_UNKNOWN, 0 }, 3, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Contains", KN_BUILTIN_IO_DICT_CONTAINS, { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Count",    KN_BUILTIN_IO_DICT_COUNT,    { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Remove",   KN_BUILTIN_IO_DICT_REMOVE,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.dict", "IsEmpty",  KN_BUILTIN_IO_DICT_IS_EMPTY, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Keys",     KN_BUILTIN_IO_DICT_KEYS,     { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" } }, SAFETY_SAFE },
    { "IO.Collection.dict", "Values",   KN_BUILTIN_IO_DICT_VALUES,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" } }, SAFETY_SAFE },
};

static const KnStdFunc g_list_funcs[] = {
    { "IO.Collection.list", "Create", KN_BUILTIN_IO_LIST_NEW,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Add",    KN_BUILTIN_IO_LIST_ADD,   { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Fetch",  KN_BUILTIN_IO_LIST_GET,   { TY_ANY, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Set",    KN_BUILTIN_IO_LIST_SET,   { TY_VOID, TY_UNKNOWN, 0 }, 3, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_INT, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Insert", KN_BUILTIN_IO_LIST_INSERT, { TY_VOID, TY_UNKNOWN, 0 }, 3, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_INT, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "TryFetch", KN_BUILTIN_IO_LIST_TRY_FETCH, { TY_ANY, TY_UNKNOWN, 0 }, 3, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_INT, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Count",  KN_BUILTIN_IO_LIST_COUNT, { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
    { "IO.Collection.list", "Clear",  KN_BUILTIN_IO_LIST_CLEAR, { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
    { "IO.Collection.list", "RemoveAt", KN_BUILTIN_IO_LIST_REMOVE_AT, { TY_ANY, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Contains", KN_BUILTIN_IO_LIST_CONTAINS, { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "Pop",      KN_BUILTIN_IO_LIST_POP,      { TY_ANY, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
    { "IO.Collection.list", "Peek",     KN_BUILTIN_IO_LIST_PEEK,     { TY_ANY, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
    { "IO.Collection.list", "IndexOf",  KN_BUILTIN_IO_LIST_INDEX_OF, { TY_INT, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.list", "IsEmpty",  KN_BUILTIN_IO_LIST_IS_EMPTY, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
};

static const KnStdFunc g_set_funcs[] = {
    { "IO.Collection.set", "Create",   KN_BUILTIN_IO_SET_NEW,      { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.set", "Add",      KN_BUILTIN_IO_SET_ADD,      { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.set", "Contains", KN_BUILTIN_IO_SET_CONTAINS, { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.set", "Count",    KN_BUILTIN_IO_SET_COUNT,    { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" } }, SAFETY_SAFE },
    { "IO.Collection.set", "Remove",   KN_BUILTIN_IO_SET_REMOVE,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Collection.set", "Clear",    KN_BUILTIN_IO_SET_CLEAR,    { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" } }, SAFETY_SAFE },
    { "IO.Collection.set", "IsEmpty",  KN_BUILTIN_IO_SET_IS_EMPTY, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" } }, SAFETY_SAFE },
    { "IO.Collection.set", "Values",   KN_BUILTIN_IO_SET_VALUES,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.set" } }, SAFETY_SAFE },
};

static const KnStdFunc g_type_funcs[] = {
    { "IO.Type", "bool",   KN_BUILTIN_IO_TYPE_BOOL,   { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "byte",   KN_BUILTIN_IO_TYPE_BYTE,   { TY_BYTE, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "char",   KN_BUILTIN_IO_TYPE_CHAR,   { TY_CHAR, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "int",    KN_BUILTIN_IO_TYPE_INT,    { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "float",  KN_BUILTIN_IO_TYPE_FLOAT,  { TY_FLOAT, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "f16",    KN_BUILTIN_IO_TYPE_F16,    { TY_F16, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "f32",    KN_BUILTIN_IO_TYPE_F32,    { TY_F32, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "f64",    KN_BUILTIN_IO_TYPE_F64,    { TY_F64, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "f128",   KN_BUILTIN_IO_TYPE_F128,   { TY_F128, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "i8",     KN_BUILTIN_IO_TYPE_I8,     { TY_I8, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "i16",    KN_BUILTIN_IO_TYPE_I16,    { TY_I16, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "i32",    KN_BUILTIN_IO_TYPE_I32,    { TY_I32, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "i64",    KN_BUILTIN_IO_TYPE_I64,    { TY_I64, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "u8",     KN_BUILTIN_IO_TYPE_U8,     { TY_U8, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "u16",    KN_BUILTIN_IO_TYPE_U16,    { TY_U16, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "u32",    KN_BUILTIN_IO_TYPE_U32,    { TY_U32, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "u64",    KN_BUILTIN_IO_TYPE_U64,    { TY_U64, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "isize",  KN_BUILTIN_IO_TYPE_ISIZE,  { TY_ISIZE, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "usize",  KN_BUILTIN_IO_TYPE_USIZE,  { TY_USIZE, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "string", KN_BUILTIN_IO_TYPE_STRING, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "any",    KN_BUILTIN_IO_TYPE_ANY,    { TY_ANY, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "dict",   KN_BUILTIN_IO_TYPE_DICT,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type", "list",   KN_BUILTIN_IO_TYPE_LIST,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_any_funcs[] = {
    { "IO.Type.any", "IsNull",   KN_BUILTIN_IO_ANY_ISNULL,   { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "ToString", KN_BUILTIN_IO_ANY_TOSTRING, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "Equals",   KN_BUILTIN_IO_ANY_EQUALS,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_ANY, TY_UNKNOWN, 0 }, { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "Tag",      KN_BUILTIN_IO_ANY_TAG,      { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "TypeName", KN_BUILTIN_IO_ANY_TYPENAME, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsNumber", KN_BUILTIN_IO_ANY_ISNUMBER, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsPointer", KN_BUILTIN_IO_ANY_ISPOINTER, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsInt",    KN_BUILTIN_IO_ANY_ISINT,    { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsFloat",  KN_BUILTIN_IO_ANY_ISFLOAT,  { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsString", KN_BUILTIN_IO_ANY_ISSTRING, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsBool",   KN_BUILTIN_IO_ANY_ISBOOL,   { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsChar",   KN_BUILTIN_IO_ANY_ISCHAR,   { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Type.any", "IsObject", KN_BUILTIN_IO_ANY_ISOBJECT, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_ANY, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_system_funcs[] = {
    { "IO.System", "Exit",        KN_BUILTIN_IO_SYSTEM_EXIT,        { TY_VOID, TY_UNKNOWN, 0 }, 1, { { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.System", "CommandLine", KN_BUILTIN_IO_SYSTEM_COMMANDLINE, { TY_STRING, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.System", "FileExists",  KN_BUILTIN_IO_SYSTEM_FILE_EXISTS, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.System", "Exec",        KN_BUILTIN_IO_SYSTEM_EXEC,        { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.System", "LoadLibrary", KN_BUILTIN_IO_SYSTEM_LOAD_LIBRARY, { TY_PTR, TY_BYTE, 0, 1 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.System", "GetSymbol",   KN_BUILTIN_IO_SYSTEM_GET_SYMBOL,   { TY_PTR, TY_BYTE, 0, 1 }, 2, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.System", "CloseLibrary", KN_BUILTIN_IO_SYSTEM_CLOSE_LIBRARY, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_BYTE, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.System", "LastError",   KN_BUILTIN_IO_SYSTEM_LAST_ERROR,   { TY_STRING, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_async_funcs[] = {
    { "IO.Async", "Spawn",       KN_BUILTIN_IO_ASYNC_SPAWN,        { TY_PTR, TY_BYTE, 0, 1 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Async", "IsCompleted", KN_BUILTIN_IO_ASYNC_IS_COMPLETED, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_BYTE, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Async", "Wait",        KN_BUILTIN_IO_ASYNC_WAIT,         { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Async", "ExitCode",    KN_BUILTIN_IO_ASYNC_EXIT_CODE,    { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_BYTE, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Async", "Close",       KN_BUILTIN_IO_ASYNC_CLOSE,        { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_BYTE, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Async", "Run",         KN_BUILTIN_IO_ASYNC_RUN,          { TY_ANY, TY_UNKNOWN, 0 }, 1, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Async.Task" } }, SAFETY_SAFE },
    { "IO.Async", "Sleep",       KN_BUILTIN_IO_ASYNC_SLEEP,        { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Async.Task" }, 1, { { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Async", "Yield",       KN_BUILTIN_IO_ASYNC_YIELD,        { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Async.Task" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_meta_funcs[] = {
    { "IO.Meta", "Query", KN_BUILTIN_IO_META_QUERY, { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Meta", "Of",    KN_BUILTIN_IO_META_OF,    { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Meta", "Has",   KN_BUILTIN_IO_META_HAS,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Meta", "Find",  KN_BUILTIN_IO_META_FIND,  { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.dict" }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_path_funcs[] = {
    { "IO.Path", "Combine",   KN_BUILTIN_IO_PATH_COMBINE,   { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Join",      KN_BUILTIN_IO_PATH_COMBINE,   { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Join3",     KN_BUILTIN_IO_PATH_JOIN3,     { TY_STRING, TY_UNKNOWN, 0 }, 3, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Join4",     KN_BUILTIN_IO_PATH_JOIN4,     { TY_STRING, TY_UNKNOWN, 0 }, 4, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "FileName",  KN_BUILTIN_IO_PATH_FILE_NAME, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Extension", KN_BUILTIN_IO_PATH_EXTENSION, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Directory", KN_BUILTIN_IO_PATH_DIRECTORY, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Parent",    KN_BUILTIN_IO_PATH_DIRECTORY, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Normalize", KN_BUILTIN_IO_PATH_NORMALIZE, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Cwd", KN_BUILTIN_IO_PATH_CWD, { TY_STRING, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "ChangeExtension", KN_BUILTIN_IO_PATH_CHANGE_EXTENSION, { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "HasExtension", KN_BUILTIN_IO_PATH_HAS_EXTENSION, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "IsAbsolute", KN_BUILTIN_IO_PATH_IS_ABSOLUTE, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "IsRelative", KN_BUILTIN_IO_PATH_IS_RELATIVE, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "BaseName", KN_BUILTIN_IO_PATH_BASE_NAME, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "Stem", KN_BUILTIN_IO_PATH_BASE_NAME, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Path", "EnsureTrailingSeparator", KN_BUILTIN_IO_PATH_ENSURE_TRAILING_SEPARATOR, { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_text_funcs[] = {
    { "IO.Text", "Length",     KN_BUILTIN_IO_TEXT_LENGTH,      { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "IsEmpty",    KN_BUILTIN_IO_TEXT_IS_EMPTY,    { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Count",      KN_BUILTIN_IO_TEXT_COUNT,       { TY_INT, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Contains",   KN_BUILTIN_IO_TEXT_CONTAINS,    { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "StartsWith", KN_BUILTIN_IO_TEXT_STARTS_WITH, { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "EndsWith",   KN_BUILTIN_IO_TEXT_ENDS_WITH,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "IndexOf",    KN_BUILTIN_IO_TEXT_INDEX_OF,    { TY_INT, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "LastIndexOf", KN_BUILTIN_IO_TEXT_LAST_INDEX_OF, { TY_INT, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Trim",       KN_BUILTIN_IO_TEXT_TRIM,        { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Replace",    KN_BUILTIN_IO_TEXT_REPLACE,     { TY_STRING, TY_UNKNOWN, 0 }, 3, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "ToUpper",    KN_BUILTIN_IO_TEXT_TO_UPPER,    { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "ToLower",    KN_BUILTIN_IO_TEXT_TO_LOWER,    { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Slice",      KN_BUILTIN_IO_TEXT_SLICE,       { TY_STRING, TY_UNKNOWN, 0 }, 3, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_INT, TY_UNKNOWN, 0 }, { TY_INT, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Split",      KN_BUILTIN_IO_TEXT_SPLIT,       { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Join",       KN_BUILTIN_IO_TEXT_JOIN,        { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Text", "Lines",      KN_BUILTIN_IO_TEXT_LINES,       { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_file_funcs[] = {
    { "IO.File", "Exists",        KN_BUILTIN_IO_SYSTEM_FILE_EXISTS,    { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Create",        KN_BUILTIN_IO_FILE_CREATE,           { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Touch",         KN_BUILTIN_IO_FILE_TOUCH,            { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadAllText",   KN_BUILTIN_IO_FILE_READ_ALL_TEXT,    { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadFirstLine", KN_BUILTIN_IO_FILE_READ_FIRST_LINE,  { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "WriteAllText",  KN_BUILTIN_IO_FILE_WRITE_ALL_TEXT,   { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "AppendAllText", KN_BUILTIN_IO_FILE_APPEND_ALL_TEXT,  { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "AppendLine",    KN_BUILTIN_IO_FILE_APPEND_LINE,      { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Delete",        KN_BUILTIN_IO_FILE_DELETE,           { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "DeleteIfExists", KN_BUILTIN_IO_FILE_DELETE_IF_EXISTS, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Size",          KN_BUILTIN_IO_FILE_SIZE,             { TY_INT, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "IsEmpty",       KN_BUILTIN_IO_FILE_IS_EMPTY,         { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Copy",          KN_BUILTIN_IO_FILE_COPY,             { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "Move",          KN_BUILTIN_IO_FILE_MOVE,             { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadOrDefault", KN_BUILTIN_IO_FILE_READ_OR_DEFAULT,  { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadIfExists",  KN_BUILTIN_IO_FILE_READ_IF_EXISTS,   { TY_STRING, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadOr",        KN_BUILTIN_IO_FILE_READ_OR,          { TY_STRING, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "ReadLines",     KN_BUILTIN_IO_FILE_READ_LINES,       { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.File", "WriteLines",    KN_BUILTIN_IO_FILE_WRITE_LINES,      { TY_BOOL, TY_UNKNOWN, 0 }, 2, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Collection.list" } }, SAFETY_SAFE },
    { "IO.File", "ReplaceText",   KN_BUILTIN_IO_FILE_REPLACE_TEXT,     { TY_BOOL, TY_UNKNOWN, 0 }, 3, { { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 }, { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_directory_funcs[] = {
    { "IO.Directory", "Exists",         KN_BUILTIN_IO_DIRECTORY_EXISTS,          { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Directory", "Create",         KN_BUILTIN_IO_DIRECTORY_CREATE,          { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Directory", "Ensure",         KN_BUILTIN_IO_DIRECTORY_ENSURE,          { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Directory", "Delete",         KN_BUILTIN_IO_DIRECTORY_DELETE,          { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Directory", "DeleteIfExists", KN_BUILTIN_IO_DIRECTORY_DELETE_IF_EXISTS, { TY_BOOL, TY_UNKNOWN, 0 }, 1, { { TY_STRING, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_error_funcs[] = {
    { "IO.Error", "LastTrace",    KN_BUILTIN_IO_ERROR_LASTTRACE, { TY_STRING, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Error", "HasException", KN_BUILTIN_IO_ERROR_HAS,       { TY_BOOL, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Error", "Clear",        KN_BUILTIN_IO_ERROR_CLEAR,     { TY_VOID, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
    { "IO.Error", "Current",      KN_BUILTIN_IO_ERROR_CURRENT,   { TY_CLASS, TY_UNKNOWN, 0, 0, "IO.Error" }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_gc_funcs[] = {
    { "IO.GC", "Collect", KN_BUILTIN_IO_GC_COLLECT, { TY_VOID, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_SAFE },
};

static const KnStdFunc g_memory_funcs[] = {
    { "IO.Memory", "Copy",    KN_BUILTIN_IO_MEMORY_COPY,    { TY_VOID, TY_UNKNOWN, 0 }, 3, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_PTR, TY_BYTE, 0, 1 }, { TY_USIZE, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Memory", "Set",     KN_BUILTIN_IO_MEMORY_SET,     { TY_VOID, TY_UNKNOWN, 0 }, 3, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_BYTE, TY_UNKNOWN, 0 }, { TY_USIZE, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Memory", "Zero",    KN_BUILTIN_IO_MEMORY_ZERO,    { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_USIZE, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Memory", "Compare", KN_BUILTIN_IO_MEMORY_COMPARE, { TY_INT, TY_UNKNOWN, 0 }, 3, { { TY_PTR, TY_BYTE, 0, 1 }, { TY_PTR, TY_BYTE, 0, 1 }, { TY_USIZE, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
};

static const KnStdFunc g_volatile_funcs[] = {
    { "IO.Volatile", "Read8",   KN_BUILTIN_IO_VOLATILE_READ8,   { TY_U8, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_U8, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Read16",  KN_BUILTIN_IO_VOLATILE_READ16,  { TY_U16, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_U16, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Read32",  KN_BUILTIN_IO_VOLATILE_READ32,  { TY_U32, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_U32, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Read64",  KN_BUILTIN_IO_VOLATILE_READ64,  { TY_U64, TY_UNKNOWN, 0 }, 1, { { TY_PTR, TY_U64, 0, 1 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Write8",  KN_BUILTIN_IO_VOLATILE_WRITE8,  { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_U8, 0, 1 }, { TY_U8, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Write16", KN_BUILTIN_IO_VOLATILE_WRITE16, { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_U16, 0, 1 }, { TY_U16, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Write32", KN_BUILTIN_IO_VOLATILE_WRITE32, { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_U32, 0, 1 }, { TY_U32, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Volatile", "Write64", KN_BUILTIN_IO_VOLATILE_WRITE64, { TY_VOID, TY_UNKNOWN, 0 }, 2, { { TY_PTR, TY_U64, 0, 1 }, { TY_U64, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
};

static const KnStdFunc g_panic_funcs[] = {
    { "IO.Panic", "Abort", KN_BUILTIN_IO_PANIC_ABORT, { TY_VOID, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
    { "IO.Panic", "Halt",  KN_BUILTIN_IO_PANIC_HALT,  { TY_VOID, TY_UNKNOWN, 0 }, 0, { { TY_UNKNOWN, TY_UNKNOWN, 0 } }, SAFETY_TRUSTED },
};

static const KnStdModule g_modules[] = {
    { "IO.Console",      g_console_funcs, (int)(sizeof(g_console_funcs) / sizeof(g_console_funcs[0])) },
    { "IO.Time",         g_time_funcs,    (int)(sizeof(g_time_funcs) / sizeof(g_time_funcs[0])) },
    { "IO.Type.string",  g_string_funcs,  (int)(sizeof(g_string_funcs) / sizeof(g_string_funcs[0])) },
    { "IO.Collection.dict", g_dict_funcs, (int)(sizeof(g_dict_funcs) / sizeof(g_dict_funcs[0])) },
    { "IO.Collection.list", g_list_funcs, (int)(sizeof(g_list_funcs) / sizeof(g_list_funcs[0])) },
    { "IO.Collection.set",  g_set_funcs,  (int)(sizeof(g_set_funcs) / sizeof(g_set_funcs[0])) },
    { "IO.Type",         g_type_funcs,    (int)(sizeof(g_type_funcs) / sizeof(g_type_funcs[0])) },
    { "IO.Type.any",     g_any_funcs,     (int)(sizeof(g_any_funcs) / sizeof(g_any_funcs[0])) },
    { "IO.System",       g_system_funcs,  (int)(sizeof(g_system_funcs) / sizeof(g_system_funcs[0])) },
    { "IO.Async",        g_async_funcs,   (int)(sizeof(g_async_funcs) / sizeof(g_async_funcs[0])) },
    { "IO.Meta",         g_meta_funcs,    (int)(sizeof(g_meta_funcs) / sizeof(g_meta_funcs[0])) },
    { "IO.Path",         g_path_funcs,    (int)(sizeof(g_path_funcs) / sizeof(g_path_funcs[0])) },
    { "IO.Text",         g_text_funcs,    (int)(sizeof(g_text_funcs) / sizeof(g_text_funcs[0])) },
    { "IO.File",         g_file_funcs,    (int)(sizeof(g_file_funcs) / sizeof(g_file_funcs[0])) },
    { "IO.Directory",    g_directory_funcs, (int)(sizeof(g_directory_funcs) / sizeof(g_directory_funcs[0])) },
    { "IO.Error",        g_error_funcs,   (int)(sizeof(g_error_funcs) / sizeof(g_error_funcs[0])) },
    { "IO.GC",           g_gc_funcs,      (int)(sizeof(g_gc_funcs) / sizeof(g_gc_funcs[0])) },
    { "IO.Memory",       g_memory_funcs,  (int)(sizeof(g_memory_funcs) / sizeof(g_memory_funcs[0])) },
    { "IO.Volatile",     g_volatile_funcs, (int)(sizeof(g_volatile_funcs) / sizeof(g_volatile_funcs[0])) },
    { "IO.Panic",        g_panic_funcs,   (int)(sizeof(g_panic_funcs) / sizeof(g_panic_funcs[0])) },
    { "IO.Target",       0, 0 },
    { "IO.Target.OS",    0, 0 },
    { "IO.Target.Arch",  0, 0 },
    { "IO.Target.Env",   0, 0 },
    { "IO.Host",         0, 0 },
    { "IO.Host.OS",      0, 0 },
    { "IO.Host.Arch",    0, 0 },
    { "IO.Host.Env",     0, 0 },
    { "IO.Runtime",      0, 0 },
    { "IO.Runtime.Kind", 0, 0 },
    { "IO.Version",      0, 0 },
    { "IO.Version.VM",   0, 0 },
};

static unsigned builtin_profile_mask(KnBuiltinKind kind)
{
    switch (kind)
    {
    case KN_BUILTIN_IO_TYPE_BOOL:
    case KN_BUILTIN_IO_TYPE_BYTE:
    case KN_BUILTIN_IO_TYPE_CHAR:
    case KN_BUILTIN_IO_TYPE_INT:
    case KN_BUILTIN_IO_TYPE_FLOAT:
    case KN_BUILTIN_IO_TYPE_F16:
    case KN_BUILTIN_IO_TYPE_F32:
    case KN_BUILTIN_IO_TYPE_F64:
    case KN_BUILTIN_IO_TYPE_F128:
    case KN_BUILTIN_IO_TYPE_I8:
    case KN_BUILTIN_IO_TYPE_I16:
    case KN_BUILTIN_IO_TYPE_I32:
    case KN_BUILTIN_IO_TYPE_I64:
    case KN_BUILTIN_IO_TYPE_U8:
    case KN_BUILTIN_IO_TYPE_U16:
    case KN_BUILTIN_IO_TYPE_U32:
    case KN_BUILTIN_IO_TYPE_U64:
    case KN_BUILTIN_IO_TYPE_ISIZE:
    case KN_BUILTIN_IO_TYPE_USIZE:
    case KN_BUILTIN_IO_TYPE_STRING:
    case KN_BUILTIN_IO_TYPE_ANY:
    case KN_BUILTIN_IO_TYPE_DICT:
    case KN_BUILTIN_IO_TYPE_LIST:
    case KN_BUILTIN_IO_ANY_ISNULL:
    case KN_BUILTIN_IO_ANY_EQUALS:
    case KN_BUILTIN_IO_ANY_TAG:
    case KN_BUILTIN_IO_ANY_TYPENAME:
    case KN_BUILTIN_IO_ANY_ISNUMBER:
    case KN_BUILTIN_IO_ANY_ISPOINTER:
    case KN_BUILTIN_IO_ANY_ISINT:
    case KN_BUILTIN_IO_ANY_ISFLOAT:
    case KN_BUILTIN_IO_ANY_ISSTRING:
    case KN_BUILTIN_IO_ANY_ISBOOL:
    case KN_BUILTIN_IO_ANY_ISCHAR:
    case KN_BUILTIN_IO_ANY_ISOBJECT:
    case KN_BUILTIN_IO_STRING_LENGTH:
    case KN_BUILTIN_IO_STRING_EQUALS:
    case KN_BUILTIN_IO_MATH_ABS:
    case KN_BUILTIN_IO_MATH_MIN:
    case KN_BUILTIN_IO_MATH_MAX:
    case KN_BUILTIN_IO_MATH_CLAMP:
    case KN_BUILTIN_ARRAY_LENGTH:
    case KN_BUILTIN_ARRAY_INDEX:
    case KN_BUILTIN_STRING_INDEX:
    case KN_BUILTIN_TYPEOF:
    case KN_BUILTIN_IO_MEMORY_COPY:
    case KN_BUILTIN_IO_MEMORY_SET:
    case KN_BUILTIN_IO_MEMORY_ZERO:
    case KN_BUILTIN_IO_MEMORY_COMPARE:
    case KN_BUILTIN_IO_VOLATILE_READ8:
    case KN_BUILTIN_IO_VOLATILE_READ16:
    case KN_BUILTIN_IO_VOLATILE_READ32:
    case KN_BUILTIN_IO_VOLATILE_READ64:
    case KN_BUILTIN_IO_VOLATILE_WRITE8:
    case KN_BUILTIN_IO_VOLATILE_WRITE16:
    case KN_BUILTIN_IO_VOLATILE_WRITE32:
    case KN_BUILTIN_IO_VOLATILE_WRITE64:
    case KN_BUILTIN_IO_PANIC_ABORT:
    case KN_BUILTIN_IO_PANIC_HALT:
    case KN_BUILTIN_IO_ASYNC_RUN:
    case KN_BUILTIN_IO_ASYNC_SLEEP:
    case KN_BUILTIN_IO_ASYNC_YIELD:
        return KN_STD_MASK_HOSTED | KN_STD_MASK_FREESTANDING_CORE | KN_STD_MASK_FREESTANDING_ALLOC | KN_STD_MASK_FREESTANDING_GC;

    case KN_BUILTIN_IO_STRING_CONCAT:
        return KN_STD_MASK_HOSTED | KN_STD_MASK_FREESTANDING_ALLOC | KN_STD_MASK_FREESTANDING_GC;

    case KN_BUILTIN_IO_ANY_TOSTRING:
    case KN_BUILTIN_IO_STRING_TOCHARS:
    case KN_BUILTIN_IO_TEXT_LENGTH:
    case KN_BUILTIN_IO_TEXT_IS_EMPTY:
    case KN_BUILTIN_IO_TEXT_COUNT:
    case KN_BUILTIN_IO_TEXT_CONTAINS:
    case KN_BUILTIN_IO_TEXT_STARTS_WITH:
    case KN_BUILTIN_IO_TEXT_ENDS_WITH:
    case KN_BUILTIN_IO_TEXT_INDEX_OF:
    case KN_BUILTIN_IO_TEXT_LAST_INDEX_OF:
    case KN_BUILTIN_IO_TEXT_TRIM:
    case KN_BUILTIN_IO_TEXT_REPLACE:
    case KN_BUILTIN_IO_TEXT_TO_UPPER:
    case KN_BUILTIN_IO_TEXT_TO_LOWER:
    case KN_BUILTIN_IO_TEXT_SLICE:
        return KN_STD_MASK_HOSTED | KN_STD_MASK_FREESTANDING_ALLOC | KN_STD_MASK_FREESTANDING_GC;

    case KN_BUILTIN_IO_DICT_NEW:
    case KN_BUILTIN_IO_DICT_SET:
    case KN_BUILTIN_IO_DICT_GET:
    case KN_BUILTIN_IO_DICT_TRY_FETCH:
    case KN_BUILTIN_IO_DICT_CONTAINS:
    case KN_BUILTIN_IO_DICT_COUNT:
    case KN_BUILTIN_IO_DICT_REMOVE:
    case KN_BUILTIN_IO_DICT_IS_EMPTY:
    case KN_BUILTIN_IO_DICT_KEYS:
    case KN_BUILTIN_IO_DICT_VALUES:
    case KN_BUILTIN_IO_LIST_NEW:
    case KN_BUILTIN_IO_LIST_ADD:
    case KN_BUILTIN_IO_LIST_GET:
    case KN_BUILTIN_IO_LIST_SET:
    case KN_BUILTIN_IO_LIST_INSERT:
    case KN_BUILTIN_IO_LIST_TRY_FETCH:
    case KN_BUILTIN_IO_LIST_COUNT:
    case KN_BUILTIN_IO_LIST_CLEAR:
    case KN_BUILTIN_IO_LIST_REMOVE_AT:
    case KN_BUILTIN_IO_LIST_CONTAINS:
    case KN_BUILTIN_IO_LIST_POP:
    case KN_BUILTIN_IO_LIST_PEEK:
    case KN_BUILTIN_IO_LIST_INDEX_OF:
    case KN_BUILTIN_IO_LIST_IS_EMPTY:
    case KN_BUILTIN_IO_SET_NEW:
    case KN_BUILTIN_IO_SET_ADD:
    case KN_BUILTIN_IO_SET_CONTAINS:
    case KN_BUILTIN_IO_SET_COUNT:
    case KN_BUILTIN_IO_SET_REMOVE:
    case KN_BUILTIN_IO_SET_CLEAR:
    case KN_BUILTIN_IO_SET_IS_EMPTY:
    case KN_BUILTIN_IO_SET_VALUES:
    case KN_BUILTIN_ARRAY_ADD:
    case KN_BUILTIN_IO_TEXT_SPLIT:
    case KN_BUILTIN_IO_TEXT_JOIN:
    case KN_BUILTIN_IO_TEXT_LINES:
    case KN_BUILTIN_BLOCK_RUN:
    case KN_BUILTIN_BLOCK_JUMP:
    case KN_BUILTIN_BLOCK_RUN_UNTIL:
    case KN_BUILTIN_BLOCK_JUMP_AND_RUN_UNTIL:
    case KN_BUILTIN_IO_ERROR_LASTTRACE:
    case KN_BUILTIN_IO_ERROR_HAS:
    case KN_BUILTIN_IO_ERROR_CLEAR:
    case KN_BUILTIN_IO_ERROR_CURRENT:
    case KN_BUILTIN_IO_GC_COLLECT:
    case KN_BUILTIN_IO_META_QUERY:
    case KN_BUILTIN_IO_META_OF:
    case KN_BUILTIN_IO_META_HAS:
    case KN_BUILTIN_IO_META_FIND:
        return KN_STD_MASK_HOSTED | KN_STD_MASK_FREESTANDING_GC;

    case KN_BUILTIN_IO_CONSOLE_PRINTLINE:
    case KN_BUILTIN_IO_CONSOLE_PRINT:
    case KN_BUILTIN_IO_CONSOLE_READLINE:
    case KN_BUILTIN_IO_CONSOLE_WRITE:
    case KN_BUILTIN_IO_CONSOLE_WRITELINE:
    case KN_BUILTIN_IO_CONSOLE_PRINTINT:
    case KN_BUILTIN_IO_CONSOLE_PRINTBOOL:
    case KN_BUILTIN_IO_CONSOLE_PRINTCHAR:
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEINT:
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEBOOL:
    case KN_BUILTIN_IO_CONSOLE_PRINTLINECHAR:
    case KN_BUILTIN_IO_CONSOLE_WRITEINT:
    case KN_BUILTIN_IO_CONSOLE_WRITEBOOL:
    case KN_BUILTIN_IO_CONSOLE_WRITECHAR:
    case KN_BUILTIN_IO_TIME_NOW:
    case KN_BUILTIN_IO_TIME_GETTICK:
    case KN_BUILTIN_IO_TIME_SLEEP:
    case KN_BUILTIN_IO_TIME_NANOTICK:
    case KN_BUILTIN_IO_TIME_FORMAT:
    case KN_BUILTIN_IO_SYSTEM_EXIT:
    case KN_BUILTIN_IO_SYSTEM_COMMANDLINE:
    case KN_BUILTIN_IO_SYSTEM_FILE_EXISTS:
    case KN_BUILTIN_IO_SYSTEM_EXEC:
    case KN_BUILTIN_IO_SYSTEM_LOAD_LIBRARY:
    case KN_BUILTIN_IO_SYSTEM_GET_SYMBOL:
    case KN_BUILTIN_IO_SYSTEM_CLOSE_LIBRARY:
    case KN_BUILTIN_IO_SYSTEM_LAST_ERROR:
    case KN_BUILTIN_IO_ASYNC_SPAWN:
    case KN_BUILTIN_IO_ASYNC_IS_COMPLETED:
    case KN_BUILTIN_IO_ASYNC_WAIT:
    case KN_BUILTIN_IO_ASYNC_EXIT_CODE:
    case KN_BUILTIN_IO_ASYNC_CLOSE:
    case KN_BUILTIN_IO_PATH_COMBINE:
    case KN_BUILTIN_IO_PATH_JOIN3:
    case KN_BUILTIN_IO_PATH_JOIN4:
    case KN_BUILTIN_IO_PATH_FILE_NAME:
    case KN_BUILTIN_IO_PATH_EXTENSION:
    case KN_BUILTIN_IO_PATH_DIRECTORY:
    case KN_BUILTIN_IO_PATH_NORMALIZE:
    case KN_BUILTIN_IO_PATH_CWD:
    case KN_BUILTIN_IO_PATH_CHANGE_EXTENSION:
    case KN_BUILTIN_IO_PATH_HAS_EXTENSION:
    case KN_BUILTIN_IO_PATH_IS_ABSOLUTE:
    case KN_BUILTIN_IO_PATH_IS_RELATIVE:
    case KN_BUILTIN_IO_PATH_BASE_NAME:
    case KN_BUILTIN_IO_PATH_ENSURE_TRAILING_SEPARATOR:
    case KN_BUILTIN_IO_FILE_CREATE:
    case KN_BUILTIN_IO_FILE_TOUCH:
    case KN_BUILTIN_IO_FILE_READ_ALL_TEXT:
    case KN_BUILTIN_IO_FILE_READ_FIRST_LINE:
    case KN_BUILTIN_IO_FILE_WRITE_ALL_TEXT:
    case KN_BUILTIN_IO_FILE_APPEND_ALL_TEXT:
    case KN_BUILTIN_IO_FILE_APPEND_LINE:
    case KN_BUILTIN_IO_FILE_DELETE:
    case KN_BUILTIN_IO_FILE_DELETE_IF_EXISTS:
    case KN_BUILTIN_IO_FILE_SIZE:
    case KN_BUILTIN_IO_FILE_IS_EMPTY:
    case KN_BUILTIN_IO_FILE_COPY:
    case KN_BUILTIN_IO_FILE_MOVE:
    case KN_BUILTIN_IO_FILE_READ_OR_DEFAULT:
    case KN_BUILTIN_IO_FILE_READ_IF_EXISTS:
    case KN_BUILTIN_IO_FILE_READ_OR:
    case KN_BUILTIN_IO_FILE_READ_LINES:
    case KN_BUILTIN_IO_FILE_WRITE_LINES:
    case KN_BUILTIN_IO_FILE_REPLACE_TEXT:
    case KN_BUILTIN_IO_DIRECTORY_EXISTS:
    case KN_BUILTIN_IO_DIRECTORY_CREATE:
    case KN_BUILTIN_IO_DIRECTORY_ENSURE:
    case KN_BUILTIN_IO_DIRECTORY_DELETE:
    case KN_BUILTIN_IO_DIRECTORY_DELETE_IF_EXISTS:
        return KN_STD_MASK_HOSTED;

    case KN_BUILTIN_NONE:
    default:
        return KN_STD_MASK_HOSTED;
    }
}

static bool module_has_visible_func(const KnStdModule *m)
{
    if (!m) return false;
    if (m->count == 0)
        return true;
    for (int i = 0; i < m->count; i++)
    {
        if (kn_std_builtin_allowed(m->funcs[i].kind))
            return true;
    }
    return false;
}

int kn_std_module_count(void)
{
    int count = 0;
    for (size_t i = 0; i < sizeof(g_modules) / sizeof(g_modules[0]); i++)
        if (module_has_visible_func(&g_modules[i]))
            count++;
    return count;
}

const KnStdModule *kn_std_module_at(int index)
{
    if (index < 0)
        return 0;
    int visible = 0;
    for (size_t i = 0; i < sizeof(g_modules) / sizeof(g_modules[0]); i++)
    {
        if (!module_has_visible_func(&g_modules[i]))
            continue;
        if (visible == index)
            return &g_modules[i];
        visible++;
    }
    return 0;
}

const KnStdFunc *kn_std_find(const char *module, const char *name)
{
    if (!module || !name) return 0;
    for (size_t m = 0; m < sizeof(g_modules) / sizeof(g_modules[0]); m++)
    {
        if (kn_strcmp(g_modules[m].name, module) != 0)
            continue;
        for (int i = 0; i < g_modules[m].count; i++)
        {
            const KnStdFunc *f = &g_modules[m].funcs[i];
            if (kn_strcmp(f->name, name) == 0 && kn_std_builtin_allowed(f->kind))
                return f;
        }
    }
    return 0;
}

const KnStdFunc *kn_std_find_by_kind(KnBuiltinKind kind)
{
    for (size_t m = 0; m < sizeof(g_modules) / sizeof(g_modules[0]); m++)
    {
        for (int i = 0; i < g_modules[m].count; i++)
        {
            const KnStdFunc *f = &g_modules[m].funcs[i];
            if (f->kind == kind && kn_std_builtin_allowed(f->kind))
                return f;
        }
    }
    return 0;
}

bool kn_std_module_has(const char *module)
{
    if (!module) return false;
    for (size_t m = 0; m < sizeof(g_modules) / sizeof(g_modules[0]); m++)
    {
        if (kn_strcmp(g_modules[m].name, module) == 0 && module_has_visible_func(&g_modules[m]))
            return true;
    }
    return false;
}

void kn_std_set_profile(int profile)
{
    if (profile < KN_STD_PROFILE_HOSTED || profile > KN_STD_PROFILE_FREESTANDING_GC)
        profile = KN_STD_PROFILE_HOSTED;
    g_std_profile = profile;
}

int kn_std_get_profile(void)
{
    return g_std_profile;
}

bool kn_std_builtin_allowed(KnBuiltinKind kind)
{
    unsigned mask = builtin_profile_mask(kind);
    if (g_std_profile < KN_STD_PROFILE_HOSTED || g_std_profile > KN_STD_PROFILE_FREESTANDING_GC)
        return (mask & KN_STD_MASK_HOSTED) != 0;
    return (mask & (1u << g_std_profile)) != 0;
}
