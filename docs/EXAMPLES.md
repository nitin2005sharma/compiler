# Example Programs

These example files are in `examples/` and load automatically in the local app UI.

## Best examples to run

### `demo_resume_showcase.src`

Best for:
- a single resume/demo program that shows many passes together
- side-by-side IR diff
- optimization explanations
- loop analysis in the CFG/Dominator View

What to inspect:
- `Optimization Dashboard`
- `Optimization Explanations`
- `IR Diff`
- `CFG/Dominator View`

Expected optimizer highlights:
- constant folding and propagation
- dead-code and dead-store cleanup
- loop-invariant hoisting
- loop preheader creation
- loop unswitching
- strength reduction
- induction-variable optimization

### `demo_constant_cleanup.src`

Best for:
- constant folding
- constant propagation
- algebraic simplification
- dead store elimination
- unreachable loop removal

Expected optimizer highlights:
- `2 + 3` becomes `5`
- `x + 0` and `x * 1` simplify away
- dead stores to unused variables disappear
- the always-false loop body is removed

### `demo_loop_strength.src`

Best for:
- strength reduction
- induction-variable optimization
- seeing why loop optimizations matter

Expected optimizer highlights:
- `i * 8` is replaced by a maintained temporary
- the maintained value updates by `+ 8` each iteration
- the optimized CFG marks loop structure more clearly

### `demo_loop_invariant.src`

Best for:
- loop-invariant code motion
- loop preheader creation
- explaining "move repeated work before the loop"

Expected optimizer highlights:
- `base + step` is computed once before the loop
- the loop body reuses the hoisted temporary

### `demo_loop_unswitch.src`

Best for:
- loop unswitching
- comparing CFG before and after optimization
- explaining invariant branches inside loops

Expected optimizer highlights:
- `if (flag)` moves outside the loop
- the optimizer creates separate loop bodies for each branch

### `demo_symbols_liveness.src`

Best for:
- symbol table output
- function parameters and calls
- liveness analysis
- SSA-style versioned IR

What to inspect:
- `Symbol Table`
- `Liveness`
- `SSA IR`
- `Annotated AST Graph`

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

### `optimizations.src`

Best for:
- constant folding
- constant propagation
- dead code elimination
- constant-false loop simplification

IR highlights:
- `2 + 3` folds to `5`
- propagated constants reduce temporary work
- dead temporary instructions disappear
- the `while (1 < 0)` body is removed from optimized IR

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
