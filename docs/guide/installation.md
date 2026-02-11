# Installation

## Prerequisites

- **CMake** ≥ 3.20
- **Ninja** build system
- **LLVM 21** development libraries
- **C++20 compiler** (MSVC 2022, GCC 13+, or Clang 17+)

## Building from Source

### 1. Clone the repository

```bash
git clone https://github.com/flux-lang/flux.git
cd flux
```

### 2. Set up LLVM

Download the LLVM 21 prebuilt development package and extract it to `llvm-dev/` in the project root:

```bash
# The CMake build expects LLVM at ./llvm-dev/
# LLVMConfig.cmake should be at ./llvm-dev/lib/cmake/llvm/
```

### 3. Configure and build

#### Using VS Code Tasks (Recommended)

- Open the project in VS Code.
- Use the built-in tasks (Ctrl+Shift+B or Run Task) to build, test, and clean in Debug or Release mode.
- All tasks and launch configurations are available for both Debug and Release.
- No CMakePresets.json is required; all workflows use explicit --config Debug/Release for MSVC multi-config.

#### Manual Build

```powershell
cmake -B build -G Ninja
cmake --build build --config Debug   # or --config Release
ctest --test-dir build --output-on-failure -C Debug   # or -C Release
```

### 4. Run tests

```powershell
ctest --test-dir build --output-on-failure -C Debug   # or -C Release
```

### 5. Verify installation

```powershell
./build/Debug/flux.exe --version
./build/Debug/flux.exe examples/hello.fl --dump-tokens
# or use the Release path for Release builds
```

## Editor Support

Install the Flux VS Code extension from `editors/vscode/` for syntax highlighting, snippets, and bracket matching.

```bash
cd editors/vscode
npx @vscode/vsce package
code --install-extension flux-lang-0.1.0.vsix
```
