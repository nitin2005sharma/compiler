# Compiler Phases Guide

## Overview

This project is an educational compiler, but its pipeline follows the same big ideas used in real compilers:

1. Read source code
2. Break it into tokens
3. Parse tokens into an AST
4. Run semantic analysis on the AST
5. Generate graph outputs from the AST
6. Generate intermediate representation (IR)
7. Build a control-flow graph (CFG)
8. Write a final summary of what each phase created

The compiler can also print a step-by-step trace:

```powershell
.\build-trace-ui\Debug\compiler.exe --trace .\examples\function_calls.src out
```

That command creates:

- `out_ast.dot`
- `out_ast.png`
- `out_annotated_ast.dot`
- `out_annotated_ast.png`
- `out_ir.txt`
- `out_cfg.dot`
- `out_cfg.png`
- `out_summary.txt`

## Phase Order

| Phase | Input | Main Output | Why It Matters |
|------|------|------|------|
| `DRIVER` | CLI arguments, source file path | orchestration, output files | connects all phases together |
| `LEXER` | raw source text | token stream | turns characters into meaningful units |
| `PARSER` | token stream | AST in memory | gives the program structure |
| `SEMANTIC` | AST | symbol/function info, semantic annotations | checks if the program makes sense |
| `AST` | validated AST | plain AST graph | shows program structure visually |
| `ANNOTATED AST` | semantically annotated AST | semantic AST graph | shows resolved meaning visually |
| `IR` | AST | three-address code | easier form for later compiler work |
| `CFG` | IR | control-flow graph | shows execution paths between blocks |
| `SUMMARY` | results from all phases | end-of-run explanation | helps understand what was created |

## 1. Driver Phase

### What it does

The driver is the top-level controller in `main.cpp`.

It:
- reads command-line flags like `--trace` and `--no-png`
- opens the source file
- runs each compiler phase in order
- writes output files
- prints trace information

### What it creates

The driver itself does not create language structures like tokens or AST nodes.
It creates the final output files by calling the other phases and saving their results.

### Why it is important

Without the driver, the phases would exist but would not be connected into a usable compiler.

### In a real compiler

Real compilers also have a front-end entry point that handles:
- command-line options
- input files
- diagnostics
- output mode selection
- pipeline control

## 2. Lexer Phase

### Input

Raw source code as text.

### What it does

The lexer scans characters and groups them into tokens such as:
- keywords like `int`, `return`, `while`
- identifiers like `main`, `result`
- literals like `4`
- symbols like `(`, `)`, `{`, `}`, `;`, `+`

It also skips:
- whitespace
- `//` comments
- `/* ... */` comments

### What it creates

It creates a token stream in memory.

With `--trace`, it also prints each token, for example:

```text
[TRACE][LEXER] IDENT("add") @ line 1
[TRACE][LEXER] INT_LIT("4")=4 @ line 11
```

### Why it is important

Parsing raw characters directly is hard and error-prone.
The lexer gives the parser clean building blocks.

### In a real compiler

Real lexers do the same basic job.
Some compilers also keep:
- source columns
- trivia like comments
- macro-expansion info
- token categories for later error recovery

## 3. Parser Phase

### Input

The token stream from the lexer.

### What it does

The parser checks whether the token sequence follows the language grammar.
If it does, it builds an AST.

The AST is the first real tree representation of the program.

### What it creates

It creates the AST in memory.

Nodes include:
- `ASTProgram`
- `FuncDecl`
- `ParamDecl`
- `VarDecl`
- `Block`
- `IfStmt`
- `WhileStmt`
- `ReturnStmt`
- `AssignStmt`
- `BinaryExpr`
- `IntLiteral`
- `Ident`
- `CallExpr`

With `--trace`, the parser also prints step-by-step construction messages, for example:

```text
[TRACE][PARSER] created VarDecl for result
[TRACE][PARSER] created call expr add with 2 args
```

### Why it is important

The AST gives structure to the program.
Later phases no longer deal with flat tokens; they deal with meaningful program constructs.

### In a real compiler

Real compilers often build:
- a parse tree first
- then a simplified AST

Some compilers also build multiple tree forms later, such as:
- typed AST
- high-level IR
- mid-level IR
- low-level IR

## 4. Semantic Phase

### Input

The AST from the parser.

### What it does

Semantic analysis checks whether the program is meaningful, not just syntactically valid.

In this project it:
- creates scope stacks
- creates variable symbol information
- creates function information
- checks duplicate declarations
- checks missing `main`
- resolves identifiers to declarations
- validates assignment targets
- validates function existence
- validates function arity
- records semantic metadata on AST nodes

### What it creates

It creates semantic information such as:
- symbol tables
- function table
- scope depth on nodes
- type names on nodes
- function signatures on function and call nodes
- declaration/use links

This means the AST becomes a semantically annotated AST after this phase.

### Why it is important

A program can be syntactically correct but still wrong.

Examples:
- using an undeclared variable
- calling a missing function
- passing the wrong number of arguments

Semantic analysis is the phase that catches those mistakes.

### In a real compiler

This phase is usually much larger.
Real compilers often do:
- name resolution
- type inference
- overload resolution
- generic instantiation
- borrow/lifetime checks
- access control checks
- constant evaluation

## 5. Plain AST Output Phase

### Input

The AST.

### What it does

It converts the AST into Graphviz DOT format and optionally renders a PNG.

### What it creates

- `out_ast.dot`
- `out_ast.png`

This graph shows the structural program tree only.

### Why it is important

It is the cleanest way to inspect what the parser understood.
It helps debug grammar and tree construction.

### In a real compiler

Production compilers do not always emit AST graphs, but compiler developers often use internal dumps and graph tools for debugging.

## 6. Annotated AST Output Phase

### Input

The AST after semantic analysis has added meaning to nodes.

### What it does

It creates a richer graph than the plain AST.

It includes semantic details such as:
- symbol kind
- node type
- function signature
- scope depth
- dashed declaration/use edges

### What it creates

- `out_annotated_ast.dot`
- `out_annotated_ast.png`

### Why it is important

This is one of the most educational outputs in the project.
It does not only show the shape of the program; it shows what the program means after semantic analysis.

For example:
- an identifier node can point to the parameter or variable it resolves to
- a call node can point to the function declaration it calls
- an assignment node can point to the variable declaration it writes

### In a real compiler

Real compilers usually do not literally call this an "annotated AST graph," but they often maintain internal typed trees or resolved trees that carry very similar information.

## 7. IR Phase

### Input

The validated AST.

### What it does

It lowers the AST into a simpler intermediate representation made of quads.

This project's IR includes operations such as:
- `label`
- `goto`
- `ifFalse`
- `ret`
- `mov`
- arithmetic and relational operations
- `arg`
- `param`
- `call`

### What it creates

- `out_ir.txt`
- plain IR lines on stdout

### Why it is important

The AST is good for understanding structure.
IR is better for control flow, optimization, and later code generation.

### In a real compiler

Most real compilers use one or more IR forms because IR is much easier to optimize and translate into machine code than a source-level AST.

## 8. CFG Phase

### Input

The IR.

### What it does

It groups IR instructions into basic blocks and connects them with control-flow edges.

Each basic block is a straight-line sequence of instructions with no jump in the middle.

### What it creates

- `out_cfg.dot`
- `out_cfg.png`

The CFG graph in this project shows:
- block contents
- quad numbers
- block-to-block edges
- labels like `true`, `false`, `goto`, and `next`

### Why it is important

CFGs are central to many real compiler tasks:
- optimization
- liveness analysis
- dead code elimination
- loop analysis
- data-flow analysis

### In a real compiler

The CFG is one of the most important internal structures in production compilers.
Many advanced optimizations are built on top of it.

## 9. Summary Phase

### Input

Counts and artifacts produced by earlier phases.

### What it does

It explains what each phase created at the end of the run.

### What it creates

- `out_summary.txt`

It also prints summary lines in trace mode:

```text
[TRACE][SUMMARY] PARSER: Created an AST with ...
[TRACE][SUMMARY] IR: Created 19 IR quads ...
```

### Why it is important

This phase turns the compiler run into a readable explanation, which is especially useful for learning and debugging.

### In a real compiler

Production compilers do not always emit a human summary file, but build systems, compiler debug flags, and internal developer tooling often provide very similar reporting.

## What Is Created in Memory vs On Disk

### Created in memory

- token stream
- AST
- semantic annotations
- IR program
- CFG structure

### Written to disk

- plain AST graph
- annotated AST graph
- IR text
- CFG graph
- summary file

## Important Clarification About AST vs Annotated AST

The AST itself is created during parsing.

The annotated AST graph file is created after semantic analysis because it depends on semantic information such as:
- resolved declarations
- scope depth
- function signature
- node type information

So:

- `PARSER` creates the AST structure
- `SEMANTIC` adds meaning to that structure
- `ANNOTATED AST` output writes the semantically enriched graph

## How This Matches a Real Compiler

This project is smaller than a production compiler, but the flow is real:

1. source text
2. tokens
3. syntax tree
4. semantic resolution
5. intermediate representation
6. control-flow graph
7. later compiler work such as optimization or code generation

That is why these phases are important even in a learning project: they are the same core ideas used in full compilers.
