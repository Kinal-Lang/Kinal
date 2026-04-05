#include "kn/ast.h"

Expr *new_expr(ExprKind k, int line, int col)
{
    Expr *e = (Expr *)kn_malloc(sizeof(Expr));
    if (!e) kn_die("out of memory");
    kn_memset(e, 0, sizeof(*e));
    e->kind = k;
    e->line = line;
    e->col = col;
    e->type = type_make(TY_UNKNOWN);
    e->async_result_type = type_make(TY_UNKNOWN);
    return e;
}

Stmt *new_stmt(StmtKind k, int line, int col)
{
    Stmt *s = (Stmt *)kn_malloc(sizeof(Stmt));
    if (!s) kn_die("out of memory");
    kn_memset(s, 0, sizeof(*s));
    s->kind = k;
    s->line = line;
    s->col = col;
    return s;
}

void exprlist_push(ExprList *l, Expr *e)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        Expr **n = (Expr **)kn_malloc(sizeof(Expr *) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Expr *) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = e;
}

void exprswitchcaselist_push(ExprSwitchCaseList *l, ExprSwitchCase c)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        ExprSwitchCase *n = (ExprSwitchCase *)kn_malloc(sizeof(ExprSwitchCase) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(ExprSwitchCase) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = c;
}

void namelist_push(NameList *l, const char *name)
{
    if (!l) return;
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        const char **n = (const char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(char *) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = name;
}

void typelist_push(TypeList *l, Type t)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        Type *n = (Type *)kn_malloc(sizeof(Type) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Type) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = t;
}

void stmtlist_push(StmtList *l, Stmt *s)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        Stmt **n = (Stmt **)kn_malloc(sizeof(Stmt *) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Stmt *) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = s;
}

void paramlist_push(ParamList *l, Param p)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        Param *n = (Param *)kn_malloc(sizeof(Param) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Param) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = p;
}

void funclist_push(FuncList *l, Func f)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        Func *n = (Func *)kn_malloc(sizeof(Func) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Func) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = f;
}

void importlist_push(ImportList *l, ImportMap m)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        ImportMap *n = (ImportMap *)kn_malloc(sizeof(ImportMap) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(ImportMap) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = m;
}

void fieldlist_push(FieldList *l, Field f)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        Field *n = (Field *)kn_malloc(sizeof(Field) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Field) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = f;
}

void methodlist_push(MethodList *l, Method m)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        Method *n = (Method *)kn_malloc(sizeof(Method) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Method) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = m;
}

void classlist_push(ClassList *l, ClassDecl c)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        ClassDecl *n = (ClassDecl *)kn_malloc(sizeof(ClassDecl) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(ClassDecl) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = c;
}

void interfacelist_push(InterfaceList *l, InterfaceDecl i)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        InterfaceDecl *n = (InterfaceDecl *)kn_malloc(sizeof(InterfaceDecl) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(InterfaceDecl) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = i;
}

void enumitemlist_push(EnumItemList *l, EnumItem it)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        EnumItem *n = (EnumItem *)kn_malloc(sizeof(EnumItem) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(EnumItem) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = it;
}

void enumlist_push(EnumList *l, EnumDecl e)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        EnumDecl *n = (EnumDecl *)kn_malloc(sizeof(EnumDecl) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(EnumDecl) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = e;
}

void blockrecordlist_push(BlockRecordList *l, BlockRecord r)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        BlockRecord *n = (BlockRecord *)kn_malloc(sizeof(BlockRecord) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(BlockRecord) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = r;
}

void attrlist_push(AttrList *l, Attribute a)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        Attribute *n = (Attribute *)kn_malloc(sizeof(Attribute) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(Attribute) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = a;
}

void structlist_push(StructList *l, StructDecl s)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        StructDecl *n = (StructDecl *)kn_malloc(sizeof(StructDecl) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(StructDecl) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = s;
}

void metalist_push(MetaList *l, MetaDecl m)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        MetaDecl *n = (MetaDecl *)kn_malloc(sizeof(MetaDecl) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(MetaDecl) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = m;
}

void stmtswitchcaselist_push(StmtSwitchCaseList *l, StmtSwitchCase c)
{
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 4;
        StmtSwitchCase *n = (StmtSwitchCase *)kn_malloc(sizeof(StmtSwitchCase) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(StmtSwitchCase) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = c;
}



