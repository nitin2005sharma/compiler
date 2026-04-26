# comipler optimization visuliser

This project is an educational compiler for a C-like language subset, plus a browser UI for exploring the full pipeline before and after optimization.

It can:

- tokenize source code
- parse an AST
- run semantic analysis with scope and declaration tracking
- generate three-address IR
- build control-flow graphs before and after optimization
- run optimization passes including constant folding, constant propagation, common subexpression elimination, dead-code elimination, dead-store elimination, liveness analysis, strength reduction, induction-variable optimization, and structural loop optimization
- report measurable optimization results across a benchmark suite
- export AST and CFG diagrams with Graphviz
- serve a local UI with built-in sample programs

Helpful guides:

- `docs/COMPILER_PHASES.md`
- `docs/PIPELINE_WITH_OPTIMIZATIONS.md`
- `docs/EXAMPLES.md`

## Resume Highlights

This repo is strongest on a resume when you emphasize that it is not just a parser demo. It includes:

- a complete compiler pipeline: lexer, recursive-descent parser, semantic analysis, IR generation, CFG construction, optimization, and visualization
- optimizer passes with measured results across a reduction suite plus dedicated loop-transform benchmarks
- source-aware diagnostics with line and column reporting, highlighted spans, and expected-token parser messages
- a local browser workbench that shows pre/post optimization IR, side-by-side IR diffs, optimization explanations, symbol tables, liveness, SSA-style IR, and CFG/dominator metadata
- automated validation with CTest, golden-file regression tests, randomized parser smoke tests, and GitHub Actions CI

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
|-- benchmarks/   Benchmark programs and generated benchmark report
|-- include/      Header files for the compiler pipeline
|-- src/          Compiler implementation
|-- tests/        Positive and negative compiler test inputs
|-- ui/           Local browser workbench and API server
|-- .github/      GitHub Actions CI workflow
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
- `out_pre_opt_ir.txt`
- `out_ir.txt`
- `out_pre_opt_cfg.dot`
- `out_pre_opt_cfg.png`
- `out_cfg.dot`
- `out_cfg.png`
- `out_summary.txt`

The summary file includes optimizer counters for constant folding, constant propagation, common subexpression elimination, dead-code removal, dead-store removal, unreachable-code cleanup, loop simplification, loop-invariant hoisting, preheader creation, loop peeling, loop unrolling, loop unswitching, strength reduction, and induction-variable optimization.

## Benchmarking

Run the benchmark suite with:

```powershell
.\benchmarks\run_benchmarks.ps1 -CompilerPath .\build\Debug\compiler.exe
```

This regenerates `benchmarks/BENCHMARK_REPORT.md`.

Current measured results for the core reduction suite across 11 programs:

- IR instructions: `169 -> 130` for a `23.08%` reduction
- CFG basic blocks: `48 -> 45` for a `6.25%` reduction
- CFG control-flow edges: `43 -> 38` for a `11.63%` reduction
- optimization work recorded: `16` constant folds, `38` constant propagations, `20` dead-code removals, `17` dead-store removals, `12` unreachable-code removals, `1` loop-invariant hoist, and `1` loop preheader

The full benchmark report covers 16 programs. It also includes a separate loop-transform showcase for preheaders, loop peeling, loop unrolling, loop unswitching, strength reduction, and induction-variable optimization. Those structural passes can increase IR size while still improving loop organization, so they are reported separately from the reduction-focused aggregate.

These metrics make the project easier to describe in resume bullets and interviews because they show impact, not just feature count.

## Diagnostics

Compiler errors now include:

- stage-specific context such as `Parse error` or `Semantic error`
- exact line and column positions
- source-line excerpts with caret highlighting
- parser messages that call out the expected token when possible

That makes the compiler feel much more polished during demos and code reviews.

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

- a full resume/demo showcase with optimization explanations, IR diff, CFG loop metadata, strength reduction, and induction-variable optimization
- focused constant cleanup, loop-invariant motion, loop unswitching, strength reduction, symbol table, liveness, and SSA-style demos
- loops and branching
- nested control flow
- function calls
- multiple functions
- scope behavior

The `benchmarks/` folder includes focused optimization workloads for:

- common subexpression elimination
- dead-store elimination
- loop-invariant code motion
- loop peeling
- loop unrolling
- loop unswitching
- strength reduction
- induction-variable optimization
- nested-loop behavior

See [docs/EXAMPLES.md](docs/EXAMPLES.md) for a guide to what each sample is useful for.

## Testing And CI

Local validation:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

The test suite covers:

- sample program compilation
- semantic failure cases
- optimization regression checks
- loop-transform regression checks
- diagnostic formatting checks
- golden-file IR and CFG comparisons
- randomized parser smoke testing

CI is configured in `.github/workflows/ci.yml` to build on Windows, run the CTest suite, execute the benchmark script, and publish the benchmark report as an artifact.

## Resume Bullet Ideas

You can describe this project with bullets like:

- Built a C++ educational compiler for a C-like language with lexical analysis, recursive-descent parsing, semantic analysis, three-address IR generation, CFG construction, and a browser UI for visualizing compiler phases.
- Implemented optimization passes including constant folding, constant propagation, common subexpression elimination, dead-code elimination, dead-store elimination, liveness analysis, loop-invariant code motion, loop peeling, loop unrolling, loop unswitching, strength reduction, and induction-variable optimization, reducing IR instruction count by `23.08%` across an 11-program reduction benchmark suite.
- Added source-aware compiler diagnostics, golden-file regression tests, randomized parser fuzz-style smoke tests, and GitHub Actions CI to make the project easier to demo, validate, and maintain.

## Documentation

Start here:

- [docs/README.md](docs/README.md) for the documentation index
- [docs/USAGE.md](docs/USAGE.md) for CLI arguments and outputs
- [docs/UI.md](docs/UI.md) for the browser workbench
- [docs/COMPILER_PHASES.md](docs/COMPILER_PHASES.md) for the full pipeline walkthrough
- [docs/FORMAT.md](docs/FORMAT.md) for IR and CFG format details
- [docs/EXAMPLES.md](docs/EXAMPLES.md) for example programs
- [benchmarks/BENCHMARK_REPORT.md](benchmarks/BENCHMARK_REPORT.md) for measured optimization results
https://drive.google.com/drive/folders/1s8yia6vO4VZ1V6AchudOligXCLUMGVBn?usp=drive_link
