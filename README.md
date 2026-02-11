# Flux Programming Language

Flux is a systems programming language focused on explicitness, safety, and predictable performance. Flux is intentionally strict: every type must be declared, allocations are explicit, and there are no hidden conversions or implicit copies.

## Philosophy

- **Always explicit** — All types and ownership semantics are stated in source.
- **Ownership & borrowing** — Memory safety without a garbage collector via explicit ownership, moves, and borrows.
- **Traits over inheritance** — Reuse and abstraction through traits and composition.
- **Pattern matching** — Exhaustive, ergonomic pattern matching for control flow.
- **Native performance** — Designed to map cleanly to LLVM and produce efficient native code.

## Design Note (explicitness)

Flux intentionally forbids implicit type inference for `let` bindings and hidden conversions to preserve clarity and predictability. Code must state types explicitly, which helps auditing, tooling, and reasoning about ownership.

## Current Status

This repository contains the Flux compiler frontend (lexer, parser, AST, and semantic resolver). The project is under active development — see `ROADMAP.md` for planned features, priorities, and known issues.

## Getting Started

Prerequisites:

- C++20 toolchain (MSVC, GCC, or Clang)
- CMake 3.26+
- Bun (optional; for local JS tooling)

### Build and Run (Debug/Release)

- Use the VS Code tasks for building, testing, and running in both Debug and Release modes.
- All build/test/run tasks are available in the VS Code task runner and launch menu.
- No CMakePresets.json is required; all workflows use explicit --config Debug/Release for MSVC multi-config.
- See `ROADMAP.md` for the up-to-date workflow and task details.

Example (manual build):

```powershell
cmake -B build -G Ninja
cmake --build build --config Debug   # or --config Release
ctest --test-dir build --output-on-failure -C Debug   # or -C Release
```

## Contributing

See `CONTRIBUTING.md` for contribution guidelines. Use conventional commits. Husky + commitlint + lint-staged are configured to keep history clean and enforce quality checks.

## Files of Interest

- `src/` — compiler implementation (lexer / parser / resolver)
- `examples/` — sample `.fl` programs
- `docs/` — design notes and language specification drafts
- `ROADMAP.md` — planned features and release plan

## License

This project is released under the MIT License. See `LICENSE`.

## Security

See `SECURITY.md` for vulnerability disclosure instructions.

---

If you'd like help with a specific roadmap item, open an issue or assign the task and I can implement it next.
