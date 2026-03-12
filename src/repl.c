#include "repl.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define REPL_LINE_MAX 4096

void runREPL(VM* vm) {
    printf(NQ_COLOR_CYAN
        " ╔════════════════════════════════════╗\n"
        " ║   Nolqu v" NQ_VERSION "  — REPL Mode         ║\n"
        " ║   Type 'exit' to quit              ║\n"
        " ╚════════════════════════════════════╝\n"
        NQ_COLOR_RESET "\n");

    char line[REPL_LINE_MAX];
    char carry[REPL_LINE_MAX * 4] = "";
    int  depth = 0;

    for (;;) {
        if (depth == 0) printf(NQ_COLOR_GREEN "nq" NQ_COLOR_RESET "> ");
        else            printf(NQ_COLOR_YELLOW "...  " NQ_COLOR_RESET);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }

        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf(NQ_COLOR_CYAN "Goodbye!\n" NQ_COLOR_RESET);
            break;
        }

        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strncmp(trimmed, "if ",       3) == 0 ||
            strncmp(trimmed, "loop ",     5) == 0 ||
            strncmp(trimmed, "function ", 9) == 0) depth++;
        if (strcmp(trimmed, "end") == 0 && depth > 0) depth--;

        strncat(carry, line, sizeof(carry) - strlen(carry) - 2);
        strncat(carry, "\n", sizeof(carry) - strlen(carry) - 1);
        if (depth > 0) continue;

        Parser p;
        initParser(&p, carry, "<repl>");
        ASTNode* ast = parse(&p);

        if (p.had_error) { freeNode(ast); carry[0] = '\0'; continue; }

        CompileResult result = compile(ast, "<repl>");
        freeNode(ast);

        if (!result.had_error && result.function) {
            InterpretResult ir = runVM(vm, result.function, "<repl>");
            if (ir != INTERPRET_OK) {
                vm->stack_top   = vm->stack;
                vm->frame_count = 0;
            }
        }
        carry[0] = '\0';
        depth    = 0;
    }
}
