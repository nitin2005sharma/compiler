# UI Guide

## Overview

The browser workbench lets you:

- load sample programs from `examples/`
- edit source code in the browser
- run the compiler without leaving the page
- inspect trace output, source-aware diagnostics, optimization explanations, IR diffs, SSA-style IR, symbol tables, liveness, AST, annotated AST, and CFG results

The UI is served by `ui/server.mjs`.

## Prerequisites

Before using the UI, make sure:

- the compiler has been built with CMake
- Node.js is installed
- Graphviz is installed if you want rendered graph images

Graphviz is optional. Without it, the UI can still show text outputs and DOT files.

## Start The UI

```powershell
node .\ui\server.mjs
```

Open:

```text
http://127.0.0.1:4318
```

## How Sample Loading Works

The sample dropdown is not hardcoded into the page.

When the UI starts, `ui/public/app.js` calls:

```text
GET /api/samples
```

The server reads every `.src` file from `examples/` and returns:

- sample names
- sample source text
- detected compiler path
- detected Graphviz path

Because of that, the UI must be opened through `node .\ui\server.mjs`.

Opening `ui/public/index.html` directly from disk, or serving `ui/public/` from a different static host, will not populate the sample list unless that host also provides the API routes.

## Compile From The UI

When you click `Run Compiler`, the page sends:

```text
POST /api/compile
```

with:

- `source`: the current editor contents
- `trace`: whether trace mode is enabled

The server then:

1. creates a temporary source file
2. runs the compiler executable
3. reads generated IR and DOT outputs
4. optionally converts DOT graphs to SVG with Graphviz
5. returns everything to the browser

## UI Output Areas

### Source Program

The editor where you can load a sample or type your own program.

### Phase Trace

Displays grouped trace messages like `LEXER`, `PARSER`, `SEMANTIC`, `IR`, and `CFG` when trace mode is enabled.

### Tabs

The output panel includes tabs for:

- diagnostics
- optimization dashboard
- summary.txt
- optimization explanations
- final optimized code
- IR diff
- AST graph
- annotated AST graph
- IR before optimization
- CFG before optimization
- optimized IR
- optimized CFG
- CFG/dominator view
- CFG metadata
- SSA IR
- symbol table
- liveness
- AST DOT
- annotated AST DOT
- CFG DOT before optimization
- optimized CFG DOT
- CFG analysis DOT
- plain stdout that is not trace output

## Troubleshooting

### Sample dropdown is empty or says samples unavailable

Check:

- you started the UI with `node .\ui\server.mjs`
- the `examples/` folder exists and contains `.src` files
- the browser can reach `http://127.0.0.1:4318/api/samples`

### Compiler not found

The UI server searches for the newest executable under folders such as:

- `build/`
- `build-check/`
- other directories whose names start with `build`

Build the project first:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

You can also set an explicit compiler path with:

```powershell
$env:COMPILER_PATH = "C:\path\to\compiler.exe"
```

### Graph tabs show a missing Graphviz message

Install Graphviz, or set:

```powershell
$env:DOT_PATH = "C:\Program Files\Graphviz\bin\dot.exe"
```

The UI can still show DOT text even if Graphviz is unavailable.

## API Reference

### `GET /api/samples`

Returns available sample programs plus environment detection info.

### `POST /api/compile`

Accepts JSON:

```json
{
  "source": "int main() { return 0; }",
  "trace": true
}
```

Returns compile status, trace output, text files, and rendered graph SVG when available.
