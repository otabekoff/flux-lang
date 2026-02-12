# Installation

## Prerequisites

- **CMake** ≥ 3.26
- **C++20 compiler** (MSVC 2022, GCC 13+, or Clang 17+)
- **Ninja** (optional but recommended for faster builds)
- **Bun** (optional; required only for VS Code extension development)

## Building from Source

### 1. Clone the repository

```bash
git clone https://github.com/flux-lang/flux.git
cd flux
```

### 2. Configure and build

#### Using VS Code Tasks (Recommended)

- Open the project in VS Code.
- Use the built-in tasks (`Ctrl+Shift+B` or **Terminal > Run Task**) to build, test, and clean in **Debug** or **Release** mode.
- All tasks and launch configurations are available for both configurations.
- No `CMakePresets.json` is required; all workflows use explicit `--config Debug/Release` for MSVC multi-config.

#### Manual Build (Command Line)

We recommend using the provided helper command to configure the build:

```powershell
# Configure
cmake -B build

# Build the compiler
cmake --build build --config Debug

# Build and run all tests
ctest --test-dir build --output-on-failure -C Debug
```

To build for **Release**, simply swap `Debug` for `Release`.

### 3. Verify installation

After building, you can verify the compiler is working:

```powershell
# Windows
./build/Debug/flux.exe examples/hello.fl --dump-tokens

# Linux/macOS
./build/Debug/flux examples/hello.fl --dump-tokens
```

## Editor Support

### VS Code (Recommended)

Install the Flux VS Code extension from `editors/vscode/` for syntax highlighting, snippets, and bracket matching.

1. Navigate to the extension folder: `cd editors/vscode`
2. Install dependencies: `bun install`
3. Package and install:

```bash
npx @vscode/vsce package
code --install-extension flux-lang-0.1.0.vsix
```

## Troubleshooting

- **Missing C++20 support**: Ensure your compiler is up to date (MSVC 2022 v17.x, GCC 13+, or Clang 17+).
- **CMake version**: We require CMake 3.26+ for modern module and dependency handling.
