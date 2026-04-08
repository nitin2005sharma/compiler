# Intermediate Representation Format

## Three-Address Code (Quadruples)

Each quad has the form: `op arg1 arg2 result`

### Quad Types

#### Label
```
label: result
```
Marks a target for jumps. Result field contains the label name.

#### Goto (Unconditional Jump)
```
goto label
```
Jumps to the given label.

#### IfFalse (Conditional Jump)
```
ifFalse condition goto label
```
Jumps if condition evaluates to false (0).

#### Return
```
ret value
```
Returns the value from the current function.

#### Assignment (Mov)
```
result = arg1
```
Assigns arg1 to result.

#### Binary Operation
```
result = arg1 op arg2
```
Computes binary operation. Operators:
- Arithmetic: `+`, `-`, `*`, `/`
- Relational: `<`, `<=`, `>`, `>=`, `==`, `!=`

## Control Flow Graph (CFG)

### Basic Blocks

Nodes in CFG are basic blocks labeled B0, B1, B2, etc.
Each block contains a sequence of consecutive quads with no jumps except the last.

### Edges

Edges represent possible control flow:
- Fallthrough: Next sequential block
- Conditional: From ifFalse to target label block and fallthrough block
- Unconditional: From goto to target label block only
- Return: Terminates the block (no outgoing edge)

## Example

Source:
```c
int main() {
    int x;
    x = 5;
    if (x > 0)
        x = x - 1;
    return x;
}
```

IR Quads:
```
0: main:
1: t0 = 5
2: x = t0
3: t1 = x > 0
4: ifFalse t1 goto L0
5: t2 = x - 1
6: x = t2
7: goto L1
8: L0:
9: L1:
10: ret x
```

CFG Blocks:
- B0: quads 0-4
- B1: quads 5-7
- B2: quads 8-10

Edges:
- B0 -> B1 (ifFalse condition true)
- B0 -> B2 (ifFalse condition false)
- B1 -> B2 (goto)
- B2 -> (exit)
