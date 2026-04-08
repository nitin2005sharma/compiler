# Educational Compiler Workbench

This project is a small educational compiler for a C-like language subset, plus a browser UI for exploring each compiler phase.

It can:

- tokenize source code
- parse an AST
- run semantic analysis with scope and declaration tracking
- generate three-address IR
- build a control-flow graph
- export AST and CFG diagrams with Graphviz
- serve a local UI with built-in sample programs

## Language Features

The current language subset supports:

- `int` variable declarations
- `int` functions with parameters
- `if` / `else`
- `while`
- `return`
- assignments
- arithmetic operators: `+`, `-`, `*`, `/`
- relational operators: `<`, `<=`, `>`, `>=`, `==`, `!=`
- line comments with `//`
- block comments with `/* ... */`

## Repository Layout

```text
.
|-- examples/     Sample `.src` programs used by the UI and CLI
|-- include/      Header files for the compiler pipeline
|-- src/          Compiler implementation
|-- tests/        Positive and negative compiler test inputs
|-- ui/           Local browser workbench and API server
|-- docs/         Usage, examples, phase guide, IR format, and UI guide
`-- CMakeLists.txt
```

## Requirements

- CMake 3.10 or newer
- A C++17 compiler
- Node.js to run the local UI server
- Graphviz optional, for PNG or SVG graph rendering

## Build

### Windows PowerShell

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### Notes

- The CLI executable is typically created at `.\build\Debug\compiler.exe` on Visual Studio generators.
- On single-config generators it may be created as `.\build\compiler`.
- The UI server automatically searches `build*` directories for the newest compiler executable.

## Command-Line Usage

```powershell
.\build\Debug\compiler.exe [--trace] [--no-png] <source_file> [output_prefix]
```

### Flags

- `--trace`: print detailed phase-by-phase trace output
- `--no-png`: skip Graphviz PNG generation and write DOT files only

### Example

```powershell
.\build\Debug\compiler.exe --trace .\examples\function_calls.src out
```

This generates files such as:

- `out_ast.dot`
- `out_ast.png`
- `out_annotated_ast.dot`
- `out_annotated_ast.png`
- `out_ir.txt`
- `out_cfg.dot`
- `out_cfg.png`
- `out_summary.txt`

## Run The Browser UI

Start the local server:

```powershell
node .\ui\server.mjs
```

Then open:

```text
http://127.0.0.1:4318
```

The sample dropdown is populated by the server from `examples/`, so open the UI through `node .\ui\server.mjs` rather than opening `ui/public/index.html` directly.

## Sample Programs

The `examples/` folder includes programs for:

- loops and branching
- nested control flow
- function calls
- multiple functions
- scope behavior

See [docs/EXAMPLES.md](docs/EXAMPLES.md) for a guide to what each sample is useful for.

## Documentation

Start here:

- [docs/README.md](docs/README.md) for the documentation index
- [docs/USAGE.md](docs/USAGE.md) for CLI arguments and outputs
- [docs/UI.md](docs/UI.md) for the browser workbench
- [docs/COMPILER_PHASES.md](docs/COMPILER_PHASES.md) for the full pipeline walkthrough
- [docs/FORMAT.md](docs/FORMAT.md) for IR and CFG format details
- [docs/EXAMPLES.md](docs/EXAMPLES.md) for example programs

