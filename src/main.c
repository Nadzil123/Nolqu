#include "common.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "repl.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─────────────────────────────────────────────
//  File helpers
// ─────────────────────────────────────────────
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
        fprintf(stderr, NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET " Out of memory.\n");
        fclose(f); return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// ─────────────────────────────────────────────
//  Run a file
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
//  nq check — parse+compile only, no execution
// ─────────────────────────────────────────────
static int checkFile(const char* path) {
    char* source = readFile(path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) {
        freeNode(ast);
        fprintf(stderr, NQ_COLOR_RED "[ check ]" NQ_COLOR_RESET " %s: parse error\n", path);
        return 1;
    }

    CompileResult result = compile(ast, path);
    freeNode(ast);

    if (result.had_error || !result.function) {
        fprintf(stderr, NQ_COLOR_RED "[ check ]" NQ_COLOR_RESET " %s: compile error\n", path);
        return 1;
    }

    printf(NQ_COLOR_GREEN "[ check ]" NQ_COLOR_RESET " %s: ok\n", path);
    return 0;
}

// ─────────────────────────────────────────────
//  nq test — run test files, report results
//
//  A test file uses assert() to validate behaviour.
//  If it exits cleanly (no uncaught error): PASS
//  If it throws / returns non-zero:         FAIL
//
//  Usage:
//    nq test file_test.nq           — run one test file
//    nq test tests/                 — run all *_test.nq in directory
// ─────────────────────────────────────────────
static int runTestFile(VM* vm, const char* path) {
    // Reinitialise VM state for each test
    vm->stack_top   = vm->stack;
    vm->frame_count = 0;
    vm->try_depth   = 0;
    vm->thrown      = NIL_VAL;

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

static bool endsWithTestNq(const char* name) {
    size_t len = strlen(name);
    if (len < 8) return false;
    // Accept: *_test.nq  OR  test_*.nq  OR  *.test.nq
    return (strcmp(name + len - 8, "_test.nq") == 0) ||
           (strncmp(name, "test_", 5) == 0 && len > 8) ||
           (strstr(name, ".test.nq") != NULL);
}

static int runTests(const char* path) {
    VM vm;
    initVM(&vm);

    int passed = 0, failed = 0;

    // Single file
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        int r = runTestFile(&vm, path);
        if (r == 0) {
            printf(NQ_COLOR_GREEN "  PASS" NQ_COLOR_RESET " %s\n", path);
            passed++;
        } else {
            printf(NQ_COLOR_RED   "  FAIL" NQ_COLOR_RESET " %s\n", path);
            failed++;
        }
    } else {
        // Directory — scan for test files
        DIR* dir = opendir(path);
        if (!dir) {
            fprintf(stderr, NQ_COLOR_RED "[ test ]" NQ_COLOR_RESET
                " Cannot open: %s\n", path);
            freeVM(&vm);
            return 1;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!endsWithTestNq(entry->d_name)) continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            int r = runTestFile(&vm, full);
            if (r == 0) {
                printf(NQ_COLOR_GREEN "  PASS" NQ_COLOR_RESET " %s\n", entry->d_name);
                passed++;
            } else {
                printf(NQ_COLOR_RED   "  FAIL" NQ_COLOR_RESET " %s\n", entry->d_name);
                failed++;
            }
        }
        closedir(dir);
    }

    int total = passed + failed;
    printf("\n");
    if (failed == 0) {
        printf(NQ_COLOR_GREEN NQ_COLOR_BOLD
            "  All %d test(s) passed.\n" NQ_COLOR_RESET, total);
    } else {
        printf(NQ_COLOR_RED NQ_COLOR_BOLD
            "  %d/%d test(s) failed.\n" NQ_COLOR_RESET, failed, total);
    }

    freeVM(&vm);
    return failed > 0 ? 1 : 0;
}

// ─────────────────────────────────────────────
//  Version / Help
// ─────────────────────────────────────────────
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
    printf("  nq check <file.nq>     Parse and compile only (no run)\n");
    printf("  nq test <file|dir>     Run test files (*_test.nq / test_*.nq)\n");
    printf("  nq repl                Start interactive REPL mode\n");
    printf("  nq version             Show version\n");
    printf("  nq help                Show this help\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Examples:" NQ_COLOR_RESET "\n");
    printf("  nq hello.nq\n");
    printf("  nq check program.nq\n");
    printf("  nq test tests/\n");
    printf("  nq repl\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Keywords:" NQ_COLOR_RESET "\n");
    printf("  let  print  if  else  loop  function  return\n");
    printf("  import  try  catch  end  true  false  nil\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Built-in functions:" NQ_COLOR_RESET "\n");
    printf("  I/O:      input([prompt])  print\n");
    printf("  Type:     str(v)  num(v)  type(v)\n");
    printf("            is_nil  is_num  is_str  is_bool  is_array\n");
    printf("  Math:     sqrt  floor  ceil  round  abs  pow  min  max\n");
    printf("  String:   upper  lower  slice  trim  replace  split\n");
    printf("            startswith  endswith  index  repeat  join\n");
    printf("  Array:    len  push  pop  remove  contains  sort\n");
    printf("  Random:   random()  rand_int(lo, hi)\n");
    printf("  File:     file_read  file_write  file_append\n");
    printf("            file_exists  file_lines\n");
    printf("  Error:    error(msg)  assert(cond [, msg])\n");
    printf("  Time:     clock()\n");
    printf("  Memory:   mem_usage()  gc_collect()\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Stdlib modules:" NQ_COLOR_RESET "\n");
    printf("  import \"stdlib/math\"    clamp  lerp  sign\n");
    printf("  import \"stdlib/array\"   map  filter  reduce  reverse\n");
    printf("  import \"stdlib/file\"    read_or_default  write_lines  count_lines\n");
    printf("\n");
}

// ─────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc == 1) {
        VM vm; initVM(&vm);
        runREPL(&vm);
        freeVM(&vm);
        return 0;
    }

    const char* cmd = argv[1];

    // Single-word commands (no file arg)
    if (strcmp(cmd, "repl") == 0) {
        VM vm; initVM(&vm);
        runREPL(&vm);
        freeVM(&vm);
        return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printVersion(); return 0;
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        printHelp(); return 0;
    }

    // Two-word commands
    if (argc == 3) {
        if (strcmp(cmd, "run") == 0) {
            VM vm; initVM(&vm);
            int r = runFile(&vm, argv[2]);
            freeVM(&vm); return r;
        }
        if (strcmp(cmd, "check") == 0) {
            return checkFile(argv[2]);
        }
        if (strcmp(cmd, "test") == 0) {
            return runTests(argv[2]);
        }
    }

    // Bare filename
    if (argc == 2) {
        VM vm; initVM(&vm);
        int r = runFile(&vm, cmd);
        freeVM(&vm); return r;
    }

    fprintf(stderr, NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET " Invalid usage.\n\n");
    printHelp();
    return 1;
}
