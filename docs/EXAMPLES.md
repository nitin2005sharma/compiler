# Example Programs

These example files are in `examples/` and load automatically in the local app UI.

## Best examples to run

### `example1.src`

Best for:
- branch edges
- loop edges
- full CFG shape

IR highlights:
- `ifFalse`
- `goto`
- labels such as `L0`, `L1`, `L2`

### `function_calls.src`

Best for:
- function call edges
- `param`, `arg`, and `call` IR instructions
- annotated AST function-signature links

IR highlights:
- `left = arg 0`
- `param value`
- `t1 = call add, 2`

### `nested_control.src`

Best for:
- nested `if` and `while`
- branch + loop flow in one function
- block structure in AST and CFG

IR highlights:
- relational comparisons
- nested labels
- loop-back `goto`

### `scope_demo.src`

Best for:
- nested scopes
- declaration/use links in annotated AST
- same variable name in different blocks

What to inspect:
- annotated AST scope depths
- dashed `uses` and `writes` links

### `countdown.src`

Best for:
- function calls plus loops
- compact CFG with both call flow and loop flow

IR highlights:
- `arg`
- `param`
- `call`
- `ifFalse`

### `multi_function_paths.src`

Best for:
- several functions
- return paths inside helper functions
- inter-function call edges in CFG

IR highlights:
- multiple function labels
- branch labels inside helper functions
- chained calls from `main`

## How to run in the app

1. Start the server:

```powershell
node .\ui\server.mjs
```

2. Open:

```text
http://127.0.0.1:4318
```

Opening `ui/public/index.html` directly, or serving the UI from a different static server, will not populate the sample dropdown because the list comes from `GET /api/samples` in `ui/server.mjs`.

3. Pick any example from the sample dropdown.

4. Click `Run Compiler`.

5. Inspect:
- `Phase Trace`
- `IR`
- `AST Graph`
- `Annotated AST Graph`
- `CFG Graph`

## How to run from the terminal

Example:

```powershell
.\build-trace-ui\Debug\compiler.exe --trace .\examples\nested_control.src nested_control_out
```

That creates:
- `nested_control_out_ast.dot`
- `nested_control_out_annotated_ast.dot`
- `nested_control_out_ir.txt`
- `nested_control_out_cfg.dot`
- `nested_control_out_summary.txt`
