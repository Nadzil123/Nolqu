# Nolqu Roadmap

> This roadmap reflects the current state of Nolqu and planned direction toward a stable release.
> Features may shift between versions as development progresses.

---

## ✅ v0.1.0 — Foundation
Variables, conditionals, loops, functions, recursion, REPL, CLI, bytecode VM.

## ✅ v0.2.0 — User Input & Builtins
`input()`, `str()`, `num()`, `type()`. Full English translation.

## ✅ v0.3.0 — Arrays, Stdlib, Modules
Array / list with `[]` syntax, index access, index assignment.  
Math stdlib: `sqrt`, `pow`, `abs`, `floor`, `ceil`, `round`, `max`, `min`.  
String stdlib: `upper`, `lower`, `slice`, `trim`, `replace`, `split`, `startswith`, `endswith`.  
Module system: `import "file"` — active and working.  
Bugfix: stack corruption in `if` blocks without `else` inside functions.

---

## ✅ v0.4.0 — Scope Improvements
`if` and `loop` blocks now create their own scope. Variables declared inside a block are destroyed when the block ends. Shadowing works correctly — inner `let` doesn't affect the outer variable.  
Bugfix: slot-0 reservation missing for top-level script, causing block-local variables to resolve incorrectly.

---

## ✅ v0.5.0 — Extended Stdlib
`random()`, `rand_int(lo, hi)`. String: `index(s, sub)`, `repeat(s, n)`. Array: `sort(arr)`, `join(arr, sep)`.

---

## ✅ v0.6.0 — Error Handling
`try`/`catch`/`end` blocks. `error(message)` builtin. Catchable runtime errors: division by zero, modulo by zero, arithmetic type mismatch. Nested try/catch up to 64 levels. Bugfix: THROW_ERROR macro goto fix.

---

## ✅ v0.7.0 — File I/O
`file_read(path)`, `file_write(path, content)`, `file_append(path, content)`, `file_exists(path)`, `file_lines(path)`. All file errors are catchable via `try/catch`. Module `stdlib/file.nq` adds `read_or_default`, `write_lines`, `count_lines`.

---

## ✅ v0.8.0 — Garbage Collector *(Beta)*
Mark-and-sweep GC. Automatic trigger at 1 MB threshold, grows to 2× surviving heap after each cycle. Marks all roots: stack, call frames, globals, pending thrown value. Recursively marks array items and function constants. `gc_collect()` builtin for manual trigger.

---

## ✅ v0.9.0 — Stabilization *(Beta)*
New builtins: `assert`, `clock`, `mem_usage`, `is_nil/num/str/bool/array`. "Did you mean?" suggestions for undefined variables (Levenshtein distance). Compiler warning for unused local variables (`_` prefix suppresses). Improved REPL: `help`, `clear`, proportional depth prompt, heap-allocated buffer, clean VM reset. Upgraded error messages: `file:line` format, colored call chain, comparison errors catchable. Zero warnings build.

---

## v1.0.0-rc1 — Release Candidate
**Focus:** Final testing and polish.

- Full test suite across all features
- Documentation review
- No new features — bugfixes only

---

## v1.0.0 — Stable Release 🎉
- Stable syntax (no breaking changes after this)
- Stable runtime
- Core stdlib complete
- Module system stable
- Garbage collector stable
- Ready for real programs

---

## Summary

| Version | Focus |
|---------|-------|
| v0.1 | Foundation ✅ |
| v0.2 | User input & builtins ✅ |
| v0.3 | Arrays, stdlib, modules ✅ |
| v0.4 | Scope improvements ✅ |
| v0.5 | Extended stdlib ✅ |
| v0.6 | Error handling ✅ |
| v0.7 | File I/O ✅ |
| v0.8 | Garbage collector ✅ |
| v0.9 | Stabilization ✅ |
| v1.0.0-rc1 | Release candidate |
| v1.0.0 | Stable release 🎉 |
