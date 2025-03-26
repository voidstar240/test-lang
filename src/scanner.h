#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TKN_EOF = 0x00,

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

    // NON-PARSED
    TKN_COMMENT_INLINE = 0x80,
    TKN_COMMENT_BLOCK,

    // ERRORS
    TKNERR_UNEXPECTED = 0xC0,
    TKNERR_OPEN_COMMENT,
    TKNERR_OPEN_CHAR,
    TKNERR_OPEN_STRING,
    TKNERR_OPEN_IDENTIFIER,
} TokenTag;

typedef struct {
    uint8_t tag;
    size_t start;
    size_t len;
} Token;

static inline bool tkn_is_err(Token tkn) {
    return tkn.tag >= 0xC0;
}

static inline bool tkn_is_parsed(Token tkn) {
    return tkn.tag < 0x80;
}

typedef struct {
    const char* src;
    size_t cur_index;
} Scanner;

static inline Scanner scnr_init(const char* src) {
    return (Scanner){
        .src = src,
        .cur_index = 0,
    };
}

Token scnr_next(Scanner* scnr);

static inline const char* tkn_tag_name(TokenTag tag) {
    switch (tag) {
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
        case TKN_COMMENT_INLINE: return "comment_inline";
        case TKN_COMMENT_BLOCK: return "comment_block";
        case TKNERR_UNEXPECTED: return "ERROR_UNEXPECTED";
        case TKNERR_OPEN_COMMENT: return "ERROR_OPEN_COMMENT";
        case TKNERR_OPEN_CHAR: return "ERROR_OPEN_CHAR";
        case TKNERR_OPEN_STRING: return "ERROR_OPEN_STRING";
        case TKNERR_OPEN_IDENTIFIER: return "ERROR_OPEN_IDENTIFIER";
    }
}

#endif
