# Flux Language Project — Roadmap to v1.0 Stable

## Status: February 2026

> This document tracks every feature, component, and fix required to bring the Flux compiler from its current state (frontend-only: lexer → parser → AST → basic resolver) to a production-ready **v1.0** release with code generation, a standard library, and proper tooling.

---

## Philosophy Reminder

Flux is strictly explicit: every type must be stated, every allocation is visible, and there are no hidden conversions or copies. **Type inference for let bindings is not supported and will not be implemented.**

## Versioning

Flux follows semantic versioning for released stable versions. During active development before the 1.0.0 stable milestone, the project will use an initial development version line of `0.1.0`.

- Current development version: `0.1.0`
- Stable milestone: `1.0.0` (reserved for the first stable release)

---

## Current State (as of February 2026)

The Flux compiler frontend is largely complete at the **syntax** level. The lexer tokenizes all specified operators and keywords; the parser produces a full AST for modules, functions, structs, enums, traits, impls, generics, pattern matching, ownership syntax, async/await, annotations, and control flow. A basic semantic resolver performs scope-based name resolution, simple type checking, and match exhaustiveness analysis.

**What works today:** parsing and semantic analysis of `.fl` source files; all 12 example programs pass.

**What does not exist yet:** code generation, IR, runtime, standard library, module/file resolution, borrow checking, advanced generics, testing infrastructure, and all tooling.

---

## Phase 1 — Semantic Analysis Hardening

Complete the resolver so that every language feature described in the spec is fully validated before code generation begins.

### 1.1 Type System

- [x] **Type inference for `let` bindings** — Not supported by design. Flux requires explicit type annotations for all let bindings.
- [x] **Full numeric type hierarchy (complete)** — All widening/narrowing rules between Int8–Int128, UInt8–UInt128, Float32/64/128, and pointer types are enforced. Helpers and exhaustive tests for all widths and promotion rules are implemented and pass in both Debug and Release. Strict policy review for implicit int→float promotion is complete. See `numeric_promotion.cpp` for test coverage.
- [x] **Struct type resolution** — track field types; validate field access (`point.x`) returns the correct type instead of `Unknown`. (Tested in C++: see struct_field_test)
- [x] **Tuple types** — tuple expressions `(a, b)` and tuple destructuring `let (x, y): (T, U) = pair;` are fully supported in the semantic analysis and tested in both Debug and Release. See `tuple_test` for coverage.
- [x] **Array/Slice types** — `Array<T, N>` with compile-time size, `Slice<T>` as a view. Parses `[T; N]` syntax in type annotations.
- [x] **Reference types** — distinguish `&T` (immutable borrow) from `&mut T` (mutable borrow) at the type level. (Complete, tested in reference.cpp)
- [x] **Function types / closures** — infer and check lambda/closure types: `|x: Int32| -> Int32 { x + 1 }`.
- [x] **`Option<T>` and `Result<T, E>`** — built-in generic types with exhaustiveness integration in `match`. (Complete, handles Some/None and Ok/Err)
- [x] **`Never` type propagation** — `panic()` and infinite loops produce `Never`; allow `Never` to coerce to any type. Unreachable code after diverging expressions is detected.
- [x] **Type alias resolution** — fully resolve chains: `type A = B; type B = Int32;` → `A` is `Int32`. Implemented with recursive resolution and cycle detection.

### 1.2 Generics & Trait Bounds

- [x] **Trait bound enforcement** — when a function declares `func foo<T: Display>(x: T)`, verify that the argument type implements `Display`. Supports single and multi-bounds (`T: Display + Clone`). Tested in `trait_bound.cpp`.
- [x] 1.2 Implement **where clause validation** (generic function/struct definitions) like `where T: Ord + Clone`.
- [x] **Monomorphization tracking** — record concrete instantiations of generic types/functions for codegen. Implemented with uniqueness checks and support for nested generics.
- [x] **Associated types** — `trait Iterator { type Item; }` and resolution in impl blocks.
- [x] **Generic method resolution** — resolve calls like `Box<Int32>::new(42)` to the correct monomorphized impl method. (Handled via monomorphization tracking in `CallExpr`)

### 1.3 Traits & Impls

- [x] **Trait conformance checking** — verify every `impl Display for Point` provides all required methods with correct signatures. (Tested in `trait_conformance.cpp`)
- [x] **Default method bodies** — allow traits to have default implementations. (Tested in `trait_default_methods.cpp`)
- [x] **Trait method dispatch** — resolve `x.to_string()` through the trait impl, not just by name. (Complete, supports inherent/trait methods, references, and generics)
- [x] **Orphan rules** — prevent implementing foreign traits on foreign types.
- [x] **Impl method self-type validation** — check that `self` parameter type matches the impl target. (Includes `&self` and `&mut self` matching)

### 1.4 Ownership & Borrowing

- [x] **Move semantics enforcement** — after `let b = move a;`, mark `a` as consumed; error on subsequent use. (Implemented in Phase 1.4)
- [ ] **Borrow checker (basic)** — prevent aliasing mutable borrows; enforce single `&mut` or multiple `&` rule.
- [ ] **Lifetime analysis (simplified)** — ensure references do not outlive their referents within a function scope.
- [ ] **Drop semantics** — insert implicit `drop()` at scope exit; validate manual `drop()` calls.
- [x] **Copy vs Move distinction** — primitive types (Int, Float, Bool, Char) are Copy; structs/enums are Move by default unless annotated. (Implemented in Phase 1.4)

### 1.5 Pattern Matching

- [x] **Tuple destructuring type checking** — validate types in `let (x, y) = expr;`.
- [x] **Struct pattern type checking** — validate `Point { x, y }` pattern field types.
- [x] **Nested pattern exhaustiveness** — check exhaustiveness for `Some(Some(x))` and similar nested patterns.
- [x] **Range pattern validation** — `1..10` patterns: validate bounds are of the same type.
- [x] **Guard type checking** — match guards `if condition` must be `Bool`.
- [x] **Or-patterns** — `Red | Blue => { ... }` combining multiple patterns in one arm.

### 1.6 Control Flow Analysis

- [x] **Definite assignment analysis** — error on reading a variable before it's assigned.
- [x] **Unreachable code detection** — warn (error in Flux) on code after `return`, `break`, `continue`, `panic()`.
- [x] **Loop break-value analysis** — `break value` in `loop` for expression-valued loops.
- [x] **For-loop iterator type propagation** — infer the loop variable type from the iterable.

### 1.7 Error Handling [COMPLETED]

- [x] **`?` operator type checking** — the operand must be `Result<T, E>` or `Option<T>`; the enclosing function must return a compatible error type.
- [x] **Error propagation chain validation** — propagated error types must match or be convertible to the function's return error type.

### 1.8 Visibility Enforcement

- [x] **`pub` / `private` access checking** — private struct fields and functions not accessible outside their module. (Default-private behavior enforced)
- [ ] **Module-level visibility** — enforce visibility across module boundaries once module resolution is implemented.

### 1.9 Concurrency Semantics

- [ ] **`async` function validation** — async functions must return a future-like type.
- [ ] **`await` context checking** — `await` is only valid inside an `async` function.
- [ ] **`spawn` validation** — spawned expression must be `async` or a callable.
- [ ] **`Send`/`Sync` marker traits** — prevent non-thread-safe data from crossing spawn boundaries.

- [ ] Review all of the phase 1 tasks are completed fully or anything missing. Check everything. Build fresh and run tests. Make sure everything is working and completed fully. If anything missing, not done, not implemented, not completed or partially done about pahse 1 tasks, then append as the new tasks to the phase 1 in ROADMAP.md.

---

## Phase 2 — Intermediate Representation (IR)

Design and implement a lowered IR suitable for optimization and code generation.

- [ ] **Define Flux IR** — a typed, SSA-based IR with basic blocks, phi nodes, and explicit control flow.
- [ ] **AST-to-IR lowering** — translate every AST node into IR instructions.
- [ ] **IR validation pass** — verify IR well-formedness (type consistency, dominator tree, etc.).
- [ ] **IR pretty-printer** — human-readable IR dump for debugging (`--emit-ir` flag).
- [ ] **Constant folding** — evaluate compile-time constant expressions.
- [ ] **Dead code elimination** — remove unreachable IR blocks.
- [ ] **Inlining heuristics** — inline small functions, `@inline`-annotated functions.

---

## Phase 3 — Code Generation (LLVM Backend)

Produce native executables via LLVM.

- [ ] **LLVM integration** — link against LLVM C API or LLVM-C++ API; set up JIT or AOT pipeline.
- [ ] **Primitive type mapping** — map Flux `Int32` → LLVM `i32`, `Float64` → LLVM `double`, `Bool` → `i1`, etc.
- [ ] **Function codegen** — emit LLVM IR for function definitions, calls, returns.
- [ ] **Control flow codegen** — `if`/`else`, `while`, `for`, `loop`, `break`, `continue`, `match`.
- [ ] **Struct/enum layout** — compute field offsets, padding, alignment; emit struct types.
- [ ] **Class codegen** — heap allocation, vtable dispatch for classes.
- [ ] **Generics monomorphization** — generate concrete versions of generic functions/types.
- [ ] **Pattern matching compilation** — lower `match` to decision trees / jump tables.
- [ ] **String representation** — define runtime string layout (length-prefixed, UTF-8).
- [ ] **Closure/lambda codegen** — capture environment, emit function pointers.
- [ ] **Error handling codegen** — `Result`/`Option` as tagged unions; `?` operator as conditional branch.
- [ ] **Async/await codegen** — state machine transformation for async functions.
- [ ] **Ownership/drop codegen** — insert destructor calls, move semantics at IR level.
- [ ] **Debug info (DWARF)** — emit source-level debug information for debuggers.
- [ ] **Target support** — x86-64 (primary), AArch64, WebAssembly (stretch).

---

## Phase 4 — Standard Library

Implement the core types and functions referenced in the language spec.

### 4.1 Core Types

- [ ] `Option<T>` — `Some(T)` / `None` with `unwrap()`, `map()`, `and_then()`, `or_else()`, `is_some()`, `is_none()`.
- [ ] `Result<T, E>` — `Ok(T)` / `Err(E)` with `unwrap()`, `map()`, `map_err()`, `and_then()`, `or_else()`.
- [ ] `Box<T>` — heap-allocated smart pointer with `new()`, `Deref`, single-owner semantics.
- [ ] `Rc<T>` — reference-counted pointer with `new()`, `clone()`.
- [ ] `Arc<T>` — atomic reference-counted pointer for concurrency.

### 4.2 Collections

- [ ] `Vec<T>` — dynamic array with `new()`, `push()`, `pop()`, `len()`, `get()`, `iter()`, index access.
- [ ] `Map<K, V>` — hash map with `new()`, `insert()`, `get()`, `remove()`, `contains_key()`, `iter()`.
- [ ] `Set<T>` — hash set with `new()`, `insert()`, `remove()`, `contains()`, `iter()`.
- [ ] `Array<T, N>` — fixed-size array.
- [ ] `Slice<T>` — borrowed view into contiguous memory.
- [ ] `String` — UTF-8 string with `len()`, `chars()`, `contains()`, `split()`, `trim()`, concatenation.

### 4.3 IO Module

- [ ] `io::println(msg: String)` — print line to stdout.
- [ ] `io::print(msg: String)` — print without newline.
- [ ] `io::print_int(n: Int32)` — print integer.
- [ ] `io::readln() -> String` — read line from stdin.
- [ ] `io::eprintln(msg: String)` — print to stderr.
- [ ] File I/O: `io::File::open()`, `read()`, `write()`, `close()`.

### 4.4 Core Traits

- [ ] `Display` — `to_string(self) -> String`.
- [ ] `Debug` — debug representation.
- [ ] `Clone` — `clone(self) -> Self`.
- [ ] `Copy` — marker trait for bitwise-copyable types.
- [ ] `Drop` — destructor: `drop(self) -> Void`.
- [ ] `Iterator` — `next(self) -> Option<Item>` with associated type `Item`.
- [ ] `PartialEq`, `Eq` — equality comparison.
- [ ] `PartialOrd`, `Ord` — ordering comparison.
- [ ] `Hash` — hashing support.
- [ ] `Send`, `Sync` — concurrency marker traits.
- [ ] `Default` — `default() -> Self`.

### 4.5 Built-in Functions

- [ ] `panic(msg: String) -> Never` — abort with message.
- [ ] `assert(cond: Bool) -> Void` — assert condition or panic.
- [ ] `drop(value: T) -> Void` — explicitly destroy a value.
- [ ] `sizeof<T>() -> UIntPtr` — size of type in bytes.
- [ ] `typeof(expr) -> String` — runtime type name (debug).

---

## Phase 5 — Module System & Package Manager

### 5.1 Module Resolution

- [ ] **Multi-file modules** — `import math::trig;` resolves to `math/trig.fl`.
- [ ] **Module tree** — build full module hierarchy from directory structure.
- [ ] **Circular import detection** — error on circular module dependencies.
- [ ] **Re-exports** — `pub use inner::Thing;` to re-export symbols.
- [ ] **Qualified paths** — `math::trig::sin(x)` fully resolved.

### 5.2 Package Manager (`fluxpkg`)

- [ ] **Go style package manager** — design and code a package manager that is similar to Go's package manager for Flux.
- [ ] Use `package-manager.md` as starting point
- [ ] **Manifest file** — `flux.toml` with `[package]`, `[dependencies]`, `[build]` sections.
- [ ] **Dependency resolution** — fetch and resolve transitive dependencies.
- [ ] **Version constraints** — semver ranges (e.g., `^1.2.0`, `~0.3`).
- [ ] **Registry** — central package registry (future).
- [ ] **Build commands** — `flux build`, `flux run`, `flux test`, `flux check`.

---

## Phase 6 — Tooling & Developer Experience

### 6.1 Compiler CLI

- [ ] `flux build <file>` — compile to executable.
- [ ] `flux run <file>` — compile and run.
- [ ] `flux check <file>` — type-check without codegen.
- [ ] `flux fmt <file>` — format source code.
- [ ] `flux test` — discover and run `@test` annotated functions.
- [ ] `--emit-ast` — dump AST.
- [ ] `--emit-ir` — dump IR.
- [ ] `--emit-llvm` — dump LLVM IR.
- [ ] `-O0` / `-O1` / `-O2` / `-O3` — optimization levels.
- [ ] `--target <triple>` — cross-compilation target.

### 6.2 Diagnostics

- [ ] **Source locations on all errors** — accurate line:column from tokens through to semantic errors.
- [ ] **Error recovery** — continue parsing/resolving after the first error to report multiple diagnostics.
- [ ] **Warning levels** — unused variables, unused imports, shadowed names, unreachable code.
- [ ] **Colored terminal output** — ANSI color in error messages with source snippets.
- [ ] **Suggestion/fixit hints** — "did you mean 'x'?" for typos.

### 6.3 Testing Framework

- [ ] **`@test` annotation** — mark functions as unit tests.
- [ ] **Test runner** — `flux test` discovers and runs all `@test` functions.
- [ ] **Assertions** — `assert_eq(a, b)`, `assert_ne(a, b)`, `assert_true(cond)`.
- [ ] **Test output** — pass/fail counts, failure details with source locations.
- [ ] **Automated regression tests** — CI pipeline running all example files.

### 6.4 Documentation Generator

- [ ] **`@doc` annotation** — attach documentation to functions, types, modules.
- [ ] **`flux doc`** — generate HTML/Markdown documentation from source.

### 6.5 IDE Support

- [ ] **Language Server Protocol (LSP)** — go-to-definition, find references, hover info, completions.
- [ ] **VS Code extension** — syntax highlighting, error squiggles, diagnostics integration.
- [ ] **Treesitter grammar** — for editors like Neovim, Helix.

---

## Phase 7 — Runtime & Concurrency

- [ ] **Runtime initialization** — `main()` entry point setup, stack guard, global constructors.
- [ ] **Allocator** — default heap allocator, pluggable allocator trait.
- [ ] **Garbage-free ownership** — deterministic destruction, no GC.
- [ ] **Async runtime** — event loop, task scheduler for `async`/`await`/`spawn`.
- [ ] **Channels** — `Channel<T>` for message-passing between spawned tasks.
- [ ] **Mutex/RwLock** — synchronization primitives.
- [ ] **Thread support** — OS-level threads when `spawn` targets `unsafe` threading.

---

## Phase 8 — Optimization & Performance

- [ ] **Constant propagation** — propagate known values through the IR.
- [ ] **Dead code elimination** — remove unused functions and unreachable blocks.
- [ ] **Inlining** — inline small functions and `@inline`-annotated functions.
- [ ] **Loop optimizations** — loop unrolling, hoisting, strength reduction.
- [ ] **Escape analysis** — stack-allocate `Box<T>` when it doesn't escape.
- [ ] **Devirtualization** — replace trait dispatch with direct calls when the concrete type is known.
- [ ] **Tail call optimization** — convert tail-recursive calls to loops.
- [ ] **Link-time optimization (LTO)** — cross-module inlining and dead code removal.

---

## Phase 9 — Hardening & Stability

- [ ] **Fuzz testing** — fuzz the lexer, parser, and resolver with random/malformed inputs.
- [ ] **Stress tests** — large programs (10k+ lines) to test performance and memory.
- [ ] **Error message audit** — every possible error has a clear, actionable message.
- [ ] **Specification conformance tests** — one test per spec feature.
- [ ] **Memory safety audit** — valgrind/AddressSanitizer on the compiler itself.
- [x] **Cross-platform CI** — Windows, Linux, macOS builds. (Via GitHub Actions)
- [ ] **Semantic versioning** — define stability guarantees for the language and compiler.

---

## Bug Fixes & Known Issues (Current)

| #   | Issue                                                                                        | File(s)                        | Status |
| --- | -------------------------------------------------------------------------------------------- | ------------------------------ | ------ |
| 1   | Field access (`point.x`) returns `Unknown` type — no struct field type lookup in `type_of()` | `resolver.cpp`                 | Open   |
| 2   | Method call resolution (`obj.method()`) returns `Unknown` — no method dispatch               | `resolver.cpp`                 | Open   |
| 3   | Index expression (`arr[i]`) returns `Unknown` — no element type propagation                  | `resolver.cpp`                 | Open   |
| 4   | No type inference — `let x = 42;` requires explicit `: Int32`                                | `resolver.cpp`                 | Open   |
| 5   | `where` clauses are parsed but not enforced                                                  | `resolver.cpp`                 | Open   |
| 6   | Trait conformance is not checked — `impl Display for Point` methods aren't validated         | `resolver.cpp`                 | Open   |
| 7   | `pub`/`private` visibility parsed but not enforced                                           | `resolver.cpp`                 | Open   |
| 8   | `?` error propagation parsed but not semantically checked                                    | `resolver.cpp`                 | Open   |
| 9   | `unsafe` keyword recognized but not semantically modeled                                     | `resolver.cpp`                 | Open   |
| 10  | `await` allowed outside `async` functions                                                    | `resolver.cpp`                 | Open   |
| 11  | No move tracking — using a variable after `move` is not an error                             | `resolver.cpp`                 | Fixed  |
| 12  | Diagnostics lack accurate source locations (many errors report `0:0`)                        | `resolver.cpp`, `diagnostic.h` | Open   |
| 13  | Single-error-abort — resolver throws on first error instead of collecting multiple           | `resolver.cpp`                 | Open   |
| 17  | `parse_type()` does not support `[T; N]` array type syntax in annotations                    | `parser.cpp`                   | Fixed  |
| 14  | Lambda/closure types never checked                                                           | `resolver.cpp`                 | Fixed  |
| 15  | `CompoundAssignStmt` not handled in resolver's `resolve_statement()` (uses `AssignStmt`)     | `resolver.cpp`                 | Open   |
| 16  | Annotations parsed but never processed                                                       | `parser.cpp`, `resolver.cpp`   | Open   |

---

## Technical Details

### Architecture

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│  Source   │───▶│  Lexer   │───▶│  Parser  │───▶│ Resolver │───▶│ Codegen  │
│  (.fl)    │    │ (tokens) │    │  (AST)   │    │(semantic)│    │ (LLVM IR)│
└──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘
                                                                  (missing)
```

### Language & Build

| Item                    | Value                                   |
| ----------------------- | --------------------------------------- |
| Implementation language | C++20                                   |
| Build system            | CMake 3.26+                             |
| Compiler                | MSVC (Windows), GCC/Clang (Linux/macOS) |
| Target backend          | LLVM (planned)                          |
| Source location         | `src/`                                  |
| Examples                | `examples/*.fl`                         |
| Documentation           | `D:\flux\docs\` (19 markdown files)     |

### Source File Inventory

| File                        | Lines | Purpose                                          |
| --------------------------- | ----- | ------------------------------------------------ |
| `src/lexer/token.h`         | ~230  | Token kinds enum, `to_string()`                  |
| `src/lexer/lexer.h`         | ~45   | Lexer class declaration                          |
| `src/lexer/lexer.cpp`       | ~530  | Tokenizer: chars → tokens                        |
| `src/lexer/diagnostic.h`    | ~25   | `DiagnosticError` exception with line/col        |
| `src/ast/ast.h`             | ~430  | All AST node types                               |
| `src/ast/ast_printer.h`     | ~35   | AST printer class declaration                    |
| `src/ast/ast_printer.cpp`   | ~460  | AST debug pretty-printer                         |
| `src/parser/parser.h`       | ~80   | Parser class declaration                         |
| `src/parser/parser.cpp`     | ~1040 | Recursive-descent + Pratt parser                 |
| `src/semantic/type.h`       | ~45   | `TypeKind` enum, `Type` struct                   |
| `src/semantic/symbol.h`     | ~30   | `Symbol` struct (name, kind, type)               |
| `src/semantic/scope.h`      | ~50   | `Scope` class (symbol table + parent chain)      |
| `src/semantic/resolver.h`   | ~70   | Resolver class declaration                       |
| `src/semantic/resolver.cpp` | ~1125 | Semantic analysis: scopes, types, exhaustiveness |
| `src/main.cpp`              | ~45   | CLI entry point                                  |

### Key Data Structures

- **`Scope`** — linked list of hash maps; each scope has a `parent` pointer. `declare()` inserts a `Symbol`; `lookup()` walks the chain.
- **`Symbol`** — `{name, kind, is_mutable, is_const, type, param_types}`. Kinds: `Variable`, `Function`.
- **`Type`** — `{TypeKind, name}`. Kinds: `Int`, `Float`, `Bool`, `String`, `Char`, `Enum`, `Struct`, `Ref`, `Tuple`, `Void`, `Never`, `Unknown`.
- **`enum_variants_`** — `unordered_map<string, vector<string>>`: enum name → variant names. Used for exhaustiveness checking and bare-variant-name resolution.
- **`struct_fields_`** — `unordered_map<string, vector<pair<string,string>>>`: struct name → [(field name, field type)].
- **`type_aliases_`** — `unordered_map<string, string>`: alias name → target type name.

### Parsing Strategy

- **Expressions**: Pratt parsing with precedence climbing. Precedences: Assignment (1) < Or (2) < And (3) < Equality (4) < Comparison (5) < Bitwise (6–8) < Shift (9) < Additive (10) < Multiplicative (11) < Unary (12) < Postfix/Call/Index (13).
- **Statements**: Recursive descent with keyword-driven dispatch.
- **Declarations**: Parsed at module level; functions, structs, enums, traits, impls are forward-declared before body resolution.

### Semantic Analysis Strategy

- **Two-pass module resolution**: first pass declares all types and function signatures (forward visibility); second pass resolves function bodies.
- **Type checking**: structural — `Type{kind, name}` equality. No subtyping, no coercion (except generic param bypass).
- **Match exhaustiveness**: tracks seen variants in `unordered_set<string>`; compares count against enum variant list. Wildcards bypass checking. Bool requires both `true` and `false`.
- **Enum variant resolution**: bare identifier `Red` is checked against all enum variant lists via `is_enum_variant()`; qualified `Color::Red` is resolved by the `ColonColon` BinaryExpr handler in `type_of()`.

---

### Recent Updates

- VS Code tasks and launch configurations have been fully updated.
- All build, test, and run tasks now support both Debug and Release configurations.
- No CMakePresets.json is required; all workflows use explicit --config Debug/Release for MSVC multi-config.
- Output directories for binaries and tests are set to build/Debug and build/Release.
- Syntax highlighting files are synchronized between editors/vscode and docs/.vitepress.

### Developer Workflow (VS Code)

- **Build (Debug):** Use the "Build Flux (Debug)" task to build the project in Debug mode.
- **Build (Release):** Use the "Build Flux (Release)" task for Release mode.
- **Run Tests:** Use the "Run Tests (Debug)" or "Run Tests (Release)" tasks to run all unit tests in the selected configuration.
- **Clean:** Use the "Clean (Debug)" or "Clean (Release)" tasks to remove build artifacts for the selected configuration.
- **Launch/Debug:** Use the launch configurations in .vscode/launch.json to debug the compiler, run examples, or debug tests in either Debug or Release mode.
- **No CMakePresets.json:** All CMake commands are explicit; no preset usage or CMAKE_BUILD_TYPE is required.

### Task Status

- [x] Build/test system fully supports Debug and Release
- [x] All VS Code tasks and launch configs are correct and warning-free
- [x] Syntax highlighting files are synchronized
- [x] Documentation and roadmap updated

---
