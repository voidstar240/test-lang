#include "lexer.h"

#define range_A_Z 'A': case 'B': case 'C': case 'D': case 'E': case 'F':\
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':\
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':\
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':\
        case 'Y': case 'Z'

#define range_a_z 'a': case 'b': case 'c': case 'd': case 'e': case 'f':\
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':\
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':\
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':\
        case 'y': case 'z'

#define range_1_9 '1': case '2': case '3': case '4': case '5': case '6':\
        case '7': case '8': case '9'

#define range_0_9 '0': case '1': case '2': case '3': case '4': case '5':\
        case '6': case '7': case '8': case '9'

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

static Token scan_number(Scanner* scnr) {
    while (true) {
        switch (scnr_peek(scnr)) {
            case 'E':
                scnr->current++;
                if (scnr_peek(scnr) == '-') {
                    switch (scnr_peek_next(scnr)) {
                        case range_0_9:
                            scnr->current += 2;
                            continue;
                    }
                    return mktoken(scnr, TKN_NUMBER);
                }
                continue;
            case '.':
            case '_':
            case range_0_9:
                scnr->current++;
                continue;
            default: return mktoken(scnr, TKN_NUMBER);
        }
    }
}

static Token scan_identifier(Scanner* scnr) {
    while (true) {
        switch (scnr_peek(scnr)) {
            case range_A_Z:
            case range_a_z:
            case range_0_9:
            case '_':
                scnr->current++;
                continue;
            default: return mktoken(scnr, TKN_IDENTIFIER);
        }
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
            switch (scnr_peek(scnr)) {
                case range_0_9: return scan_number(scnr);
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

        // STRING
        case '"':;
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

        // NUMBER
        case '0':
            switch (scnr_peek(scnr)) {
                case 'b':
                    // binary
                    return mktoken(scnr, TKN__NULL);
                case 'o':
                    // octal
                    return mktoken(scnr, TKN__NULL);
                case 'x':
                    // hex
                    return mktoken(scnr, TKN__NULL);
            } // FALLTHROUGH
        case range_1_9:
            return scan_number(scnr);

        // IDENTIFIER
        case range_A_Z:
        case range_a_z:
        case '_':
            return scan_identifier(scnr);
    }
    return mktoken(scnr, TKN__NULL);
}
