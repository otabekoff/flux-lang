# Reference Types in Flux

Flux supports reference types for both immutable and mutable borrows, similar to Rust. The type system and resolver now distinguish between `&T` (immutable reference) and `&mut T` (mutable reference) at the type level.

## Syntax

- Immutable reference: `&T`
- Mutable reference: `&mut T`

## Type System

- The resolver and type system propagate mutability for references.
- `Type` struct includes an `is_mut_ref` flag to distinguish `&T` from `&mut T`.

## Example

```
let x: Int32 = 42;
let y: &Int32 = &x;        // Immutable reference
let z: &mut Int32 = &mut x; // Mutable reference
```

## Tests

See `tests/reference.cpp` for coverage of both immutable and mutable reference type inference.

## Status

- Fully implemented and tested as of February 2026.
- See ROADMAP.md for progress tracking.
