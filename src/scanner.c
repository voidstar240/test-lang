#include "scanner.h"

static inline bool scnr_is_last(const Scanner* scnr) {
    return scnr->current + 1 >= scnr->start + scnr->length;
}

static inline char scnr_peek(const Scanner* scnr) {
    if (scnr_is_end(scnr)) { return '\0'; }
    return scnr->current[0];
}

static inline char scnr_peek_next(const Scanner* scnr) {
    if (scnr_is_last(scnr)) { return '\0'; }
    return scnr->current[1];
}

static inline char scnr_advance(Scanner* scnr) {
    if (scnr_is_end(scnr)) { return '\0'; }
    const char c = *scnr->current;
    scnr->current++;
    return c;
}

static inline bool scnr_match(Scanner* scnr, char c) {
    if (scnr_peek(scnr) != c) { return false; }
    scnr->current++;
    return true;
}

static void skip_line_comment_body(Scanner* scnr) {
    while (true) {
        switch (scnr_peek(scnr)) {
            case '\n':
            case '\0':
                return;

            default:
                scnr->current++;
        }
    }
}

static void skip_block_comment_body(Scanner* scnr) {
    size_t depth = 1;
    while (depth > 0){
        switch (scnr_peek(scnr)) {
            case '/':
                if (scnr_peek_next(scnr) == '*') {
                    depth++;
                    scnr->current++;
                    scnr->col++;
                }
                scnr->current++;
                scnr->col++;
                break;

            case '*':
                if (scnr_peek_next(scnr) == '/') {
                    depth--;
                    scnr->current++;
                    scnr->col++;
                }
                scnr->current++;
                scnr->col++;
                break;

            case '\n':
                scnr->current++;
                scnr->line++;
                scnr->col = 1;
                break;

            case '\0': return;

            default:
                scnr->current++;
                scnr->col++;
                break;
        }
    }
}

void scnr_skip_whitespace(Scanner* scnr) {
    while (true) {
        switch (scnr_peek(scnr)) {
            case '\n':
                scnr->current++;
                scnr->line++;
                scnr->col = 1;
                break;

            case ' ':
            case '\t':
            case '\r':
                scnr->current++;
                scnr->col++;
                break;
                
            case '/':
                switch (scnr_peek_next(scnr)) {
                    case '/': // LINE COMMENT
                        scnr->current += 2;
                        skip_line_comment_body(scnr);
                        break;

                    case '*': // BLOCK COMMENT
                        scnr->current += 2;
                        scnr->col += 2;
                        skip_block_comment_body(scnr);
                        break;
                    default: return;
                }
                break;

            default: return;
        }
    }
}

static Token mktoken(const Scanner* scnr, TokenType type) {
    return (Token){
        .type = type,
        .start = scnr->start,
        .length = (size_t)(scnr->current - scnr->start),
        .col = scnr->col,
        .line = scnr->line,
    };
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

static Token scan_char(Scanner* scnr) {
    bool esc = false;
    while (true) {
        switch (scnr_peek(scnr)) {
            case '\0': return mktoken(scnr, TKN__NULL);
            case '\n': return mktoken(scnr, TKN__NULL);
            case '\\':
                scnr->current++;
                if (!esc) {
                    esc = true;
                } else {
                    esc = false;
                }
                continue;
            case '\'':
                scnr->current++;
                if (!esc) {
                    return mktoken(scnr, TKN_CHAR);
                }
                esc = false;
                continue;
            default:
                scnr->current++;
                esc = false;
                continue;
        }
    }
}

static Token scan_string(Scanner* scnr) {
    bool esc = false;
    while (true) {
        switch (scnr_peek(scnr)) {
            case '\0': return mktoken(scnr, TKN__NULL);
            case '\n': return mktoken(scnr, TKN__NULL);
            case '\\':
                scnr->current++;
                if (!esc) {
                    esc = true;
                } else {
                    esc = false;
                }
                continue;
            case '"':
                scnr->current++;
                if (!esc) {
                    return mktoken(scnr, TKN_STRING);
                }
                esc = false;
                continue;
            default:
                scnr->current++;
                esc = false;
                continue;
        }
    }
}

static Token scan_number(Scanner* scnr) {
    const char first = scnr->start[0];
    bool has_dec = first == '.';
    bool has_exp = false;
    if (first == '0') {
        switch (scnr_peek(scnr)) {
            case 'b':
                scnr->current++;
                // TODO scan binary number
                return mktoken(scnr, TKN__NULL);
            case 'o':
                scnr->current++;
                // TODO scan octal number
                return mktoken(scnr, TKN__NULL);
            case 'x':
                scnr->current++;
                // TODO scan hex number
                return mktoken(scnr, TKN__NULL);
        }
    }
    while (true) {
        const char c = scnr_peek(scnr);
        if (c == '.') {
            if (has_dec) {
                return mktoken(scnr, TKN_NUMBER);
            } 
            has_dec = true;
            scnr->current++;
        } else if (c == 'E') {
            if (has_exp) { return mktoken(scnr, TKN_NUMBER); }
            scnr->current++;
            has_exp = true;
            has_dec = true;
            const char p = scnr_peek(scnr);
            if (p == '-') {
                if (!is_digit(scnr_peek_next(scnr))) {
                    return mktoken(scnr, TKN_NUMBER);
                }
                scnr->current += 2;
            } else if (is_digit(p)) {
                scnr->current++;
            } else {
                return mktoken(scnr, TKN_NUMBER);
            }
        } else if (is_digit(c) || c == '_') {
            scnr->current++;
        } else {
            return mktoken(scnr, TKN_NUMBER);
        }
    }
}

static Token scan_identifier(Scanner* scnr) {
    while (true) {
        if (!is_identifier_char(scnr_peek(scnr))) {
            return mktoken(scnr, TKN_IDENTIFIER);
        }
        scnr->current++;
    }
}

Token scnr_scan_token(Scanner* scnr) {
    scnr->col += (size_t)(scnr->current - scnr->start);
    scnr_skip_whitespace(scnr);
    scnr->length -= (size_t)(scnr->current - scnr->start);
    scnr->start = scnr->current;
    const char c = scnr_advance(scnr);
    switch (c) {
        case '\0': return mktoken(scnr, TKN_EOF);

        case '(': return mktoken(scnr, TKN_L_PAREN);
        case ')': return mktoken(scnr, TKN_R_PAREN);
        case '{': return mktoken(scnr, TKN_L_BRACE);
        case '}': return mktoken(scnr, TKN_R_BRACE);
        case '[': return mktoken(scnr, TKN_L_BRACKET);
        case ']': return mktoken(scnr, TKN_R_BRACKET);
        case ',': return mktoken(scnr, TKN_COMMA);
        case ':': return mktoken(scnr, TKN_COLON);
        case ';': return mktoken(scnr, TKN_SEMICOLON);
        case '?': return mktoken(scnr, TKN_QUESTION);

        case '.':
            if (is_identifier_char(scnr_peek(scnr))) {
                return scan_number(scnr);
            }
            return mktoken(scnr, TKN_DOT);

        case '+': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_PLUS_EQUAL : TKN_PLUS);
        case '-': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_MINUS_EQUAL : TKN_MINUS);
        case '*': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_STAR_EQUAL : TKN_STAR);
        case '/': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_SLASH_EQUAL : TKN_SLASH);
        case '%': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_PERCENT_EQUAL : TKN_PERCENT);
        case '~': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_TILDE_EQUAL : TKN_TILDE);
        case '^': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_CARET_EQUAL : TKN_CARET);
        case '!': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_BANG_EQUAL : TKN_BANG);
        case '=': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_EQUAL_EQUAL : TKN_EQUAL);

        case '<': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_LESS_EQUAL :
                scnr_match(scnr, '<') ?
                    scnr_match(scnr, '=') ? TKN_LESS_LESS_EQUAL
                        : TKN_LESS_LESS
                    : TKN_LESS);
        case '>': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_GREATER_EQUAL :
                scnr_match(scnr, '>') ?
                    scnr_match(scnr, '=') ? TKN_GREATER_GREATER_EQUAL
                        : TKN_GREATER_GREATER
                    : TKN_GREATER);

        case '&': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_AMP_EQUAL :
                scnr_match(scnr, '&') ? TKN_AMP_AMP : TKN_AMP);
        case '|': return mktoken(scnr,
            scnr_match(scnr, '=') ? TKN_PIPE_EQUAL :
                scnr_match(scnr, '|') ? TKN_PIPE_PIPE : TKN_PIPE);

        case '\'': return scan_char(scnr);
        case '"': return scan_string(scnr);

        default:
            if (is_digit(c)) {
                return scan_number(scnr);
            } else if (is_identifier_char(c)) {
                return scan_identifier(scnr);
            } else {
                return mktoken(scnr, TKN__NULL);
            }
    }
}
