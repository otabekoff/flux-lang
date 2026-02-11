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

---

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

---

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

Get-ChildItem examples\*.fl | ForEach-Object { $result = & .\cmake-build-debug\flux.exe $_.FullName 2>&1 ; $last = $result | Select-Object -Last 1 ; Write-Host "$($\_.Name): $last" }

---

C:\Users\marko\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe -- build D:\project\tutorial\flux\cmake-build-debug -- target flux -j 6

---

Create official installers for win, macos/linux.

---

g++ -std=c++20 -I./src -o build/Debug/tuple_test.exe tests/tuple.cpp


ctest --test-dir build --output-on-failure -C Debug
cmake --build build --config Debug --target reference_test
build/Debug/reference_test.exe
ctest --test-dir build --output-on-failure -C Debug
cmake --build build --target option_result --config Debug
---

PS D:\project\tutorial\flux> git push
bun test v1.2.21 (7c45ed97)
No tests found!

Tests need ".test", "_test_", ".spec" or "_spec_" in the filename (ex: "MyApp.test.ts")

Learn more about bun test: https://bun.com/docs/cli/test
husky - pre-push script failed (code 1)
error: failed to push some refs to 'https://github.com/otabekoff/flux-lang.git'
PS D:\project\tutorial\flux> 

---


Also note you could do this too. Even if it was for .idea, or JetBrains CLion when problems occur.

C:\Users\marko\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe --build D:\project\tutorial\flux\cmake-build-debug --target flux -j 6

---

Setup these for our project:

CMake based, multi-platform projects
 By GitHub Actions

CMake based, multi-platform projects logo
Build and test a CMake based project on multiple platforms.

C

workflow at .github/workflows/cmake-multi-platform.yml.

And the second worflow at .github/workflows/msvc.yml which is:

Microsoft C++ Code Analysis
 By Microsoft

Microsoft C++ Code Analysis logo
Code Analysis with the Microsoft C & C++ Compiler for CMake based projects.

Code scanning


