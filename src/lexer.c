#include "lexer.h"

// ─────────────────────────────────────────────
//  Keyword table
// ─────────────────────────────────────────────
typedef struct { const char* word; int len; TokenType type; } Keyword;

static Keyword keywords[] = {
    {"let",      3, TK_LET},
    {"print",    5, TK_PRINT},
    {"if",       2, TK_IF},
    {"else",     4, TK_ELSE},
    {"loop",     4, TK_LOOP},
    {"function", 8, TK_FUNCTION},
    {"return",   6, TK_RETURN},
    {"import",   6, TK_IMPORT},
    {"end",      3, TK_END},
    {"true",     4, TK_TRUE},
    {"false",    5, TK_FALSE},
    {"nil",      3, TK_NIL},
    {"and",      3, TK_AND},
    {"or",       2, TK_OR},
    {"not",      3, TK_NOT},
    {NULL,       0, TK_IDENT},
};

// ─────────────────────────────────────────────
//  Lexer helpers
// ─────────────────────────────────────────────
void initLexer(Lexer* lex, const char* source) {
    lex->source  = source;
    lex->start   = source;
    lex->current = source;
    lex->line    = 1;
}

static bool   isAtEnd(Lexer* lex)    { return *lex->current == '\0'; }
static char   peek(Lexer* lex)       { return *lex->current; }
static char   peekNext(Lexer* lex)   { return isAtEnd(lex) ? '\0' : lex->current[1]; }
static char   advance(Lexer* lex)    { return *lex->current++; }
static bool   match(Lexer* lex, char c) {
    if (isAtEnd(lex) || *lex->current != c) return false;
    lex->current++;
    return true;
}

static Token makeToken(Lexer* lex, TokenType type) {
    Token t;
    t.type   = type;
    t.start  = lex->start;
    t.length = (int)(lex->current - lex->start);
    t.line   = lex->line;
    t.error  = NULL;
    return t;
}

static Token errorToken(Lexer* lex, const char* msg) {
    Token t;
    t.type   = TK_ERROR;
    t.start  = lex->start;
    t.length = (int)(lex->current - lex->start);
    t.line   = lex->line;
    t.error  = msg;
    return t;
}

// Skip whitespace (spaces, tabs, carriage returns) and comments
// STOP at newlines — they are significant tokens
static void skipWhitespace(Lexer* lex) {
    for (;;) {
        char c = peek(lex);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance(lex);
                break;
            case '#': // single-line comment
                while (!isAtEnd(lex) && peek(lex) != '\n') advance(lex);
                break;
            default:
                return;
        }
    }
}

// ─────────────────────────────────────────────
//  Specific token scanners
// ─────────────────────────────────────────────
static Token scanString(Lexer* lex) {
    while (!isAtEnd(lex) && peek(lex) != '"') {
        if (peek(lex) == '\n') lex->line++;
        if (peek(lex) == '\\') {
            advance(lex); // skip escape char
            if (!isAtEnd(lex)) advance(lex);
        } else {
            advance(lex);
        }
    }
    if (isAtEnd(lex)) {
        return errorToken(lex, "String tidak ditutup. Apakah kamu lupa tanda '\"' di akhir?");
    }
    advance(lex); // closing "
    return makeToken(lex, TK_STRING);
}

static Token scanNumber(Lexer* lex) {
    while (isdigit(peek(lex))) advance(lex);
    if (peek(lex) == '.' && isdigit(peekNext(lex))) {
        advance(lex); // consume '.'
        while (isdigit(peek(lex))) advance(lex);
    }
    return makeToken(lex, TK_NUMBER);
}

static Token scanIdent(Lexer* lex) {
    while (isalnum(peek(lex)) || peek(lex) == '_') advance(lex);
    int len = (int)(lex->current - lex->start);
    // Check keywords
    for (int i = 0; keywords[i].word != NULL; i++) {
        if (keywords[i].len == len &&
            memcmp(lex->start, keywords[i].word, (size_t)len) == 0) {
            return makeToken(lex, keywords[i].type);
        }
    }
    return makeToken(lex, TK_IDENT);
}

// ─────────────────────────────────────────────
//  Main tokenizer
// ─────────────────────────────────────────────
Token nextToken(Lexer* lex) {
    skipWhitespace(lex);
    lex->start = lex->current;

    if (isAtEnd(lex)) return makeToken(lex, TK_EOF);

    char c = advance(lex);

    // Identifiers and keywords
    if (isalpha(c) || c == '_') return scanIdent(lex);
    // Numbers
    if (isdigit(c)) return scanNumber(lex);

    switch (c) {
        case '\n':
            lex->line++;
            return makeToken(lex, TK_NEWLINE);

        case '+': return makeToken(lex, TK_PLUS);
        case '-': return makeToken(lex, TK_MINUS);
        case '*': return makeToken(lex, TK_STAR);
        case '/': return makeToken(lex, TK_SLASH);
        case '%': return makeToken(lex, TK_PERCENT);
        case '(': return makeToken(lex, TK_LPAREN);
        case ')': return makeToken(lex, TK_RPAREN);
        case ',': return makeToken(lex, TK_COMMA);
        case '[': return makeToken(lex, TK_LBRACKET);
        case ']': return makeToken(lex, TK_RBRACKET);

        case '=':
            return makeToken(lex, match(lex, '=') ? TK_EQEQ : TK_EQ);
        case '!':
            if (match(lex, '=')) return makeToken(lex, TK_BANGEQ);
            return errorToken(lex, "Karakter '!' tidak valid. Maksudmu '!='?");
        case '<':
            return makeToken(lex, match(lex, '=') ? TK_LTEQ : TK_LT);
        case '>':
            return makeToken(lex, match(lex, '=') ? TK_GTEQ : TK_GT);
        case '.':
            if (match(lex, '.')) return makeToken(lex, TK_DOTDOT);
            return errorToken(lex, "Karakter '.' tidak valid. Mungkin maksudmu '..' untuk concat?");
        case '"':
            return scanString(lex);

        default: {
            // Build friendly error message
            static char errbuf[64];
            snprintf(errbuf, sizeof(errbuf),
                "Karakter tidak dikenal: '%c' (ASCII %d)", c, (int)c);
            return errorToken(lex, errbuf);
        }
    }
}

// ─────────────────────────────────────────────
//  Debug helpers
// ─────────────────────────────────────────────
const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TK_NUMBER:   return "ANGKA";
        case TK_STRING:   return "TEKS";
        case TK_IDENT:    return "NAMA";
        case TK_LET:      return "let";
        case TK_PRINT:    return "print";
        case TK_IF:       return "if";
        case TK_ELSE:     return "else";
        case TK_LOOP:     return "loop";
        case TK_FUNCTION: return "function";
        case TK_RETURN:   return "return";
        case TK_IMPORT:   return "import";
        case TK_END:      return "end";
        case TK_TRUE:     return "true";
        case TK_FALSE:    return "false";
        case TK_NIL:      return "nil";
        case TK_AND:      return "and";
        case TK_OR:       return "or";
        case TK_NOT:      return "not";
        case TK_PLUS:     return "+";
        case TK_MINUS:    return "-";
        case TK_STAR:     return "*";
        case TK_SLASH:    return "/";
        case TK_PERCENT:  return "%";
        case TK_EQ:       return "=";
        case TK_EQEQ:     return "==";
        case TK_BANGEQ:   return "!=";
        case TK_LT:       return "<";
        case TK_GT:       return ">";
        case TK_LTEQ:     return "<=";
        case TK_GTEQ:     return ">=";
        case TK_DOTDOT:   return "..";
        case TK_LPAREN:   return "(";
        case TK_RPAREN:   return ")";
        case TK_COMMA:    return ",";
        case TK_NEWLINE:  return "BARIS_BARU";
        case TK_EOF:      return "EOF";
        case TK_ERROR:    return "ERROR";
        default:          return "???";
    }
}
