#pragma once
#include "kn/util.h"
#include "kn/source.h"

struct Expr;

typedef enum
{
    TY_VOID,
    TY_BOOL,
    TY_BYTE,
    TY_CHAR,
    TY_INT,
    TY_FLOAT,
    TY_F16,
    TY_F32,
    TY_F64,
    TY_F128,
    TY_I8,
    TY_I16,
    TY_I32,
    TY_I64,
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    TY_ISIZE,
    TY_USIZE,
    TY_STRING,
    TY_ANY,
    TY_NULL,
    TY_PTR,
    TY_ARRAY,
    TY_CLASS,
    TY_STRUCT,
    TY_ENUM,
    TY_UNKNOWN
} TypeKind;

typedef struct
{
    TypeKind kind;
    TypeKind elem;
    int64_t array_len; // -1 for dynamic arrays
    int ptr_depth; // pointer depth for TY_PTR (0 for non-pointers)
    const char *name; // for class/struct/enum types
    struct Expr *array_len_expr; // unresolved constant-length expression from parser
} Type;

static inline Type type_make(TypeKind k)
{
    Type t;
    t.kind = k;
    t.elem = TY_UNKNOWN;
    t.array_len = 0;
    t.array_len_expr = 0;
    t.ptr_depth = 0;
    t.name = 0;
    return t;
}

static inline Type type_ptr(TypeKind elem)
{
    Type t;
    t.kind = TY_PTR;
    t.elem = elem;
    t.array_len = 0;
    t.array_len_expr = 0;
    t.ptr_depth = 1;
    t.name = 0;
    return t;
}

static inline Type type_array(TypeKind elem, int64_t len)
{
    Type t;
    t.kind = TY_ARRAY;
    t.elem = elem;
    t.array_len = len;
    t.array_len_expr = 0;
    t.ptr_depth = 0;
    t.name = 0;
    return t;
}

static inline bool type_is_array(Type t)
{
    return t.kind == TY_ARRAY;
}

static inline Type type_class(const char *name)
{
    Type t;
    t.kind = TY_CLASS;
    t.elem = TY_UNKNOWN;
    t.array_len = 0;
    t.array_len_expr = 0;
    t.ptr_depth = 0;
    t.name = name;
    return t;
}

static inline Type type_struct(const char *name)
{
    Type t;
    t.kind = TY_STRUCT;
    t.elem = TY_UNKNOWN;
    t.array_len = 0;
    t.array_len_expr = 0;
    t.ptr_depth = 0;
    t.name = name;
    return t;
}

static inline Type type_enum(const char *name)
{
    Type t;
    t.kind = TY_ENUM;
    t.elem = TY_UNKNOWN;
    t.array_len = 0;
    t.array_len_expr = 0;
    t.ptr_depth = 0;
    t.name = name;
    return t;
}

typedef enum
{
    ACC_PUBLIC,
    ACC_PRIVATE,
    ACC_PROTECTED,
    ACC_INTERNAL
} Access;

typedef enum
{
    SAFETY_SAFE,
    SAFETY_TRUSTED,
    SAFETY_UNSAFE
} Safety;

typedef enum
{
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_BOOL,
    EXPR_CHAR,
    EXPR_DEFAULT,
    EXPR_NULL,
    EXPR_STRING,
    EXPR_VAR,
    EXPR_CALL,
    EXPR_MEMBER_CALL,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_ARRAY,
    EXPR_DICT,
    EXPR_ASSIGN,
    EXPR_MEMBER,
    EXPR_INDEX,
    EXPR_IF,
    EXPR_SWITCH,
    EXPR_IS,
    EXPR_THIS,
    EXPR_BASE,
    EXPR_NEW,
    EXPR_CAST,
    EXPR_FUNC_REF,
    EXPR_INVOKE,
    EXPR_ANON_FUNC,
    EXPR_BLOCK_LITERAL,
    EXPR_AWAIT
} ExprKind;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Param Param;
typedef struct ExprSwitchCase ExprSwitchCase;
typedef struct StmtSwitchCase StmtSwitchCase;

typedef struct
{
    Expr **items;
    int count;
    int cap;
} ExprList;

struct ExprSwitchCase
{
    Expr *match;
    Expr *body;
    int is_default;
    int line;
    int col;
};

typedef struct
{
    ExprSwitchCase *items;
    int count;
    int cap;
} ExprSwitchCaseList;

typedef struct
{
    const char **items;
    int count;
    int cap;
} NameList;

typedef struct
{
    Type *items;
    int count;
    int cap;
} TypeList;

typedef struct
{
    const char *name;
    int ip;
    int nested_depth;
} BlockRecord;

typedef struct
{
    BlockRecord *items;
    int count;
    int cap;
} BlockRecordList;

typedef struct
{
    const char *name;
    const char *resolved_name;
    ExprList args;
} Attribute;

typedef struct
{
    Attribute *items;
    int count;
    int cap;
} AttrList;

struct Expr
{
    ExprKind kind;
    int line;
    int col;
    Type type; // resolved by sema
    Type async_result_type; // hidden awaited result type for Task-like expressions
    union
    {
        int64_t int_val;
        double float_val;
        struct { const char *ptr; int len; } str_val;
        struct { const char *name; } var;
        struct { const char *name; ExprList args; NameList arg_names; int builtin_id; TypeList type_args; } call;
        struct { Expr *recv; const char *name; ExprList args; NameList arg_names; int builtin_id; TypeList type_args; int is_static; const char *static_type; int method_index; const char *method_owner; int is_qual_call; const char *qual_name; } member_call;
        struct { Expr *callee; ExprList args; NameList arg_names; } invoke;
        struct { int op; Expr *left; Expr *right; } binary;
        struct { int op; Expr *expr; int is_postfix; } unary;
        struct { ExprList items; } array;
        struct { ExprList keys; ExprList values; } dict;
        struct { Expr *target; Expr *value; } assign;
        struct { Expr *cond; Expr *then_expr; Expr *else_expr; } if_expr;
        struct { Expr *value; ExprSwitchCaseList cases; } switch_expr;
        struct { Expr *expr; Type target; } is_expr;
        struct { Expr *recv; const char *name; int is_static; const char *static_type; int field_index; const char *field_owner; int is_enum_const; int64_t enum_value; const char *enum_type; } member;
        struct { Expr *recv; Expr *index; } index;
        struct { const char *class_name; ExprList args; NameList arg_names; int ctor_index; } new_expr;
        struct { Type type; Expr *expr; } cast;
        struct { Expr *expr; } await_expr;
        struct { const char *qname; int builtin_id; TypeList type_args; } func_ref;
        struct { Type ret_type; Param *params; int param_count; int param_cap; Stmt *body; Safety safety; const char *debug_name; const char *qname; int id; } anon_func;
        struct { Safety safety; Stmt *body; BlockRecordList records; const char *debug_name; int id; } block_lit;
    } v;
};

typedef enum
{
    ST_BLOCK,
    ST_VAR,
    ST_ASSIGN,
    ST_EXPR,
    ST_IF,
    ST_SWITCH,
    ST_WHILE,
    ST_FOR,
    ST_RETURN,
    ST_BREAK,
    ST_CONTINUE,
    ST_TRY,
    ST_THROW,
    ST_BLOCK_RECORD,
    ST_BLOCK_JUMP
} StmtKind;

typedef struct
{
    Stmt **items;
    int count;
    int cap;
} StmtList;

struct StmtSwitchCase
{
    Expr *match;
    Stmt *body;
    int is_default;
    int line;
    int col;
};

typedef struct
{
    StmtSwitchCase *items;
    int count;
    int cap;
} StmtSwitchCaseList;

	struct Stmt
	{
	    StmtKind kind;
	    int line;
	    int col;
	    union
	    {
	        struct { StmtList stmts; } block;
	        // `name_line/name_col` points at the identifier token for better diagnostics (e.g. unused variable).
	        struct { Type type; const char *name; int name_line; int name_col; Expr *init; int is_const; const KnSource *src; const char *unit; } var;
	        struct { const char *name; Expr *value; } assign;
	        struct { Expr *expr; } expr;
        struct { Expr *cond; Stmt *then_s; Stmt *else_s; const char *pattern_name; int pattern_line; int pattern_col; int is_const; int const_value; } ifs;
            struct { Expr *value; StmtSwitchCaseList cases; } switchs;
	        struct { Expr *cond; Stmt *body; } whiles;
	        struct { Stmt *init; Expr *cond; Expr *post; Stmt *body; } fors;
        struct { Expr *expr; } ret;
        struct { Stmt *try_block; Stmt *catch_block; Type catch_type; const char *catch_name; int has_param; } trys;
        struct { Expr *expr; } throws;
        struct { const char *name; int ip; int nested_depth; } record;
        struct { Expr *target; } jump;
    } v;
};

struct Param
{
    const char *name;
    Type type;
    Expr *default_value;
    const KnSource *src;
    int line;
    int col;
    int materialized_default;
};

typedef struct
{
    Param *items;
    int count;
    int cap;
} ParamList;

typedef struct
{
    const char *name;
    Type ret_type;
    ParamList params;
    const char **type_params;
    int type_param_count;
    int type_param_cap;
    Stmt *body;
    Safety safety;
    AttrList attrs;
    const KnSource *src;
    int is_extern;
    int extern_abi;
    const char *extern_symbol;
    int is_delegate;
    int delegate_abi;
    const char *delegate_symbol_name;
    int is_static;
    int is_async;
    Type async_result_type;
    int is_generic_instance;
    const char *generic_template_qname;
    Type *generic_args;
    int generic_arg_count;
    const char *unit;
    const char *qname;
    int line;
    int col;
} Func;

enum
{
    KN_EXTERN_ABI_C = 0,
    KN_EXTERN_ABI_SYSTEM = 1
};

enum
{
    KN_DELEGATE_ABI_KINAL = 0,
    KN_DELEGATE_ABI_C = 1
};

typedef struct
{
    Func *items;
    int count;
    int cap;
} FuncList;

typedef enum
{
    IMPORT_OPEN,
    IMPORT_ALIAS,
    IMPORT_SYMBOL
} ImportKind;

typedef struct
{
    int kind; // ImportKind
    const char *module;
    const char *local;
    const char *remote;
    const char *unit;
    int global; // for IMPORT_SYMBOL: 1 means callable as global symbol alias
    int line;
    int col;
} ImportMap;

typedef struct
{
    ImportMap *items;
    int count;
    int cap;
} ImportList;

typedef struct
{
    const char *name;
    Type type;
    Expr *init;
    Access access;
    int is_static;
    AttrList attrs;
    const KnSource *src;
    int line;
    int col;
} Field;

typedef struct
{
    Field *items;
    int count;
    int cap;
} FieldList;

typedef struct
{
    const char *name;
    Type ret_type;
    ParamList params;
    Stmt *body;
    Safety safety;
    Access access;
    int is_static;
    int is_virtual;
    int is_override;
    int is_abstract;
    int is_sealed;
    int is_constructor;
    int is_async;
    Type async_result_type;
    AttrList attrs;
    const KnSource *src;
    int line;
    int col;
} Method;

typedef struct
{
    Method *items;
    int count;
    int cap;
} MethodList;

typedef struct
{
    const char *name;
    const char *qname;
    const char *owner_type_qname;
    const char *base;
    const char **interfaces;
    int interface_count;
    AttrList attrs;
    FieldList fields;
    MethodList methods;
    int is_static_class;
    int is_abstract;
    int is_sealed;
    const KnSource *src;
    int line;
    int col;
    const char *unit;
} ClassDecl;

typedef struct
{
    ClassDecl *items;
    int count;
    int cap;
} ClassList;

typedef struct
{
    const char *name;
    const char *qname;
    const char *owner_type_qname;
    AttrList attrs;
    MethodList methods;
    const KnSource *src;
    int line;
    int col;
    const char *unit;
} InterfaceDecl;

typedef struct
{
    InterfaceDecl *items;
    int count;
    int cap;
} InterfaceList;

typedef struct
{
    const char *name;
    int64_t value;
    const KnSource *src;
    int line;
    int col;
} EnumItem;

typedef struct
{
    EnumItem *items;
    int count;
    int cap;
} EnumItemList;

typedef struct
{
    const char *name;
    const char *qname;
    const char *owner_type_qname;
    Type underlying;
    AttrList attrs;
    EnumItemList items;
    const KnSource *src;
    int line;
    int col;
    const char *unit;
} EnumDecl;

typedef struct
{
    EnumDecl *items;
    int count;
    int cap;
} EnumList;

typedef struct
{
    const char *name;
    const char *qname;
    const char *owner_type_qname;
    AttrList attrs;
    FieldList fields;
    int is_packed;
    int align; // 0 means default
    const KnSource *src;
    int line;
    int col;
    const char *unit;
} StructDecl;

typedef struct
{
    StructDecl *items;
    int count;
    int cap;
} StructList;

enum
{
    KN_META_TARGET_NONE = 0,
    KN_META_TARGET_FUNCTION = 1 << 0,
    KN_META_TARGET_EXTERN_FUNCTION = 1 << 1,
    KN_META_TARGET_DELEGATE_FUNCTION = 1 << 2,
    KN_META_TARGET_CLASS = 1 << 3,
    KN_META_TARGET_STRUCT = 1 << 4,
    KN_META_TARGET_ENUM = 1 << 5,
    KN_META_TARGET_INTERFACE = 1 << 6,
    KN_META_TARGET_METHOD = 1 << 7,
    KN_META_TARGET_FIELD = 1 << 8
};

enum
{
    KN_META_KEEP_NONE = 0,
    KN_META_KEEP_SOURCE = 1,
    KN_META_KEEP_COMPILE = 2,
    KN_META_KEEP_RUNTIME = 3
};

enum
{
    KN_META_OWNER_NONE = 0,
    KN_META_OWNER_FUNCTION = 1,
    KN_META_OWNER_CLASS = 2,
    KN_META_OWNER_STRUCT = 3,
    KN_META_OWNER_ENUM = 4,
    KN_META_OWNER_INTERFACE = 5,
    KN_META_OWNER_METHOD = 6,
    KN_META_OWNER_FIELD = 7
};

typedef struct
{
    const char *name;
    const char *qname;
    ParamList params;
    uint32_t target_mask;
    int keep_kind;
    int repeatable;
    const KnSource *src;
    int line;
    int col;
    const char *unit;
} MetaDecl;

typedef struct
{
    MetaDecl *items;
    int count;
    int cap;
} MetaList;

Expr *new_expr(ExprKind kind, int line, int col);
Stmt *new_stmt(StmtKind kind, int line, int col);
void exprlist_push(ExprList *list, Expr *e);
void exprswitchcaselist_push(ExprSwitchCaseList *list, ExprSwitchCase c);
void namelist_push(NameList *list, const char *name);
void typelist_push(TypeList *list, Type t);
void blockrecordlist_push(BlockRecordList *list, BlockRecord r);
void stmtlist_push(StmtList *list, Stmt *s);
void stmtswitchcaselist_push(StmtSwitchCaseList *list, StmtSwitchCase c);
void attrlist_push(AttrList *list, Attribute a);
void paramlist_push(ParamList *list, Param p);
void funclist_push(FuncList *list, Func f);
void importlist_push(ImportList *list, ImportMap m);
void fieldlist_push(FieldList *list, Field f);
void methodlist_push(MethodList *list, Method m);
void classlist_push(ClassList *list, ClassDecl c);
void interfacelist_push(InterfaceList *list, InterfaceDecl i);
void enumitemlist_push(EnumItemList *list, EnumItem it);
void enumlist_push(EnumList *list, EnumDecl e);
void structlist_push(StructList *list, StructDecl s);
void metalist_push(MetaList *list, MetaDecl m);
