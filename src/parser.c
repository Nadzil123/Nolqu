#include "parser.h"
#include "memory.h"
#include <string.h>

// ─────────────────────────────────────────────
//  Error reporting
// ─────────────────────────────────────────────
static void errorAt(Parser* p, Token* tok, const char* msg) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->had_error  = true;

    fprintf(stderr, NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET);
    if (p->source_path)
        fprintf(stderr, " %s", p->source_path);
    fprintf(stderr, ":%d\n", tok->line);

    fprintf(stderr, "  " NQ_COLOR_BOLD "%s" NQ_COLOR_RESET "\n", msg);

    if (tok->type == TK_ERROR && tok->error) {
        fprintf(stderr, "  Detail: %s\n", tok->error);
    } else if (tok->type != TK_EOF && tok->type != TK_NEWLINE) {
        fprintf(stderr, "  Ditemukan: '%.*s'\n", tok->length, tok->start);
    }
    fprintf(stderr, "\n");
}

static void errorAtCurrent(Parser* p, const char* msg) {
    errorAt(p, &p->current, msg);
}
static void errorAtPrev(Parser* p, const char* msg) {
    errorAt(p, &p->previous, msg);
}

// ─────────────────────────────────────────────
//  Token navigation
// ─────────────────────────────────────────────
static void advance(Parser* p) {
    p->previous = p->current;
    for (;;) {
        p->current = nextToken(&p->lexer);
        if (p->current.type != TK_ERROR) break;
        errorAtCurrent(p, "Karakter tidak dikenal.");
    }
}

static bool check(Parser* p, TokenType t) {
    return p->current.type == t;
}

static bool match(Parser* p, TokenType t) {
    if (!check(p, t)) return false;
    advance(p);
    return true;
}

static void expect(Parser* p, TokenType t, const char* msg) {
    if (p->current.type == t) {
        advance(p);
        return;
    }
    errorAtCurrent(p, msg);
}

// Skip blank newlines between statements
static void skipNewlines(Parser* p) {
    while (check(p, TK_NEWLINE)) advance(p);
}

// Expect end of statement (newline or EOF)
static void expectNewline(Parser* p) {
    if (check(p, TK_EOF)) return;
    if (check(p, TK_NEWLINE)) {
        advance(p);
        return;
    }
    // Allow for blocks that end with 'end' on same line? No — force newline.
    errorAtCurrent(p, "Harap tulis satu pernyataan per baris.");
}

// ─────────────────────────────────────────────
//  String helpers
// ─────────────────────────────────────────────
static char* dupStr(const char* src, int len) {
    char* s = (char*)malloc((size_t)(len + 1));
    memcpy(s, src, (size_t)len);
    s[len] = '\0';
    return s;
}

// Extract string literal value (strips quotes, handles escapes)
static char* extractString(const char* src, int total_len, int* out_len) {
    // src points to opening ", total_len includes both quotes
    int content_len = total_len - 2;
    const char* content = src + 1;

    // Allocate enough space
    char* buf = (char*)malloc((size_t)(content_len + 1));
    int j = 0;
    for (int i = 0; i < content_len; i++) {
        if (content[i] == '\\' && i + 1 < content_len) {
            i++;
            switch (content[i]) {
                case 'n':  buf[j++] = '\n'; break;
                case 't':  buf[j++] = '\t'; break;
                case 'r':  buf[j++] = '\r'; break;
                case '"':  buf[j++] = '"';  break;
                case '\\': buf[j++] = '\\'; break;
                default:   buf[j++] = '\\'; buf[j++] = content[i]; break;
            }
        } else {
            buf[j++] = content[i];
        }
    }
    buf[j] = '\0';
    if (out_len) *out_len = j;
    return buf;
}

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static ASTNode* parseStmt(Parser* p);
static ASTNode* parseExpr(Parser* p);
static ASTNode* parseOr(Parser* p);
static ASTNode* parseAnd(Parser* p);
static ASTNode* parseEquality(Parser* p);
static ASTNode* parseComparison(Parser* p);
static ASTNode* parseAddition(Parser* p);
static ASTNode* parseMultiply(Parser* p);
static ASTNode* parseUnary(Parser* p);
static ASTNode* parseCall(Parser* p);
static ASTNode* parsePrimary(Parser* p);
static ASTNode* parseBlock(Parser* p);

// ─────────────────────────────────────────────
//  Statement parsers
// ─────────────────────────────────────────────

static ASTNode* parseLetStmt(Parser* p) {
    int line = p->previous.line;
    // 'let' already consumed
    expect(p, TK_IDENT, "Harap tulis nama variabel setelah 'let'. Contoh: let nama = \"Budi\"");
    char* name = dupStr(p->previous.start, p->previous.length);

    expect(p, TK_EQ, "Harap tambahkan '=' setelah nama variabel. Contoh: let x = 10");

    ASTNode* val = parseExpr(p);
    expectNewline(p);

    ASTNode* n = makeNode(NODE_LET, line);
    n->data.let.name  = name;
    n->data.let.value = val;
    return n;
}

static ASTNode* parsePrintStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* expr = parseExpr(p);
    expectNewline(p);

    ASTNode* n = makeNode(NODE_PRINT, line);
    n->data.print.expr = expr;
    return n;
}

static ASTNode* parseIfStmt(Parser* p) {
    int line = p->previous.line;
    // 'if' already consumed
    ASTNode* cond = parseExpr(p);
    expectNewline(p);

    ASTNode* then_block = parseBlock(p);
    ASTNode* else_block = NULL;

    if (check(p, TK_ELSE)) {
        advance(p);
        expectNewline(p);
        else_block = parseBlock(p);
    }

    expect(p, TK_END, "Harap tutup blok 'if' dengan 'end'");
    expectNewline(p);

    ASTNode* n = makeNode(NODE_IF, line);
    n->data.if_stmt.cond       = cond;
    n->data.if_stmt.then_block = then_block;
    n->data.if_stmt.else_block = else_block;
    return n;
}

static ASTNode* parseLoopStmt(Parser* p) {
    int line = p->previous.line;
    // 'loop' already consumed
    ASTNode* cond = parseExpr(p);
    expectNewline(p);

    ASTNode* body = parseBlock(p);
    expect(p, TK_END, "Harap tutup blok 'loop' dengan 'end'");
    expectNewline(p);

    ASTNode* n = makeNode(NODE_LOOP, line);
    n->data.loop.cond = cond;
    n->data.loop.body = body;
    return n;
}

static ASTNode* parseFunctionDecl(Parser* p) {
    int line = p->previous.line;
    // 'function' already consumed
    expect(p, TK_IDENT, "Harap tulis nama fungsi setelah 'function'");
    char* name = dupStr(p->previous.start, p->previous.length);

    expect(p, TK_LPAREN, "Harap buka parameter fungsi dengan '('");

    // Parse parameters
    char** params   = NULL;
    int    n_params = 0;
    int    p_cap    = 0;

    while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
        expect(p, TK_IDENT, "Harap tulis nama parameter yang valid");
        char* pname = dupStr(p->previous.start, p->previous.length);

        // Grow params array
        if (n_params >= p_cap) {
            int old = p_cap;
            p_cap = GROW_CAPACITY(old);
            params = GROW_ARRAY(char*, params, old, p_cap);
        }
        params[n_params++] = pname;

        if (!match(p, TK_COMMA)) break;
    }
    expect(p, TK_RPAREN, "Harap tutup parameter fungsi dengan ')'");
    expectNewline(p);

    ASTNode* body = parseBlock(p);
    expect(p, TK_END, "Harap tutup fungsi dengan 'end'");
    expectNewline(p);

    ASTNode* n = makeNode(NODE_FUNCTION, line);
    n->data.function.name        = name;
    n->data.function.params      = params;
    n->data.function.param_count = n_params;
    n->data.function.body        = body;
    return n;
}

static ASTNode* parseReturnStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* expr = NULL;

    if (!check(p, TK_NEWLINE) && !check(p, TK_EOF)) {
        expr = parseExpr(p);
    }
    expectNewline(p);

    ASTNode* n = makeNode(NODE_RETURN, line);
    n->data.ret.expr = expr;
    return n;
}

static ASTNode* parseImportStmt(Parser* p) {
    int line = p->previous.line;
    expect(p, TK_STRING, "Harap tulis path file setelah 'import'. Contoh: import \"stdlib/math\"");
    char* path = extractString(p->previous.start, p->previous.length, NULL);
    expectNewline(p);

    ASTNode* n = makeNode(NODE_IMPORT, line);
    n->data.import.path = path;
    return n;
}

// ─────────────────────────────────────────────
//  parseStmt — dispatch to correct parser
// ─────────────────────────────────────────────
static ASTNode* parseStmt(Parser* p) {
    skipNewlines(p);
    if (check(p, TK_EOF)) return NULL;

    advance(p);
    Token tok = p->previous;

    switch (tok.type) {
        case TK_LET:      return parseLetStmt(p);
        case TK_PRINT:    return parsePrintStmt(p);
        case TK_IF:       return parseIfStmt(p);
        case TK_LOOP:     return parseLoopStmt(p);
        case TK_FUNCTION: return parseFunctionDecl(p);
        case TK_RETURN:   return parseReturnStmt(p);
        case TK_IMPORT:   return parseImportStmt(p);

        case TK_IDENT: {
            // Look ahead: is this "name = expr" (assignment)?
            if (check(p, TK_EQ)) {
                int line = tok.line;
                char* name = dupStr(tok.start, tok.length);
                advance(p); // consume '='
                ASTNode* val = parseExpr(p);
                expectNewline(p);
                ASTNode* n = makeNode(NODE_ASSIGN, line);
                n->data.assign.name  = name;
                n->data.assign.value = val;
                return n;
            }
            // Otherwise: expression statement (e.g., function call)
            // Put the ident back as an expression
            // We need to parse it as a primary and continue as expression
            // We'll re-parse by building an ident node and going through call parsing
            ASTNode* ident = makeNode(NODE_IDENT, tok.line);
            ident->data.ident.name = dupStr(tok.start, tok.length);

            // Try to parse a call: name(...)
            ASTNode* expr = ident;
            while (check(p, TK_LPAREN)) {
                int line = p->current.line;
                advance(p); // consume '('
                ASTNode* call_node = makeNode(NODE_CALL, line);
                call_node->data.call.callee = expr;
                initNodeList(&call_node->data.call.args);

                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    ASTNode* arg = parseExpr(p);
                    appendNode(&call_node->data.call.args, arg);
                    if (!match(p, TK_COMMA)) break;
                }
                expect(p, TK_RPAREN, "Harap tutup argumen fungsi dengan ')'");
                expr = call_node;
            }

            // Could also be arithmetic follow-on, but let's treat as expr stmt
            // Check if there are binary operators following
            // (for cases like: someFunc() + 1  — though unusual as a statement)
            expectNewline(p);
            ASTNode* stmt = makeNode(NODE_EXPR_STMT, tok.line);
            stmt->data.expr_stmt.expr = expr;
            return stmt;
        }

        // Handle EOF/END gracefully (caller should catch these)
        case TK_END:
        case TK_ELSE:
        case TK_EOF:
            // Put it "back" by not consuming — but we already advanced.
            // The block parser will handle this by checking these tokens.
            // We signal "no more statements" by returning NULL.
            return NULL;

        default: {
            // Unexpected token: try to parse as expression statement
            // (e.g. a bare number or string — uncommon but possible)
            // Rewind: we advanced, but we can't un-advance.
            // Just report error and skip to next newline for recovery.
            char errbuf[128];
            snprintf(errbuf, sizeof(errbuf),
                "Pernyataan tidak valid dimulai dengan '%s'. "
                "Apakah kamu lupa keyword 'let', 'print', atau 'if'?",
                tokenTypeName(tok.type));
            errorAt(p, &tok, errbuf);
            // Skip to end of line for error recovery
            while (!check(p, TK_NEWLINE) && !check(p, TK_EOF)) advance(p);
            if (check(p, TK_NEWLINE)) advance(p);
            p->panic_mode = false;
            return NULL;
        }
    }
}

// ─────────────────────────────────────────────
//  parseBlock — parse statements until end/else/EOF
// ─────────────────────────────────────────────
static ASTNode* parseBlock(Parser* p) {
    ASTNode* block = makeNode(NODE_BLOCK, p->current.line);
    initNodeList(&block->data.block.stmts);

    while (!check(p, TK_END)  &&
           !check(p, TK_ELSE) &&
           !check(p, TK_EOF)) {
        skipNewlines(p);
        if (check(p, TK_END) || check(p, TK_ELSE) || check(p, TK_EOF)) break;
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&block->data.block.stmts, stmt);
    }
    return block;
}

// ─────────────────────────────────────────────
//  Expression parsers (recursive descent)
// ─────────────────────────────────────────────
static ASTNode* parseExpr(Parser* p)  { return parseOr(p); }

static ASTNode* parseOr(Parser* p) {
    ASTNode* left = parseAnd(p);
    while (check(p, TK_OR)) {
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseAnd(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = TK_OR;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseAnd(Parser* p) {
    ASTNode* left = parseEquality(p);
    while (check(p, TK_AND)) {
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseEquality(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = TK_AND;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseEquality(Parser* p) {
    ASTNode* left = parseComparison(p);
    while (check(p, TK_EQEQ) || check(p, TK_BANGEQ)) {
        TokenType op = p->current.type;
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseComparison(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = op;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseComparison(Parser* p) {
    ASTNode* left = parseAddition(p);
    while (check(p, TK_LT) || check(p, TK_GT) ||
           check(p, TK_LTEQ) || check(p, TK_GTEQ)) {
        TokenType op = p->current.type;
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseAddition(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = op;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseAddition(Parser* p) {
    ASTNode* left = parseMultiply(p);
    while (check(p, TK_PLUS) || check(p, TK_MINUS) || check(p, TK_DOTDOT)) {
        TokenType op = p->current.type;
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseMultiply(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = op;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseMultiply(Parser* p) {
    ASTNode* left = parseUnary(p);
    while (check(p, TK_STAR) || check(p, TK_SLASH) || check(p, TK_PERCENT)) {
        TokenType op = p->current.type;
        advance(p);
        int line = p->previous.line;
        ASTNode* right = parseUnary(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = op;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseUnary(Parser* p) {
    if (check(p, TK_MINUS) || check(p, TK_NOT)) {
        TokenType op = p->current.type;
        advance(p);
        int line = p->previous.line;
        ASTNode* operand = parseUnary(p);
        ASTNode* n = makeNode(NODE_UNARY, line);
        n->data.unary.op      = op;
        n->data.unary.operand = operand;
        return n;
    }
    return parseCall(p);
}

static ASTNode* parseCall(Parser* p) {
    ASTNode* expr = parsePrimary(p);

    while (check(p, TK_LPAREN)) {
        int line = p->current.line;
        advance(p); // consume '('

        ASTNode* call = makeNode(NODE_CALL, line);
        call->data.call.callee = expr;
        initNodeList(&call->data.call.args);

        while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
            ASTNode* arg = parseExpr(p);
            appendNode(&call->data.call.args, arg);
            if (!match(p, TK_COMMA)) break;
        }
        expect(p, TK_RPAREN, "Harap tutup argumen fungsi dengan ')'");
        expr = call;
    }
    return expr;
}

static ASTNode* parsePrimary(Parser* p) {
    // Number literal
    if (check(p, TK_NUMBER)) {
        advance(p);
        double val = strtod(p->previous.start, NULL);
        ASTNode* n = makeNode(NODE_NUMBER, p->previous.line);
        n->data.number.value = val;
        return n;
    }
    // String literal
    if (check(p, TK_STRING)) {
        advance(p);
        int slen = 0;
        char* sval = extractString(p->previous.start, p->previous.length, &slen);
        ASTNode* n = makeNode(NODE_STRING, p->previous.line);
        n->data.string.value  = sval;
        n->data.string.length = slen;
        return n;
    }
    // true
    if (check(p, TK_TRUE)) {
        advance(p);
        ASTNode* n = makeNode(NODE_BOOL, p->previous.line);
        n->data.boolean.value = true;
        return n;
    }
    // false
    if (check(p, TK_FALSE)) {
        advance(p);
        ASTNode* n = makeNode(NODE_BOOL, p->previous.line);
        n->data.boolean.value = false;
        return n;
    }
    // nil
    if (check(p, TK_NIL)) {
        advance(p);
        return makeNode(NODE_NIL, p->previous.line);
    }
    // Identifier
    if (check(p, TK_IDENT)) {
        advance(p);
        ASTNode* n = makeNode(NODE_IDENT, p->previous.line);
        n->data.ident.name = dupStr(p->previous.start, p->previous.length);
        return n;
    }
    // Grouped expression: (expr)
    if (check(p, TK_LPAREN)) {
        advance(p);
        ASTNode* inner = parseExpr(p);
        expect(p, TK_RPAREN, "Harap tutup ekspresi kurung dengan ')'");
        return inner;
    }

    // Error recovery
    errorAtCurrent(p, "Ekspresi tidak valid. Harap tulis angka, teks, variabel, atau ekspresi.");
    // Return a nil node so we can continue
    return makeNode(NODE_NIL, p->current.line);
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void initParser(Parser* p, const char* source, const char* path) {
    initLexer(&p->lexer, source);
    p->had_error   = false;
    p->panic_mode  = false;
    p->source_path = path;
    // Prime the pump
    advance(p);
}

ASTNode* parse(Parser* p) {
    ASTNode* root = makeNode(NODE_PROGRAM, 1);
    initNodeList(&root->data.block.stmts);

    skipNewlines(p);
    while (!check(p, TK_EOF)) {
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&root->data.block.stmts, stmt);
        skipNewlines(p);
        p->panic_mode = false; // reset after each statement
    }
    return root;
}
