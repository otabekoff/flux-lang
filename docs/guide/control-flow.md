# Control Flow

## If / Else

```flux
func checkAge(age: Int32) -> String {
    if age >= 18 {
        return "Adult";
    } else if age >= 13 {
        return "Teenager";
    } else {
        return "Child";
    }
}
```

## Match

Exhaustive pattern matching over values:

```flux
func describe(x: Int32) -> String {
    match x {
        0 => { return "zero"; }
        1 => { return "one"; }
        _ => { return "other"; }
    }
}
```

### Enum Matching

```flux
enum Color {
    Red,
    Green,
    Blue,
    Custom(Int32, Int32, Int32),
}

func toHex(c: Color) -> String {
    match c {
        Color::Red => { return "#FF0000"; }
        Color::Green => { return "#00FF00"; }
        Color::Blue => { return "#0000FF"; }
        Color::Custom(r, g, b) => { return formatRGB(r, g, b); }
    }
}

### Match Guards

Pattern matching can include boolean guards:

```flux
let number: Int32 = 15;
match number {
    n if n < 0 => std::io::println("Negative"),
    n if n == 0 => std::io::println("Zero"),
    n if n > 0 and n < 10 => std::io::println("Small positive"),
    _ => std::io::println("Large positive")
}
```
```

## For Loop

Iterates over ranges or collections:

```flux
// Range loop (inclusive start, exclusive end)
for i: Int32 in range(0, 10) {
    std::io::println(i.to_string());
}
```

Collection iteration:

```flux
func sum(numbers: &Vec<Int32>) -> Int32 {
    let mut total: Int32 = 0;
    for n in numbers {
        total = total + n;
    }
    return total;
}
```

## While Loop

```flux
func countdown(start: Int32) -> Void {
    let mut n: Int32 = start;
    while n > 0 {
        io::print_int(n);
        n = n - 1;
    }
}
```

## Loop (Infinite)

```flux
func waitForInput() -> String {
    loop {
        let input: String = io::read_line();
        if input != "" {
            return input;
        }
    }
}
```

## Break and Continue

```flux
func findFirst(items: &Vec<Int32>, target: Int32) -> Int32 {
    let mut index: Int32 = 0;
    for item in items {
        if item == target {
            break;
        }
        index = index + 1;
    }
    return index;
}
```
