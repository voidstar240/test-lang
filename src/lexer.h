#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TKN__NULL = 0,
    TKN_EOF = 1,

    TKN_L_PAREN,   TKN_R_PAREN,
    TKN_L_BRACE,   TKN_R_BRACE,
    TKN_L_BRACKET, TKN_R_BRACKET,

    TKN_COMMA, TKN_DOT, TKN_COLON,
    TKN_SEMICOLON, TKN_QUESTION,

    TKN_PLUS,            TKN_PLUS_EQUAL,
    TKN_MINUS,           TKN_MINUS_EQUAL,
    TKN_STAR,            TKN_STAR_EQUAL,
    TKN_SLASH,           TKN_SLASH_EQUAL,
    TKN_PERCENT,         TKN_PERCENT_EQUAL,
    TKN_TILDE,           TKN_TILDE_EQUAL,
    TKN_CARET,           TKN_CARET_EQUAL,
    TKN_BANG,            TKN_BANG_EQUAL,
    TKN_EQUAL,           TKN_EQUAL_EQUAL,
    TKN_LESS,            TKN_LESS_EQUAL,
    TKN_LESS_LESS,       TKN_LESS_LESS_EQUAL,
    TKN_GREATER,         TKN_GREATER_EQUAL,
    TKN_GREATER_GREATER, TKN_GREATER_GREATER_EQUAL,
    TKN_AMP,             TKN_AMP_EQUAL,  TKN_AMP_AMP,
    TKN_PIPE,            TKN_PIPE_EQUAL, TKN_PIPE_PIPE,

    // LITERALS
    TKN_CHAR, TKN_STRING, TKN_NUMBER, TKN_IDENTIFIER, 

    // KEYWORDS
    TKN_CONST, TKN_ELSE, TKN_FALSE,  TKN_FN,   TKN_FOR,   TKN_IF,  TKN_NONE,
    TKN_ONES,  TKN_OR,   TKN_RETURN, TKN_TRUE, TKN_UNDEF, TKN_VAR, TKN_WHILE,
    TKN_ZEROS,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    size_t line;
    size_t col;
} Token;

typedef struct {
    const char* start;
    const char* current;
    size_t length;
    size_t line;
    size_t col;
} Scanner;

static inline Scanner scnr_init(const char* data, size_t length) {
    return (Scanner){
        .start = data,
        .current = data,
        .length = length,
        .line = 1,
        .col = 1,
    };
}

static inline bool scnr_is_end(const Scanner* scnr) {
    return scnr->current >= scnr->start + scnr->length;
}

// Skips whitespace and comments
void scnr_skip_whitespace(Scanner* scnr);

Token scnr_scan_token(Scanner* scnr);

static inline const char* token_type_str(TokenType type) {
    switch (type) {
        case TKN__NULL: return "NULL";
        case TKN_EOF: return "EOF";
        case TKN_L_PAREN: return "l_paren";
        case TKN_R_PAREN: return "r_paren";
        case TKN_L_BRACE: return "l_brace";
        case TKN_R_BRACE: return "r_brace";
        case TKN_L_BRACKET: return "l_bracket";
        case TKN_R_BRACKET: return "r_bracket";
        case TKN_COMMA: return "comma";
        case TKN_DOT: return "dot";
        case TKN_COLON: return "colon";
        case TKN_SEMICOLON: return "semicolon";
        case TKN_QUESTION: return "question";
        case TKN_PLUS: return "plus";
        case TKN_PLUS_EQUAL: return "plus_equal";
        case TKN_MINUS: return "minus";
        case TKN_MINUS_EQUAL: return "minus_equal";
        case TKN_STAR: return "star";
        case TKN_STAR_EQUAL: return "star_equal";
        case TKN_SLASH: return "slash";
        case TKN_SLASH_EQUAL: return "slash_equal";
        case TKN_PERCENT: return "percent";
        case TKN_PERCENT_EQUAL: return "percent_equal";
        case TKN_TILDE: return "tilde";
        case TKN_TILDE_EQUAL: return "tilde_equal";
        case TKN_CARET: return "caret";
        case TKN_CARET_EQUAL: return "caret_equal";
        case TKN_BANG: return "bang";
        case TKN_BANG_EQUAL: return "bang_equal";
        case TKN_EQUAL: return "equal";
        case TKN_EQUAL_EQUAL: return "equal_equal";
        case TKN_LESS: return "less";
        case TKN_LESS_EQUAL: return "less_equal";
        case TKN_LESS_LESS: return "less_less";
        case TKN_LESS_LESS_EQUAL: return "less_less_equal";
        case TKN_GREATER: return "greater";
        case TKN_GREATER_EQUAL: return "greater_equal";
        case TKN_GREATER_GREATER: return "greater_greater";
        case TKN_GREATER_GREATER_EQUAL: return "greater_greater_equal";
        case TKN_AMP: return "amp";
        case TKN_AMP_EQUAL: return "amp_equal";
        case TKN_AMP_AMP: return "amp_amp";
        case TKN_PIPE: return "pipe";
        case TKN_PIPE_EQUAL: return "pipe_equal";
        case TKN_PIPE_PIPE: return "pipe_pipe";
        case TKN_CHAR: return "char";
        case TKN_STRING: return "string";
        case TKN_NUMBER: return "number";
        case TKN_IDENTIFIER: return "identifier";
        case TKN_CONST: return "const";
        case TKN_ELSE: return "else";
        case TKN_FALSE: return "false";
        case TKN_FN: return "fn";
        case TKN_FOR: return "for";
        case TKN_IF: return "if";
        case TKN_NONE: return "none";
        case TKN_ONES: return "ones";
        case TKN_OR: return "or";
        case TKN_RETURN: return "return";
        case TKN_TRUE: return "true";
        case TKN_UNDEF: return "undef";
        case TKN_VAR: return "var";
        case TKN_WHILE: return "while";
        case TKN_ZEROS: return "zeros";
        default: return "NO_NAME";
    }
}

#endif
