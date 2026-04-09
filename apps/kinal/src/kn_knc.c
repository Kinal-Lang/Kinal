#include "kn/knc.h"
#include "kn/diag.h"
#include "kn/lexer.h"
#include "kn/std.h"
#include "kn/util.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
    KNC_VALUE_NULL = 0,
    KNC_VALUE_INT = 1,
    KNC_VALUE_FLOAT = 2,
    KNC_VALUE_BOOL = 3,
    KNC_VALUE_STRING = 4,
    KNC_VALUE_CHAR = 5,
    KNC_VALUE_ARRAY = 6,
    KNC_VALUE_OBJECT = 7,
    KNC_VALUE_POINTER = 8
} KncValueKind;

typedef enum
{
    KNC_OP_LOAD_INT = 0,
    KNC_OP_LOAD_FLOAT = 1,
    KNC_OP_LOAD_BOOL = 2,
    KNC_OP_LOAD_STRING = 3,
    KNC_OP_LOAD_CHAR = 4,
    KNC_OP_MOVE = 5,
    KNC_OP_ADD_INT = 6,
    KNC_OP_SUB_INT = 7,
    KNC_OP_MUL_INT = 8,
    KNC_OP_DIV_INT = 9,
    KNC_OP_ADD_FLOAT = 10,
    KNC_OP_SUB_FLOAT = 11,
    KNC_OP_MUL_FLOAT = 12,
    KNC_OP_DIV_FLOAT = 13,
    KNC_OP_NEG_INT = 14,
    KNC_OP_NEG_FLOAT = 15,
    KNC_OP_EQ = 16,
    KNC_OP_NE = 17,
    KNC_OP_LT_INT = 18,
    KNC_OP_LE_INT = 19,
    KNC_OP_GT_INT = 20,
    KNC_OP_GE_INT = 21,
    KNC_OP_LT_FLOAT = 22,
    KNC_OP_LE_FLOAT = 23,
    KNC_OP_GT_FLOAT = 24,
    KNC_OP_GE_FLOAT = 25,
    KNC_OP_NOT_BOOL = 26,
    KNC_OP_BITAND_INT = 27,
    KNC_OP_BITOR_INT = 28,
    KNC_OP_BITXOR_INT = 29,
    KNC_OP_SHL_INT = 30,
    KNC_OP_SHR_INT = 31,
    KNC_OP_BITNOT_INT = 32,
    KNC_OP_INT_TO_STRING = 33,
    KNC_OP_BOOL_TO_STRING = 34,
    KNC_OP_CHAR_TO_STRING = 35,
    KNC_OP_STRING_TO_INT = 36,
    KNC_OP_STRING_TO_BOOL = 37,
    KNC_OP_CHAR_TO_INT = 38,
    KNC_OP_INT_TO_CHAR = 39,
    KNC_OP_STRING_TO_CHAR = 40,
    KNC_OP_JUMP = 41,
    KNC_OP_JUMP_IF_FALSE = 42,
    KNC_OP_JUMP_IF_TRUE = 43,
    KNC_OP_CALL_BUILTIN = 44,
    KNC_OP_CALL_FUNC = 45,
    KNC_OP_NEW_OBJECT = 46,
    KNC_OP_NEW_ARRAY = 47,
    KNC_OP_LOAD_FIELD = 48,
    KNC_OP_STORE_FIELD = 49,
    KNC_OP_LOAD_INDEX = 50,
    KNC_OP_STORE_INDEX = 51,
    KNC_OP_ARRAY_LENGTH = 52,
    KNC_OP_ARRAY_ADD = 53,
    KNC_OP_CALL_VIRT = 54,
    KNC_OP_IS_TYPE = 55,
    KNC_OP_LOAD_GLOBAL = 56,
    KNC_OP_STORE_GLOBAL = 57,
    KNC_OP_RET = 58,
    KNC_OP_HALT = 59,
    KNC_OP_MAKE_FUNC = 60,
    KNC_OP_CALL_CLOSURE = 61,
    KNC_OP_NEW_CELL = 62,
    KNC_OP_LOAD_CELL = 63,
    KNC_OP_STORE_CELL = 64,
    KNC_OP_LOAD_CAPTURE = 65,
    KNC_OP_STORE_CAPTURE = 66,
    KNC_OP_LOAD_CAPTURE_CELL = 67,
    KNC_OP_PUSH_TRY = 68,
    KNC_OP_POP_TRY = 69,
    KNC_OP_THROW = 70,
    KNC_OP_MAKE_PTR_LOCAL = 71,
    KNC_OP_MAKE_PTR_GLOBAL = 72,
    KNC_OP_MAKE_PTR_INDEX = 73,
    KNC_OP_MAKE_PTR_FIELD = 74,
    KNC_OP_MAKE_PTR_FUNC = 75,
    KNC_OP_MAKE_PTR_CELL = 76,
    KNC_OP_LOAD_PTR = 77,
    KNC_OP_STORE_PTR = 78,
    KNC_OP_ADD_PTR = 79,
    KNC_OP_LOOP_INT_LT_INC = 80,
    KNC_OP_LOOP_INT_LE_INC = 81,
    KNC_OP_LOOP_INT_GT_DEC = 82,
    KNC_OP_LOOP_INT_GE_DEC = 83,
    KNC_OP_LOOP_INT_EQ_INC = 84,
    KNC_OP_LOOP_INT_NE_INC = 85,
    KNC_OP_LOOP_INT_EQ_DEC = 86,
    KNC_OP_LOOP_INT_NE_DEC = 87,
    KNC_OP_JUMP_IF_INT_LT_FALSE = 88,
    KNC_OP_JUMP_IF_INT_LE_FALSE = 89,
    KNC_OP_JUMP_IF_INT_GT_FALSE = 90,
    KNC_OP_JUMP_IF_INT_GE_FALSE = 91,
    KNC_OP_JUMP_IF_INT_EQ_FALSE = 92,
    KNC_OP_JUMP_IF_INT_NE_FALSE = 93,
    KNC_OP_INC_INT = 94,
    KNC_OP_DEC_INT = 95,
    KNC_OP_INT_TO_FLOAT = 96,
    KNC_OP_FLOAT_TO_INT = 97,
    KNC_OP_FLOAT_TO_STRING = 98,
    KNC_OP_STRING_TO_FLOAT = 99,
    KNC_OP_BOOL_TO_INT = 100,
    KNC_OP_BOOL_TO_FLOAT = 101,
    KNC_OP_FLOAT_TO_BOOL = 102,
    KNC_OP_LOOP_BACK_INT_LT_INC = 103,
    KNC_OP_LOOP_BACK_INT_LE_INC = 104,
    KNC_OP_LOOP_BACK_INT_GT_DEC = 105,
    KNC_OP_LOOP_BACK_INT_GE_DEC = 106,
    KNC_OP_LOOP_BACK_INT_EQ_INC = 107,
    KNC_OP_LOOP_BACK_INT_NE_INC = 108,
    KNC_OP_LOOP_BACK_INT_EQ_DEC = 109,
    KNC_OP_LOOP_BACK_INT_NE_DEC = 110,
    KNC_OP_LOOP_ARRAY_FILL_INT_LT_INC = 111,
    KNC_OP_LOOP_ARRAY_FILL_INT_LE_INC = 112,
    KNC_OP_LOOP_ARRAY_SUM_INT_LT_INC = 113,
    KNC_OP_LOOP_ARRAY_SUM_INT_LE_INC = 114,
    KNC_OP_LOOP_SUM_INT_LT_INC = 115,
    KNC_OP_LOOP_SUM_INT_LE_INC = 116,
    KNC_OP_ADD_INT_IMM = 117,
    KNC_OP_SUB_INT_IMM = 118,
    KNC_OP_MUL_INT_IMM = 119,
    KNC_OP_DIV_INT_IMM = 120,
    KNC_OP_LT_INT_IMM = 121,
    KNC_OP_LE_INT_IMM = 122,
    KNC_OP_GT_INT_IMM = 123,
    KNC_OP_GE_INT_IMM = 124,
    KNC_OP_BITAND_INT_IMM = 125,
    KNC_OP_BITOR_INT_IMM = 126,
    KNC_OP_BITXOR_INT_IMM = 127,
    KNC_OP_SHL_INT_IMM = 128,
    KNC_OP_SHR_INT_IMM = 129
} KncOpCode;

typedef enum
{
    KNC_TYPE_CLASS = 1,
    KNC_TYPE_STRUCT = 2,
    KNC_TYPE_INTERFACE = 3
} KncTypeKind;

typedef enum
{
    KNC_IS_TARGET_ANY = 0,
    KNC_IS_TARGET_BOOL = 1,
    KNC_IS_TARGET_CHAR = 2,
    KNC_IS_TARGET_INT = 3,
    KNC_IS_TARGET_FLOAT = 4,
    KNC_IS_TARGET_STRING = 5,
    KNC_IS_TARGET_TYPE = 6
} KncIsTargetKind;

typedef enum
{
    KNC_CALLABLE_FUNC = 0,
    KNC_CALLABLE_METHOD = 1
} KncCallableKind;

typedef struct
{
    int *items;
    int count;
    int cap;
} IntBuf;

typedef struct
{
    double *items;
    int count;
    int cap;
} FloatBuf;

typedef struct
{
    const char **items;
    int count;
    int cap;
} StrBuf;

typedef struct
{
    unsigned char *items;
    int count;
    int cap;
} ByteBuf;

typedef struct
{
    int *items;
    int count;
    int cap;
} PatchBuf;

typedef struct
{
    const char *name;
    int reg;
    Type type;
    int storage;
    int capture_slot;
} KncLocal;

typedef enum
{
    KNC_LOCAL_VALUE = 0,
    KNC_LOCAL_CELL = 1,
    KNC_LOCAL_CAPTURE = 2
} KncLocalStorage;

typedef struct
{
    KncLocal *items;
    int count;
    int cap;
} LocalBuf;

typedef struct
{
    int continue_ip;
    PatchBuf continue_patches;
    PatchBuf break_patches;
} LoopState;

typedef struct
{
    LoopState *items;
    int count;
    int cap;
} LoopBuf;

typedef struct
{
    const char *qname;
    int kind;
    int field_count;
    int base_type_index;
    int *vtable;
    int vtable_count;
    int vtable_cap;
    int *interface_types;
    int interface_count;
    int interface_cap;
    int *direct_methods;
    int direct_method_count;
    const ClassDecl *class_decl;
    const StructDecl *struct_decl;
    const InterfaceDecl *interface_decl;
} KncTypeRecord;

typedef struct
{
    KncTypeRecord *items;
    int count;
    int cap;
} TypeBuf;

typedef struct
{
    const char *name;
    Type type;
    int slot;
} KncGlobalRecord;

typedef struct
{
    KncGlobalRecord *items;
    int count;
    int cap;
} GlobalBuf;

typedef struct
{
    const char *owner_qname;
    int field_index;
    Type type;
    int slot;
} KncStaticFieldRecord;

typedef struct
{
    KncStaticFieldRecord *items;
    int count;
    int cap;
} StaticFieldBuf;

typedef struct
{
    const char *name;
    Type type;
    int slot;
} KncCaptureRecord;

typedef struct
{
    KncCaptureRecord *items;
    int count;
    int cap;
} CaptureBuf;

typedef struct
{
    int callable_kind;
    const Func *func;
    const Method *method;
    const ClassDecl *owner_class;
    const StructDecl *owner_struct;
    int method_index;
    int is_constructor;
    int is_instance;
    const char *debug_name;
    int index;
    int built;
    int param_count;
    int reg_count;
    int return_kind;
    Param *synthetic_params;
    int synthetic_param_count;
    Stmt *synthetic_body;
    Type synthetic_ret_type;
    const KnSource *synthetic_src;
    int synthetic_id;
    int synthetic_is_block;
    BlockRecordList *synthetic_block_records;
    CaptureBuf captures;
    ByteBuf code;
} KncFuncRecord;

typedef struct
{
    KncFuncRecord *items;
    int count;
    int cap;
    IntBuf ints;
    FloatBuf floats;
    StrBuf strings;
    TypeBuf types;
    GlobalBuf globals;
    StaticFieldBuf static_fields;
    int entry_index;
    int main_entry_index;
    int enable_superloop;
    const KnSource *fallback_src;
    ClassList *classes;
    InterfaceList *interfaces;
    StructList *structs;
} KncProgram;

typedef struct
{
    int reg;
    Type type;
} KncValue;

typedef struct KncFuncState KncFuncState;
static KncValue compile_expr(KncFuncState *st, Expr *e);

struct KncFuncState
{
    KncProgram *program;
    const Func *func;
    const KnSource *src;
    int func_index;
    ByteBuf *code;
    LocalBuf locals;
    LoopBuf loops;
    Type self_type;
    int has_this;
    int next_reg;
    int max_reg;
    int in_runtime_block;
    int block_start_reg;
    int block_until_reg;
    int block_body_start_ip;
    BlockRecordList *block_records;
    PatchBuf block_start_patches;
    PatchBuf *block_record_patches;
    int *block_record_addrs;
};

static void intbuf_push(IntBuf *buf, int value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        int *next = (int *)kn_realloc(buf->items, sizeof(int) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void strbuf_push(StrBuf *buf, const char *value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        const char **next = (const char **)kn_realloc(buf->items, sizeof(char *) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void floatbuf_push(FloatBuf *buf, double value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        double *next = (double *)kn_realloc(buf->items, sizeof(double) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void bytebuf_push(ByteBuf *buf, unsigned char value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 64;
        unsigned char *next = (unsigned char *)kn_realloc(buf->items, (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void patchbuf_push(PatchBuf *buf, int value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        int *next = (int *)kn_realloc(buf->items, sizeof(int) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void localbuf_push(LocalBuf *buf, KncLocal value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 16;
        KncLocal *next = (KncLocal *)kn_realloc(buf->items, sizeof(KncLocal) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void loopbuf_push(LoopBuf *buf, LoopState value)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        LoopState *next = (LoopState *)kn_realloc(buf->items, sizeof(LoopState) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = value;
}

static void funcbuf_push(KncProgram *program, KncFuncRecord record)
{
    if (program->count + 1 > program->cap)
    {
        int new_cap = program->cap ? program->cap * 2 : 16;
        KncFuncRecord *next = (KncFuncRecord *)kn_realloc(program->items, sizeof(KncFuncRecord) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        program->items = next;
        program->cap = new_cap;
    }
    program->items[program->count++] = record;
}

static void typebuf_push(TypeBuf *buf, KncTypeRecord record)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        KncTypeRecord *next = (KncTypeRecord *)kn_realloc(buf->items, sizeof(KncTypeRecord) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = record;
}

static void globalbuf_push(GlobalBuf *buf, KncGlobalRecord record)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        KncGlobalRecord *next = (KncGlobalRecord *)kn_realloc(buf->items, sizeof(KncGlobalRecord) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = record;
}

static void staticfieldbuf_push(StaticFieldBuf *buf, KncStaticFieldRecord record)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        KncStaticFieldRecord *next = (KncStaticFieldRecord *)kn_realloc(buf->items, sizeof(KncStaticFieldRecord) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = record;
}

static void capturebuf_push(CaptureBuf *buf, KncCaptureRecord record)
{
    if (buf->count + 1 > buf->cap)
    {
        int new_cap = buf->cap ? buf->cap * 2 : 8;
        KncCaptureRecord *next = (KncCaptureRecord *)kn_realloc(buf->items, sizeof(KncCaptureRecord) * (size_t)new_cap);
        if (!next)
            kn_die("out of memory");
        buf->items = next;
        buf->cap = new_cap;
    }
    buf->items[buf->count++] = record;
}

static void int_array_ensure(int **items, int *cap, int needed)
{
    if (needed <= *cap)
        return;
    {
        int new_cap = *cap ? *cap : 8;
        while (new_cap < needed)
            new_cap *= 2;
        {
            int *next = (int *)kn_realloc(*items, sizeof(int) * (size_t)new_cap);
            if (!next)
                kn_die("out of memory");
            *items = next;
            *cap = new_cap;
        }
    }
}

static int program_add_int(KncProgram *program, int value);

static int type_is_integerish(Type t)
{
    switch (t.kind)
    {
    case TY_BYTE:
    case TY_CHAR:
    case TY_INT:
    case TY_I8:
    case TY_I16:
    case TY_I32:
    case TY_I64:
    case TY_U8:
    case TY_U16:
    case TY_U32:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE:
    case TY_ENUM:
        return 1;
    default:
        return 0;
    }
}

static int type_is_floatish(Type t)
{
    switch (t.kind)
    {
    case TY_FLOAT:
    case TY_F16:
    case TY_F32:
    case TY_F64:
    case TY_F128:
        return 1;
    default:
        return 0;
    }
}

static int expr_int_immediate_index(KncFuncState *st, Expr *e, int *index)
{
    if (!e || !index)
        return 0;
    switch (e->kind)
    {
    case EXPR_INT:
        *index = program_add_int(st->program, (int)e->v.int_val);
        return 1;
    case EXPR_CHAR:
        *index = program_add_int(st->program, (int)e->v.int_val);
        return 1;
    default:
        return 0;
    }
}

static int type_is_vm_value(Type t)
{
    if (type_is_integerish(t))
        return 1;
    if (type_is_floatish(t))
        return 1;
    switch (t.kind)
    {
    case TY_BOOL:
    case TY_STRING:
    case TY_ARRAY:
    case TY_PTR:
    case TY_CLASS:
    case TY_STRUCT:
    case TY_ANY:
    case TY_NULL:
    case TY_VOID:
        return 1;
    default:
        return 0;
    }
}

static int value_kind_from_type(Type t)
{
    if (t.kind == TY_CHAR)
        return KNC_VALUE_CHAR;
    if (type_is_integerish(t))
        return KNC_VALUE_INT;
    if (type_is_floatish(t))
        return KNC_VALUE_FLOAT;
    switch (t.kind)
    {
    case TY_BOOL:   return KNC_VALUE_BOOL;
    case TY_STRING: return KNC_VALUE_STRING;
    case TY_ARRAY:  return KNC_VALUE_ARRAY;
    case TY_PTR:    return KNC_VALUE_POINTER;
    case TY_CLASS:
    case TY_STRUCT: return KNC_VALUE_OBJECT;
    default:        return KNC_VALUE_NULL;
    }
}

static char *knc_dup_text(const char *s)
{
    size_t n;
    char *out;
    if (!s)
        s = "";
    n = kn_strlen(s);
    out = (char *)kn_malloc(n + 1);
    if (!out)
        kn_die("out of memory");
    kn_memcpy(out, s, n);
    out[n] = 0;
    return out;
}

static const char *knc_base_type_name(TypeKind k)
{
    switch (k)
    {
    case TY_VOID: return "void";
    case TY_BOOL: return "bool";
    case TY_BYTE: return "byte";
    case TY_CHAR: return "char";
    case TY_INT: return "int";
    case TY_FLOAT: return "float";
    case TY_F16: return "f16";
    case TY_F32: return "f32";
    case TY_F64: return "f64";
    case TY_F128: return "f128";
    case TY_I8: return "i8";
    case TY_I16: return "i16";
    case TY_I32: return "i32";
    case TY_I64: return "i64";
    case TY_U8: return "u8";
    case TY_U16: return "u16";
    case TY_U32: return "u32";
    case TY_U64: return "u64";
    case TY_ISIZE: return "isize";
    case TY_USIZE: return "usize";
    case TY_STRING: return "string";
    case TY_ANY: return "any";
    case TY_NULL: return "null";
    default: return "unknown";
    }
}

static void knc_append_type_text(char *buf, size_t cap, size_t *off, Type t)
{
    if (t.kind == TY_ARRAY)
    {
        Type elem = type_make(t.elem);
        elem.name = t.name;
        knc_append_type_text(buf, cap, off, elem);
        kn_append(buf, cap, off, "[]");
        return;
    }
    if (t.kind == TY_PTR)
    {
        Type base = type_make(t.elem);
        base.name = t.name;
        knc_append_type_text(buf, cap, off, base);
        for (int i = 0; i < t.ptr_depth; i++)
            kn_append(buf, cap, off, "*");
        return;
    }
    if ((t.kind == TY_CLASS || t.kind == TY_STRUCT || t.kind == TY_ENUM) && t.name)
    {
        kn_append(buf, cap, off, t.name);
        return;
    }
    kn_append(buf, cap, off, knc_base_type_name(t.kind));
}

static char *knc_type_text(Type t)
{
    char buf[256];
    size_t off = 0;
    kn_memset(buf, 0, sizeof(buf));
    knc_append_type_text(buf, sizeof(buf), &off, t);
    return knc_dup_text(buf);
}

static int knc_type_equal(Type a, Type b)
{
    if (a.kind != b.kind) return 0;
    if (a.kind == TY_ARRAY)
    {
        if (a.elem != b.elem || a.array_len != b.array_len) return 0;
        if ((a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM) && a.name && b.name)
            return kn_strcmp(a.name, b.name) == 0;
        return a.elem == b.elem;
    }
    if (a.kind == TY_PTR)
    {
        if (a.elem != b.elem) return 0;
        if (a.ptr_depth != b.ptr_depth) return 0;
        if ((a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM) && a.name && b.name)
            return kn_strcmp(a.name, b.name) == 0;
        return 1;
    }
    if (a.kind == TY_CLASS || a.kind == TY_STRUCT || a.kind == TY_ENUM)
        return a.name && b.name && kn_strcmp(a.name, b.name) == 0;
    return 1;
}

static void knc_diag(const KnSource *src, int line, int col, const char *detail)
{
    kn_diag_report(src, KN_STAGE_CODEGEN, line, col, 1,
                   "Unsupported KNC Feature", detail, 0);
}

static void knc_diag_expr(KncFuncState *st, Expr *e, const char *detail)
{
    knc_diag(st && st->src ? st->src : 0, e ? e->line : 1, e ? e->col : 1, detail);
}

static void knc_diag_stmt(KncFuncState *st, Stmt *s, const char *detail)
{
    knc_diag(st && st->src ? st->src : 0, s ? s->line : 1, s ? s->col : 1, detail);
}

static int program_add_int(KncProgram *program, int value)
{
    for (int i = 0; i < program->ints.count; i++)
        if (program->ints.items[i] == value)
            return i;
    intbuf_push(&program->ints, value);
    return program->ints.count - 1;
}

static int program_add_float(KncProgram *program, double value)
{
    for (int i = 0; i < program->floats.count; i++)
        if (program->floats.items[i] == value)
            return i;
    floatbuf_push(&program->floats, value);
    return program->floats.count - 1;
}

static int program_add_string(KncProgram *program, const char *value)
{
    for (int i = 0; i < program->strings.count; i++)
        if (kn_strcmp(program->strings.items[i], value) == 0)
            return i;
    strbuf_push(&program->strings, value);
    return program->strings.count - 1;
}

static const char *class_qname_or_name(const ClassDecl *c)
{
    return c ? (c->qname ? c->qname : c->name) : 0;
}

static const char *build_method_debug_name(const ClassDecl *c, const char *method_name)
{
    const char *owner = class_qname_or_name(c);
    size_t owner_len;
    size_t method_len;
    char *out;

    if (!owner || !method_name)
        return method_name ? method_name : owner;

    owner_len = kn_strlen(owner);
    method_len = kn_strlen(method_name);
    out = (char *)kn_malloc(owner_len + 1 + method_len + 1);
    if (!out)
        return method_name;
    kn_memcpy(out, owner, owner_len);
    out[owner_len] = '.';
    kn_memcpy(out + owner_len + 1, method_name, method_len);
    out[owner_len + 1 + method_len] = 0;
    return out;
}

static const char *struct_qname_or_name(const StructDecl *s)
{
    return s ? (s->qname ? s->qname : s->name) : 0;
}

static const char *interface_qname_or_name(const InterfaceDecl *it)
{
    return it ? (it->qname ? it->qname : it->name) : 0;
}

static ClassDecl *program_find_class(KncProgram *program, const char *qname)
{
    if (!program || !program->classes || !qname)
        return 0;
    for (int i = 0; i < program->classes->count; i++)
    {
        ClassDecl *c = &program->classes->items[i];
        const char *name = class_qname_or_name(c);
        if (name && kn_strcmp(name, qname) == 0)
            return c;
    }
    return 0;
}

static StructDecl *program_find_struct(KncProgram *program, const char *qname)
{
    if (!program || !program->structs || !qname)
        return 0;
    for (int i = 0; i < program->structs->count; i++)
    {
        StructDecl *s = &program->structs->items[i];
        const char *name = struct_qname_or_name(s);
        if (name && kn_strcmp(name, qname) == 0)
            return s;
    }
    return 0;
}

static InterfaceDecl *program_find_interface(KncProgram *program, const char *qname)
{
    if (!program || !program->interfaces || !qname)
        return 0;
    for (int i = 0; i < program->interfaces->count; i++)
    {
        InterfaceDecl *it = &program->interfaces->items[i];
        const char *name = interface_qname_or_name(it);
        if (name && kn_strcmp(name, qname) == 0)
            return it;
    }
    return 0;
}

static int program_find_type(KncProgram *program, const char *qname)
{
    if (!program || !qname)
        return -1;
    for (int i = 0; i < program->types.count; i++)
    {
        if (program->types.items[i].qname && kn_strcmp(program->types.items[i].qname, qname) == 0)
            return i;
    }
    return -1;
}

static int program_find_global_slot(KncProgram *program, const char *name)
{
    if (!program || !name)
        return -1;
    for (int i = 0; i < program->globals.count; i++)
        if (program->globals.items[i].name && kn_strcmp(program->globals.items[i].name, name) == 0)
            return program->globals.items[i].slot;
    return -1;
}

static Type program_find_global_type(KncProgram *program, const char *name)
{
    if (!program || !name)
        return type_make(TY_UNKNOWN);
    for (int i = 0; i < program->globals.count; i++)
        if (program->globals.items[i].name && kn_strcmp(program->globals.items[i].name, name) == 0)
            return program->globals.items[i].type;
    return type_make(TY_UNKNOWN);
}

static int program_add_global(KncProgram *program, const char *name, Type type)
{
    KncGlobalRecord rec;
    int existing = program_find_global_slot(program, name);
    if (existing >= 0)
        return existing;
    rec.name = name;
    rec.type = type;
    rec.slot = program->globals.count;
    globalbuf_push(&program->globals, rec);
    return rec.slot;
}

static int program_find_static_field_slot(KncProgram *program, const char *owner_qname, int field_index)
{
    if (!program || !owner_qname || field_index < 0)
        return -1;
    for (int i = 0; i < program->static_fields.count; i++)
    {
        KncStaticFieldRecord *rec = &program->static_fields.items[i];
        if (rec->field_index == field_index &&
            rec->owner_qname &&
            kn_strcmp(rec->owner_qname, owner_qname) == 0)
            return rec->slot;
    }
    return -1;
}

static int program_add_static_field(KncProgram *program, const char *owner_qname, int field_index, Type type)
{
    KncStaticFieldRecord rec;
    int existing = program_find_static_field_slot(program, owner_qname, field_index);
    if (existing >= 0)
        return existing;
    rec.owner_qname = owner_qname;
    rec.field_index = field_index;
    rec.type = type;
    rec.slot = program->globals.count;
    globalbuf_push(&program->globals, (KncGlobalRecord){ owner_qname, type, rec.slot });
    staticfieldbuf_push(&program->static_fields, rec);
    return rec.slot;
}

static void int_array_push_unique(int **items, int *count, int *cap, int value)
{
    for (int i = 0; i < *count; i++)
        if ((*items)[i] == value)
            return;
    int_array_ensure(items, cap, *count + 1);
    (*items)[(*count)++] = value;
}

static int class_total_field_count(KncProgram *program, const ClassDecl *c)
{
    int count = 0;
    if (!c)
        return 0;
    if (c->base)
    {
        ClassDecl *base = program_find_class(program, c->base);
        count += class_total_field_count(program, base);
    }
    count += c->fields.count;
    return count;
}

static int struct_total_field_count(const StructDecl *s)
{
    return s ? s->fields.count : 0;
}

static int class_find_field_index(KncProgram *program, const ClassDecl *c, const char *name, Type *out_type)
{
    int base_fields = 0;
    if (!c || !name)
        return -1;
    if (c->base)
    {
        ClassDecl *base = program_find_class(program, c->base);
        int idx = class_find_field_index(program, base, name, out_type);
        if (idx >= 0)
            return idx;
        base_fields = class_total_field_count(program, base);
    }
    for (int i = 0; i < c->fields.count; i++)
    {
        if (kn_strcmp(c->fields.items[i].name, name) == 0)
        {
            if (out_type)
                *out_type = c->fields.items[i].type;
            return base_fields + i;
        }
    }
    return -1;
}

static int struct_find_field_index(const StructDecl *s, const char *name, Type *out_type)
{
    if (!s || !name)
        return -1;
    for (int i = 0; i < s->fields.count; i++)
    {
        if (kn_strcmp(s->fields.items[i].name, name) == 0)
        {
            if (out_type)
                *out_type = s->fields.items[i].type;
            return i;
        }
    }
    return -1;
}

static void emit_u8(ByteBuf *code, int value)
{
    bytebuf_push(code, (unsigned char)(value & 0xff));
}

static void emit_u16(ByteBuf *code, int value)
{
    bytebuf_push(code, (unsigned char)(value & 0xff));
    bytebuf_push(code, (unsigned char)((value >> 8) & 0xff));
}

static int emit_jump_placeholder(ByteBuf *code, int opcode)
{
    emit_u8(code, opcode);
    emit_u16(code, 0);
    return code->count - 2;
}

static int emit_branch_placeholder(ByteBuf *code, int opcode, int cond_reg)
{
    emit_u8(code, opcode);
    emit_u8(code, cond_reg);
    emit_u16(code, 0);
    return code->count - 2;
}

static int emit_cmp_branch_placeholder(ByteBuf *code, int opcode, int lhs_reg, int rhs_reg)
{
    emit_u8(code, opcode);
    emit_u8(code, lhs_reg);
    emit_u8(code, rhs_reg);
    emit_u16(code, 0);
    return code->count - 2;
}

static void patch_u16(ByteBuf *code, int operand_pos, int value)
{
    if (!code || operand_pos < 0 || operand_pos + 1 >= code->count)
        kn_die("invalid knc jump patch");
    code->items[operand_pos] = (unsigned char)(value & 0xff);
    code->items[operand_pos + 1] = (unsigned char)((value >> 8) & 0xff);
}

static int emit_condition_false_branch(KncFuncState *st, Expr *cond)
{
    KncValue lhs;
    KncValue rhs;
    KncValue value;
    int opcode = -1;

    if (!st || !cond)
        return -1;

    if (cond->kind == EXPR_BINARY &&
        cond->v.binary.left &&
        cond->v.binary.right &&
        type_is_integerish(cond->v.binary.left->type) &&
        type_is_integerish(cond->v.binary.right->type))
    {
        switch (cond->v.binary.op)
        {
        case TOK_LT: opcode = KNC_OP_JUMP_IF_INT_LT_FALSE; break;
        case TOK_LE: opcode = KNC_OP_JUMP_IF_INT_LE_FALSE; break;
        case TOK_GT: opcode = KNC_OP_JUMP_IF_INT_GT_FALSE; break;
        case TOK_GE: opcode = KNC_OP_JUMP_IF_INT_GE_FALSE; break;
        case TOK_EQ: opcode = KNC_OP_JUMP_IF_INT_EQ_FALSE; break;
        case TOK_NE: opcode = KNC_OP_JUMP_IF_INT_NE_FALSE; break;
        default: break;
        }
    }

    if (opcode >= 0)
    {
        lhs = compile_expr(st, cond->v.binary.left);
        if (kn_diag_error_count() > 0)
            return -1;
        rhs = compile_expr(st, cond->v.binary.right);
        if (kn_diag_error_count() > 0)
            return -1;
        return emit_cmp_branch_placeholder(st->code, opcode, lhs.reg, rhs.reg);
    }

    value = compile_expr(st, cond);
    if (kn_diag_error_count() > 0)
        return -1;
    return emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, value.reg);
}

static int current_ip(ByteBuf *code)
{
    return code ? code->count : 0;
}

static int alloc_reg(KncFuncState *st)
{
    int reg = st->next_reg++;
    if (st->next_reg > st->max_reg)
        st->max_reg = st->next_reg;
    if (reg > 254)
        kn_die("knc register overflow");
    return reg;
}

static void add_local(KncFuncState *st, const char *name, Type type, int reg)
{
    KncLocal local;
    local.name = name;
    local.type = type;
    local.reg = reg;
    local.storage = KNC_LOCAL_VALUE;
    local.capture_slot = -1;
    localbuf_push(&st->locals, local);
}

static KncLocal *find_local(KncFuncState *st, const char *name)
{
    for (int i = st->locals.count - 1; i >= 0; i--)
        if (kn_strcmp(st->locals.items[i].name, name) == 0)
            return &st->locals.items[i];
    return 0;
}

static void add_capture_local(KncFuncState *st, const char *name, Type type, int capture_slot)
{
    KncLocal local;
    local.name = name;
    local.type = type;
    local.reg = -1;
    local.storage = KNC_LOCAL_CAPTURE;
    local.capture_slot = capture_slot;
    localbuf_push(&st->locals, local);
}

static const KnSource *record_src(const KncFuncRecord *record)
{
    if (!record)
        return 0;
    if (record->synthetic_src)
        return record->synthetic_src;
    if (record->callable_kind == KNC_CALLABLE_METHOD)
        return record->method ? record->method->src : 0;
    return record->func ? record->func->src : 0;
}

static Type record_return_type(const KncFuncRecord *record)
{
    if (!record)
        return type_make(TY_UNKNOWN);
    if (record->synthetic_body)
        return record->synthetic_ret_type;
    if (record->callable_kind == KNC_CALLABLE_METHOD)
        return record->method ? record->method->ret_type : type_make(TY_UNKNOWN);
    return record->func ? record->func->ret_type : type_make(TY_UNKNOWN);
}

static ParamList *record_params(const KncFuncRecord *record)
{
    static ParamList synthetic;
    if (!record)
        return 0;
    if (record->synthetic_body)
    {
        synthetic.items = record->synthetic_params;
        synthetic.count = record->synthetic_param_count;
        synthetic.cap = record->synthetic_param_count;
        return &synthetic;
    }
    if (record->callable_kind == KNC_CALLABLE_METHOD)
        return record->method ? (ParamList *)&record->method->params : 0;
    return record->func ? (ParamList *)&record->func->params : 0;
}

static Stmt *record_body(const KncFuncRecord *record)
{
    if (!record)
        return 0;
    if (record->synthetic_body)
        return record->synthetic_body;
    if (record->callable_kind == KNC_CALLABLE_METHOD)
        return record->method ? record->method->body : 0;
    return record->func ? record->func->body : 0;
}

static int record_is_static(const KncFuncRecord *record)
{
    if (!record)
        return 0;
    if (record->callable_kind == KNC_CALLABLE_METHOD)
        return record->method ? record->method->is_static : 0;
    return record->func ? record->func->is_static : 1;
}

static Type record_receiver_type(const KncFuncRecord *record)
{
    if (!record)
        return type_make(TY_UNKNOWN);
    if (record->owner_class)
        return type_class(class_qname_or_name(record->owner_class));
    if (record->owner_struct)
        return type_struct(struct_qname_or_name(record->owner_struct));
    return type_make(TY_UNKNOWN);
}

static int program_find_function(KncProgram *program, const char *name)
{
    if (!name)
        return -1;
    for (int i = 0; i < program->count; i++)
    {
        KncFuncRecord *rec = &program->items[i];
        if (rec->callable_kind != KNC_CALLABLE_FUNC || !rec->func)
            continue;
        if (rec->func->qname && kn_strcmp(rec->func->qname, name) == 0)
            return rec->index;
        if (rec->func->name && kn_strcmp(rec->func->name, name) == 0)
            return rec->index;
    }
    return -1;
}

static int program_find_method_function(KncProgram *program, const char *owner_qname, int method_index)
{
    int type_index = program_find_type(program, owner_qname);
    if (type_index < 0)
        return -1;
    if (method_index < 0)
        return -1;
    if (method_index >= program->types.items[type_index].direct_method_count)
        return -1;
    return program->types.items[type_index].direct_methods[method_index];
}

static int program_method_is_virtual(KncProgram *program, const char *owner_qname, int method_index)
{
    ClassDecl *c = program_find_class(program, owner_qname);
    if (!c || method_index < 0 || method_index >= c->methods.count)
        return 0;
    return c->methods.items[method_index].is_virtual || c->methods.items[method_index].is_override || c->methods.items[method_index].is_abstract;
}

static int capturebuf_find_name(const CaptureBuf *buf, const char *name)
{
    if (!buf || !name)
        return -1;
    for (int i = 0; i < buf->count; i++)
        if (buf->items[i].name && kn_strcmp(buf->items[i].name, name) == 0)
            return i;
    return -1;
}

static void collect_visible_captures(KncFuncState *st, CaptureBuf *out)
{
    if (!st || !out)
        return;
    for (int i = st->locals.count - 1; i >= 0; i--)
    {
        KncLocal *local = &st->locals.items[i];
        KncCaptureRecord rec;
        if (!local->name || capturebuf_find_name(out, local->name) >= 0)
            continue;
        rec.name = local->name;
        rec.type = local->type;
        rec.slot = out->count;
        capturebuf_push(out, rec);
    }
}

static int block_record_slot(KncFuncState *st, int ip)
{
    if (!st || !st->block_records)
        return -1;
    for (int i = 0; i < st->block_records->count; i++)
        if (st->block_records->items[i].ip == ip)
            return i;
    return -1;
}

static int map_builtin_id(int builtin_id)
{
    switch ((KnBuiltinKind)builtin_id)
    {
    case KN_BUILTIN_IO_CONSOLE_PRINTLINE:      return 0;
    case KN_BUILTIN_IO_CONSOLE_PRINT:          return 1;
    case KN_BUILTIN_IO_CONSOLE_READLINE:       return 2;
    case KN_BUILTIN_IO_CONSOLE_WRITE:          return 3;
    case KN_BUILTIN_IO_CONSOLE_WRITELINE:      return 4;
    case KN_BUILTIN_IO_CONSOLE_PRINTINT:       return 5;
    case KN_BUILTIN_IO_CONSOLE_PRINTBOOL:      return 6;
    case KN_BUILTIN_IO_CONSOLE_PRINTCHAR:      return 7;
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEINT:   return 8;
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEBOOL:  return 9;
    case KN_BUILTIN_IO_CONSOLE_PRINTLINECHAR:  return 10;
    case KN_BUILTIN_IO_CONSOLE_WRITEINT:       return 11;
    case KN_BUILTIN_IO_CONSOLE_WRITEBOOL:      return 12;
    case KN_BUILTIN_IO_CONSOLE_WRITECHAR:      return 13;
    case KN_BUILTIN_IO_TIME_NOW:               return 14;
    case KN_BUILTIN_IO_TIME_GETTICK:           return 15;
    case KN_BUILTIN_IO_TIME_SLEEP:             return 16;
    case KN_BUILTIN_IO_TIME_NANOTICK:          return 17;
    case KN_BUILTIN_IO_TIME_FORMAT:            return 18;
    case KN_BUILTIN_IO_STRING_LENGTH:          return 19;
    case KN_BUILTIN_IO_STRING_CONCAT:          return 20;
    case KN_BUILTIN_IO_STRING_EQUALS:          return 21;
    case KN_BUILTIN_IO_STRING_TOCHARS:         return 22;
    case KN_BUILTIN_IO_FILE_CREATE:            return 85;
    case KN_BUILTIN_IO_FILE_TOUCH:             return 86;
    case KN_BUILTIN_IO_FILE_READ_ALL_TEXT:     return 87;
    case KN_BUILTIN_IO_FILE_READ_FIRST_LINE:   return 88;
    case KN_BUILTIN_IO_FILE_WRITE_ALL_TEXT:    return 89;
    case KN_BUILTIN_IO_FILE_APPEND_ALL_TEXT:   return 90;
    case KN_BUILTIN_IO_FILE_APPEND_LINE:       return 91;
    case KN_BUILTIN_IO_FILE_DELETE:            return 92;
    case KN_BUILTIN_IO_FILE_DELETE_IF_EXISTS:  return 93;
    case KN_BUILTIN_IO_FILE_SIZE:              return 94;
    case KN_BUILTIN_IO_FILE_IS_EMPTY:          return 95;
    case KN_BUILTIN_IO_FILE_COPY:              return 96;
    case KN_BUILTIN_IO_FILE_MOVE:              return 97;
    case KN_BUILTIN_IO_FILE_READ_OR_DEFAULT:   return 98;
    case KN_BUILTIN_IO_FILE_READ_IF_EXISTS:    return 99;
    case KN_BUILTIN_IO_FILE_READ_OR:           return 100;
    case KN_BUILTIN_IO_FILE_REPLACE_TEXT:      return 103;
    case KN_BUILTIN_IO_ANY_ISNULL:             return 130;
    case KN_BUILTIN_IO_ANY_TOSTRING:           return 131;
    case KN_BUILTIN_IO_ANY_EQUALS:             return 132;
    case KN_BUILTIN_IO_ANY_TYPENAME:           return 134;
    case KN_BUILTIN_IO_ANY_ISNUMBER:           return 135;
    case KN_BUILTIN_IO_ANY_ISPOINTER:          return 136;
    case KN_BUILTIN_IO_ANY_ISINT:              return 137;
    case KN_BUILTIN_IO_ANY_ISFLOAT:            return 138;
    case KN_BUILTIN_IO_ANY_ISSTRING:           return 139;
    case KN_BUILTIN_IO_ANY_ISBOOL:             return 140;
    case KN_BUILTIN_IO_ANY_ISCHAR:             return 141;
    case KN_BUILTIN_IO_ANY_ISOBJECT:           return 142;
    case KN_BUILTIN_IO_SYSTEM_FILE_EXISTS:     return 145;
    case KN_BUILTIN_IO_TEXT_CONTAINS:          return 146;
    case KN_BUILTIN_IO_ERROR_LASTTRACE:        return 147;
    case KN_BUILTIN_IO_ERROR_HAS:              return 148;
    case KN_BUILTIN_IO_ERROR_CLEAR:            return 149;
    case KN_BUILTIN_IO_ERROR_CURRENT:          return 150;
    case KN_BUILTIN_IO_GC_COLLECT:            return 151;
    default:                                   return -1;
    }
}

static int map_runtime_extern_symbol(const char *name)
{
    size_t len;
    const char *tail;
    if (!name)
        return -1;
    len = kn_strlen(name);
    tail = name;
    for (size_t i = len; i > 0; i--)
    {
        if (name[i - 1] == '.')
        {
            tail = name + i;
            break;
        }
    }
    if (kn_strcmp(tail, "__kn_time_tick") == 0)
        return map_builtin_id(KN_BUILTIN_IO_TIME_GETTICK);
    if (kn_strcmp(tail, "__kn_time_sleep") == 0)
        return map_builtin_id(KN_BUILTIN_IO_TIME_SLEEP);
    if (kn_strcmp(tail, "__kn_time_nano") == 0)
        return map_builtin_id(KN_BUILTIN_IO_TIME_NANOTICK);
    if (kn_strcmp(tail, "__kn_sys_file_exists") == 0)
        return map_builtin_id(KN_BUILTIN_IO_SYSTEM_FILE_EXISTS);
    return -1;
}

static void emit_default_value(KncFuncState *st, int reg, Type type)
{
    if (type.kind == TY_CHAR)
    {
        emit_u8(st->code, KNC_OP_LOAD_CHAR);
        emit_u8(st->code, reg);
        emit_u16(st->code, program_add_int(st->program, 0));
        return;
    }
    if (type_is_integerish(type))
    {
        emit_u8(st->code, KNC_OP_LOAD_INT);
        emit_u8(st->code, reg);
        emit_u16(st->code, program_add_int(st->program, 0));
        return;
    }
    if (type_is_floatish(type))
    {
        emit_u8(st->code, KNC_OP_LOAD_FLOAT);
        emit_u8(st->code, reg);
        emit_u16(st->code, program_add_float(st->program, 0.0));
        return;
    }
    if (type.kind == TY_BOOL)
    {
        emit_u8(st->code, KNC_OP_LOAD_BOOL);
        emit_u8(st->code, reg);
        emit_u8(st->code, 0);
        return;
    }
    if (type.kind == TY_STRING)
    {
        emit_u8(st->code, KNC_OP_LOAD_STRING);
        emit_u8(st->code, reg);
        emit_u16(st->code, program_add_string(st->program, ""));
        return;
    }
    if (type.kind == TY_ARRAY)
    {
        Type elem_type = type_make(type.elem);
        int len_reg = alloc_reg(st);
        elem_type.name = type.name;
        emit_u8(st->code, KNC_OP_LOAD_INT);
        emit_u8(st->code, len_reg);
        emit_u16(st->code, program_add_int(st->program, type.array_len >= 0 ? (int)type.array_len : 0));
        emit_u8(st->code, KNC_OP_NEW_ARRAY);
        emit_u8(st->code, reg);
        emit_u8(st->code, len_reg);
        emit_u8(st->code, value_kind_from_type(elem_type));
        return;
    }
    if (type.kind == TY_STRUCT)
    {
        int type_index = program_find_type(st->program, type.name);
        if (type_index < 0)
        {
            knc_diag(st->src, st->func && st->func->line ? st->func->line : 1,
                     st->func && st->func->col ? st->func->col : 1,
                     "This struct type is not supported by the bootstrap KNC emitter yet");
            return;
        }
        emit_u8(st->code, KNC_OP_NEW_OBJECT);
        emit_u8(st->code, reg);
        emit_u16(st->code, type_index);
        return;
    }
    emit_u8(st->code, KNC_OP_MOVE);
    emit_u8(st->code, reg);
    emit_u8(st->code, reg);
}

static KncValue compile_expr(KncFuncState *st, Expr *e);
static int compile_stmt(KncFuncState *st, Stmt *s);
static int compile_store_member(KncFuncState *st, Expr *target, KncValue value);
static int compile_store_index(KncFuncState *st, Expr *target, KncValue value);
static int compile_store_ptr(KncFuncState *st, Expr *target, KncValue value);
static void patch_all(ByteBuf *code, PatchBuf *patches, int target);
static KncValue emit_builtin_call_regs(KncFuncState *st, Type result_type, int mapped, int arg_count, const int *arg_regs);
static KncValue emit_stringify_value(KncFuncState *st, KncValue value);
static KncValue emit_string_const(KncFuncState *st, const char *text);
static KncValue emit_string_concat(KncFuncState *st, KncValue left, KncValue right);

static int emit_try_placeholder(KncFuncState *st, int catch_reg, int catch_kind, int catch_payload)
{
    emit_u8(st->code, KNC_OP_PUSH_TRY);
    emit_u8(st->code, catch_reg);
    emit_u8(st->code, catch_kind);
    emit_u16(st->code, catch_payload);
    emit_u16(st->code, 0);
    return st->code->count - 2;
}

static int emit_int_const_reg(KncFuncState *st, int value)
{
    int reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_LOAD_INT);
    emit_u8(st->code, reg);
    emit_u16(st->code, program_add_int(st->program, value));
    return reg;
}

static KncValue move_to_fresh_reg(KncFuncState *st, KncValue value)
{
    KncValue out = value;
    int dst = alloc_reg(st);
    emit_u8(st->code, KNC_OP_MOVE);
    emit_u8(st->code, dst);
    emit_u8(st->code, value.reg);
    out.reg = dst;
    return out;
}

static void ensure_local_cell(KncFuncState *st, KncLocal *local)
{
    if (!st || !local || local->storage != KNC_LOCAL_VALUE)
        return;
    emit_u8(st->code, KNC_OP_NEW_CELL);
    emit_u8(st->code, local->reg);
    emit_u8(st->code, local->reg);
    local->storage = KNC_LOCAL_CELL;
}

static int knc_stmt_is_empty_block(const Stmt *s)
{
    return s && s->kind == ST_BLOCK && s->v.block.stmts.count == 0;
}

static int knc_reverse_cmp_op(int op)
{
    switch (op)
    {
    case TOK_LT: return TOK_GT;
    case TOK_LE: return TOK_GE;
    case TOK_GT: return TOK_LT;
    case TOK_GE: return TOK_LE;
    default:     return op;
    }
}

static int knc_expr_is_superloop_pure(const Expr *e, const char *loop_name)
{
    if (!e)
        return 0;
    switch (e->kind)
    {
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_BOOL:
    case EXPR_STRING:
    case EXPR_CHAR:
        return 1;
    case EXPR_VAR:
        return !loop_name || kn_strcmp(e->v.var.name, loop_name) != 0;
    case EXPR_UNARY:
        switch (e->v.unary.op)
        {
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_NOT:
        case TOK_TILDE:
            return knc_expr_is_superloop_pure(e->v.unary.expr, loop_name);
        default:
            return 0;
        }
    case EXPR_BINARY:
        switch (e->v.binary.op)
        {
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
            return knc_expr_is_superloop_pure(e->v.binary.left, loop_name) &&
                   knc_expr_is_superloop_pure(e->v.binary.right, loop_name);
        default:
            return 0;
        }
    default:
        return 0;
    }
}

static int knc_expr_is_int_const(const Expr *e, int64_t value)
{
    return e && e->kind == EXPR_INT && e->v.int_val == value;
}

static int knc_select_loop_op(int delta_sign, int cmp_op, int is_backedge, KncOpCode *out_op)
{
    if (!out_op)
        return 0;
    if (delta_sign > 0)
    {
        switch (cmp_op)
        {
        case TOK_EQ: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_EQ_INC : KNC_OP_LOOP_INT_EQ_INC; break;
        case TOK_NE: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_NE_INC : KNC_OP_LOOP_INT_NE_INC; break;
        case TOK_LT: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_LT_INC : KNC_OP_LOOP_INT_LT_INC; break;
        case TOK_LE: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_LE_INC : KNC_OP_LOOP_INT_LE_INC; break;
        default:     return 0;
        }
    }
    else if (delta_sign < 0)
    {
        switch (cmp_op)
        {
        case TOK_EQ: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_EQ_DEC : KNC_OP_LOOP_INT_EQ_DEC; break;
        case TOK_NE: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_NE_DEC : KNC_OP_LOOP_INT_NE_DEC; break;
        case TOK_GT: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_GT_DEC : KNC_OP_LOOP_INT_GT_DEC; break;
        case TOK_GE: *out_op = is_backedge ? KNC_OP_LOOP_BACK_INT_GE_DEC : KNC_OP_LOOP_INT_GE_DEC; break;
        default:     return 0;
        }
    }
    else
    {
        return 0;
    }
    return 1;
}

static int knc_match_loop_update_name_and_value(KncFuncState *st,
                                                const char *loop_name,
                                                Expr *value,
                                                KncLocal **out_local,
                                                int *out_delta_sign)
{
    KncLocal *local;
    Expr *left;
    Expr *right;

    if (!st || !loop_name || !value || value->kind != EXPR_BINARY || !out_local || !out_delta_sign)
        return 0;

    local = find_local(st, loop_name);
    if (!local || local->storage != KNC_LOCAL_VALUE || !type_is_integerish(local->type))
        return 0;

    left = value->v.binary.left;
    right = value->v.binary.right;
    if (!left || !right)
        return 0;

    switch (value->v.binary.op)
    {
    case TOK_PLUS:
        if (left->kind == EXPR_VAR && kn_strcmp(left->v.var.name, loop_name) == 0 && knc_expr_is_int_const(right, 1))
        {
            *out_local = local;
            *out_delta_sign = 1;
            return 1;
        }
        if (right->kind == EXPR_VAR && kn_strcmp(right->v.var.name, loop_name) == 0 && knc_expr_is_int_const(left, 1))
        {
            *out_local = local;
            *out_delta_sign = 1;
            return 1;
        }
        return 0;
    case TOK_MINUS:
        if (left->kind == EXPR_VAR && kn_strcmp(left->v.var.name, loop_name) == 0 && knc_expr_is_int_const(right, 1))
        {
            *out_local = local;
            *out_delta_sign = -1;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

static int knc_match_loop_update_expr(KncFuncState *st,
                                      Expr *post,
                                      KncLocal **out_local,
                                      int *out_delta_sign)
{
    if (!st || !post || !out_local || !out_delta_sign)
        return 0;
    if (post->kind == EXPR_UNARY && post->v.unary.expr && post->v.unary.expr->kind == EXPR_VAR)
    {
        KncLocal *local;
        const char *loop_name = post->v.unary.expr->v.var.name;
        int delta_sign;

        if (post->v.unary.op == TOK_PLUSPLUS)
            delta_sign = 1;
        else if (post->v.unary.op == TOK_MINUSMINUS)
            delta_sign = -1;
        else
            return 0;

        local = find_local(st, loop_name);
        if (!local || local->storage != KNC_LOCAL_VALUE || !type_is_integerish(local->type))
            return 0;

        *out_local = local;
        *out_delta_sign = delta_sign;
        return 1;
    }
    if (post->kind == EXPR_ASSIGN && post->v.assign.target && post->v.assign.target->kind == EXPR_VAR)
        return knc_match_loop_update_name_and_value(st,
                                                    post->v.assign.target->v.var.name,
                                                    post->v.assign.value,
                                                    out_local,
                                                    out_delta_sign);
    return 0;
}

static int knc_match_loop_update_stmt(KncFuncState *st,
                                      Stmt *post,
                                      KncLocal **out_local,
                                      int *out_delta_sign)
{
    if (!st || !post || !out_local || !out_delta_sign)
        return 0;
    if (post->kind == ST_ASSIGN)
        return knc_match_loop_update_name_and_value(st,
                                                    post->v.assign.name,
                                                    post->v.assign.value,
                                                    out_local,
                                                    out_delta_sign);
    if (post->kind == ST_EXPR)
        return knc_match_loop_update_expr(st, post->v.expr.expr, out_local, out_delta_sign);
    return 0;
}

static int knc_match_loop_cond(KncFuncState *st,
                               Expr *cond,
                               KncLocal *local,
                               int delta_sign,
                               Expr **out_limit,
                               KncOpCode *out_op,
                               int is_backedge)
{
    Expr *limit_expr = 0;
    int cmp_op;

    if (!st || !cond || !local || !out_limit || !out_op)
        return 0;
    if (cond->kind != EXPR_BINARY || !cond->v.binary.left || !cond->v.binary.right)
        return 0;

    cmp_op = cond->v.binary.op;
    if (cond->v.binary.left->kind == EXPR_VAR && kn_strcmp(cond->v.binary.left->v.var.name, local->name) == 0)
    {
        limit_expr = cond->v.binary.right;
    }
    else if (cond->v.binary.right->kind == EXPR_VAR && kn_strcmp(cond->v.binary.right->v.var.name, local->name) == 0)
    {
        limit_expr = cond->v.binary.left;
        cmp_op = knc_reverse_cmp_op(cmp_op);
    }
    else
    {
        return 0;
    }

    if (!knc_expr_is_superloop_pure(limit_expr, local->name))
        return 0;
    if (!knc_select_loop_op(delta_sign, cmp_op, is_backedge, out_op))
        return 0;

    *out_limit = limit_expr;
    return 1;
}

static int knc_local_is_raw_int_array(KncLocal *local)
{
    Type elem_type;

    if (!local || local->storage != KNC_LOCAL_VALUE || local->type.kind != TY_ARRAY)
        return 0;

    elem_type = type_make(local->type.elem);
    elem_type.name = local->type.name;
    return value_kind_from_type(elem_type) == KNC_VALUE_INT;
}

static int knc_extract_var_assignment(Stmt *stmt, const char **out_name, Expr **out_value)
{
    if (!stmt || !out_name || !out_value)
        return 0;
    if (stmt->kind == ST_ASSIGN)
    {
        *out_name = stmt->v.assign.name;
        *out_value = stmt->v.assign.value;
        return *out_name && *out_value;
    }
    if (stmt->kind == ST_EXPR && stmt->v.expr.expr && stmt->v.expr.expr->kind == EXPR_ASSIGN &&
        stmt->v.expr.expr->v.assign.target && stmt->v.expr.expr->v.assign.target->kind == EXPR_VAR &&
        stmt->v.expr.expr->v.assign.value)
    {
        *out_name = stmt->v.expr.expr->v.assign.target->v.var.name;
        *out_value = stmt->v.expr.expr->v.assign.value;
        return *out_name && *out_value;
    }
    return 0;
}

static int knc_match_raw_int_array_index_expr(KncFuncState *st,
                                              Expr *expr,
                                              KncLocal *counter_local,
                                              KncLocal **out_array)
{
    KncLocal *array_local;

    if (!st || !expr || !counter_local || !out_array)
        return 0;
    if (expr->kind != EXPR_INDEX || !expr->v.index.recv || !expr->v.index.index)
        return 0;
    if (expr->v.index.recv->kind != EXPR_VAR || expr->v.index.index->kind != EXPR_VAR)
        return 0;
    if (kn_strcmp(expr->v.index.index->v.var.name, counter_local->name) != 0)
        return 0;

    array_local = find_local(st, expr->v.index.recv->v.var.name);
    if (!knc_local_is_raw_int_array(array_local))
        return 0;

    *out_array = array_local;
    return 1;
}

static int knc_match_array_fill_stmt(KncFuncState *st,
                                     Stmt *stmt,
                                     KncLocal *counter_local,
                                     KncLocal **out_array)
{
    Expr *expr;

    if (!st || !stmt || !counter_local || !out_array)
        return 0;
    if (stmt->kind != ST_EXPR || !stmt->v.expr.expr)
        return 0;

    expr = stmt->v.expr.expr;
    if (expr->kind != EXPR_ASSIGN || !expr->v.assign.target || !expr->v.assign.value)
        return 0;
    if (!knc_match_raw_int_array_index_expr(st, expr->v.assign.target, counter_local, out_array))
        return 0;
    if (expr->v.assign.value->kind != EXPR_VAR ||
        kn_strcmp(expr->v.assign.value->v.var.name, counter_local->name) != 0)
        return 0;

    return 1;
}

static int knc_match_array_sum_stmt(KncFuncState *st,
                                    Stmt *stmt,
                                    KncLocal *counter_local,
                                    KncLocal **out_sum,
                                    KncLocal **out_array)
{
    const char *sum_name;
    Expr *value;
    Expr *index_expr;
    KncLocal *sum_local;

    if (!st || !stmt || !counter_local || !out_sum || !out_array)
        return 0;
    if (!knc_extract_var_assignment(stmt, &sum_name, &value))
        return 0;

    sum_local = find_local(st, sum_name);
    if (!sum_local || sum_local == counter_local || sum_local->storage != KNC_LOCAL_VALUE || !type_is_integerish(sum_local->type))
        return 0;
    if (!value || value->kind != EXPR_BINARY || value->v.binary.op != TOK_PLUS ||
        !value->v.binary.left || !value->v.binary.right)
        return 0;

    if (value->v.binary.left->kind == EXPR_VAR && kn_strcmp(value->v.binary.left->v.var.name, sum_name) == 0)
    {
        index_expr = value->v.binary.right;
    }
    else if (value->v.binary.right->kind == EXPR_VAR && kn_strcmp(value->v.binary.right->v.var.name, sum_name) == 0)
    {
        index_expr = value->v.binary.left;
    }
    else
    {
        return 0;
    }

    if (!knc_match_raw_int_array_index_expr(st, index_expr, counter_local, out_array))
        return 0;

    *out_sum = sum_local;
    return 1;
}

static int knc_match_sum_stmt(KncFuncState *st,
                              Stmt *stmt,
                              KncLocal *counter_local,
                              KncLocal **out_sum)
{
    const char *sum_name;
    Expr *value;
    Expr *term_expr;
    KncLocal *sum_local;

    if (!st || !stmt || !counter_local || !out_sum)
        return 0;
    if (!knc_extract_var_assignment(stmt, &sum_name, &value))
        return 0;

    sum_local = find_local(st, sum_name);
    if (!sum_local || sum_local == counter_local || sum_local->storage != KNC_LOCAL_VALUE || !type_is_integerish(sum_local->type))
        return 0;
    if (!value || value->kind != EXPR_BINARY || value->v.binary.op != TOK_PLUS ||
        !value->v.binary.left || !value->v.binary.right)
        return 0;

    if (value->v.binary.left->kind == EXPR_VAR && kn_strcmp(value->v.binary.left->v.var.name, sum_name) == 0)
    {
        term_expr = value->v.binary.right;
    }
    else if (value->v.binary.right->kind == EXPR_VAR && kn_strcmp(value->v.binary.right->v.var.name, sum_name) == 0)
    {
        term_expr = value->v.binary.left;
    }
    else
    {
        return 0;
    }

    if (term_expr->kind != EXPR_VAR || kn_strcmp(term_expr->v.var.name, counter_local->name) != 0)
        return 0;

    *out_sum = sum_local;
    return 1;
}

static int knc_match_superloop_inc_cond(KncFuncState *st,
                                        Expr *cond,
                                        KncLocal *counter_local,
                                        Expr **out_limit,
                                        int *out_inclusive)
{
    Expr *limit_expr = 0;
    int cmp_op;

    if (!st || !cond || !counter_local || !out_limit || !out_inclusive)
        return 0;
    if (cond->kind != EXPR_BINARY || !cond->v.binary.left || !cond->v.binary.right)
        return 0;

    cmp_op = cond->v.binary.op;
    if (cond->v.binary.left->kind == EXPR_VAR && kn_strcmp(cond->v.binary.left->v.var.name, counter_local->name) == 0)
    {
        limit_expr = cond->v.binary.right;
    }
    else if (cond->v.binary.right->kind == EXPR_VAR && kn_strcmp(cond->v.binary.right->v.var.name, counter_local->name) == 0)
    {
        limit_expr = cond->v.binary.left;
        cmp_op = knc_reverse_cmp_op(cmp_op);
    }
    else
    {
        return 0;
    }

    if (!knc_expr_is_superloop_pure(limit_expr, counter_local->name))
        return 0;
    if (cmp_op == TOK_LT)
        *out_inclusive = 0;
    else if (cmp_op == TOK_LE)
        *out_inclusive = 1;
    else
        return 0;

    *out_limit = limit_expr;
    return 1;
}

static int knc_match_array_fill_superloop_while(KncFuncState *st,
                                                Stmt *s,
                                                KncLocal **out_array,
                                                KncLocal **out_counter,
                                                Expr **out_limit,
                                                KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *array_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_WHILE || !st->program || !st->program->enable_superloop ||
        !s->v.whiles.cond || !s->v.whiles.body)
        return 0;

    body = s->v.whiles.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 2)
        return 0;
    if (!knc_match_loop_update_stmt(st, body->v.block.stmts.items[1], &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_array_fill_stmt(st, body->v.block.stmts.items[0], counter_local, &array_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.whiles.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_array = array_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_ARRAY_FILL_INT_LE_INC : KNC_OP_LOOP_ARRAY_FILL_INT_LT_INC;
    return 1;
}

static int knc_match_array_sum_superloop_while(KncFuncState *st,
                                               Stmt *s,
                                               KncLocal **out_sum,
                                               KncLocal **out_array,
                                               KncLocal **out_counter,
                                               Expr **out_limit,
                                               KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *sum_local;
    KncLocal *array_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_WHILE || !st->program || !st->program->enable_superloop ||
        !s->v.whiles.cond || !s->v.whiles.body)
        return 0;

    body = s->v.whiles.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 2)
        return 0;
    if (!knc_match_loop_update_stmt(st, body->v.block.stmts.items[1], &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_array_sum_stmt(st, body->v.block.stmts.items[0], counter_local, &sum_local, &array_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.whiles.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_sum = sum_local;
    *out_array = array_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_ARRAY_SUM_INT_LE_INC : KNC_OP_LOOP_ARRAY_SUM_INT_LT_INC;
    return 1;
}

static int knc_match_array_fill_superloop_for(KncFuncState *st,
                                              Stmt *s,
                                              KncLocal **out_array,
                                              KncLocal **out_counter,
                                              Expr **out_limit,
                                              KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *array_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_FOR || !st->program || !st->program->enable_superloop ||
        !s->v.fors.cond || !s->v.fors.post || !s->v.fors.body)
        return 0;

    body = s->v.fors.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 1)
        return 0;
    if (!knc_match_loop_update_expr(st, s->v.fors.post, &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_array_fill_stmt(st, body->v.block.stmts.items[0], counter_local, &array_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.fors.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_array = array_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_ARRAY_FILL_INT_LE_INC : KNC_OP_LOOP_ARRAY_FILL_INT_LT_INC;
    return 1;
}

static int knc_match_array_sum_superloop_for(KncFuncState *st,
                                             Stmt *s,
                                             KncLocal **out_sum,
                                             KncLocal **out_array,
                                             KncLocal **out_counter,
                                             Expr **out_limit,
                                             KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *sum_local;
    KncLocal *array_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_FOR || !st->program || !st->program->enable_superloop ||
        !s->v.fors.cond || !s->v.fors.post || !s->v.fors.body)
        return 0;

    body = s->v.fors.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 1)
        return 0;
    if (!knc_match_loop_update_expr(st, s->v.fors.post, &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_array_sum_stmt(st, body->v.block.stmts.items[0], counter_local, &sum_local, &array_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.fors.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_sum = sum_local;
    *out_array = array_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_ARRAY_SUM_INT_LE_INC : KNC_OP_LOOP_ARRAY_SUM_INT_LT_INC;
    return 1;
}

static int knc_match_sum_superloop_while(KncFuncState *st,
                                         Stmt *s,
                                         KncLocal **out_sum,
                                         KncLocal **out_counter,
                                         Expr **out_limit,
                                         KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *sum_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_WHILE || !st->program || !st->program->enable_superloop ||
        !s->v.whiles.cond || !s->v.whiles.body)
        return 0;

    body = s->v.whiles.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 2)
        return 0;
    if (!knc_match_loop_update_stmt(st, body->v.block.stmts.items[1], &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_sum_stmt(st, body->v.block.stmts.items[0], counter_local, &sum_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.whiles.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_sum = sum_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_SUM_INT_LE_INC : KNC_OP_LOOP_SUM_INT_LT_INC;
    return 1;
}

static int knc_match_sum_superloop_for(KncFuncState *st,
                                       Stmt *s,
                                       KncLocal **out_sum,
                                       KncLocal **out_counter,
                                       Expr **out_limit,
                                       KncOpCode *out_op)
{
    Stmt *body;
    KncLocal *counter_local;
    KncLocal *sum_local;
    int delta_sign;
    int inclusive;

    if (!st || !s || s->kind != ST_FOR || !st->program || !st->program->enable_superloop ||
        !s->v.fors.cond || !s->v.fors.post || !s->v.fors.body)
        return 0;

    body = s->v.fors.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count != 1)
        return 0;
    if (!knc_match_loop_update_expr(st, s->v.fors.post, &counter_local, &delta_sign) || delta_sign != 1)
        return 0;
    if (!knc_match_sum_stmt(st, body->v.block.stmts.items[0], counter_local, &sum_local))
        return 0;
    if (!knc_match_superloop_inc_cond(st, s->v.fors.cond, counter_local, out_limit, &inclusive))
        return 0;

    *out_sum = sum_local;
    *out_counter = counter_local;
    *out_op = inclusive ? KNC_OP_LOOP_SUM_INT_LE_INC : KNC_OP_LOOP_SUM_INT_LT_INC;
    return 1;
}

static int knc_match_empty_superloop_for(KncFuncState *st,
                                         Stmt *s,
                                         KncLocal **out_local,
                                         Expr **out_limit,
                                         KncOpCode *out_op)
{
    Expr *post;
    Expr *cond;
    KncLocal *local;
    int delta_sign;

    if (!st || !s || s->kind != ST_FOR || !st->program || !st->program->enable_superloop)
        return 0;
    if (!knc_stmt_is_empty_block(s->v.fors.body) || !s->v.fors.cond || !s->v.fors.post)
        return 0;

    post = s->v.fors.post;
    cond = s->v.fors.cond;
    if (!knc_match_loop_update_expr(st, post, &local, &delta_sign))
        return 0;
    if (!knc_match_loop_cond(st, cond, local, delta_sign, out_limit, out_op, 0))
        return 0;

    *out_local = local;
    return 1;
}

static int knc_match_fused_backedge_for(KncFuncState *st,
                                        Stmt *s,
                                        KncLocal **out_local,
                                        Expr **out_limit,
                                        KncOpCode *out_op)
{
    Expr *post;
    Expr *cond;
    KncLocal *local;
    int delta_sign;

    if (!st || !s || s->kind != ST_FOR || !s->v.fors.cond || !s->v.fors.post)
        return 0;

    post = s->v.fors.post;
    cond = s->v.fors.cond;
    if (!knc_match_loop_update_expr(st, post, &local, &delta_sign))
        return 0;
    if (!knc_match_loop_cond(st, cond, local, delta_sign, out_limit, out_op, 1))
        return 0;

    *out_local = local;
    return 1;
}

static int knc_match_fused_backedge_while(KncFuncState *st,
                                          Stmt *s,
                                          KncLocal **out_local,
                                          Expr **out_limit,
                                          KncOpCode *out_op)
{
    Stmt *body;
    Stmt *post;
    KncLocal *local;
    int delta_sign;

    if (!st || !s || s->kind != ST_WHILE || !s->v.whiles.cond || !s->v.whiles.body)
        return 0;

    body = s->v.whiles.body;
    if (body->kind != ST_BLOCK || body->v.block.stmts.count <= 0)
        return 0;

    post = body->v.block.stmts.items[body->v.block.stmts.count - 1];
    if (!knc_match_loop_update_stmt(st, post, &local, &delta_sign))
        return 0;
    if (!knc_match_loop_cond(st, s->v.whiles.cond, local, delta_sign, out_limit, out_op, 1))
        return 0;

    *out_local = local;
    return 1;
}

static int build_capture_env(KncFuncState *st, CaptureBuf *captures, int minimum_slots)
{
    int len_reg;
    int env_reg;
    int slot_count = minimum_slots;

    if (!st)
        return 255;
    if (captures)
        for (int i = 0; i < captures->count; i++)
            if (captures->items[i].slot + 1 > slot_count)
                slot_count = captures->items[i].slot + 1;
    if (slot_count <= 0)
        return 255;

    len_reg = alloc_reg(st);
    env_reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_LOAD_INT);
    emit_u8(st->code, len_reg);
    emit_u16(st->code, program_add_int(st->program, slot_count));
    emit_u8(st->code, KNC_OP_NEW_ARRAY);
    emit_u8(st->code, env_reg);
    emit_u8(st->code, len_reg);
    emit_u8(st->code, KNC_VALUE_INT);

    for (int i = 0; i < minimum_slots; i++)
    {
        int zero_reg = emit_int_const_reg(st, 0);
        int idx_reg = emit_int_const_reg(st, i);
        emit_u8(st->code, KNC_OP_STORE_INDEX);
        emit_u8(st->code, env_reg);
        emit_u8(st->code, idx_reg);
        emit_u8(st->code, zero_reg);
    }

    for (int i = 0; captures && i < captures->count; i++)
    {
        KncLocal *local = find_local(st, captures->items[i].name);
        int cell_reg = 255;
        int idx_reg = emit_int_const_reg(st, captures->items[i].slot);

        if (!local)
        {
            knc_diag(st->src, 1, 1, "Internal KNC emitter error: missing visible capture local");
            return 255;
        }

        if (local->storage == KNC_LOCAL_CAPTURE)
        {
            cell_reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_CAPTURE_CELL);
            emit_u8(st->code, cell_reg);
            emit_u16(st->code, local->capture_slot);
        }
        else
        {
            ensure_local_cell(st, local);
            cell_reg = local->reg;
        }

        emit_u8(st->code, KNC_OP_STORE_INDEX);
        emit_u8(st->code, env_reg);
        emit_u8(st->code, idx_reg);
        emit_u8(st->code, cell_reg);
    }

    return env_reg;
}

static int ensure_block_function_record(KncFuncState *st, Expr *e)
{
    KncFuncRecord rec;
    CaptureBuf captures;
    Param *params;

    if (!st || !e || e->kind != EXPR_BLOCK_LITERAL)
        return -1;

    for (int i = 0; i < st->program->count; i++)
    {
        KncFuncRecord *it = &st->program->items[i];
        if (it->synthetic_body && it->synthetic_is_block && it->synthetic_id == e->v.block_lit.id)
            return it->index;
    }

    kn_memset(&captures, 0, sizeof(captures));
    collect_visible_captures(st, &captures);
    for (int i = 0; i < captures.count; i++)
        captures.items[i].slot = i + 1;

    params = (Param *)kn_malloc(sizeof(Param) * 2u);
    if (!params)
        kn_die("out of memory");
    kn_memset(params, 0, sizeof(Param) * 2u);
    params[0].name = "__knc_block_start";
    params[0].type = type_make(TY_INT);
    params[1].name = "__knc_block_until";
    params[1].type = type_make(TY_INT);

    kn_memset(&rec, 0, sizeof(rec));
    rec.callable_kind = KNC_CALLABLE_FUNC;
    rec.debug_name = e->v.block_lit.debug_name ? e->v.block_lit.debug_name : "__knc_block";
    rec.index = st->program->count;
    rec.synthetic_params = params;
    rec.synthetic_param_count = 2;
    rec.synthetic_body = e->v.block_lit.body;
    rec.synthetic_ret_type = type_make(TY_VOID);
    rec.synthetic_src = st->src;
    rec.synthetic_id = e->v.block_lit.id;
    rec.synthetic_is_block = 1;
    rec.synthetic_block_records = &e->v.block_lit.records;
    rec.captures = captures;
    funcbuf_push(st->program, rec);
    return rec.index;
}

static void emit_block_dispatch(KncFuncState *st, int target_reg)
{
    int cmp_reg;
    int zero_reg;

    if (!st || !st->in_runtime_block)
        return;

    zero_reg = emit_int_const_reg(st, 0);
    cmp_reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_EQ);
    emit_u8(st->code, cmp_reg);
    emit_u8(st->code, target_reg);
    emit_u8(st->code, zero_reg);
    patchbuf_push(&st->block_start_patches, emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_TRUE, cmp_reg));

    if (st->block_records)
    {
        for (int i = 0; i < st->block_records->count; i++)
        {
            int ip_reg = emit_int_const_reg(st, st->block_records->items[i].ip);
            int rec_cmp_reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_EQ);
            emit_u8(st->code, rec_cmp_reg);
            emit_u8(st->code, target_reg);
            emit_u8(st->code, ip_reg);
            patchbuf_push(&st->block_record_patches[i], emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_TRUE, rec_cmp_reg));
        }
    }

    emit_u8(st->code, KNC_OP_RET);
    emit_u8(st->code, 255);
}

static KncValue compile_block_invoke(KncFuncState *st, Expr *site, Expr *recv_expr, int builtin_id, ExprList *args)
{
    KncValue out;
    KncValue recv;
    int env_reg;
    int idx0_reg;
    int start_reg;
    int until_reg;

    out.reg = 255;
    out.type = type_make(TY_VOID);

    recv = compile_expr(st, recv_expr);
    if (kn_diag_error_count() > 0)
        return out;

    env_reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_LOAD_FIELD);
    emit_u8(st->code, env_reg);
    emit_u8(st->code, recv.reg);
    emit_u16(st->code, 1);

    idx0_reg = emit_int_const_reg(st, 0);

    start_reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_LOAD_INDEX);
    emit_u8(st->code, start_reg);
    emit_u8(st->code, env_reg);
    emit_u8(st->code, idx0_reg);

    if (builtin_id == KN_BUILTIN_BLOCK_JUMP)
    {
        if (!args || args->count != 1)
        {
            knc_diag_expr(st, site, "Bootstrap KNC emitter requires exactly one block Jump argument");
            return out;
        }
        start_reg = compile_expr(st, args->items[0]).reg;
        if (kn_diag_error_count() > 0)
            return out;
        until_reg = emit_int_const_reg(st, -1);
    }
    else if (builtin_id == KN_BUILTIN_BLOCK_RUN)
    {
        until_reg = emit_int_const_reg(st, -1);
    }
    else if (builtin_id == KN_BUILTIN_BLOCK_RUN_UNTIL)
    {
        if (!args || args->count != 1)
        {
            knc_diag_expr(st, site, "Bootstrap KNC emitter requires exactly one block RunUntil argument");
            return out;
        }
        until_reg = compile_expr(st, args->items[0]).reg;
        if (kn_diag_error_count() > 0)
            return out;
    }
    else
    {
        if (!args || args->count != 2)
        {
            knc_diag_expr(st, site, "Bootstrap KNC emitter requires exactly two block JumpAndRunUntil arguments");
            return out;
        }
        start_reg = compile_expr(st, args->items[0]).reg;
        if (kn_diag_error_count() > 0)
            return out;
        until_reg = compile_expr(st, args->items[1]).reg;
        if (kn_diag_error_count() > 0)
            return out;
    }

    emit_u8(st->code, KNC_OP_CALL_CLOSURE);
    emit_u8(st->code, 255);
    emit_u8(st->code, recv.reg);
    emit_u8(st->code, 2);
    emit_u8(st->code, start_reg);
    emit_u8(st->code, until_reg);
    emit_u8(st->code, 255);
    emit_u8(st->code, 255);
    return out;
}

static int ensure_anon_function_record(KncFuncState *st, Expr *e)
{
    KncFuncRecord rec;
    CaptureBuf captures;

    if (!st || !e || e->kind != EXPR_ANON_FUNC)
        return -1;

    for (int i = 0; i < st->program->count; i++)
    {
        KncFuncRecord *it = &st->program->items[i];
        if (it->synthetic_body && !it->synthetic_is_block && it->synthetic_id == e->v.anon_func.id)
            return it->index;
    }

    kn_memset(&captures, 0, sizeof(captures));
    collect_visible_captures(st, &captures);

    kn_memset(&rec, 0, sizeof(rec));
    rec.callable_kind = KNC_CALLABLE_FUNC;
    rec.debug_name = e->v.anon_func.debug_name ? e->v.anon_func.debug_name : "__knc_lambda";
    rec.index = st->program->count;
    rec.synthetic_params = e->v.anon_func.params;
    rec.synthetic_param_count = e->v.anon_func.param_count;
    rec.synthetic_body = e->v.anon_func.body;
    rec.synthetic_ret_type = e->v.anon_func.ret_type;
    rec.synthetic_src = st->src;
    rec.synthetic_id = e->v.anon_func.id;
    rec.synthetic_is_block = 0;
    rec.captures = captures;
    funcbuf_push(st->program, rec);
    return rec.index;
}

static void emit_make_function(KncFuncState *st, int dst_reg, int type_index, int target_kind, int target_index, int env_reg)
{
    emit_u8(st->code, KNC_OP_MAKE_FUNC);
    emit_u8(st->code, dst_reg);
    emit_u16(st->code, type_index);
    emit_u8(st->code, target_kind);
    emit_u16(st->code, target_index);
    emit_u8(st->code, env_reg);
}

static KncValue emit_bool_const(KncFuncState *st, int value)
{
    KncValue out;
    out.reg = alloc_reg(st);
    out.type = type_make(TY_BOOL);
    emit_u8(st->code, KNC_OP_LOAD_BOOL);
    emit_u8(st->code, out.reg);
    emit_u8(st->code, value ? 1 : 0);
    return out;
}

static int map_catch_target(KncFuncState *st, Type target, int *out_kind, int *out_payload)
{
    int kind = KNC_IS_TARGET_ANY;
    int payload = 0;

    if (target.kind == TY_ANY)
    {
        kind = KNC_IS_TARGET_ANY;
    }
    else if (target.kind == TY_BOOL)
    {
        kind = KNC_IS_TARGET_BOOL;
    }
    else if (target.kind == TY_CHAR)
    {
        kind = KNC_IS_TARGET_CHAR;
    }
    else if (type_is_integerish(target) || target.kind == TY_ENUM)
    {
        kind = KNC_IS_TARGET_INT;
    }
    else if (type_is_floatish(target))
    {
        kind = KNC_IS_TARGET_FLOAT;
    }
    else if (target.kind == TY_STRING)
    {
        kind = KNC_IS_TARGET_STRING;
    }
    else if (target.kind == TY_CLASS && target.name)
    {
        payload = program_find_type(st->program, target.name);
        if (payload < 0)
        {
            knc_diag(st->src, 1, 1, "This catch type is not supported by the bootstrap KNC emitter yet");
            return -1;
        }
        kind = KNC_IS_TARGET_TYPE;
    }
    else
    {
        knc_diag(st->src, 1, 1, "This catch type is not supported by the bootstrap KNC emitter yet");
        return -1;
    }

    if (out_kind)
        *out_kind = kind;
    if (out_payload)
        *out_payload = payload;
    return 0;
}

static KncValue compile_is_check_from_value(KncFuncState *st, Expr *site, KncValue value, Type target)
{
    KncValue out;
    int target_kind = -1;
    int target_payload = -1;
    out.reg = -1;
    out.type = type_make(TY_BOOL);

    if (value.type.kind == TY_NULL)
        return emit_bool_const(st, 0);

    if (value.type.kind == TY_ANY)
    {
        if (target.kind == TY_BOOL)
            target_kind = KNC_IS_TARGET_BOOL;
        else if (target.kind == TY_CHAR)
            target_kind = KNC_IS_TARGET_CHAR;
        else if (type_is_integerish(target) || target.kind == TY_ENUM)
            target_kind = KNC_IS_TARGET_INT;
        else if (type_is_floatish(target))
            target_kind = KNC_IS_TARGET_FLOAT;
        else if (target.kind == TY_STRING)
            target_kind = KNC_IS_TARGET_STRING;
        else if (target.kind == TY_CLASS && target.name)
        {
            target_payload = program_find_type(st->program, target.name);
            if (target_payload >= 0)
                target_kind = KNC_IS_TARGET_TYPE;
        }
        if (target_kind < 0)
            return emit_bool_const(st, 0);
    }
    else if (value.type.kind == TY_STRING && target.kind == TY_STRING)
    {
        target_kind = KNC_IS_TARGET_STRING;
    }
    else if ((value.type.kind == TY_CLASS || value.type.kind == TY_STRUCT) &&
             target.kind == TY_CLASS && target.name)
    {
        target_payload = program_find_type(st->program, target.name);
        if (target_payload < 0)
        {
            knc_diag_expr(st, site, "This Is target type is not supported by the bootstrap KNC emitter yet");
            return out;
        }
        target_kind = KNC_IS_TARGET_TYPE;
    }
    else if (value.type.kind == TY_BOOL && target.kind == TY_BOOL)
    {
        return emit_bool_const(st, 1);
    }
    else if (value.type.kind == TY_CHAR && target.kind == TY_CHAR)
    {
        return emit_bool_const(st, 1);
    }
    else if (type_is_integerish(value.type) && type_is_integerish(target))
    {
        return emit_bool_const(st, value.type.kind == target.kind);
    }
    else if (type_is_floatish(value.type) && type_is_floatish(target))
    {
        return emit_bool_const(st, value.type.kind == target.kind);
    }
    else if (value.type.kind == TY_ENUM && target.kind == TY_ENUM)
    {
        return emit_bool_const(st, knc_type_equal(value.type, target));
    }
    else
    {
        return emit_bool_const(st, 0);
    }

    out.reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_IS_TYPE);
    emit_u8(st->code, out.reg);
    emit_u8(st->code, value.reg);
    emit_u8(st->code, target_kind);
    emit_u16(st->code, target_payload >= 0 ? target_payload : 0);
    return out;
}

static KncValue emit_builtin_call_regs(KncFuncState *st, Type result_type, int mapped, int arg_count, const int *arg_regs)
{
    KncValue out;
    out.reg = 255;
    out.type = result_type;
    if (result_type.kind != TY_VOID)
        out.reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_CALL_BUILTIN);
    emit_u8(st->code, out.reg);
    emit_u16(st->code, mapped);
    emit_u8(st->code, arg_count);
    emit_u8(st->code, arg_count > 0 ? arg_regs[0] : 255);
    emit_u8(st->code, arg_count > 1 ? arg_regs[1] : 255);
    emit_u8(st->code, arg_count > 2 ? arg_regs[2] : 255);
    emit_u8(st->code, arg_count > 3 ? arg_regs[3] : 255);
    return out;
}

static KncValue emit_string_const(KncFuncState *st, const char *text)
{
    KncValue out;
    out.reg = alloc_reg(st);
    out.type = type_make(TY_STRING);
    emit_u8(st->code, KNC_OP_LOAD_STRING);
    emit_u8(st->code, out.reg);
    emit_u16(st->code, program_add_string(st->program, text ? text : ""));
    return out;
}

static KncValue emit_string_concat(KncFuncState *st, KncValue left, KncValue right)
{
    int regs[2];
    regs[0] = left.reg;
    regs[1] = right.reg;
    return emit_builtin_call_regs(st, type_make(TY_STRING), map_builtin_id(KN_BUILTIN_IO_STRING_CONCAT), 2, regs);
}

static KncValue emit_stringify_value(KncFuncState *st, KncValue value)
{
    KncValue out;
    int regs[1];

    if (value.type.kind == TY_STRING)
        return value;

    out.type = type_make(TY_STRING);

    if (value.type.kind == TY_CHAR)
    {
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_CHAR_TO_STRING);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, value.reg);
        return out;
    }

    if (value.type.kind == TY_BOOL)
    {
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_BOOL_TO_STRING);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, value.reg);
        return out;
    }

    if (type_is_integerish(value.type) || value.type.kind == TY_ENUM)
    {
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_INT_TO_STRING);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, value.reg);
        return out;
    }

    regs[0] = value.reg;
    return emit_builtin_call_regs(st, type_make(TY_STRING), map_builtin_id(KN_BUILTIN_IO_ANY_TOSTRING), 1, regs);
}

static KncValue compile_builtin_invoke(KncFuncState *st,
                                       Type result_type,
                                       int builtin_id,
                                       Expr *recv,
                                       int is_instance_recv,
                                       ExprList *args)
{
    int mapped = map_builtin_id(builtin_id);
    int arg_regs[4] = { 255, 255, 255, 255 };
    int arg_count = 0;
    KncValue out;
    out.reg = 255;
    out.type = result_type;

    if (builtin_id == KN_BUILTIN_IO_CONSOLE_PRINT || builtin_id == KN_BUILTIN_IO_CONSOLE_PRINTLINE)
    {
        KncValue joined;
        int call_regs[1];

        if (!args || args->count <= 0)
        {
            joined = emit_string_const(st, "");
        }
        else
        {
            KncValue sep = emit_string_const(st, " ");
            joined.reg = -1;
            joined.type = type_make(TY_STRING);
            for (int i = 0; i < args->count; i++)
            {
                KncValue part = compile_expr(st, args->items[i]);
                if (kn_diag_error_count() > 0)
                    return out;
                part = emit_stringify_value(st, part);
                if (i == 0)
                {
                    joined = part;
                }
                else
                {
                    joined = emit_string_concat(st, joined, sep);
                    joined = emit_string_concat(st, joined, part);
                }
            }
        }

        call_regs[0] = joined.reg;
        return emit_builtin_call_regs(st, result_type, map_builtin_id(builtin_id), 1, call_regs);
    }

    if (builtin_id == KN_BUILTIN_TYPEOF)
    {
        Type source_type = type_make(TY_UNKNOWN);
        const char *text;
        if (recv)
            source_type = recv->type;
        else if (args && args->count > 0 && args->items[0])
            source_type = args->items[0]->type;
        text = knc_type_text(source_type);
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_STRING);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, program_add_string(st->program, text));
        return out;
    }

    if (mapped < 0)
    {
        knc_diag_expr(st, recv ? recv : (args && args->count > 0 ? args->items[0] : 0),
                      "This builtin call is not mapped in the bootstrap KNC emitter yet");
        return out;
    }

    if (is_instance_recv && recv)
    {
        KncValue rv = compile_expr(st, recv);
        if (kn_diag_error_count() > 0)
            return out;
        arg_regs[arg_count++] = rv.reg;
    }

    if (args)
    {
        for (int i = 0; i < args->count; i++)
        {
            if (arg_count >= 4)
            {
                knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most four builtin call arguments");
                return out;
            }
            KncValue av = compile_expr(st, args->items[i]);
            if (kn_diag_error_count() > 0)
                return out;
            arg_regs[arg_count++] = av.reg;
        }
    }

    return emit_builtin_call_regs(st, result_type, mapped, arg_count, arg_regs);
}

static KncValue compile_direct_call(KncFuncState *st, Expr *site, const char *name, Type result_type, ExprList *args)
{
    KncValue out;
    int arg_regs[4] = { 255, 255, 255, 255 };
    int arg_count = 0;
    int func_index = program_find_function(st->program, name);

    out.reg = 255;
    out.type = result_type;

    if (func_index < 0)
    {
        int extern_builtin = map_runtime_extern_symbol(name);
        if (extern_builtin >= 0)
        {
            for (int i = 0; args && i < args->count; i++)
            {
                KncValue av;
                if (arg_count >= 4)
                {
                    knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most four function call arguments");
                    return out;
                }
                av = compile_expr(st, args->items[i]);
                if (kn_diag_error_count() > 0)
                    return out;
                arg_regs[arg_count++] = av.reg;
            }
            return emit_builtin_call_regs(st, result_type, extern_builtin, arg_count, arg_regs);
        }
        knc_diag_expr(st, site, "This function call target is not supported by the bootstrap KNC emitter yet");
        return out;
    }

    for (int i = 0; args && i < args->count; i++)
    {
        if (arg_count >= 4)
        {
            knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most four function call arguments");
            return out;
        }
        {
            KncValue av = compile_expr(st, args->items[i]);
            if (kn_diag_error_count() > 0)
                return out;
            arg_regs[arg_count++] = av.reg;
        }
    }

    if (result_type.kind != TY_VOID)
        out.reg = alloc_reg(st);

    emit_u8(st->code, KNC_OP_CALL_FUNC);
    emit_u8(st->code, out.reg);
    emit_u16(st->code, func_index);
    emit_u8(st->code, arg_count);
    emit_u8(st->code, arg_regs[0]);
    emit_u8(st->code, arg_regs[1]);
    emit_u8(st->code, arg_regs[2]);
    emit_u8(st->code, arg_regs[3]);
    return out;
}

static KncValue compile_call_func_index(KncFuncState *st, Expr *site, int func_index, Type result_type, ExprList *args)
{
    KncValue out;
    int arg_regs[4] = { 255, 255, 255, 255 };
    int arg_count = 0;

    out.reg = 255;
    out.type = result_type;

    if (func_index < 0)
    {
        knc_diag_expr(st, site, "This function call target is not supported by the bootstrap KNC emitter yet");
        return out;
    }

    for (int i = 0; args && i < args->count; i++)
    {
        KncValue av;
        if (arg_count >= 4)
        {
            knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most four function call arguments");
            return out;
        }
        av = compile_expr(st, args->items[i]);
        if (kn_diag_error_count() > 0)
            return out;
        arg_regs[arg_count++] = av.reg;
    }

    if (result_type.kind != TY_VOID)
        out.reg = alloc_reg(st);

    emit_u8(st->code, KNC_OP_CALL_FUNC);
    emit_u8(st->code, out.reg);
    emit_u16(st->code, func_index);
    emit_u8(st->code, arg_count);
    emit_u8(st->code, arg_regs[0]);
    emit_u8(st->code, arg_regs[1]);
    emit_u8(st->code, arg_regs[2]);
    emit_u8(st->code, arg_regs[3]);
    return out;
}

static KncValue compile_direct_method_call(KncFuncState *st,
                                           Expr *site,
                                           int func_index,
                                           Type result_type,
                                           KncValue recv,
                                           ExprList *args)
{
    KncValue out;
    int arg_regs[4] = { 255, 255, 255, 255 };
    int arg_count = 0;

    out.reg = 255;
    out.type = result_type;

    if (func_index < 0)
    {
        knc_diag_expr(st, site, "This method call target is not supported by the bootstrap KNC emitter yet");
        return out;
    }

    arg_regs[arg_count++] = recv.reg;
    for (int i = 0; args && i < args->count; i++)
    {
        KncValue av;
        if (arg_count >= 4)
        {
            knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most four method call arguments including This");
            return out;
        }
        av = compile_expr(st, args->items[i]);
        if (kn_diag_error_count() > 0)
            return out;
        arg_regs[arg_count++] = av.reg;
    }

    if (result_type.kind != TY_VOID)
        out.reg = alloc_reg(st);

    emit_u8(st->code, KNC_OP_CALL_FUNC);
    emit_u8(st->code, out.reg);
    emit_u16(st->code, func_index);
    emit_u8(st->code, arg_count);
    emit_u8(st->code, arg_regs[0]);
    emit_u8(st->code, arg_regs[1]);
    emit_u8(st->code, arg_regs[2]);
    emit_u8(st->code, arg_regs[3]);
    return out;
}

static KncValue compile_virtual_method_call(KncFuncState *st,
                                            Expr *site,
                                            int slot_index,
                                            Type result_type,
                                            KncValue recv,
                                            ExprList *args)
{
    KncValue out;
    int arg_regs[3] = { 255, 255, 255 };
    int arg_count = 0;

    out.reg = 255;
    out.type = result_type;

    for (int i = 0; args && i < args->count; i++)
    {
        KncValue av;
        if (arg_count >= 3)
        {
            knc_diag_expr(st, args->items[i], "KNC bootstrap VM currently supports at most three explicit virtual call arguments");
            return out;
        }
        av = compile_expr(st, args->items[i]);
        if (kn_diag_error_count() > 0)
            return out;
        arg_regs[arg_count++] = av.reg;
    }

    if (result_type.kind != TY_VOID)
        out.reg = alloc_reg(st);

    emit_u8(st->code, KNC_OP_CALL_VIRT);
    emit_u8(st->code, out.reg);
    emit_u8(st->code, recv.reg);
    emit_u16(st->code, slot_index);
    emit_u8(st->code, arg_count);
    emit_u8(st->code, arg_regs[0]);
    emit_u8(st->code, arg_regs[1]);
    emit_u8(st->code, arg_regs[2]);
    return out;
}

static int compile_assign_to_local(KncFuncState *st, Expr *target, Expr *value_expr, Type result_type)
{
    KncValue value;
    KncLocal *local;

    value = compile_expr(st, value_expr);
    if (kn_diag_error_count() > 0)
        return -1;

    if (target && target->kind == EXPR_VAR)
    {
        local = find_local(st, target->v.var.name);
        if (!local)
        {
            int global_slot = program_find_global_slot(st->program, target->v.var.name);
            if (global_slot >= 0)
            {
                emit_u8(st->code, KNC_OP_STORE_GLOBAL);
                emit_u8(st->code, value.reg);
                emit_u16(st->code, global_slot);
                (void)result_type;
                return value.reg;
            }
            knc_diag_expr(st, target, "Unknown local variable in KNC emitter");
            return -1;
        }
        if (local->storage == KNC_LOCAL_CAPTURE)
        {
            emit_u8(st->code, KNC_OP_STORE_CAPTURE);
            emit_u8(st->code, value.reg);
            emit_u16(st->code, local->capture_slot);
            (void)result_type;
            return value.reg;
        }
        if (local->storage == KNC_LOCAL_CELL)
        {
            emit_u8(st->code, KNC_OP_STORE_CELL);
            emit_u8(st->code, local->reg);
            emit_u8(st->code, value.reg);
            (void)result_type;
            return value.reg;
        }
        if (value.reg != local->reg)
        {
            emit_u8(st->code, KNC_OP_MOVE);
            emit_u8(st->code, local->reg);
            emit_u8(st->code, value.reg);
        }
        (void)result_type;
        return local->reg;
    }

    if (target && target->kind == EXPR_MEMBER)
        return compile_store_member(st, target, value);
    if (target && target->kind == EXPR_INDEX)
        return compile_store_index(st, target, value);
    if (target && target->kind == EXPR_UNARY && target->v.unary.op == TOK_STAR)
        return compile_store_ptr(st, target, value);

    knc_diag_expr(st, target, "This assignment target is not supported by the bootstrap KNC emitter yet");
    return -1;
}

static KncValue compile_short_circuit(KncFuncState *st, Expr *e)
{
    KncValue out;
    KncValue lhs;
    KncValue rhs;
    int jmp_to_rhs;
    int jmp_to_end;
    int false_idx;
    int true_idx;

    out.reg = alloc_reg(st);
    out.type = type_make(TY_BOOL);
    lhs = compile_expr(st, e->v.binary.left);
    if (kn_diag_error_count() > 0)
        return out;

    if (e->v.binary.op == TOK_ANDAND)
    {
        emit_u8(st->code, KNC_OP_LOAD_BOOL);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, 0);
        jmp_to_end = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, lhs.reg);
        rhs = compile_expr(st, e->v.binary.right);
        if (kn_diag_error_count() > 0)
            return out;
        emit_u8(st->code, KNC_OP_MOVE);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, rhs.reg);
        patch_u16(st->code, jmp_to_end, current_ip(st->code));
        return out;
    }

    emit_u8(st->code, KNC_OP_LOAD_BOOL);
    emit_u8(st->code, out.reg);
    emit_u8(st->code, 1);
    jmp_to_rhs = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, lhs.reg);
    jmp_to_end = emit_jump_placeholder(st->code, KNC_OP_JUMP);
    false_idx = current_ip(st->code);
    patch_u16(st->code, jmp_to_rhs, false_idx);
    rhs = compile_expr(st, e->v.binary.right);
    if (kn_diag_error_count() > 0)
        return out;
    emit_u8(st->code, KNC_OP_MOVE);
    emit_u8(st->code, out.reg);
    emit_u8(st->code, rhs.reg);
    true_idx = current_ip(st->code);
    patch_u16(st->code, jmp_to_end, true_idx);
    return out;
}

static KncValue compile_if_expr(KncFuncState *st, Expr *e)
{
    KncValue out;
    KncValue cond;
    KncValue then_value;
    KncValue else_value;
    int else_patch;
    int end_patch;

    out.reg = alloc_reg(st);
    out.type = e->type;

    cond = compile_expr(st, e->v.if_expr.cond);
    if (kn_diag_error_count() > 0)
        return out;

    else_patch = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, cond.reg);
    then_value = compile_expr(st, e->v.if_expr.then_expr);
    if (kn_diag_error_count() > 0)
        return out;
    if (then_value.reg != out.reg)
    {
        emit_u8(st->code, KNC_OP_MOVE);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, then_value.reg);
    }
    end_patch = emit_jump_placeholder(st->code, KNC_OP_JUMP);
    patch_u16(st->code, else_patch, current_ip(st->code));

    else_value = compile_expr(st, e->v.if_expr.else_expr);
    if (kn_diag_error_count() > 0)
        return out;
    if (else_value.reg != out.reg)
    {
        emit_u8(st->code, KNC_OP_MOVE);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, else_value.reg);
    }
    patch_u16(st->code, end_patch, current_ip(st->code));
    return out;
}

static KncValue compile_incdec(KncFuncState *st, Expr *e)
{
    KncLocal *local;
    KncValue out;
    int one_reg;
    int new_reg;
    int dst_reg;

    out.reg = -1;
    out.type = e->type;

    if (!e->v.unary.expr || e->v.unary.expr->kind != EXPR_VAR)
    {
        knc_diag_expr(st, e, "Only local variable ++/-- is supported by the bootstrap KNC emitter yet");
        return out;
    }

    local = find_local(st, e->v.unary.expr->v.var.name);
    if (!local)
    {
        knc_diag_expr(st, e, "Unknown local variable in ++/-- expression");
        return out;
    }

    if (type_is_integerish(local->type))
    {
        if (e->v.unary.is_postfix)
        {
            dst_reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_MOVE);
            emit_u8(st->code, dst_reg);
            emit_u8(st->code, local->reg);
            out.reg = dst_reg;
        }
        else
        {
            out.reg = local->reg;
        }

        emit_u8(st->code, e->v.unary.op == TOK_PLUSPLUS ? KNC_OP_INC_INT : KNC_OP_DEC_INT);
        emit_u8(st->code, local->reg);
        return out;
    }

    one_reg = alloc_reg(st);
    emit_u8(st->code, KNC_OP_LOAD_INT);
    emit_u8(st->code, one_reg);
    emit_u16(st->code, program_add_int(st->program, 1));

    new_reg = alloc_reg(st);
    emit_u8(st->code, e->v.unary.op == TOK_PLUSPLUS ? KNC_OP_ADD_INT : KNC_OP_SUB_INT);
    emit_u8(st->code, new_reg);
    emit_u8(st->code, local->reg);
    emit_u8(st->code, one_reg);

    if (e->v.unary.is_postfix)
    {
        dst_reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_MOVE);
        emit_u8(st->code, dst_reg);
        emit_u8(st->code, local->reg);
        out.reg = dst_reg;
    }
    else
    {
        out.reg = new_reg;
    }

    emit_u8(st->code, KNC_OP_MOVE);
    emit_u8(st->code, local->reg);
    emit_u8(st->code, new_reg);
    return out;
}

static int compile_store_member(KncFuncState *st, Expr *target, KncValue value)
{
    KncValue recv;

    if (!target || target->kind != EXPR_MEMBER || !target->v.member.recv)
    {
        knc_diag_expr(st, target, "Invalid member assignment target in bootstrap KNC emitter");
        return -1;
    }

    if (target->v.member.is_static)
    {
        int slot = program_find_static_field_slot(st->program, target->v.member.field_owner, target->v.member.field_index);
        if (slot < 0)
        {
            knc_diag_expr(st, target, "Static field assignment is not supported by the bootstrap KNC emitter yet");
            return -1;
        }
        emit_u8(st->code, KNC_OP_STORE_GLOBAL);
        emit_u8(st->code, value.reg);
        emit_u16(st->code, slot);
        return value.reg;
    }

    recv = compile_expr(st, target->v.member.recv);
    if (kn_diag_error_count() > 0)
        return -1;

    emit_u8(st->code, KNC_OP_STORE_FIELD);
    emit_u8(st->code, recv.reg);
    emit_u8(st->code, value.reg);
    emit_u16(st->code, target->v.member.field_index);
    return value.reg;
}

static int compile_store_index(KncFuncState *st, Expr *target, KncValue value)
{
    KncValue recv;
    KncValue idx;

    if (!target || target->kind != EXPR_INDEX)
    {
        knc_diag_expr(st, target, "Invalid index assignment target in bootstrap KNC emitter");
        return -1;
    }

    recv = compile_expr(st, target->v.index.recv);
    idx = compile_expr(st, target->v.index.index);
    if (kn_diag_error_count() > 0)
        return -1;

    emit_u8(st->code, KNC_OP_STORE_INDEX);
    emit_u8(st->code, recv.reg);
    emit_u8(st->code, idx.reg);
    emit_u8(st->code, value.reg);
    return value.reg;
}

static int compile_store_ptr(KncFuncState *st, Expr *target, KncValue value)
{
    KncValue ptr;

    if (!target || target->kind != EXPR_UNARY || target->v.unary.op != TOK_STAR || !target->v.unary.expr)
    {
        knc_diag_expr(st, target, "Invalid pointer assignment target in bootstrap KNC emitter");
        return -1;
    }

    ptr = compile_expr(st, target->v.unary.expr);
    if (kn_diag_error_count() > 0)
        return -1;

    emit_u8(st->code, KNC_OP_STORE_PTR);
    emit_u8(st->code, ptr.reg);
    emit_u8(st->code, value.reg);
    return value.reg;
}

static KncValue compile_expr(KncFuncState *st, Expr *e)
{
    KncValue out;
    out.reg = -1;
    out.type = e ? e->type : type_make(TY_UNKNOWN);

    if (!e)
    {
        knc_diag(st->src, 1, 1, "Internal KNC emitter error: missing expression");
        return out;
    }

    switch (e->kind)
    {
    case EXPR_INT:
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_INT);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, program_add_int(st->program, (int)e->v.int_val));
        return out;

    case EXPR_FLOAT:
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_FLOAT);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, program_add_float(st->program, e->v.float_val));
        return out;

    case EXPR_BOOL:
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_BOOL);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, e->v.int_val ? 1 : 0);
        return out;

    case EXPR_CHAR:
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_CHAR);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, program_add_int(st->program, (int)e->v.int_val));
        return out;

    case EXPR_STRING:
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_STRING);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, program_add_string(st->program, e->v.str_val.ptr ? e->v.str_val.ptr : ""));
        return out;

    case EXPR_NULL:
    case EXPR_DEFAULT:
        out.reg = alloc_reg(st);
        emit_default_value(st, out.reg, e->type);
        return out;

    case EXPR_THIS:
    case EXPR_BASE:
        if (!st->has_this)
        {
            KncLocal *this_local = find_local(st, "This");
            if (!this_local)
            {
                knc_diag_expr(st, e, "This receiver is not available in the bootstrap KNC emitter yet");
                return out;
            }
            out.type = e->type;
            if (this_local->storage == KNC_LOCAL_CAPTURE)
            {
                out.reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_LOAD_CAPTURE);
                emit_u8(st->code, out.reg);
                emit_u16(st->code, this_local->capture_slot);
                return out;
            }
            if (this_local->storage == KNC_LOCAL_CELL)
            {
                out.reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_LOAD_CELL);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, this_local->reg);
                return out;
            }
            out.reg = this_local->reg;
            return out;
        }
        out.reg = 0;
        out.type = e->type;
        return out;

    case EXPR_ARRAY:
    {
        Type elem_type = type_make(e->type.elem);
        int length = (e->type.kind == TY_ARRAY && e->type.array_len >= 0) ? (int)e->type.array_len : e->v.array.items.count;
        int len_reg;

        elem_type.name = e->type.name;
        out.reg = alloc_reg(st);
        len_reg = alloc_reg(st);

        emit_u8(st->code, KNC_OP_LOAD_INT);
        emit_u8(st->code, len_reg);
        emit_u16(st->code, program_add_int(st->program, length));
        emit_u8(st->code, KNC_OP_NEW_ARRAY);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, len_reg);
        emit_u8(st->code, value_kind_from_type(elem_type));

        for (int i = 0; i < length; i++)
        {
            KncValue idx;
            KncValue item;

            idx.reg = alloc_reg(st);
            idx.type = type_make(TY_INT);
            emit_u8(st->code, KNC_OP_LOAD_INT);
            emit_u8(st->code, idx.reg);
            emit_u16(st->code, program_add_int(st->program, i));

            if (i < e->v.array.items.count)
                item = compile_expr(st, e->v.array.items.items[i]);
            else
            {
                item.reg = alloc_reg(st);
                item.type = elem_type;
                emit_default_value(st, item.reg, elem_type);
            }
            if (kn_diag_error_count() > 0)
                return out;

            emit_u8(st->code, KNC_OP_STORE_INDEX);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, idx.reg);
            emit_u8(st->code, item.reg);
        }
        return out;
    }

    case EXPR_VAR:
    {
        KncLocal *local = find_local(st, e->v.var.name);
        if (!local)
        {
            if (st->has_this)
            {
                Type field_type = type_make(TY_UNKNOWN);
                int field_index = -1;
                if (st->self_type.kind == TY_CLASS)
                    field_index = class_find_field_index(st->program, program_find_class(st->program, st->self_type.name), e->v.var.name, &field_type);
                else if (st->self_type.kind == TY_STRUCT)
                    field_index = struct_find_field_index(program_find_struct(st->program, st->self_type.name), e->v.var.name, &field_type);
                if (field_index >= 0)
                {
                    out.reg = alloc_reg(st);
                    out.type = field_type;
                    emit_u8(st->code, KNC_OP_LOAD_FIELD);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, 0);
                    emit_u16(st->code, field_index);
                    return out;
                }
            }
            {
                int global_slot = program_find_global_slot(st->program, e->v.var.name);
                if (global_slot >= 0)
                {
                    out.reg = alloc_reg(st);
                    out.type = program_find_global_type(st->program, e->v.var.name);
                    emit_u8(st->code, KNC_OP_LOAD_GLOBAL);
                    emit_u8(st->code, out.reg);
                    emit_u16(st->code, global_slot);
                    return out;
                }
            }
            knc_diag_expr(st, e, "This variable reference is not supported by the bootstrap KNC emitter yet");
            return out;
        }
        if (local->storage == KNC_LOCAL_CAPTURE)
        {
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_CAPTURE);
            emit_u8(st->code, out.reg);
            emit_u16(st->code, local->capture_slot);
            out.type = local->type;
            return out;
        }
        if (local->storage == KNC_LOCAL_CELL)
        {
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_CELL);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, local->reg);
            out.type = local->type;
            return out;
        }
        out.reg = local->reg;
        out.type = local->type;
        return out;
    }

    case EXPR_MEMBER:
        if (e->v.member.is_enum_const)
        {
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_INT);
            emit_u8(st->code, out.reg);
            emit_u16(st->code, program_add_int(st->program, (int)e->v.member.enum_value));
            return out;
        }
        if (e->v.member.is_static)
        {
            int slot = program_find_static_field_slot(st->program, e->v.member.field_owner, e->v.member.field_index);
            if (slot < 0)
            {
                knc_diag_expr(st, e, "Static field access is not supported by the bootstrap KNC emitter yet");
                return out;
            }
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_GLOBAL);
            emit_u8(st->code, out.reg);
            emit_u16(st->code, slot);
            return out;
        }
        {
            KncValue recv = compile_expr(st, e->v.member.recv);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_LOAD_FIELD);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, recv.reg);
            emit_u16(st->code, e->v.member.field_index);
            return out;
        }

    case EXPR_ASSIGN:
        out.reg = compile_assign_to_local(st, e->v.assign.target, e->v.assign.value, e->type);
        return out;

    case EXPR_UNARY:
        if (e->v.unary.op == TOK_PLUSPLUS || e->v.unary.op == TOK_MINUSMINUS)
            return compile_incdec(st, e);
        if (e->v.unary.op == TOK_STAR)
        {
            KncValue ptr = compile_expr(st, e->v.unary.expr);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            out.type = e->type;
            emit_u8(st->code, KNC_OP_LOAD_PTR);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, ptr.reg);
            return out;
        }
        if (e->v.unary.op == TOK_AMP)
        {
            Expr *src = e->v.unary.expr;
            out.reg = alloc_reg(st);
            out.type = e->type;

            if (src && src->kind == EXPR_FUNC_REF)
            {
                int func_index = -1;
                if (src->v.func_ref.qname)
                    func_index = program_find_function(st->program, src->v.func_ref.qname);
                if (func_index < 0)
                {
                    knc_diag_expr(st, e, "This function address target is not supported by the bootstrap KNC emitter yet");
                    return out;
                }
                emit_u8(st->code, KNC_OP_MAKE_PTR_FUNC);
                emit_u8(st->code, out.reg);
                emit_u16(st->code, func_index);
                return out;
            }

            if (src && src->kind == EXPR_VAR)
            {
                KncLocal *local = find_local(st, src->v.var.name);
                if (local)
                {
                    if (local->storage == KNC_LOCAL_CAPTURE)
                    {
                        int cell_reg = alloc_reg(st);
                        emit_u8(st->code, KNC_OP_LOAD_CAPTURE_CELL);
                        emit_u8(st->code, cell_reg);
                        emit_u16(st->code, local->capture_slot);
                        emit_u8(st->code, KNC_OP_MAKE_PTR_CELL);
                        emit_u8(st->code, out.reg);
                        emit_u8(st->code, cell_reg);
                        return out;
                    }
                    if (local->storage == KNC_LOCAL_CELL)
                    {
                        emit_u8(st->code, KNC_OP_MAKE_PTR_CELL);
                        emit_u8(st->code, out.reg);
                        emit_u8(st->code, local->reg);
                        return out;
                    }
                    emit_u8(st->code, KNC_OP_MAKE_PTR_LOCAL);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, local->reg);
                    return out;
                }
                {
                    int global_slot = program_find_global_slot(st->program, src->v.var.name);
                    if (global_slot >= 0)
                    {
                        emit_u8(st->code, KNC_OP_MAKE_PTR_GLOBAL);
                        emit_u8(st->code, out.reg);
                        emit_u16(st->code, global_slot);
                        return out;
                    }
                }
            }

            if (src && src->kind == EXPR_INDEX)
            {
                KncValue recv = compile_expr(st, src->v.index.recv);
                KncValue idx = compile_expr(st, src->v.index.index);
                if (kn_diag_error_count() > 0)
                    return out;
                emit_u8(st->code, KNC_OP_MAKE_PTR_INDEX);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, recv.reg);
                emit_u8(st->code, idx.reg);
                return out;
            }

            if (src && src->kind == EXPR_MEMBER && !src->v.member.is_static)
            {
                KncValue recv = compile_expr(st, src->v.member.recv);
                if (kn_diag_error_count() > 0)
                    return out;
                emit_u8(st->code, KNC_OP_MAKE_PTR_FIELD);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, recv.reg);
                emit_u16(st->code, src->v.member.field_index);
                return out;
            }

            knc_diag_expr(st, e, "This address-of target is not supported by the bootstrap KNC emitter yet");
            return out;
        }
        if (e->v.unary.op == TOK_MINUS)
        {
            KncValue value = compile_expr(st, e->v.unary.expr);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, type_is_floatish(value.type) ? KNC_OP_NEG_FLOAT : KNC_OP_NEG_INT);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, value.reg);
            return out;
        }
        if (e->v.unary.op == TOK_NOT)
        {
            KncValue value = compile_expr(st, e->v.unary.expr);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_NOT_BOOL);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, value.reg);
            return out;
        }
        if (e->v.unary.op == TOK_TILDE)
        {
            KncValue value = compile_expr(st, e->v.unary.expr);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_BITNOT_INT);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, value.reg);
            return out;
        }
        knc_diag_expr(st, e, "This unary operator is not supported by the bootstrap KNC emitter yet");
        return out;

    case EXPR_BINARY:
        if (e->v.binary.op == TOK_ANDAND || e->v.binary.op == TOK_OROR)
            return compile_short_circuit(st, e);
        else
        {
            KncValue lhs = compile_expr(st, e->v.binary.left);
            KncValue rhs;
            int rhs_int_imm_index = -1;
            int use_float = type_is_floatish(e->v.binary.left->type) || type_is_floatish(e->v.binary.right->type);
            int has_rhs_int_imm = !use_float && expr_int_immediate_index(st, e->v.binary.right, &rhs_int_imm_index);
            int use_rhs_int_imm = 0;
            TokenType binary_op = (TokenType)e->v.binary.op;
            switch (binary_op)
            {
            case TOK_PLUS:
                use_rhs_int_imm = has_rhs_int_imm && e->type.kind != TY_PTR && e->type.kind != TY_STRING;
                break;
            case TOK_MINUS:
                use_rhs_int_imm = has_rhs_int_imm && e->type.kind != TY_PTR;
                break;
            case TOK_STAR:
            case TOK_SLASH:
            case TOK_LT:
            case TOK_LE:
            case TOK_GT:
            case TOK_GE:
            case TOK_AMP:
            case TOK_BITOR:
            case TOK_XOR:
            case TOK_SHL:
            case TOK_SHR:
                use_rhs_int_imm = has_rhs_int_imm;
                break;
            default:
                break;
            }
            rhs.reg = -1;
            rhs.type = e->v.binary.right ? e->v.binary.right->type : type_make(TY_UNKNOWN);
            if (!use_rhs_int_imm)
                rhs = compile_expr(st, e->v.binary.right);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);

            switch (binary_op)
            {
            case TOK_PLUS:
                if (e->type.kind == TY_PTR)
                {
                    KncValue ptrv = lhs.type.kind == TY_PTR ? lhs : rhs;
                    KncValue offv = lhs.type.kind == TY_PTR ? rhs : lhs;
                    emit_u8(st->code, KNC_OP_ADD_PTR);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, ptrv.reg);
                    emit_u8(st->code, offv.reg);
                    return out;
                }
                if (e->type.kind == TY_STRING)
                {
                    out.reg = alloc_reg(st);
                    emit_u8(st->code, KNC_OP_CALL_BUILTIN);
                    emit_u8(st->code, out.reg);
                    emit_u16(st->code, 20);
                    emit_u8(st->code, 2);
                    emit_u8(st->code, lhs.reg);
                    emit_u8(st->code, rhs.reg);
                    emit_u8(st->code, 255);
                    emit_u8(st->code, 255);
                    return out;
                }
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_ADD_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_ADD_FLOAT : KNC_OP_ADD_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_MINUS:
                if (e->type.kind == TY_PTR)
                {
                    int neg_reg = alloc_reg(st);
                    emit_u8(st->code, KNC_OP_NEG_INT);
                    emit_u8(st->code, neg_reg);
                    emit_u8(st->code, rhs.reg);
                    emit_u8(st->code, KNC_OP_ADD_PTR);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u8(st->code, neg_reg);
                    return out;
                }
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_SUB_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_SUB_FLOAT : KNC_OP_SUB_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_STAR:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_MUL_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_MUL_FLOAT : KNC_OP_MUL_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_SLASH:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_DIV_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_DIV_FLOAT : KNC_OP_DIV_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_PERCENT:
            {
                int div_reg = alloc_reg(st);
                int mul_reg = alloc_reg(st);
                if (use_float)
                {
                    knc_diag_expr(st, e, "Floating-point '%' is not supported by the bootstrap KNC emitter yet");
                    return out;
                }
                emit_u8(st->code, KNC_OP_DIV_INT);
                emit_u8(st->code, div_reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                emit_u8(st->code, KNC_OP_MUL_INT);
                emit_u8(st->code, mul_reg);
                emit_u8(st->code, div_reg);
                emit_u8(st->code, rhs.reg);
                emit_u8(st->code, KNC_OP_SUB_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, mul_reg);
                return out;
            }
            case TOK_EQ:
                emit_u8(st->code, KNC_OP_EQ);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_NE:
                emit_u8(st->code, KNC_OP_NE);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_LT:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_LT_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    out.type = type_make(TY_BOOL);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_LT_FLOAT : KNC_OP_LT_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_LE:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_LE_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    out.type = type_make(TY_BOOL);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_LE_FLOAT : KNC_OP_LE_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_GT:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_GT_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    out.type = type_make(TY_BOOL);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_GT_FLOAT : KNC_OP_GT_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_GE:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_GE_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    out.type = type_make(TY_BOOL);
                    return out;
                }
                emit_u8(st->code, use_float ? KNC_OP_GE_FLOAT : KNC_OP_GE_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                out.type = type_make(TY_BOOL);
                return out;
            case TOK_AMP:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_BITAND_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, KNC_OP_BITAND_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_BITOR:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_BITOR_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, KNC_OP_BITOR_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_XOR:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_BITXOR_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, KNC_OP_BITXOR_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_SHL:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_SHL_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, KNC_OP_SHL_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            case TOK_SHR:
                if (use_rhs_int_imm)
                {
                    emit_u8(st->code, KNC_OP_SHR_INT_IMM);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, lhs.reg);
                    emit_u16(st->code, rhs_int_imm_index);
                    return out;
                }
                emit_u8(st->code, KNC_OP_SHR_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, lhs.reg);
                emit_u8(st->code, rhs.reg);
                return out;
            default:
                knc_diag_expr(st, e, "This binary operator is not supported by the bootstrap KNC emitter yet");
                return out;
            }
        }

    case EXPR_IF:
        return compile_if_expr(st, e);

    case EXPR_SWITCH:
    {
        KncValue switch_value = compile_expr(st, e->v.switch_expr.value);
        PatchBuf end_patches = {0};
        Expr *default_body = 0;
        if (kn_diag_error_count() > 0)
            return out;
        out.reg = alloc_reg(st);
        out.type = e->type;

        for (int i = 0; i < e->v.switch_expr.cases.count; i++)
        {
            ExprSwitchCase *c = &e->v.switch_expr.cases.items[i];
            int next_patch;
            if (c->is_default)
            {
                default_body = c->body;
                continue;
            }
            {
                KncValue match = compile_expr(st, c->match);
                KncValue cond;
                KncValue bodyv;
                if (kn_diag_error_count() > 0)
                    return out;
                cond.reg = alloc_reg(st);
                cond.type = type_make(TY_BOOL);
                emit_u8(st->code, KNC_OP_EQ);
                emit_u8(st->code, cond.reg);
                emit_u8(st->code, switch_value.reg);
                emit_u8(st->code, match.reg);
                next_patch = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, cond.reg);
                bodyv = compile_expr(st, c->body);
                if (kn_diag_error_count() > 0)
                    return out;
                if (bodyv.reg != out.reg)
                {
                    emit_u8(st->code, KNC_OP_MOVE);
                    emit_u8(st->code, out.reg);
                    emit_u8(st->code, bodyv.reg);
                }
                patchbuf_push(&end_patches, emit_jump_placeholder(st->code, KNC_OP_JUMP));
                patch_u16(st->code, next_patch, current_ip(st->code));
            }
        }

        if (default_body)
        {
            KncValue dv = compile_expr(st, default_body);
            if (kn_diag_error_count() > 0)
                return out;
            if (dv.reg != out.reg)
            {
                emit_u8(st->code, KNC_OP_MOVE);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, dv.reg);
            }
        }
        patch_all(st->code, &end_patches, current_ip(st->code));
        return out;
    }

    case EXPR_IS:
    {
        KncValue value = compile_expr(st, e->v.is_expr.expr);
        if (kn_diag_error_count() > 0)
            return out;
        return compile_is_check_from_value(st, e, value, e->v.is_expr.target);
    }

    case EXPR_CALL:
        if (e->v.call.builtin_id != KN_BUILTIN_NONE)
            return compile_builtin_invoke(st, e->type, e->v.call.builtin_id, 0, 0, &e->v.call.args);
        return compile_direct_call(st, e, e->v.call.name, e->type, &e->v.call.args);

    case EXPR_MEMBER_CALL:
        if (e->v.member_call.builtin_id == KN_BUILTIN_BLOCK_RUN ||
            e->v.member_call.builtin_id == KN_BUILTIN_BLOCK_JUMP ||
            e->v.member_call.builtin_id == KN_BUILTIN_BLOCK_RUN_UNTIL ||
            e->v.member_call.builtin_id == KN_BUILTIN_BLOCK_JUMP_AND_RUN_UNTIL)
        {
            return compile_block_invoke(st, e, e->v.member_call.recv, e->v.member_call.builtin_id, &e->v.member_call.args);
        }
        if (e->v.member_call.builtin_id == KN_BUILTIN_ARRAY_LENGTH)
        {
            KncValue recv = compile_expr(st, e->v.member_call.recv);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_ARRAY_LENGTH);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, recv.reg);
            return out;
        }
        if (e->v.member_call.builtin_id == KN_BUILTIN_ARRAY_ADD)
        {
            KncValue recv = compile_expr(st, e->v.member_call.recv);
            KncValue value;
            if (kn_diag_error_count() > 0)
                return out;
            if (e->v.member_call.args.count != 1)
            {
                knc_diag_expr(st, e, "Bootstrap KNC emitter requires exactly one array Add argument");
                return out;
            }
            value = compile_expr(st, e->v.member_call.args.items[0]);
            if (kn_diag_error_count() > 0)
                return out;
            out.reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_ARRAY_ADD);
            emit_u8(st->code, out.reg);
            emit_u8(st->code, recv.reg);
            emit_u8(st->code, value.reg);
            return out;
        }
        if (e->v.member_call.builtin_id != KN_BUILTIN_NONE)
        {
            int is_instance_recv = e->v.member_call.recv && !e->v.member_call.is_static;
            return compile_builtin_invoke(st, e->type, e->v.member_call.builtin_id,
                                          e->v.member_call.recv, is_instance_recv, &e->v.member_call.args);
        }
        if (e->v.member_call.is_qual_call && e->v.member_call.qual_name)
            return compile_direct_call(st, e, e->v.member_call.qual_name, e->type, &e->v.member_call.args);
        if (e->v.member_call.is_static && e->v.member_call.method_owner)
        {
            int func_index = program_find_method_function(st->program, e->v.member_call.method_owner, e->v.member_call.method_index);
            return compile_call_func_index(st, e, func_index, e->type, &e->v.member_call.args);
        }
        if (e->v.member_call.recv && e->v.member_call.method_owner)
        {
            KncValue recv = compile_expr(st, e->v.member_call.recv);
            if (kn_diag_error_count() > 0)
                return out;
            if (e->v.member_call.recv->kind == EXPR_BASE ||
                !program_method_is_virtual(st->program, e->v.member_call.method_owner, e->v.member_call.method_index))
            {
                int func_index = program_find_method_function(st->program, e->v.member_call.method_owner, e->v.member_call.method_index);
                return compile_direct_method_call(st, e, func_index, e->type, recv, &e->v.member_call.args);
            }
            return compile_virtual_method_call(st, e, e->v.member_call.method_index, e->type, recv, &e->v.member_call.args);
        }
        knc_diag_expr(st, e, "This member call is not supported by the bootstrap KNC emitter yet");
        return out;

    case EXPR_INDEX:
    {
        KncValue recv = compile_expr(st, e->v.index.recv);
        KncValue idx = compile_expr(st, e->v.index.index);
        if (kn_diag_error_count() > 0)
            return out;
        out.reg = alloc_reg(st);
        emit_u8(st->code, KNC_OP_LOAD_INDEX);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, recv.reg);
        emit_u8(st->code, idx.reg);
        return out;
    }

    case EXPR_NEW:
    {
        int type_index = program_find_type(st->program, e->v.new_expr.class_name);
        int ctor_func_index = program_find_method_function(st->program, e->v.new_expr.class_name, e->v.new_expr.ctor_index);
        out.reg = alloc_reg(st);
        if (type_index < 0)
        {
            knc_diag_expr(st, e, "This class type is not supported by the bootstrap KNC emitter yet");
            return out;
        }
        emit_u8(st->code, KNC_OP_NEW_OBJECT);
        emit_u8(st->code, out.reg);
        emit_u16(st->code, type_index);
        if (e->v.new_expr.ctor_index >= 0)
            (void)compile_direct_method_call(st, e, ctor_func_index, type_make(TY_VOID), out, &e->v.new_expr.args);
        return out;
    }

    case EXPR_FUNC_REF:
    {
        int function_type_index = program_find_type(st->program, "IO.Type.Object.Function");
        int target_kind = 0;
        int target_index = -1;
        out.reg = alloc_reg(st);
        out.type = e->type;

        if (function_type_index < 0)
        {
            knc_diag_expr(st, e, "Function object type is not available in the bootstrap KNC emitter yet");
            return out;
        }

        if (e->v.func_ref.builtin_id != KN_BUILTIN_NONE)
        {
            target_kind = 1;
            target_index = map_builtin_id(e->v.func_ref.builtin_id);
        }
        else if (e->v.func_ref.qname)
        {
            target_index = program_find_function(st->program, e->v.func_ref.qname);
            if (target_index < 0)
            {
                target_kind = 1;
                target_index = map_runtime_extern_symbol(e->v.func_ref.qname);
            }
        }

        if (target_index < 0)
        {
            knc_diag_expr(st, e, "This function reference target is not supported by the bootstrap KNC emitter yet");
            return out;
        }

        emit_make_function(st, out.reg, function_type_index, target_kind, target_index, 255);
        return out;
    }

    case EXPR_ANON_FUNC:
    {
        int function_type_index = program_find_type(st->program, "IO.Type.Object.Function");
        int func_index = ensure_anon_function_record(st, e);
        CaptureBuf captures;
        int env_reg;

        out.reg = alloc_reg(st);
        out.type = e->type;

        if (function_type_index < 0)
        {
            knc_diag_expr(st, e, "Function object type is not available in the bootstrap KNC emitter yet");
            return out;
        }
        if (func_index < 0)
        {
            knc_diag_expr(st, e, "This anonymous function is not supported by the bootstrap KNC emitter yet");
            return out;
        }

        kn_memset(&captures, 0, sizeof(captures));
        collect_visible_captures(st, &captures);
        env_reg = build_capture_env(st, &captures, 0);
        if (kn_diag_error_count() > 0)
            return out;

        emit_make_function(st, out.reg, function_type_index, 0, func_index, env_reg);
        return out;
    }

    case EXPR_BLOCK_LITERAL:
    {
        int block_type_index = program_find_type(st->program, "IO.Type.Object.Block");
        int func_index = ensure_block_function_record(st, e);
        CaptureBuf captures;
        int env_reg;

        out.reg = alloc_reg(st);
        out.type = e->type;

        if (block_type_index < 0)
        {
            knc_diag_expr(st, e, "Block object type is not available in the bootstrap KNC emitter yet");
            return out;
        }
        if (func_index < 0)
        {
            knc_diag_expr(st, e, "This block literal is not supported by the bootstrap KNC emitter yet");
            return out;
        }

        kn_memset(&captures, 0, sizeof(captures));
        collect_visible_captures(st, &captures);
        for (int i = 0; i < captures.count; i++)
            captures.items[i].slot = i + 1;
        env_reg = build_capture_env(st, &captures, 1);
        if (kn_diag_error_count() > 0)
            return out;

        emit_make_function(st, out.reg, block_type_index, 0, func_index, env_reg);
        return out;
    }

    case EXPR_INVOKE:
        if (e->v.invoke.callee && e->v.invoke.callee->kind == EXPR_FUNC_REF)
        {
            if (e->v.invoke.callee->v.func_ref.builtin_id != KN_BUILTIN_NONE)
                return compile_builtin_invoke(st, e->type, e->v.invoke.callee->v.func_ref.builtin_id,
                                              0, 0, &e->v.invoke.args);
            if (e->v.invoke.callee->v.func_ref.qname)
                return compile_direct_call(st, e, e->v.invoke.callee->v.func_ref.qname, e->type, &e->v.invoke.args);
        }
    {
        KncValue callee = compile_expr(st, e->v.invoke.callee);
        int arg_regs[4] = { 255, 255, 255, 255 };
        int arg_count = 0;
        if (kn_diag_error_count() > 0)
            return out;
        if (!(callee.type.kind == TY_CLASS && callee.type.name &&
              kn_strcmp(callee.type.name, "IO.Type.Object.Function") == 0))
        {
            knc_diag_expr(st, e, "This invoke expression is not supported by the bootstrap KNC emitter yet");
            return out;
        }
        for (int i = 0; i < e->v.invoke.args.count; i++)
        {
            KncValue av;
            if (arg_count >= 4)
            {
                knc_diag_expr(st, e->v.invoke.args.items[i], "KNC bootstrap VM currently supports at most four function call arguments");
                return out;
            }
            av = compile_expr(st, e->v.invoke.args.items[i]);
            if (kn_diag_error_count() > 0)
                return out;
            arg_regs[arg_count++] = av.reg;
        }
        out.reg = e->type.kind == TY_VOID ? 255 : alloc_reg(st);
        out.type = e->type;
        emit_u8(st->code, KNC_OP_CALL_CLOSURE);
        emit_u8(st->code, out.reg);
        emit_u8(st->code, callee.reg);
        emit_u8(st->code, arg_count);
        emit_u8(st->code, arg_regs[0]);
        emit_u8(st->code, arg_regs[1]);
        emit_u8(st->code, arg_regs[2]);
        emit_u8(st->code, arg_regs[3]);
        return out;
    }

    case EXPR_CAST:
    {
        KncValue value = compile_expr(st, e->v.cast.expr);
        if (kn_diag_error_count() > 0)
            return out;
        if (knc_type_equal(value.type, e->type))
        {
            out = value;
            out.type = e->type;
            return out;
        }

        out.reg = alloc_reg(st);
        out.type = e->type;

        if (e->type.kind == TY_STRING)
        {
            if (value.type.kind == TY_INT || value.type.kind == TY_I8 || value.type.kind == TY_I16 ||
                value.type.kind == TY_I32 || value.type.kind == TY_I64 || value.type.kind == TY_U8 ||
                value.type.kind == TY_U16 || value.type.kind == TY_U32 || value.type.kind == TY_U64 ||
                value.type.kind == TY_ISIZE || value.type.kind == TY_USIZE || value.type.kind == TY_ENUM)
            {
                emit_u8(st->code, KNC_OP_INT_TO_STRING);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_BOOL)
            {
                emit_u8(st->code, KNC_OP_BOOL_TO_STRING);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_CHAR)
            {
                emit_u8(st->code, KNC_OP_CHAR_TO_STRING);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (type_is_floatish(value.type))
            {
                emit_u8(st->code, KNC_OP_FLOAT_TO_STRING);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
        }

        if (e->type.kind == TY_CHAR)
        {
            if (type_is_integerish(value.type))
            {
                emit_u8(st->code, KNC_OP_INT_TO_CHAR);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_STRING)
            {
                emit_u8(st->code, KNC_OP_STRING_TO_CHAR);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (type_is_floatish(value.type))
            {
                int int_reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_FLOAT_TO_INT);
                emit_u8(st->code, int_reg);
                emit_u8(st->code, value.reg);
                emit_u8(st->code, KNC_OP_INT_TO_CHAR);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, int_reg);
                return out;
            }
        }

        if (type_is_integerish(e->type) && e->type.kind != TY_CHAR)
        {
            if (value.type.kind == TY_STRING)
            {
                emit_u8(st->code, KNC_OP_STRING_TO_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_CHAR)
            {
                emit_u8(st->code, KNC_OP_CHAR_TO_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                out.type = e->type;
                return out;
            }
            if (value.type.kind == TY_BOOL)
            {
                emit_u8(st->code, KNC_OP_BOOL_TO_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                out.type = e->type;
                return out;
            }
            if (type_is_floatish(value.type))
            {
                emit_u8(st->code, KNC_OP_FLOAT_TO_INT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                out.type = e->type;
                return out;
            }
        }

        if (type_is_floatish(e->type))
        {
            if (type_is_integerish(value.type))
            {
                emit_u8(st->code, KNC_OP_INT_TO_FLOAT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_CHAR)
            {
                int int_reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_CHAR_TO_INT);
                emit_u8(st->code, int_reg);
                emit_u8(st->code, value.reg);
                emit_u8(st->code, KNC_OP_INT_TO_FLOAT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, int_reg);
                return out;
            }
            if (value.type.kind == TY_BOOL)
            {
                emit_u8(st->code, KNC_OP_BOOL_TO_FLOAT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_STRING)
            {
                emit_u8(st->code, KNC_OP_STRING_TO_FLOAT);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
        }

        if (e->type.kind == TY_BOOL)
        {
            if (value.type.kind == TY_STRING)
            {
                emit_u8(st->code, KNC_OP_STRING_TO_BOOL);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (type_is_floatish(value.type))
            {
                emit_u8(st->code, KNC_OP_FLOAT_TO_BOOL);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                return out;
            }
            if (value.type.kind == TY_CHAR)
            {
                int int_reg = alloc_reg(st);
                int zero_reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_CHAR_TO_INT);
                emit_u8(st->code, int_reg);
                emit_u8(st->code, value.reg);
                emit_u8(st->code, KNC_OP_LOAD_INT);
                emit_u8(st->code, zero_reg);
                emit_u16(st->code, program_add_int(st->program, 0));
                emit_u8(st->code, KNC_OP_NE);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, int_reg);
                emit_u8(st->code, zero_reg);
                return out;
            }
            if (type_is_integerish(value.type))
            {
                int zero_reg = alloc_reg(st);
                emit_u8(st->code, KNC_OP_LOAD_INT);
                emit_u8(st->code, zero_reg);
                emit_u16(st->code, program_add_int(st->program, 0));
                emit_u8(st->code, KNC_OP_NE);
                emit_u8(st->code, out.reg);
                emit_u8(st->code, value.reg);
                emit_u8(st->code, zero_reg);
                return out;
            }
        }

        knc_diag_expr(st, e, "This cast is not supported by the bootstrap KNC emitter yet");
        return out;
    }

    default:
        knc_diag_expr(st, e, "This expression kind is not supported by the bootstrap KNC emitter yet");
        return out;
    }
}

static void patch_all(ByteBuf *code, PatchBuf *patches, int target)
{
    if (!patches)
        return;
    for (int i = 0; i < patches->count; i++)
        patch_u16(code, patches->items[i], target);
    patches->count = 0;
}

static LoopState *current_loop(KncFuncState *st)
{
    if (st->loops.count <= 0)
        return 0;
    return &st->loops.items[st->loops.count - 1];
}

static int compile_stmt(KncFuncState *st, Stmt *s)
{
    int saved_locals;
    if (!s)
        return 0;

    switch (s->kind)
    {
    case ST_BLOCK:
        saved_locals = st->locals.count;
        for (int i = 0; i < s->v.block.stmts.count; i++)
        {
            if (compile_stmt(st, s->v.block.stmts.items[i]) != 0)
                return -1;
        }
        st->locals.count = saved_locals;
        return 0;

    case ST_VAR:
    {
        Type local_type = s->v.var.type.kind == TY_UNKNOWN && s->v.var.init ? s->v.var.init->type : s->v.var.type;
        if (s->v.var.init)
        {
            int reg;
            Type saved_init_type = s->v.var.init->type;
            if (local_type.kind == TY_ARRAY && s->v.var.init->kind == EXPR_ARRAY)
                s->v.var.init->type = local_type;
            KncValue value = compile_expr(st, s->v.var.init);
            if (local_type.kind == TY_ARRAY && s->v.var.init->kind == EXPR_ARRAY)
                s->v.var.init->type = saved_init_type;
            if (kn_diag_error_count() > 0)
                return -1;
            reg = alloc_reg(st);
            add_local(st, s->v.var.name, local_type, reg);
            if (value.reg != reg)
            {
                emit_u8(st->code, KNC_OP_MOVE);
                emit_u8(st->code, reg);
                emit_u8(st->code, value.reg);
            }
        }
        else
        {
            int reg = alloc_reg(st);
            add_local(st, s->v.var.name, local_type, reg);
            emit_default_value(st, reg, local_type);
        }
        return 0;
    }

    case ST_ASSIGN:
    {
        KncLocal *local = find_local(st, s->v.assign.name);
        KncValue value;
        if (!local)
        {
            if (st->has_this)
            {
                Type field_type = type_make(TY_UNKNOWN);
                int field_index = -1;
                if (st->self_type.kind == TY_CLASS)
                    field_index = class_find_field_index(st->program, program_find_class(st->program, st->self_type.name), s->v.assign.name, &field_type);
                else if (st->self_type.kind == TY_STRUCT)
                    field_index = struct_find_field_index(program_find_struct(st->program, st->self_type.name), s->v.assign.name, &field_type);
                if (field_index >= 0)
                {
                    value = compile_expr(st, s->v.assign.value);
                    if (kn_diag_error_count() > 0)
                        return -1;
                    emit_u8(st->code, KNC_OP_STORE_FIELD);
                    emit_u8(st->code, 0);
                    emit_u8(st->code, value.reg);
                    emit_u16(st->code, field_index);
                    return 0;
                }
            }
            {
                int global_slot = program_find_global_slot(st->program, s->v.assign.name);
                if (global_slot >= 0)
                {
                    value = compile_expr(st, s->v.assign.value);
                    if (kn_diag_error_count() > 0)
                        return -1;
                    emit_u8(st->code, KNC_OP_STORE_GLOBAL);
                    emit_u8(st->code, value.reg);
                    emit_u16(st->code, global_slot);
                    return 0;
                }
            }
            knc_diag_stmt(st, s, "Only local variable assignment is supported by the bootstrap KNC emitter yet");
            return -1;
        }
        value = compile_expr(st, s->v.assign.value);
        if (kn_diag_error_count() > 0)
            return -1;
        if (value.reg != local->reg)
        {
            emit_u8(st->code, KNC_OP_MOVE);
            emit_u8(st->code, local->reg);
            emit_u8(st->code, value.reg);
        }
        return 0;
    }

    case ST_EXPR:
        (void)compile_expr(st, s->v.expr.expr);
        return kn_diag_error_count() > 0 ? -1 : 0;

    case ST_RETURN:
        if (s->v.ret.expr)
        {
            KncValue value = compile_expr(st, s->v.ret.expr);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, KNC_OP_RET);
            emit_u8(st->code, value.reg);
        }
        else
        {
            emit_u8(st->code, KNC_OP_RET);
            emit_u8(st->code, 255);
        }
        return 0;

    case ST_THROW:
    {
        KncValue value = compile_expr(st, s->v.throws.expr);
        if (kn_diag_error_count() > 0)
            return -1;
        emit_u8(st->code, KNC_OP_THROW);
        emit_u8(st->code, value.reg);
        return 0;
    }

    case ST_TRY:
    {
        int saved_try_locals = st->locals.count;
        int catch_reg = 255;
        int catch_kind = KNC_IS_TARGET_ANY;
        int catch_payload = 0;
        int catch_patch;
        int end_patch;

        if (map_catch_target(st, s->v.trys.catch_type, &catch_kind, &catch_payload) != 0)
            return -1;

        if (s->v.trys.has_param)
            catch_reg = alloc_reg(st);

        catch_patch = emit_try_placeholder(st, catch_reg, catch_kind, catch_payload);
        if (compile_stmt(st, s->v.trys.try_block) != 0)
            return -1;
        emit_u8(st->code, KNC_OP_POP_TRY);
        end_patch = emit_jump_placeholder(st->code, KNC_OP_JUMP);
        patch_u16(st->code, catch_patch, current_ip(st->code));

        if (s->v.trys.has_param)
            add_local(st, s->v.trys.catch_name, s->v.trys.catch_type, catch_reg);
        if (compile_stmt(st, s->v.trys.catch_block) != 0)
            return -1;
        st->locals.count = saved_try_locals;
        patch_u16(st->code, end_patch, current_ip(st->code));
        return 0;
    }

    case ST_IF:
    {
        int saved_locals = st->locals.count;
        int has_pattern = s->v.ifs.pattern_name &&
                          s->v.ifs.cond &&
                          s->v.ifs.cond->kind == EXPR_IS;
        KncValue cond;
        KncValue pattern_value;
        int else_patch;
        int end_patch = -1;
        pattern_value.reg = -1;
        pattern_value.type = type_make(TY_UNKNOWN);
        if (s->v.ifs.is_const)
        {
            Stmt *branch = s->v.ifs.const_value ? s->v.ifs.then_s : s->v.ifs.else_s;
            if (branch && compile_stmt(st, branch) != 0)
                return -1;
            st->locals.count = saved_locals;
            return 0;
        }
        if (has_pattern)
        {
            pattern_value = compile_expr(st, s->v.ifs.cond->v.is_expr.expr);
            if (kn_diag_error_count() > 0)
                return -1;
            cond = compile_is_check_from_value(st, s->v.ifs.cond, pattern_value, s->v.ifs.cond->v.is_expr.target);
        }
        else
        {
            cond = compile_expr(st, s->v.ifs.cond);
        }
        if (kn_diag_error_count() > 0)
            return -1;
        else_patch = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, cond.reg);
        if (has_pattern)
        {
            int pat_reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_MOVE);
            emit_u8(st->code, pat_reg);
            emit_u8(st->code, pattern_value.reg);
            add_local(st, s->v.ifs.pattern_name, s->v.ifs.cond->v.is_expr.target, pat_reg);
        }
        if (compile_stmt(st, s->v.ifs.then_s) != 0)
            return -1;
        st->locals.count = saved_locals;
        if (s->v.ifs.else_s)
            end_patch = emit_jump_placeholder(st->code, KNC_OP_JUMP);
        patch_u16(st->code, else_patch, current_ip(st->code));
        if (s->v.ifs.else_s)
        {
            if (compile_stmt(st, s->v.ifs.else_s) != 0)
                return -1;
            st->locals.count = saved_locals;
            patch_u16(st->code, end_patch, current_ip(st->code));
        }
        return 0;
    }

    case ST_SWITCH:
    {
        KncValue switch_value = compile_expr(st, s->v.switchs.value);
        PatchBuf end_patches = {0};
        Stmt *default_body = 0;
        if (kn_diag_error_count() > 0)
            return -1;

        for (int i = 0; i < s->v.switchs.cases.count; i++)
        {
            StmtSwitchCase *c = &s->v.switchs.cases.items[i];
            int next_patch;
            if (c->is_default)
            {
                default_body = c->body;
                continue;
            }
            {
                KncValue match = compile_expr(st, c->match);
                KncValue cond;
                if (kn_diag_error_count() > 0)
                    return -1;
                cond.reg = alloc_reg(st);
                cond.type = type_make(TY_BOOL);
                emit_u8(st->code, KNC_OP_EQ);
                emit_u8(st->code, cond.reg);
                emit_u8(st->code, switch_value.reg);
                emit_u8(st->code, match.reg);
                next_patch = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, cond.reg);
                if (compile_stmt(st, c->body) != 0)
                    return -1;
                patchbuf_push(&end_patches, emit_jump_placeholder(st->code, KNC_OP_JUMP));
                patch_u16(st->code, next_patch, current_ip(st->code));
            }
        }

        if (default_body)
        {
            if (compile_stmt(st, default_body) != 0)
                return -1;
        }
        patch_all(st->code, &end_patches, current_ip(st->code));
        return 0;
    }

    case ST_WHILE:
    {
        KncLocal *reduce_sum_local = 0;
        KncLocal *reduce_counter_local = 0;
        Expr *reduce_limit = 0;
        KncOpCode reduce_op = (KncOpCode)0;
        KncLocal *array_super_local = 0;
        KncLocal *array_sum_local = 0;
        KncLocal *counter_super_local = 0;
        Expr *array_super_limit = 0;
        KncOpCode array_super_op = (KncOpCode)0;
        int cond_ip = current_ip(st->code);
        int body_ip;
        int end_patch;
        KncLocal *fused_local = 0;
        Expr *fused_limit = 0;
        KncOpCode fused_op = (KncOpCode)0;
        LoopState loop;

        if (knc_match_sum_superloop_while(st, s, &reduce_sum_local, &reduce_counter_local, &reduce_limit, &reduce_op))
        {
            KncValue limit = compile_expr(st, reduce_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, reduce_op);
            emit_u8(st->code, reduce_sum_local->reg);
            emit_u8(st->code, reduce_counter_local->reg);
            emit_u8(st->code, limit.reg);
            return 0;
        }
        if (knc_match_array_fill_superloop_while(st, s, &array_super_local, &counter_super_local, &array_super_limit, &array_super_op))
        {
            KncValue limit = compile_expr(st, array_super_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, array_super_op);
            emit_u8(st->code, array_super_local->reg);
            emit_u8(st->code, counter_super_local->reg);
            emit_u8(st->code, limit.reg);
            return 0;
        }
        if (knc_match_array_sum_superloop_while(st, s, &array_sum_local, &array_super_local, &counter_super_local, &array_super_limit, &array_super_op))
        {
            KncValue limit = compile_expr(st, array_super_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, array_super_op);
            emit_u8(st->code, array_sum_local->reg);
            emit_u8(st->code, array_super_local->reg);
            emit_u8(st->code, counter_super_local->reg);
            emit_u8(st->code, limit.reg);
            return 0;
        }

        kn_memset(&loop, 0, sizeof(loop));
        loop.continue_ip = cond_ip;
        end_patch = emit_condition_false_branch(st, s->v.whiles.cond);
        if (kn_diag_error_count() > 0 || end_patch < 0)
            return -1;
        body_ip = current_ip(st->code);
        (void)knc_match_fused_backedge_while(st, s, &fused_local, &fused_limit, &fused_op);
        loopbuf_push(&st->loops, loop);
        if (fused_local && fused_limit)
        {
            Stmt *body = s->v.whiles.body;
            for (int i = 0; i + 1 < body->v.block.stmts.count; i++)
                if (compile_stmt(st, body->v.block.stmts.items[i]) != 0)
                    return -1;
        }
        else if (compile_stmt(st, s->v.whiles.body) != 0)
        {
            return -1;
        }
        patch_all(st->code, &current_loop(st)->continue_patches, cond_ip);
        if (fused_local && fused_limit)
        {
            KncValue limit = compile_expr(st, fused_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, fused_op);
            emit_u8(st->code, fused_local->reg);
            emit_u8(st->code, limit.reg);
            emit_u16(st->code, body_ip);
        }
        else
        {
            emit_u8(st->code, KNC_OP_JUMP);
            emit_u16(st->code, cond_ip);
        }
        patch_u16(st->code, end_patch, current_ip(st->code));
        patch_all(st->code, &current_loop(st)->break_patches, current_ip(st->code));
        st->loops.count--;
        return 0;
    }

    case ST_FOR:
    {
        int saved_for_locals = st->locals.count;
        int cond_ip;
        int body_ip;
        int end_patch = -1;
        KncLocal *super_local = 0;
        Expr *super_limit = 0;
        KncOpCode super_op = (KncOpCode)0;
        KncLocal *reduce_sum_local = 0;
        KncLocal *reduce_counter_local = 0;
        Expr *reduce_limit = 0;
        KncOpCode reduce_op = (KncOpCode)0;
        KncLocal *array_super_local = 0;
        KncLocal *array_sum_local = 0;
        KncLocal *counter_super_local = 0;
        Expr *array_super_limit = 0;
        KncOpCode array_super_op = (KncOpCode)0;
        KncLocal *fused_local = 0;
        Expr *fused_limit = 0;
        KncOpCode fused_op = (KncOpCode)0;
        LoopState loop;
        kn_memset(&loop, 0, sizeof(loop));

        if (s->v.fors.init && compile_stmt(st, s->v.fors.init) != 0)
            return -1;

        if (knc_match_sum_superloop_for(st, s, &reduce_sum_local, &reduce_counter_local, &reduce_limit, &reduce_op))
        {
            KncValue limit = compile_expr(st, reduce_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, reduce_op);
            emit_u8(st->code, reduce_sum_local->reg);
            emit_u8(st->code, reduce_counter_local->reg);
            emit_u8(st->code, limit.reg);
            st->locals.count = saved_for_locals;
            return 0;
        }

        if (knc_match_array_fill_superloop_for(st, s, &array_super_local, &counter_super_local, &array_super_limit, &array_super_op))
        {
            KncValue limit = compile_expr(st, array_super_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, array_super_op);
            emit_u8(st->code, array_super_local->reg);
            emit_u8(st->code, counter_super_local->reg);
            emit_u8(st->code, limit.reg);
            st->locals.count = saved_for_locals;
            return 0;
        }
        if (knc_match_array_sum_superloop_for(st, s, &array_sum_local, &array_super_local, &counter_super_local, &array_super_limit, &array_super_op))
        {
            KncValue limit = compile_expr(st, array_super_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, array_super_op);
            emit_u8(st->code, array_sum_local->reg);
            emit_u8(st->code, array_super_local->reg);
            emit_u8(st->code, counter_super_local->reg);
            emit_u8(st->code, limit.reg);
            st->locals.count = saved_for_locals;
            return 0;
        }

        if (knc_match_empty_superloop_for(st, s, &super_local, &super_limit, &super_op))
        {
            KncValue limit = compile_expr(st, super_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, super_op);
            emit_u8(st->code, super_local->reg);
            emit_u8(st->code, limit.reg);
            st->locals.count = saved_for_locals;
            return 0;
        }

        cond_ip = current_ip(st->code);
        loop.continue_ip = -1;
        if (s->v.fors.cond)
        {
            end_patch = emit_condition_false_branch(st, s->v.fors.cond);
            if (kn_diag_error_count() > 0 || end_patch < 0)
                return -1;
        }
        body_ip = current_ip(st->code);
        (void)knc_match_fused_backedge_for(st, s, &fused_local, &fused_limit, &fused_op);
        loopbuf_push(&st->loops, loop);

        if (compile_stmt(st, s->v.fors.body) != 0)
            return -1;

        current_loop(st)->continue_ip = current_ip(st->code);
        patch_all(st->code, &current_loop(st)->continue_patches, current_loop(st)->continue_ip);
        if (fused_local && fused_limit)
        {
            KncValue limit = compile_expr(st, fused_limit);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, fused_op);
            emit_u8(st->code, fused_local->reg);
            emit_u8(st->code, limit.reg);
            emit_u16(st->code, body_ip);
        }
        else if (s->v.fors.post)
        {
            (void)compile_expr(st, s->v.fors.post);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_u8(st->code, KNC_OP_JUMP);
            emit_u16(st->code, cond_ip);
        }
        else
        {
            emit_u8(st->code, KNC_OP_JUMP);
            emit_u16(st->code, cond_ip);
        }

        if (end_patch >= 0)
            patch_u16(st->code, end_patch, current_ip(st->code));
        patch_all(st->code, &current_loop(st)->break_patches, current_ip(st->code));
        st->loops.count--;
        st->locals.count = saved_for_locals;
        return 0;
    }

    case ST_BREAK:
    {
        LoopState *loop = current_loop(st);
        if (!loop)
        {
            knc_diag_stmt(st, s, "Break is only valid inside loops in the bootstrap KNC emitter");
            return -1;
        }
        patchbuf_push(&loop->break_patches, emit_jump_placeholder(st->code, KNC_OP_JUMP));
        return 0;
    }

    case ST_CONTINUE:
    {
        LoopState *loop = current_loop(st);
        int patch_pos;
        if (!loop)
        {
            knc_diag_stmt(st, s, "Continue is only valid inside loops in the bootstrap KNC emitter");
            return -1;
        }
        patch_pos = emit_jump_placeholder(st->code, KNC_OP_JUMP);
        if (loop->continue_ip >= 0)
            patch_u16(st->code, patch_pos, loop->continue_ip);
        else
            patchbuf_push(&loop->continue_patches, patch_pos);
        return 0;
    }

    case ST_BLOCK_RECORD:
        if (!st->in_runtime_block || !st->block_records)
            return 0;
        {
            int idx = block_record_slot(st, s->v.record.ip);
            int rec_ip_reg;
            int cmp_reg;
            int cont_patch;
            if (idx >= 0)
                st->block_record_addrs[idx] = current_ip(st->code);
            rec_ip_reg = emit_int_const_reg(st, s->v.record.ip);
            cmp_reg = alloc_reg(st);
            emit_u8(st->code, KNC_OP_EQ);
            emit_u8(st->code, cmp_reg);
            emit_u8(st->code, st->block_until_reg);
            emit_u8(st->code, rec_ip_reg);
            cont_patch = emit_branch_placeholder(st->code, KNC_OP_JUMP_IF_FALSE, cmp_reg);
            emit_u8(st->code, KNC_OP_RET);
            emit_u8(st->code, 255);
            patch_u16(st->code, cont_patch, current_ip(st->code));
            return 0;
        }

    case ST_BLOCK_JUMP:
        if (!st->in_runtime_block)
            return 0;
        {
            KncValue target = compile_expr(st, s->v.jump.target);
            if (kn_diag_error_count() > 0)
                return -1;
            emit_block_dispatch(st, target.reg);
            return 0;
        }

    default:
        knc_diag_stmt(st, s, "This statement kind is not supported by the bootstrap KNC emitter yet");
        return -1;
    }
}

static int compile_function_body(KncProgram *program, KncFuncRecord *record)
{
    KncFuncState st;
    ParamList *params;
    Stmt *body;
    Type ret_type;

    kn_memset(&st, 0, sizeof(st));
    st.program = program;
    st.func = record->func;
    st.src = record_src(record) ? record_src(record) : program->fallback_src;
    st.func_index = record->index;
    st.code = &record->code;
    st.next_reg = 0;
    st.max_reg = 0;
    params = record_params(record);
    body = record_body(record);
    ret_type = record_return_type(record);

    if (record->callable_kind == KNC_CALLABLE_METHOD && record->is_instance)
    {
        st.has_this = 1;
        st.self_type = record_receiver_type(record);
        add_local(&st, "This", st.self_type, st.next_reg);
        st.next_reg++;
        st.max_reg = st.next_reg;
    }

    for (int i = 0; i < record->captures.count; i++)
        add_capture_local(&st, record->captures.items[i].name, record->captures.items[i].type, record->captures.items[i].slot);

    for (int i = 0; params && i < params->count; i++)
    {
        Param *param = &params->items[i];
        if (!type_is_vm_value(param->type))
        {
            knc_diag(st.src, param->line, param->col,
                     "This parameter type is not supported by the bootstrap KNC emitter yet");
            return -1;
        }
        add_local(&st, param->name, param->type, st.next_reg);
        st.next_reg++;
        st.max_reg = st.next_reg;
    }

    if (record->synthetic_is_block)
    {
        KncLocal *start_local = find_local(&st, "__knc_block_start");
        KncLocal *until_local = find_local(&st, "__knc_block_until");
        int record_count = record->synthetic_block_records ? record->synthetic_block_records->count : 0;

        if (!start_local || !until_local)
        {
            knc_diag(st.src, 1, 1, "Internal KNC emitter error: missing synthetic block parameters");
            return -1;
        }

        st.in_runtime_block = 1;
        st.block_start_reg = start_local->reg;
        st.block_until_reg = until_local->reg;
        st.block_records = record->synthetic_block_records;
        st.block_body_start_ip = -1;

        if (record_count > 0)
        {
            st.block_record_patches = (PatchBuf *)kn_malloc(sizeof(PatchBuf) * (size_t)record_count);
            st.block_record_addrs = (int *)kn_malloc(sizeof(int) * (size_t)record_count);
            if (!st.block_record_patches || !st.block_record_addrs)
                kn_die("out of memory");
            kn_memset(st.block_record_patches, 0, sizeof(PatchBuf) * (size_t)record_count);
            for (int i = 0; i < record_count; i++)
                st.block_record_addrs[i] = -1;
        }

        emit_block_dispatch(&st, st.block_start_reg);
        st.block_body_start_ip = current_ip(st.code);
    }

    if (body)
    {
        if (compile_stmt(&st, body) != 0)
            return -1;
    }

    if (record->synthetic_is_block)
    {
        patch_all(st.code, &st.block_start_patches, st.block_body_start_ip);
        if (st.block_records)
        {
            for (int i = 0; i < st.block_records->count; i++)
            {
                int target = st.block_record_addrs && st.block_record_addrs[i] >= 0
                    ? st.block_record_addrs[i]
                    : st.block_body_start_ip;
                patch_all(st.code, &st.block_record_patches[i], target);
            }
        }
    }

    if (ret_type.kind == TY_VOID)
    {
        emit_u8(st.code, KNC_OP_RET);
        emit_u8(st.code, 255);
    }
    else
    {
        int ret_reg = alloc_reg(&st);
        emit_default_value(&st, ret_reg, ret_type);
        emit_u8(st.code, KNC_OP_RET);
        emit_u8(st.code, ret_reg);
    }

    record->param_count = (params ? params->count : 0) + (record->callable_kind == KNC_CALLABLE_METHOD && record->is_instance ? 1 : 0);
    record->reg_count = st.max_reg;
    record->return_kind = value_kind_from_type(ret_type);
    record->built = 1;
    return 0;
}

static int build_global_init_function(KncProgram *program, StmtList *globals)
{
    KncFuncRecord rec;
    KncFuncState st;
    int func_index;

    if ((!globals || globals->count == 0) && program->static_fields.count == 0)
        return -1;

    kn_memset(&rec, 0, sizeof(rec));
    rec.callable_kind = KNC_CALLABLE_FUNC;
    rec.debug_name = "__knc_init_globals";
    rec.index = program->count;
    funcbuf_push(program, rec);
    func_index = program->count - 1;

    kn_memset(&st, 0, sizeof(st));
    st.program = program;
    st.src = program->fallback_src;
    st.func_index = func_index;
    st.code = &program->items[func_index].code;

    if (globals)
    {
        for (int i = 0; i < globals->count; i++)
        {
            Stmt *s = globals->items[i];
            int slot;
            KncValue value;

            if (!s || s->kind != ST_VAR)
            {
                knc_diag_stmt(&st, s, "Only global variable declarations are supported at top level in the bootstrap KNC emitter");
                return -1;
            }

            slot = program_find_global_slot(program, s->v.var.name);
            if (slot < 0)
            {
                knc_diag_stmt(&st, s, "Internal KNC emitter error: missing global slot");
                return -1;
            }

            if (s->v.var.init)
            {
                Type saved_init_type = s->v.var.init->type;
                if (s->v.var.type.kind == TY_ARRAY && s->v.var.init->kind == EXPR_ARRAY)
                    s->v.var.init->type = s->v.var.type;
                value = compile_expr(&st, s->v.var.init);
                if (s->v.var.type.kind == TY_ARRAY && s->v.var.init->kind == EXPR_ARRAY)
                    s->v.var.init->type = saved_init_type;
            }
            else
            {
                value.reg = alloc_reg(&st);
                value.type = s->v.var.type;
                emit_default_value(&st, value.reg, value.type);
            }

            if (kn_diag_error_count() > 0)
                return -1;

            emit_u8(st.code, KNC_OP_STORE_GLOBAL);
            emit_u8(st.code, value.reg);
            emit_u16(st.code, slot);
        }
    }

    if (program->classes)
    {
        for (int i = 0; i < program->classes->count; i++)
        {
            ClassDecl *c = &program->classes->items[i];
            const char *owner_qname = class_qname_or_name(c);
            for (int j = 0; j < c->fields.count; j++)
            {
                Field *f = &c->fields.items[j];
                int slot;
                KncValue value;
                if (!f->is_static)
                    continue;
                slot = program_find_static_field_slot(program, owner_qname, j);
                if (slot < 0)
                    continue;

                if (f->init)
                    value = compile_expr(&st, f->init);
                else
                {
                    value.reg = alloc_reg(&st);
                    value.type = f->type;
                    emit_default_value(&st, value.reg, value.type);
                }

                if (kn_diag_error_count() > 0)
                    return -1;

                emit_u8(st.code, KNC_OP_STORE_GLOBAL);
                emit_u8(st.code, value.reg);
                emit_u16(st.code, slot);
            }
        }
    }

    emit_u8(st.code, KNC_OP_RET);
    emit_u8(st.code, 255);
    program->items[func_index].param_count = 0;
    program->items[func_index].reg_count = st.max_reg;
    program->items[func_index].return_kind = KNC_VALUE_NULL;
    program->items[func_index].built = 1;
    return func_index;
}

static int build_entry_wrapper_function(KncProgram *program, int main_index, int init_index)
{
    KncFuncRecord rec;
    KncFuncState st;
    int func_index;
    Type main_ret;
    int ret_reg = 255;

    kn_memset(&rec, 0, sizeof(rec));
    rec.callable_kind = KNC_CALLABLE_FUNC;
    rec.debug_name = "__knc_entry";
    rec.index = program->count;
    funcbuf_push(program, rec);
    func_index = program->count - 1;

    kn_memset(&st, 0, sizeof(st));
    st.program = program;
    st.src = program->fallback_src;
    st.func_index = func_index;
    st.code = &program->items[func_index].code;

    if (init_index >= 0)
    {
        emit_u8(st.code, KNC_OP_CALL_FUNC);
        emit_u8(st.code, 255);
        emit_u16(st.code, init_index);
        emit_u8(st.code, 0);
        emit_u8(st.code, 255);
        emit_u8(st.code, 255);
        emit_u8(st.code, 255);
        emit_u8(st.code, 255);
    }

    main_ret = record_return_type(&program->items[main_index]);
    if (main_ret.kind != TY_VOID)
        ret_reg = alloc_reg(&st);
    emit_u8(st.code, KNC_OP_CALL_FUNC);
    emit_u8(st.code, ret_reg);
    emit_u16(st.code, main_index);
    emit_u8(st.code, 0);
    emit_u8(st.code, 255);
    emit_u8(st.code, 255);
    emit_u8(st.code, 255);
    emit_u8(st.code, 255);

    emit_u8(st.code, KNC_OP_RET);
    emit_u8(st.code, ret_reg);
    program->items[func_index].param_count = 0;
    program->items[func_index].reg_count = st.max_reg;
    program->items[func_index].return_kind = value_kind_from_type(main_ret);
    program->items[func_index].built = 1;
    return func_index;
}

static int write_bytes(FILE *fp, const void *data, size_t size)
{
    return fwrite(data, 1, size, fp) == size ? 0 : -1;
}

static int write_u8(FILE *fp, uint8_t value)
{
    return write_bytes(fp, &value, 1);
}

static int write_u16(FILE *fp, uint16_t value)
{
    unsigned char buf[2];
    buf[0] = (unsigned char)(value & 0xffu);
    buf[1] = (unsigned char)((value >> 8) & 0xffu);
    return write_bytes(fp, buf, sizeof(buf));
}

static int write_u32(FILE *fp, uint32_t value)
{
    unsigned char buf[4];
    buf[0] = (unsigned char)(value & 0xffu);
    buf[1] = (unsigned char)((value >> 8) & 0xffu);
    buf[2] = (unsigned char)((value >> 16) & 0xffu);
    buf[3] = (unsigned char)((value >> 24) & 0xffu);
    return write_bytes(fp, buf, sizeof(buf));
}

static int write_i32(FILE *fp, int value)
{
    return write_u32(fp, (uint32_t)value);
}

static int write_f64(FILE *fp, double value)
{
    uint64_t bits = 0;
    unsigned char buf[8];
    memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 8; i++)
        buf[i] = (unsigned char)((bits >> (i * 8)) & 0xffu);
    return write_bytes(fp, buf, sizeof(buf));
}

static int write_string(FILE *fp, const char *text)
{
    uint32_t len = text ? (uint32_t)kn_strlen(text) : 0;
    if (write_u32(fp, len) != 0)
        return -1;
    if (len == 0)
        return 0;
    return write_bytes(fp, text, len);
}

static int write_program_file(const char *out_path, KncProgram *program)
{
    FILE *fp = fopen(out_path, "wb");
    if (!fp)
        return -1;

    if (write_bytes(fp, "KNC2", 4) != 0 ||
        write_u16(fp, 2) != 0 ||
        write_u16(fp, 0) != 0 ||
        write_i32(fp, program->entry_index) != 0 ||
        write_i32(fp, program->globals.count) != 0 ||
        write_i32(fp, program->ints.count) != 0 ||
        write_i32(fp, program->floats.count) != 0 ||
        write_i32(fp, program->strings.count) != 0 ||
        write_i32(fp, program->types.count) != 0 ||
        write_i32(fp, program->count) != 0)
    {
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < program->ints.count; i++)
    {
        if (write_i32(fp, program->ints.items[i]) != 0)
        {
            fclose(fp);
            return -1;
        }
    }

    for (int i = 0; i < program->floats.count; i++)
    {
        if (write_f64(fp, program->floats.items[i]) != 0)
        {
            fclose(fp);
            return -1;
        }
    }

    for (int i = 0; i < program->strings.count; i++)
    {
        if (write_string(fp, program->strings.items[i]) != 0)
        {
            fclose(fp);
            return -1;
        }
    }

    for (int i = 0; i < program->types.count; i++)
    {
        KncTypeRecord *tr = &program->types.items[i];
        if (write_u8(fp, (uint8_t)tr->kind) != 0 ||
            write_u8(fp, 0) != 0 ||
            write_u16(fp, 0) != 0 ||
            write_i32(fp, tr->field_count) != 0 ||
            write_i32(fp, tr->base_type_index) != 0 ||
            write_i32(fp, tr->vtable_count) != 0 ||
            write_i32(fp, tr->interface_count) != 0 ||
            write_string(fp, tr->qname ? tr->qname : "") != 0)
        {
            fclose(fp);
            return -1;
        }

        for (int j = 0; j < tr->vtable_count; j++)
        {
            if (write_i32(fp, tr->vtable[j]) != 0)
            {
                fclose(fp);
                return -1;
            }
        }

        for (int j = 0; j < tr->interface_count; j++)
        {
            if (write_i32(fp, tr->interface_types[j]) != 0)
            {
                fclose(fp);
                return -1;
            }
        }
    }

    for (int i = 0; i < program->count; i++)
    {
        KncFuncRecord *rec = &program->items[i];
        if (write_i32(fp, rec->param_count) != 0 ||
            write_i32(fp, rec->reg_count) != 0 ||
            write_i32(fp, rec->return_kind) != 0 ||
            write_string(fp, rec->debug_name ? rec->debug_name : "") != 0 ||
            write_i32(fp, rec->code.count) != 0)
        {
            fclose(fp);
            return -1;
        }

        if (rec->code.count > 0 && write_bytes(fp, rec->code.items, (size_t)rec->code.count) != 0)
        {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static int name_matches_entry(const Func *f, const char *entry_name)
{
    if (!f || !entry_name)
        return 0;
    if (f->name && kn_strcmp(f->name, entry_name) == 0)
        return 1;
    if (f->qname)
    {
        size_t qn = kn_strlen(f->qname);
        size_t en = kn_strlen(entry_name);
        if (qn >= en && kn_strcmp(f->qname + (qn - en), entry_name) == 0)
            return 1;
    }
    return 0;
}

static int class_find_virtual_slot_in_hierarchy(KncProgram *program, const ClassDecl *c, const Method *method)
{
    if (!program || !c || !method)
        return -1;
    if (c->base)
    {
        ClassDecl *base = program_find_class(program, c->base);
        if (base)
        {
            int slot = class_find_virtual_slot_in_hierarchy(program, base, method);
            if (slot >= 0)
                return slot;
            for (int i = 0; i < base->methods.count; i++)
            {
                Method *bm = &base->methods.items[i];
                if (bm->name && method->name &&
                    kn_strcmp(bm->name, method->name) == 0 &&
                    bm->params.count == method->params.count &&
                    (bm->is_virtual || bm->is_override || bm->is_abstract))
                    return i;
            }
        }
    }
    return -1;
}

int kn_emit_knc(const char *out_path,
                MetaList *metas,
                FuncList *funcs,
                ImportList *imports,
                ClassList *classes,
                InterfaceList *interfaces,
                StructList *structs,
                EnumList *enums,
                StmtList *globals,
                const KnKncOptions *opts)
{
    KncProgram program;
    const char *entry_name = (opts && opts->entry_name && opts->entry_name[0]) ? opts->entry_name : "Main";

    (void)metas;
    (void)imports;
    (void)classes;
    (void)interfaces;
    (void)structs;
    (void)enums;

      kn_memset(&program, 0, sizeof(program));
      program.entry_index = -1;
      program.enable_superloop = (!opts || opts->enable_superloop) ? 1 : 0;
      program.classes = classes;
      program.interfaces = interfaces;
      program.structs = structs;

    if (funcs && funcs->count > 0)
        program.fallback_src = funcs->items[0].src;
    else if (globals && globals->count > 0)
        program.fallback_src = globals->items[0]->kind == ST_VAR ? globals->items[0]->v.var.src : 0;

    if (!funcs || funcs->count == 0)
    {
        knc_diag(program.fallback_src, 1, 1, "No functions available for KNC emission");
        return -1;
    }

    if (structs)
    {
        for (int i = 0; i < structs->count; i++)
        {
            StructDecl *sd = &structs->items[i];
            KncTypeRecord tr;
            kn_memset(&tr, 0, sizeof(tr));
            tr.qname = struct_qname_or_name(sd);
            tr.kind = KNC_TYPE_STRUCT;
            tr.field_count = struct_total_field_count(sd);
            tr.base_type_index = -1;
            tr.struct_decl = sd;
            typebuf_push(&program.types, tr);
        }
    }

    if (interfaces)
    {
        for (int i = 0; i < interfaces->count; i++)
        {
            InterfaceDecl *it = &interfaces->items[i];
            KncTypeRecord tr;
            kn_memset(&tr, 0, sizeof(tr));
            tr.qname = interface_qname_or_name(it);
            tr.kind = KNC_TYPE_INTERFACE;
            tr.base_type_index = -1;
            tr.interface_decl = it;
            typebuf_push(&program.types, tr);
        }
    }

    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            KncTypeRecord tr;
            kn_memset(&tr, 0, sizeof(tr));
            tr.qname = class_qname_or_name(c);
            tr.kind = KNC_TYPE_CLASS;
            tr.field_count = class_total_field_count(&program, c);
            tr.base_type_index = -1;
            tr.class_decl = c;
            tr.direct_method_count = c->methods.count;
            if (c->methods.count > 0)
            {
                tr.direct_methods = (int *)kn_malloc(sizeof(int) * (size_t)c->methods.count);
                for (int j = 0; j < c->methods.count; j++)
                    tr.direct_methods[j] = -1;
            }
            typebuf_push(&program.types, tr);
        }
    }

    if (globals)
    {
        for (int i = 0; i < globals->count; i++)
        {
            Stmt *s = globals->items[i];
            if (!s || s->kind != ST_VAR)
            {
                knc_diag_stmt(0, s, "Only global variable declarations are supported at top level in the bootstrap KNC emitter");
                return -1;
            }
            if (!type_is_vm_value(s->v.var.type))
            {
                knc_diag(s->v.var.src, s->line, s->col,
                         "This global variable type is not supported by the bootstrap KNC emitter yet");
                return -1;
            }
            (void)program_add_global(&program, s->v.var.name, s->v.var.type);
        }
    }

    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            const char *owner_qname = class_qname_or_name(c);
            for (int j = 0; j < c->fields.count; j++)
            {
                Field *f = &c->fields.items[j];
                if (!f->is_static)
                    continue;
                if (!type_is_vm_value(f->type))
                {
                    knc_diag(f->src, f->line, f->col,
                             "This static field type is not supported by the bootstrap KNC emitter yet");
                    return -1;
                }
                (void)program_add_static_field(&program, owner_qname, j, f->type);
            }
        }
    }

    for (int i = 0; funcs && i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        KncFuncRecord rec;

        if (f->is_extern || f->is_delegate || f->type_param_count > 0)
            continue;
        if (!type_is_vm_value(f->ret_type) && f->ret_type.kind != TY_VOID)
        {
            knc_diag(f->src, f->line, f->col, "This function return type is not supported by the bootstrap KNC emitter yet");
            return -1;
        }

        kn_memset(&rec, 0, sizeof(rec));
        rec.callable_kind = KNC_CALLABLE_FUNC;
        rec.func = f;
        rec.debug_name = f->qname ? f->qname : f->name;
        rec.index = program.count;
        funcbuf_push(&program, rec);

        if (program.entry_index < 0 && name_matches_entry(f, entry_name))
            program.entry_index = rec.index;
    }

    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            int type_index = program_find_type(&program, class_qname_or_name(c));
            for (int j = 0; j < c->methods.count; j++)
            {
                Method *m = &c->methods.items[j];
                KncFuncRecord rec;
                if (!m->body)
                    continue;
                if (!type_is_vm_value(m->ret_type) && m->ret_type.kind != TY_VOID)
                {
                    knc_diag(m->src, m->line, m->col, "This method return type is not supported by the bootstrap KNC emitter yet");
                    return -1;
                }
                kn_memset(&rec, 0, sizeof(rec));
                rec.callable_kind = KNC_CALLABLE_METHOD;
                rec.method = m;
                rec.owner_class = c;
                rec.method_index = j;
                rec.is_constructor = m->is_constructor;
                rec.is_instance = !m->is_static;
                rec.debug_name = build_method_debug_name(c, m->name);
                rec.index = program.count;
                funcbuf_push(&program, rec);
                if (type_index >= 0 && j < program.types.items[type_index].direct_method_count)
                    program.types.items[type_index].direct_methods[j] = rec.index;
            }
        }
    }

    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            int type_index = program_find_type(&program, class_qname_or_name(c));
            KncTypeRecord *tr;
            if (type_index < 0)
                continue;
            tr = &program.types.items[type_index];
            if (c->base)
            {
                int base_type_index = program_find_type(&program, c->base);
                if (base_type_index >= 0)
                {
                    KncTypeRecord *base = &program.types.items[base_type_index];
                    tr->base_type_index = base_type_index;
                    tr->vtable_count = base->vtable_count;
                    tr->vtable_cap = base->vtable_count;
                    if (tr->vtable_count > 0)
                    {
                        tr->vtable = (int *)kn_malloc(sizeof(int) * (size_t)tr->vtable_count);
                        for (int j = 0; j < tr->vtable_count; j++)
                        tr->vtable[j] = base->vtable[j];
                    }
                }
            }

            for (int j = 0; j < c->interface_count; j++)
            {
                int iface_type_index = program_find_type(&program, c->interfaces[j]);
                if (iface_type_index >= 0)
                    int_array_push_unique(&tr->interface_types, &tr->interface_count, &tr->interface_cap, iface_type_index);
            }

            for (int j = 0; j < c->methods.count; j++)
            {
                Method *m = &c->methods.items[j];
                int func_index = (j < tr->direct_method_count) ? tr->direct_methods[j] : -1;
                int slot = -1;
                if (m->is_static || m->is_constructor)
                    continue;
                if (!(m->is_virtual || m->is_override || m->is_abstract))
                    continue;
                slot = m->is_override ? class_find_virtual_slot_in_hierarchy(&program, c, m) : j;
                if (slot < 0)
                    slot = j;
                {
                    int old_count = tr->vtable_count;
                    int_array_ensure(&tr->vtable, &tr->vtable_cap, slot + 1);
                    if (tr->vtable_count < slot + 1)
                        tr->vtable_count = slot + 1;
                    for (int k = old_count; k < tr->vtable_count; k++)
                        tr->vtable[k] = -1;
                }
                tr->vtable[slot] = func_index;
            }
        }
    }

    if (program.entry_index < 0)
    {
        knc_diag(program.fallback_src, 1, 1, "KNC entry function was not found");
        return -1;
    }

    program.main_entry_index = program.entry_index;

    for (int i = 0; i < program.count; i++)
        if (!program.items[i].built && compile_function_body(&program, &program.items[i]) != 0)
            return -1;

    if (program.globals.count > 0)
    {
        int init_index = build_global_init_function(&program, globals);
        if (init_index < 0)
            return -1;
        for (int i = 0; i < program.count; i++)
            if (!program.items[i].built && compile_function_body(&program, &program.items[i]) != 0)
                return -1;
        program.entry_index = build_entry_wrapper_function(&program, program.main_entry_index, init_index);
        if (program.entry_index < 0)
            return -1;
    }

    if (write_program_file(out_path, &program) != 0)
    {
        knc_diag(program.fallback_src, 1, 1, "Failed to write .knc output");
        return -1;
    }
    return 0;
}
