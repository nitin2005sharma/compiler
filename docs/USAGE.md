# Compiler Usage Guide

## Command Line

```
./compiler [--trace] [--no-png] <source_file> [output_prefix]
```

**Arguments:**
- `--trace`: Print step-by-step phase output
- `--no-png`: Skip Graphviz PNG rendering and only write DOT files
- `source_file`: Path to .src program
- `output_prefix`: Prefix for output files (default: "out")

## Output Files

| File | Description |
|------|-------------|
| `{prefix}_ast.dot` | AST in Graphviz DOT format |
| `{prefix}_ast.png` | AST visualization (PNG) |
| `{prefix}_annotated_ast.dot` | Semantic annotated AST in Graphviz DOT format |
| `{prefix}_annotated_ast.png` | Semantic annotated AST visualization (PNG) |
| `{prefix}_ir.txt` | Three-address code (text) |
| `{prefix}_cfg.dot` | CFG in Graphviz DOT format |
| `{prefix}_cfg.png` | CFG visualization (PNG) |
| `{prefix}_summary.txt` | End-of-run explanation of what each phase created |

## Example

```bash
./compiler --trace test.src results
# Generates: results_ast.dot, results_ast.png, results_annotated_ast.dot,
# results_annotated_ast.png, results_ir.txt, results_cfg.dot, results_cfg.png,
# results_summary.txt
```

## Viewing Outputs

### DOT files
Convert to PNG:
```bash
dot -Tpng results_ast.dot -o results_ast.png
dot -Tpng results_annotated_ast.dot -o results_annotated_ast.png
dot -Tpng results_cfg.dot -o results_cfg.png
```

Or use any Graphviz viewer.

### IR Text
Open `results_ir.txt` in any text editor.

## Error Messages

- **Lex error**: Invalid character or token at line N
- **Parse error**: Syntax error (missing operator, etc.)
- **Semantic error**: Undeclared variable, duplicate declaration, missing main, scope issues

## Learn the Full Pipeline

See `docs/COMPILER_PHASES.md` for a phase-by-phase explanation of:
- what each phase creates
- why each phase is important
- how the outputs relate to a real compiler

See `docs/EXAMPLES.md` for ready-to-run sample programs and what to inspect in their IR and CFG output.
