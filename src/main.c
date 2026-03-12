#include "common.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "repl.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET
            " Cannot open file: %s\n"
            "  Hint: Check if the file exists and you have read permission.\n",
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
            " Not enough memory to read file.\n");
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static int runFile(VM* vm, const char* path) {
    char* source = readFile(path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) { freeNode(ast); return 1; }

    CompileResult result = compile(ast, path);
    freeNode(ast);
    if (result.had_error || !result.function) return 1;

    InterpretResult ir = runVM(vm, result.function, path);
    return (ir == INTERPRET_OK) ? 0 : 1;
}

static void printVersion(void) {
    printf(NQ_COLOR_BOLD NQ_COLOR_CYAN
        "Nolqu " NQ_VERSION NQ_COLOR_RESET "\n"
        "A simple programming language with its own VM\n"
        "Runtime: nq | File extension: .nq\n");
}

static void printHelp(void) {
    printVersion();
    printf("\n");
    printf(NQ_COLOR_BOLD "Usage:" NQ_COLOR_RESET "\n");
    printf("  nq <file.nq>           Run a Nolqu program\n");
    printf("  nq run <file.nq>       Run a Nolqu program\n");
    printf("  nq repl                Start interactive REPL mode\n");
    printf("  nq version             Show version\n");
    printf("  nq help                Show this help\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Examples:" NQ_COLOR_RESET "\n");
    printf("  nq hello.nq\n");
    printf("  nq run program.nq\n");
    printf("  nq repl\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Keywords:" NQ_COLOR_RESET "\n");
    printf("  let  print  if  else  loop  function  return  import  end\n");
    printf("  true  false  nil  and  or  not\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Built-in functions:" NQ_COLOR_RESET "\n");
    printf("  input([prompt])    Read a line from stdin\n");
    printf("  str(value)         Convert a value to string\n");
    printf("  num(value)         Convert a string to number\n");
    printf("  type(value)        Return the type name of a value\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    VM vm;
    initVM(&vm);
    int exit_code = 0;

    if (argc == 1) {
        runREPL(&vm);
    } else if (argc == 2) {
        const char* arg = argv[1];
        if      (strcmp(arg, "repl")      == 0) runREPL(&vm);
        else if (strcmp(arg, "version")   == 0 ||
                 strcmp(arg, "--version") == 0 ||
                 strcmp(arg, "-v")        == 0) printVersion();
        else if (strcmp(arg, "help")      == 0 ||
                 strcmp(arg, "--help")    == 0 ||
                 strcmp(arg, "-h")        == 0) printHelp();
        else exit_code = runFile(&vm, arg);
    } else if (argc == 3 && strcmp(argv[1], "run") == 0) {
        exit_code = runFile(&vm, argv[2]);
    } else {
        fprintf(stderr,
            NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET
            " Invalid usage.\n\n");
        printHelp();
        exit_code = 1;
    }

    freeVM(&vm);
    return exit_code;
}
