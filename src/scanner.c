#include "scanner.h"
#include <string.h>

typedef struct {
    TokenType type;
    const char* str;
} KeywordStringPair;

const KeywordStringPair pairs[] = {
    { TKN_CONST, "const" },
    { TKN_ELSE, "else" },
    { TKN_FALSE, "false" },
    { TKN_FN, "fn" },
    { TKN_FOR, "for" },
    { TKN_IF, "if" },
    { TKN_NONE, "none" },
    { TKN_ONES, "ones" },
    { TKN_OR, "or" },
    { TKN_RETURN, "return" },
    { TKN_TRUE, "true" },
    { TKN_UNDEF, "undef" },
    { TKN_VAR, "var" },
    { TKN_WHILE, "while" },
    { TKN_ZEROS, "zeros" },
};

static Token ident_to_keyword(const char* src, Token ident) {
    for (size_t i = 0; i < sizeof(pairs)/sizeof(pairs[0]); i++) {
        if (ident.len != strlen(pairs[i].str)) { continue; }
        if (strncmp(pairs[i].str, &src[ident.start], ident.len) == 0) {
            ident.type = pairs[i].type;
            return ident;
        }
    }
    return ident;
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_upper_case(char c) {
    return c >= 'A' && c <= 'Z';
}

static inline bool is_lower_case(char c) {
    return c >= 'a' && c <= 'z';
}

static inline bool is_identifier_char(char c) {
    return is_upper_case(c) || is_lower_case(c) || is_digit(c) || c == '_';
}

static inline bool is_whitespace_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool is_numeric_char(char c) {
    return is_identifier_char(c) || c == '.';
}

static inline char advance(Scanner* scnr) {
    return scnr->src[scnr->cur_index++];
}

static inline char peek(Scanner* scnr) {
    return scnr->src[scnr->cur_index];
}

static bool match(Scanner* scnr, char c) {
    if (peek(scnr) == c) {
        scnr->cur_index++;
        return true;
    }
    return false;
}

static inline void skip_whitespace(Scanner* scnr) {
    while (is_whitespace_char(peek(scnr))) {
        scnr->cur_index++;
    }
}

static inline bool advance_enclosed(Scanner* scnr, char encloser) {
    bool esc = false;
    while (true) {
        if (peek(scnr) == '\0' || peek(scnr) == '\n') {
            return false;
        }
        if (!esc) {
            if (match(scnr, encloser)) {
                break;
            } else if (match(scnr, '\\')) {
                esc = true;
            }
        } else {
            esc = false;
            scnr->cur_index++;
        }
    }
    return true;
}

TokenResult scnr_next(Scanner* scnr) {
    skip_whitespace(scnr);
    Token tok = {
        .type = 0,
        ._pad = 0,
        .start = scnr->cur_index,
        .len = 0,
    };
    const char c = advance(scnr);
    switch (c) {
        case '\0':
            tok.type = TKN_EOF;
            scnr->cur_index--; // avoid OOB access src
            break;
        case '(': tok.type = TKN_L_PAREN; break;
        case ')': tok.type = TKN_R_PAREN; break;
        case '{': tok.type = TKN_L_BRACE; break;
        case '}': tok.type = TKN_R_BRACE; break;
        case '[': tok.type = TKN_L_BRACKET; break;
        case ']': tok.type = TKN_R_BRACKET; break;
        case ',': tok.type = TKN_COMMA; break;
        case ':': tok.type = TKN_COLON; break;
        case ';': tok.type = TKN_SEMICOLON; break;
        case '?': tok.type = TKN_QUESTION; break;
        case '.':
            tok.type = TKN_DOT;
            if (is_digit(peek(scnr))) {
                tok.type = TKN_NUMBER;
                scnr->cur_index++;
                while (is_numeric_char(peek(scnr))) {
                    scnr->cur_index++;
                }
            } break;
        case '+':
            tok.type = match(scnr, '=') ? TKN_PLUS_EQUAL : TKN_PLUS;
            break;
        case '-':
            tok.type = match(scnr, '=') ? TKN_MINUS_EQUAL : TKN_MINUS;
            break;
        case '*':
            tok.type = match(scnr, '=') ? TKN_STAR_EQUAL : TKN_STAR;
            break;
        case '/':
            if (match(scnr, '=')) {
                tok.type = TKN_SLASH_EQUAL;
            } else if (match(scnr, '/')) {
                tok.type = TKN_COMMENT_INLINE;
                while (peek(scnr) != '\n' && peek(scnr) != '\0') {
                    scnr->cur_index++;
                }
            } else if (match(scnr, '*')) {
                tok.type = TKN_COMMENT_BLOCK;
                size_t depth = 1;
                while (depth > 0) {
                    const char cur = advance(scnr);
                    if (cur == '\0') {
                        scnr->cur_index--; // avoid OOB access src
                        return (TokenResult) { .error = (TokenErr){
                            ._null = TKN__NULL,
                            .err_type = TKNERR_OPEN_COMMENT,
                            .start = tok.start,
                        }};
                    }
                    if (cur == '/' && match(scnr, '*')) {
                        depth++;
                    } else if (cur == '*' && match(scnr, '/')) {
                        depth--;
                    }
                }
            } else {
                tok.type = TKN_SLASH;
            } break;
        case '%':
            tok.type = match(scnr, '=') ? TKN_PERCENT_EQUAL : TKN_PERCENT;
            break;
        case '~':
            tok.type = match(scnr, '=') ? TKN_TILDE_EQUAL : TKN_TILDE;
            break;
        case '^':
            tok.type = match(scnr, '=') ? TKN_CARET_EQUAL : TKN_CARET;
            break;
        case '!':
            tok.type = match(scnr, '=') ? TKN_BANG_EQUAL : TKN_BANG;
            break;
        case '=':
            tok.type = match(scnr, '=') ? TKN_EQUAL_EQUAL : TKN_EQUAL;
            break;
        case '<':
            if (match(scnr, '=')) {
                tok.type = TKN_LESS_EQUAL;
            } else if (match(scnr, '<')) {
                if (match(scnr, '=')) {
                    tok.type = TKN_LESS_LESS_EQUAL;
                } else {
                    tok.type = TKN_LESS_LESS;
                }
            } else {
                tok.type = TKN_LESS;
            } break;
        case '>':
            if (match(scnr, '=')) {
                tok.type = TKN_GREATER_EQUAL;
            } else if (match(scnr, '>')) {
                if (match(scnr, '=')) {
                    tok.type = TKN_GREATER_GREATER_EQUAL;
                } else {
                    tok.type = TKN_GREATER_GREATER;
                }
            } else {
                tok.type = TKN_GREATER;
            } break;
        case '&':
            if (match(scnr, '=')) {
                tok.type = TKN_AMP_EQUAL;
            } else if (match(scnr, '&')) {
                tok.type = TKN_AMP_AMP;
            } else {
                tok.type = TKN_AMP;
            } break;
        case '|':
            if (match(scnr, '=')) {
                tok.type = TKN_PIPE_EQUAL;
            } else if (match(scnr, '|')) {
                tok.type = TKN_PIPE_PIPE;
            } else {
                tok.type = TKN_PIPE;
            } break;
        case '\'':
            tok.type = TKN_CHAR;
            if (!advance_enclosed(scnr, '\'')) {
                return (TokenResult) { .error = (TokenErr){
                    ._null = TKN__NULL,
                    .err_type = TKNERR_OPEN_CHAR,
                    .start = tok.start,
                }};
            } break;
        case '"':
            tok.type = TKN_STRING;
            if (!advance_enclosed(scnr, '"')) {
                return (TokenResult) { .error = (TokenErr){
                    ._null = TKN__NULL,
                    .err_type = TKNERR_OPEN_STRING,
                    .start = tok.start,
                }};
            } break;
        case '`':
            tok.type = TKN_IDENTIFIER;
            if (!advance_enclosed(scnr, '`')) {
                return (TokenResult) { .error = (TokenErr){
                    ._null = TKN__NULL,
                    .err_type = TKNERR_OPEN_IDENTIFIER,
                    .start = tok.start,
                }};
            } break;
        default:
            if (is_digit(c)) {
                tok.type = TKN_NUMBER;
                while (is_numeric_char(peek(scnr))) {
                    scnr->cur_index++;
                }
            } else if (is_identifier_char(c)) {
                tok.type = TKN_IDENTIFIER;
                while (is_identifier_char(peek(scnr))) {
                    scnr->cur_index++;
                }
                tok.len = scnr->cur_index - tok.start;
                tok = ident_to_keyword(scnr->src, tok);
            } else {
                return (TokenResult){ .error = (TokenErr){
                    ._null = TKN__NULL,
                    .err_type = TKNERR_UNEXPECTED,
                    .start = tok.start,
                }};
            } break;
    }
    tok.len = scnr->cur_index - tok.start;
    return (TokenResult){ .token = tok };
}
