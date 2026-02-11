# Built-in Types

## Primitive Types

### Integers

| Type      | Size    | Signed | Range             |
| --------- | ------- | ------ | ----------------- |
| `Int8`    | 8-bit   | Yes    | -128 to 127       |
| `Int16`   | 16-bit  | Yes    | -32,768 to 32,767 |
| `Int32`   | 32-bit  | Yes    | -2³¹ to 2³¹-1     |
| `Int64`   | 64-bit  | Yes    | -2⁶³ to 2⁶³-1     |
| `Int128`  | 128-bit | Yes    | -2¹²⁷ to 2¹²⁷-1   |
| `UInt8`   | 8-bit   | No     | 0 to 255          |
| `UInt16`  | 16-bit  | No     | 0 to 65,535       |
| `UInt32`  | 32-bit  | No     | 0 to 2³²-1        |
| `UInt64`  | 64-bit  | No     | 0 to 2⁶⁴-1        |
| `UInt128` | 128-bit | No     | 0 to 2¹²⁸-1       |

### Floating Point

| Type      | Size   | Precision  |
| --------- | ------ | ---------- |
| `Float32` | 32-bit | ~7 digits  |
| `Float64` | 64-bit | ~15 digits |

### Other Primitives

| Type     | Description                 |
| -------- | --------------------------- |
| `Bool`   | Boolean (`true` / `false`)  |
| `Char`   | Unicode scalar value        |
| `String` | UTF-8 encoded string        |
| `Void`   | Unit type (no value)        |
| `Never`  | Bottom type (never returns) |

## Standard Library Types

### Option

```flux
enum Option<T> {
    Some(T),
    None,
}
```

### Result

```flux
enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

### Collections

| Type          | Description               |
| ------------- | ------------------------- |
| `Vec<T>`      | Growable array            |
| `Map<K, V>`   | Hash map                  |
| `Set<T>`      | Hash set                  |
| `Array<T, N>` | Fixed-size array          |
| `Slice<T>`    | View into contiguous data |

### Smart Pointers

| Type     | Description                      |
| -------- | -------------------------------- |
| `Box<T>` | Heap-allocated value             |
| `Rc<T>`  | Reference-counted pointer        |
| `Arc<T>` | Atomic reference-counted pointer |

## Reference Types

Flux supports reference types for both immutable and mutable borrows, similar to Rust. The type system and resolver now distinguish between `&T` (immutable reference) and `&mut T` (mutable reference) at the type level.

### Syntax

- Immutable reference: `&T`
- Mutable reference: `&mut T`

### Type System

- The resolver and type system propagate mutability for references.
- `Type` struct includes an `is_mut_ref` flag to distinguish `&T` from `&mut T`.

### Example

```
let x: Int32 = 42;
let y: &Int32 = &x;        // Immutable reference
let z: &mut Int32 = &mut x; // Mutable reference
```
