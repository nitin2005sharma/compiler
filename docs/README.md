# Documentation Index

This folder contains the main reference material for the compiler and UI.

## Recommended Reading Order

1. [../README.md](../README.md)
2. [USAGE.md](USAGE.md)
3. [UI.md](UI.md)
4. [EXAMPLES.md](EXAMPLES.md)
5. [COMPILER_PHASES.md](COMPILER_PHASES.md)
6. [FORMAT.md](FORMAT.md)
7. [../benchmarks/BENCHMARK_REPORT.md](../benchmarks/BENCHMARK_REPORT.md)

## Document Guide

### [USAGE.md](USAGE.md)

Command-line syntax, output files, and common error categories.

### [UI.md](UI.md)

How to start the local browser workbench, how the sample loader works, and how to troubleshoot the UI.

### [EXAMPLES.md](EXAMPLES.md)

Best sample programs to run and what to inspect in AST, annotated AST, IR, and CFG output.

### [COMPILER_PHASES.md](COMPILER_PHASES.md)

Phase-by-phase explanation of lexer, parser, semantic analysis, IR generation, CFG construction, and summary output.

### [FORMAT.md](FORMAT.md)

Reference for the three-address IR and control-flow graph structure.

### [../benchmarks/BENCHMARK_REPORT.md](../benchmarks/BENCHMARK_REPORT.md)

Measured optimization results across the benchmark suite, including IR reduction, CFG reduction, and per-program comparisons.

## Quick Start

Build the compiler:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run one sample from the CLI:

```powershell
.\build\Debug\compiler.exe --trace .\examples\function_calls.src out
```

Start the browser UI:

```powershell
node .\ui\server.mjs
```

Then open `http://127.0.0.1:4318`.
