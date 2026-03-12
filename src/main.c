#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "repl.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
//  Read entire file into a malloc'd buffer
// ─────────────────────────────────────────────
static char* readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET
            " Tidak dapat membuka file: %s\n"
            "  Petunjuk: Periksa apakah file tersebut ada dan kamu memiliki izin baca.\n",
            path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* buf = (char*)malloc((size_t)(size + 1));
    if (!buf) {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET
            " Tidak cukup memori untuk membaca file.\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

// ─────────────────────────────────────────────
//  Run a .nq source file
// ─────────────────────────────────────────────
static int runFile(VM* vm, const char* path) {
    char* source = readFile(path);
    if (!source) return 1;

    // 1. Parse
    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) {
        freeNode(ast);
        return 1;
    }

    // 2. Compile
    CompileResult result = compile(ast, path);
    freeNode(ast);

    if (result.had_error || !result.function) {
        return 1;
    }

    // 3. Execute
    InterpretResult ir = runVM(vm, result.function, path);

    switch (ir) {
        case INTERPRET_OK:            return 0;
        case INTERPRET_COMPILE_ERROR: return 1;
        case INTERPRET_RUNTIME_ERROR: return 1;
        default:                      return 1;
    }
}

// ─────────────────────────────────────────────
//  Print version info
// ─────────────────────────────────────────────
static void printVersion(void) {
    printf(NQ_COLOR_BOLD NQ_COLOR_CYAN
        "Nolqu " NQ_VERSION NQ_COLOR_RESET "\n"
        "Bahasa pemrograman mandiri dengan VM sendiri\n"
        "Runtime: nq | Ekstensi file: .nq\n"
    );
}

// ─────────────────────────────────────────────
//  Print help
// ─────────────────────────────────────────────
static void printHelp(void) {
    printVersion();
    printf("\n");
    printf(NQ_COLOR_BOLD "Penggunaan:" NQ_COLOR_RESET "\n");
    printf("  nq <file.nq>           Jalankan program Nolqu\n");
    printf("  nq run <file.nq>       Jalankan program Nolqu\n");
    printf("  nq repl                Masuk ke mode interaktif (REPL)\n");
    printf("  nq version             Tampilkan versi\n");
    printf("  nq help                Tampilkan bantuan ini\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Contoh:" NQ_COLOR_RESET "\n");
    printf("  nq hello.nq\n");
    printf("  nq run program.nq\n");
    printf("  nq repl\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Keyword Nolqu:" NQ_COLOR_RESET "\n");
    printf("  let  print  if  else  loop  function  return  import  end\n");
    printf("  true  false  nil  and  or  not\n");
    printf("\n");
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    VM vm;
    initVM(&vm);

    int exit_code = 0;

    if (argc == 1) {
        // No arguments: start REPL
        runREPL(&vm);

    } else if (argc == 2) {
        const char* arg = argv[1];

        if (strcmp(arg, "repl") == 0) {
            runREPL(&vm);

        } else if (strcmp(arg, "version") == 0 ||
                   strcmp(arg, "--version") == 0 ||
                   strcmp(arg, "-v") == 0) {
            printVersion();

        } else if (strcmp(arg, "help") == 0 ||
                   strcmp(arg, "--help") == 0 ||
                   strcmp(arg, "-h") == 0) {
            printHelp();

        } else {
            // Treat as filename
            exit_code = runFile(&vm, arg);
        }

    } else if (argc == 3 && strcmp(argv[1], "run") == 0) {
        exit_code = runFile(&vm, argv[2]);

    } else {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET
            " Penggunaan tidak valid.\n\n");
        printHelp();
        exit_code = 1;
    }

    freeVM(&vm);
    return exit_code;
}
