# Compiler Pipeline Walkthrough With Optimizations

## Goal

This guide explains the compiler step by step from source code up to CFG creation.

For every phase, it shows:
- input
- what the phase does
- output
- why the phase matters

It also includes real examples of:
- constant folding
- constant propagation
- dead code elimination
- basic loop optimization

## Main Example Used In This Guide

File:
- `examples/optimizations.src`

Source:

```c
int main() {
    int x;
    int y;

    x = 2 + 3;
    y = x + 0;

    while (1 < 0) {
        y = y + 99;
    }

    return y;
}
```

Why this example is useful:
- `2 + 3` demonstrates constant folding
- `y = x + 0` demonstrates constant propagation
- the loop body becomes unreachable after the condition is simplified
- dead temporary values are removed after propagation

Run it with:

```powershell
.\build-trace-ui\Debug\compiler.exe --trace .\examples\optimizations.src optimizations_doc
```

Generated files:
- `optimizations_doc_ast.dot`
- `optimizations_doc_ast.png`
- `optimizations_doc_annotated_ast.dot`
- `optimizations_doc_annotated_ast.png`
- `optimizations_doc_pre_opt_cfg.dot`
- `optimizations_doc_pre_opt_cfg.png`
- `optimizations_doc_ir.txt`
- `optimizations_doc_cfg.dot`
- `optimizations_doc_cfg.png`
- `optimizations_doc_summary.txt`

## Phase 1: Driver

### Input

- command-line flags
- source file path
- output prefix

### What happens

The driver:
- opens the source file
- runs each phase in order
- writes files
- prints trace output

### Output

- orchestrated compiler run
- all generated output files

### Why it matters

Without the driver, the compiler phases are not connected into a usable tool.

## Phase 2: Lexer

### Input

Raw source text.

### What happens

The lexer converts characters into tokens.

Example tokens from `optimizations.src`:

```text
INT_KW("int")
IDENT("main")
LPAREN("(")
RPAREN(")")
LBRACE("{")
IDENT("x")
ASSIGN("=")
INT_LIT("2")=2
PLUS("+")
INT_LIT("3")=3
```

### Output

A token stream.

### Why it matters

The parser should not work directly with characters.
Tokens are the clean input for syntax analysis.

## Phase 3: Parser

### Input

The token stream from the lexer.

### What happens

The parser recognizes declarations, statements, and expressions and builds an AST.

For the optimization example, it builds nodes for:
- function `main`
- variable declarations `x` and `y`
- assignment `x = 2 + 3`
- assignment `y = x + 0`
- `while (1 < 0) { ... }`
- `return y`

### Output

An AST in memory.

### Why it matters

The AST gives the program structure.
Later phases use this structure instead of raw tokens.

## Phase 4: Semantic Analysis

### Input

The AST.

### What happens

Semantic analysis:
- creates scope information
- creates symbol information for variables
- creates function information
- resolves identifier uses to declarations
- records type and scope metadata on AST nodes

For the example:
- `x` is recorded as a local variable
- `y` is recorded as a local variable
- `return y` is checked and linked to the `y` declaration

### Output

A semantically annotated AST.

### Why it matters

Syntax alone is not enough.
A program can be syntactically correct and still be wrong.
Semantic analysis makes sure the program is meaningful.

## Phase 5: Plain AST Output

### Input

The AST.

### What happens

The compiler writes a plain structural AST graph.

### Output

- `optimizations_doc_ast.dot`
- `optimizations_doc_ast.png`

### Why it matters

This shows what the parser understood structurally.

## Phase 6: Annotated AST Output

### Input

The semantically annotated AST.

### What happens

The compiler writes a richer AST graph with:
- node kind
- type
- scope depth
- function signatures
- declaration/use links

### Output

- `optimizations_doc_annotated_ast.dot`
- `optimizations_doc_annotated_ast.png`

### Why it matters

This graph shows not only structure but meaning.

## Phase 7: IR Generation

### Input

The validated AST.

### What happens

The compiler lowers the AST into three-address style IR.

### Unoptimized IR for the example

This is what the compiler creates before optimization:

```text
main:
t0 = 2 + 3
x = t0
t1 = x + 0
y = t1
L0:
t2 = 1 < 0
ifFalse t2 goto L1
t3 = y + 99
y = t3
goto L0
L1:
ret y
```

### Output

IR in memory first, then optimized IR in:
- `optimizations_doc_ir.txt`

### Why it matters

IR is much easier to optimize than an AST.

## Phase 8: Optimization Phase

### Input

The unoptimized IR.

### What happens

This compiler now runs an `OPT` phase after IR generation.

Current supported optimizations:
- constant folding
- constant propagation
- dead temporary elimination
- unreachable-code removal
- basic loop simplification when the loop condition becomes constant

## Optimization 1: Constant Folding

### Input IR

```text
t0 = 2 + 3
```

### What happens

Both operands are constants, so the expression can be evaluated at compile time.

### Output IR

```text
t0 = 5
```

### Why it matters

The program does less work at runtime.

## Optimization 2: Constant Propagation

### Input IR

```text
t0 = 5
x = t0
t1 = x + 0
```

### What happens

The compiler tracks that:
- `t0` is constant `5`
- then `x` also becomes constant `5`

So `x` can be replaced with `5`.

### Output IR

```text
x = 5
t1 = 5 + 0
```

Then the new expression can be folded again:

```text
t1 = 5
```

### Why it matters

Once constants are propagated, more optimization opportunities appear.

## Optimization 3: Dead Code Elimination

### Input IR

After propagation and folding:

```text
t0 = 5
x = 5
t1 = 5
y = 5
```

### What happens

The temporaries `t0` and `t1` are no longer needed because their values were already copied into real variables.

So these definitions are dead and can be removed.

### Output IR

```text
x = 5
y = 5
```

### Why it matters

It removes instructions that no longer affect the program result.

## Optimization 4: Basic Loop Optimization

### Input IR

```text
L0:
t2 = 1 < 0
ifFalse t2 goto L1
t3 = y + 99
y = t3
goto L0
L1:
ret y
```

### What happens

First, the comparison is folded:

```text
t2 = 0
```

Then the branch becomes:

```text
ifFalse 0 goto L1
```

That means the condition is always false, so the loop body can never run.
The compiler simplifies this to:

```text
goto L1
```

Now these instructions become unreachable:

```text
t3 = y + 99
y = t3
goto L0
```

They are removed.

### Output IR

```text
L0:
goto L1
L1:
ret y
```

### Why it matters

This is a simple form of loop optimization.
The compiler proves the loop body cannot execute and removes it.

Important note:
This is not full advanced loop optimization like loop invariant code motion.
It is a conservative, simple optimization based on a constant condition.

## Final Optimized IR

The final IR written by the compiler for the example is:

```text
0: main:
1: x = 5
2: y = 5
3: L0:
4: goto L1
5: L1:
6: ret y
```

## Phase 9: CFG Construction Before And After Optimization

### Input

- the unoptimized IR
- the optimized IR

### What happens

The compiler now builds two CFG views:

- a pre-optimization CFG from the raw IR
- an optimized CFG after constant folding, propagation, dead-code elimination, and loop simplification

That makes it easier to show why the optimization phase matters instead of only showing the final reduced graph.

### Pre-optimization CFG for the example

Key instructions still present before optimization:

```text
main:
t0 = 2 + 3
x = t0
t1 = x + 0
y = t1
L0:
t2 = 1 < 0
ifFalse t2 goto L1
t3 = y + 99
y = t3
goto L0
L1:
ret y
```

Outputs:

- `optimizations_doc_pre_opt_cfg.dot`
- `optimizations_doc_pre_opt_cfg.png`

### CFG for the optimized example

Basic blocks:

```text
B0
0: main:
1: x = 5
2: y = 5

B1
3: L0:
4: goto L1

B2
5: L1:
6: ret y
```

Edges:

```text
Entry_main -> B0
B0 -> B1
B1 -> B2
B2 -> Exit_main
```

### Output

- `optimizations_doc_pre_opt_cfg.dot`
- `optimizations_doc_pre_opt_cfg.png`
- `optimizations_doc_cfg.dot`
- `optimizations_doc_cfg.png`

### Why it matters

The CFG is the right structure for path-sensitive optimization.
Even though some optimizations in this compiler already work without CFG-based data flow, stronger optimization usually depends on CFG reasoning.

## Second Example: Rich Control Flow Before Optimization

For a fuller CFG with branches and loops, use:
- `examples/example1.src`

Why this example is useful:
- `if/else` creates branch edges
- `while` creates a loop back edge
- the CFG contains multiple blocks and paths

Run:

```powershell
.\build-trace-ui\Debug\compiler.exe --trace .\examples\example1.src example1_walk
```

This is the best example for understanding:
- `true` edges
- `false` edges
- `goto` edges
- loop back edges
- entry and exit nodes

## Input and Output Summary By Phase

| Phase | Input | Output |
|------|------|------|
| Lexer | source code text | token stream |
| Parser | token stream | AST |
| Semantic | AST | semantically annotated AST |
| AST output | AST | plain AST graph |
| Annotated AST output | annotated AST | semantic AST graph |
| IR generation | AST | unoptimized IR |
| Optimization | unoptimized IR | optimized IR |
| CFG | unoptimized IR and optimized IR | before/after control-flow graphs |
| Summary | phase results | human-readable explanation file |

## What This Means In A Real Compiler

The exact implementation details vary, but the flow is real:

1. source code
2. tokens
3. syntax tree
4. semantic resolution
5. IR generation
6. optimization
7. CFG-based reasoning
8. later code generation

That is why these topics matter:
- constant folding removes compile-time-computable work
- constant propagation spreads known values forward
- dead code elimination removes useless instructions
- loop optimization reduces unnecessary loop work
- CFG construction makes path-sensitive optimization possible

## Current Limits Of This Project

The optimizer is still intentionally simple.

It currently does:
- local constant folding
- local constant propagation
- dead temporary elimination
- unreachable-code cleanup
- constant-condition loop simplification

It does not yet do:
- full global data-flow constant propagation
- SSA-based optimization
- loop invariant code motion
- strength reduction
- induction variable analysis
- global liveness-based DCE

Those would be strong next upgrades.
