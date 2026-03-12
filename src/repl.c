#include "repl.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define REPL_LINE_MAX 4096
#define REPL_HISTORY  16

void runREPL(VM* vm) {
    printf(NQ_COLOR_CYAN
        " ╔══════════════════════════════════╗\n"
        " ║   Nolqu v" NQ_VERSION "  — Mode REPL      ║\n"
        " ║   Ketik 'keluar' untuk berhenti  ║\n"
        " ╚══════════════════════════════════╝\n"
        NQ_COLOR_RESET "\n");

    char line[REPL_LINE_MAX];
    char carry[REPL_LINE_MAX * 4] = "";  // multi-line buffer
    int  depth = 0;                       // nesting depth (if/loop/function)

    for (;;) {
        // Show prompt — deeper nesting gets "..." prompt
        if (depth == 0) {
            printf(NQ_COLOR_GREEN "nq" NQ_COLOR_RESET "> ");
        } else {
            printf(NQ_COLOR_YELLOW "...  " NQ_COLOR_RESET);
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // Remove trailing newline
        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        // Check exit commands
        if (strcmp(line, "keluar") == 0 ||
            strcmp(line, "exit")   == 0 ||
            strcmp(line, "quit")   == 0) {
            printf(NQ_COLOR_CYAN "Sampai jumpa!\n" NQ_COLOR_RESET);
            break;
        }

        // Track nesting depth for multi-line input
        // Simple heuristic: count if/loop/function vs end
        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strncmp(trimmed, "if ",       3) == 0 ||
            strncmp(trimmed, "loop ",     5) == 0 ||
            strncmp(trimmed, "function ", 9) == 0) {
            depth++;
        }
        if (strncmp(trimmed, "else", 4) == 0 && depth == 1) {
            // Don't change depth for else
        }
        if (strcmp(trimmed, "end") == 0 && depth > 0) {
            depth--;
        }

        // Append to carry buffer
        strncat(carry, line, sizeof(carry) - strlen(carry) - 2);
        strncat(carry, "\n", sizeof(carry) - strlen(carry) - 1);

        // Don't execute until block is closed
        if (depth > 0) continue;

        // Parse and compile the buffered input
        Parser p;
        initParser(&p, carry, "<repl>");
        ASTNode* ast = parse(&p);

        if (p.had_error) {
            freeNode(ast);
            carry[0] = '\0';
            continue;
        }

        CompileResult result = compile(ast, "<repl>");
        freeNode(ast);

        if (!result.had_error && result.function) {
            InterpretResult ir = runVM(vm, result.function, "<repl>");
            // Re-initialize for next input (stack was cleaned by VM)
            if (ir != INTERPRET_OK) {
                // Reset VM stack on error
                vm->stack_top   = vm->stack;
                vm->frame_count = 0;
            }
        }

        carry[0] = '\0';
        depth    = 0;
    }
}
