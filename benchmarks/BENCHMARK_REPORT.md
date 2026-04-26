# Optimization Benchmark Report

Total suite size: 16 programs

## Core Reduction Suite

Programs counted in the aggregate reduction metrics: 11

### Aggregate Results

- IR instructions: 169 -> 130 (23.08% reduction)
- CFG basic blocks: 48 -> 45 (6.25% reduction)
- CFG control-flow edges: 43 -> 38 (11.63% reduction)
- Constant folds: 16
- Constant propagations: 38
- Common-subexpression eliminations: 1
- Dead-code removals: 20
- Dead-store removals: 17
- Unreachable-code removals: 12
- Loop simplifications: 2
- Loop-invariant hoists: 1
- Loop preheaders: 1
- Loop peels: 0
- Loop unrolls: 3
- Loop unswitches: 0
- Strength reductions: 0
- Induction variables optimized: 0

### Per-Program Results

| Program | Pre-Opt IR | Opt IR | Pre Blocks | Opt Blocks | Pre Edges | Opt Edges |
|---|---:|---:|---:|---:|---:|---:|
| benchmarks\cse_block.src | 8 | 2 | 1 | 1 | 0 | 0 |
| benchmarks\dead_store.src | 5 | 2 | 1 | 1 | 0 | 0 |
| benchmarks\loop_invariant.src | 17 | 16 | 4 | 5 | 4 | 5 |
| examples\countdown.src | 14 | 18 | 5 | 6 | 4 | 6 |
| examples\example1.src | 21 | 16 | 7 | 6 | 8 | 7 |
| examples\function_calls.src | 19 | 19 | 3 | 3 | 0 | 0 |
| examples\multi_function_paths.src | 29 | 29 | 9 | 9 | 8 | 8 |
| examples\nested_control.src | 18 | 17 | 6 | 6 | 7 | 7 |
| examples\optimizations.src | 13 | 4 | 4 | 3 | 4 | 2 |
| examples\scope_demo.src | 12 | 3 | 4 | 2 | 4 | 1 |
| tests\optimizations.src | 13 | 4 | 4 | 3 | 4 | 2 |

## Loop Transform Showcase

These programs are designed to exercise structural loop transforms such as preheaders, peeling, unrolling, and unswitching. They are reported separately because these passes can increase IR size while still improving loop structure.

| Program | Pre-Opt IR | Opt IR | Loop Preheaders | Loop Peels | Loop Unrolls | Loop Unswitches | Strength Reductions | Induction Vars |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| benchmarks\loop_peel.src | 20 | 15 | 0 | 5 | 0 | 0 | 0 | 0 |
| benchmarks\loop_unroll.src | 10 | 14 | 0 | 0 | 1 | 0 | 0 | 0 |
| benchmarks\loop_unswitch.src | 24 | 29 | 0 | 0 | 0 | 1 | 0 | 0 |
| benchmarks\nested_loops.src | 23 | 150 | 2 | 8 | 0 | 0 | 0 | 0 |
| benchmarks\strength_reduction.src | 15 | 19 | 1 | 0 | 0 | 0 | 1 | 1 |

