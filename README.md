# Flux Programming Language

![Build Status](https://github.com/otabekoff/flux-lang/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)
![Versioning](https://github.com/otabekoff/flux-lang/actions/workflows/version.yml/badge.svg)
![MSVC Code Analysis](https://github.com/otabekoff/flux-lang/actions/workflows/msvc.yml/badge.svg)

Flux is a systems programming language focused on explicitness, safety, and predictable performance. Flux is intentionally strict: every type must be declared, allocations are explicit, and there are no hidden conversions or implicit copies.

## Philosophy

- **Always explicit** — All types and ownership semantics are stated in source.
- **Ownership & borrowing** — Memory safety without a garbage collector via explicit ownership, moves, and borrows.
- **Traits over inheritance** — Reuse and abstraction through traits and composition.
- **Pattern matching** — Exhaustive, ergonomic pattern matching for control flow.
- **Native performance** — Designed to map cleanly to LLVM and produce efficient native code.

## Key Features

- **Static Typing** — Every variable and parameter has an explicit type.
- **Ownership & Borrowing** — Move semantics with `move`, `&` borrow, and `&mut` mutable borrow.
- **Traits & Generics** — Interface-based polymorphism with monomorphization and **Associated Types**.
- **Where Clauses** — Expressive generic constraints (e.g., `where T: Display + Clone`).
- **Pattern Matching** — Exhaustive `match` with destructuring and guards.
- **Async/Await** — Built-in concurrency support (frontend parsed and resolved).
- **Monomorphization** — Automatic generation of concrete type/function instantiations.

## Repository Status

This repository contains the Flux compiler frontend and semantic analysis engine.

- [x] **Lexer/Parser** — Full syntax support for all features.
- [x] **Semantic Analysis** — Scope resolution, type checking, trait bounds, and exhaustiveness.
- [x] **Tooling** — VS Code extension for syntax highlighting and snippets.
- [ ] **Code Generation** — LLVM-based backend (In Progress).

## Getting Started

### Prerequisites

- **C++20 toolchain** (MSVC 2022, GCC 13+, or Clang 17+)
- **CMake 3.26+**

### Quick Build

```powershell
# Configure and Build (Debug)
cmake -B build
cmake --build build --config Debug

# Run Tests
ctest --test-dir build --output-on-failure -C Debug
```

For detailed instructions, see [docs/guide/installation.md](docs/guide/installation.md).

## Contributing

We use [conventional commits](https://www.conventionalcommits.org/). Please see `CONTRIBUTING.md` for our full contribution guidelines.

## Files of Interest

- `src/` — Compiler implementation.
- `examples/` — Sample Flux programs.
- `docs/` — Language specification and user guides.
- `ROADMAP.md` — Detailed project status and future plans.

## License

This project is released under the MIT License. See `LICENSE`.

## Security

See `SECURITY.md` for vulnerability disclosure instructions.

---

If you'd like help with a specific roadmap item, open an issue or assign the task and I can implement it next.
