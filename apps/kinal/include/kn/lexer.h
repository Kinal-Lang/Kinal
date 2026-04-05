#pragma once
#include "kn/util.h"

typedef enum
{
    TOK_EOF,
    TOK_ID,
    TOK_NUMBER,
    TOK_STRING,
    TOK_BAD_STRING,
    TOK_CHAR_LIT,
    TOK_BAD_CHAR,

    TOK_UNIT,
    TOK_GET,
    TOK_SET,
    TOK_BY,
    TOK_PROPERTY,
    TOK_FUNCTION,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_ELSEIF,
    TOK_WHILE,
    TOK_FOR,
    TOK_SWITCH,
    TOK_CASE,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_TRY,
    TOK_CATCH,
    TOK_THROW,
    TOK_CONST,
    TOK_VAR,
    TOK_SAFE,
    TOK_TRUSTED,
    TOK_UNSAFE,
    TOK_ASYNC,
    TOK_AWAIT,
    TOK_EXTERN,
    TOK_CLASS,
    TOK_INTERFACE,
    TOK_STRUCT,
    TOK_ENUM,
    TOK_PUBLIC,
    TOK_PRIVATE,
    TOK_PROTECTED,
    TOK_INTERNAL,
    TOK_STATIC,
    TOK_VIRTUAL,
    TOK_OVERRIDE,
    TOK_ABSTRACT,
    TOK_SEALED,
    TOK_CONSTRUCTOR,
    TOK_NEW,
    TOK_THIS,
    TOK_BASE,
    TOK_BLOCK,
    TOK_RECORD,
    TOK_JUMP,
    TOK_DELEGATE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_IS,
    TOK_DEFAULT,
    TOK_NULL,

    TOK_TYPE_VOID,
    TOK_TYPE_BOOL,
    TOK_TYPE_BYTE,
    TOK_TYPE_CHAR,
    TOK_TYPE_INT,
    TOK_TYPE_FLOAT,
    TOK_TYPE_F16,
    TOK_TYPE_F32,
    TOK_TYPE_F64,
    TOK_TYPE_F128,
    TOK_TYPE_I8,
    TOK_TYPE_I16,
    TOK_TYPE_I32,
    TOK_TYPE_I64,
    TOK_TYPE_U8,
    TOK_TYPE_U16,
    TOK_TYPE_U32,
    TOK_TYPE_U64,
    TOK_TYPE_ISIZE,
    TOK_TYPE_USIZE,
    TOK_TYPE_STRING,
    TOK_TYPE_ANY,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_SEMI,
    TOK_DOT,
    TOK_COLON,
    TOK_QUESTION,
    TOK_AT,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_ASSIGN,
    TOK_PLUSPLUS,
    TOK_MINUSMINUS,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN,
    TOK_PERCENT_ASSIGN,

    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,
    TOK_ANDAND,
    TOK_OROR,
    TOK_NOT,
    TOK_AMP,
    TOK_BITOR,
    TOK_XOR,
    TOK_SHL,
    TOK_SHR,
    TOK_TILDE
} TokenType;

typedef struct
{
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int col;
} Token;

typedef struct
{
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int col;
    char ch;
} Lexer;

void lex_init(Lexer *l, const char *src);
Token lex_next(Lexer *l);
