#ifndef NQ_PARSER_H
#define NQ_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"

// ─────────────────────────────────────────────
//  Parser state
// ─────────────────────────────────────────────
typedef struct {
    Lexer   lexer;
    Token   current;
    Token   previous;
    bool    had_error;
    bool    panic_mode;    // suppress cascading errors
    const char* source_path;
} Parser;

// ─────────────────────────────────────────────
//  Parser API
// ─────────────────────────────────────────────
void     initParser(Parser* p, const char* source, const char* path);
ASTNode* parse(Parser* p);          // returns NODE_PROGRAM root

#endif // NQ_PARSER_H
