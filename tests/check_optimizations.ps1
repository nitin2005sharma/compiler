param(
    [string]$CompilerPath,
    [string]$SourcePath,
    [string]$BuildDir
)

$ErrorActionPreference = "Stop"

$prefix = Join-Path $BuildDir "compiler_optimizations"
$preIrFile = "${prefix}_pre_opt_ir.txt"
$irFile = "${prefix}_ir.txt"
$preOptCfgFile = "${prefix}_pre_opt_cfg.dot"

& $CompilerPath $SourcePath $prefix | Out-Null

if (!(Test-Path $preIrFile)) {
    throw "Pre-optimization IR file was not created: $preIrFile"
}

if (!(Test-Path $irFile)) {
    throw "Optimized IR file was not created: $irFile"
}

if (!(Test-Path $preOptCfgFile)) {
    throw "Pre-optimization CFG file was not created: $preOptCfgFile"
}

$ir = Get-Content -Raw -Path $irFile
$preOptCfg = Get-Content -Raw -Path $preOptCfgFile

if ($ir -match "2 \+ 3") {
    throw "Expected constant folding to remove '2 + 3' from optimized IR."
}

if ($ir -match "y \+ 99") {
    throw "Expected constant-false loop body to be removed from optimized IR."
}

if ($ir -match "ifFalse 0 goto") {
    throw "Expected constant branch simplification to remove 'ifFalse 0 goto'."
}

if ($ir -notmatch "ret 5") {
    throw "Expected optimized IR to fold the program down to a constant return."
}

if ($preOptCfg -notmatch "ifFalse t2 goto L1") {
    throw "Expected pre-optimization CFG to preserve the original loop branch before optimization."
}
