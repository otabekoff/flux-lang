```bash
.\cmake-build-debug\flux.exe examples\hello.fl
```


## 1️⃣ Decide the error model (important design choice)

There are 3 common approaches:
- std::cerr + exit(1)
- return std::optional<T>
- throw an exception

We did 1️⃣ Decide the error model (important design choice)
There are 3 common approaches:
- std::cerr + exit(1)
- return std::optional<T>
- throw an exception

## Can later add:
- filename
- source snippet
- caret (^) display


## 7️⃣ Why this scales perfectly

This diagnostic system will later support:
- syntax errors:
  ```bash
  error: expected ';' after expression at 6:14
  ```
- type errors:
    ```bash
    error: cannot assign Int32 to String at 8:9
    ```
- borrow checker:
    ```bash
    error: use of moved value 'x' at 12:5
    ```

Nothing will be rewritten.
You just add more error producers.

----


🔍 Why this dependency is OK (and not a smell)

At this stage:

AST is still syntax-level

Binary operators are still tokens

Semantic meaning comes later

Later (optional refinement), you may replace:

TokenKind op;


with:

enum class BinaryOp { Add, Sub, Mul, Div, Eq, Lt, ... };


But not now. That comes during semantic analysis / lowering.

You did the right thing by not overengineering early.


----

Pay the technical debts.

---

Because:

No module

No function scope

No symbol table

No entry point

Later (much later), you might support:

REPL

expression-only files

test harness mode

But not now — and that’s correct.

---

Get-ChildItem examples\*.fl | ForEach-Object { $result = & .\cmake-build-debug\flux.exe $_.FullName 2>&1 ; $last = $result | Select-Object -Last 1 ; Write-Host "$($_.Name): $last" }