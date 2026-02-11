# Tagging system info

# 1) Create tag locally

Lightweight tag:

```bash
git tag v0.1.0
```

Annotated tag (recommended for releases):

```bash
git tag -a v0.1.0 -m "Release v0.1.0"
```

---

# 2) Push tag to remote

Push only this tag:

```bash
git push origin v0.1.0
```

Push all tags:

```bash
git push origin --tags
```

---

# 3) Check tags

List local tags:

```bash
git tag
```

Show tag details:

```bash
git show v0.1.0
```

---

# 4) Delete tag (local)

```bash
git tag -d v0.1.0
```

---

# 5) Delete tag from remote

```bash
git push origin --delete v0.1.0
```

---

# 6) Retag and push again

Full safe workflow:

```bash
# delete old tag locally
git tag -d v0.1.0

# delete old tag on remote
git push origin --delete v0.1.0

# create new tag
git tag -a v0.1.0 -m "Release v0.1.0"

# push new tag
git push origin v0.1.0
```

---

# 7) Tag a specific commit

```bash
git tag -a v0.1.0 <commit-hash> -m "Release v0.1.0"
```

Example:

```bash
git tag -a v0.1.0 3f2a1bc -m "Release v0.1.0"
```




-------


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

cmake --build build --target flux -j 6

`cmake --build D:\project\tutorial\flux\cmake-build-debug --target flux -j 6`

`C:\Users\marko\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe --build D:\project\tutorial\flux\cmake-build-debug --target flux -j 6`
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


[main] Configuring project: flux 
Cannot configure: No configure preset is active for this CMake project
Unable to get the location of clang-format executable - no active workspace selected


---

fat and thin arrows -> and => should be fixed.
