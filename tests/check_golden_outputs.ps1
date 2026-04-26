param(
    [string]$CompilerPath,
    [string]$RepoRoot,
    [string]$BuildDir
)

$ErrorActionPreference = "Stop"

function Normalize-Text([string]$text) {
    return ($text -replace "`r`n", "`n").Trim()
}

$cases = @(
    @{
        Source = Join-Path $RepoRoot "tests\optimizations.src"
        Prefix = Join-Path $BuildDir "golden_optimizations"
        ExpectedIR = Join-Path $RepoRoot "tests\golden\optimizations_ir.txt"
        ExpectedCfg = Join-Path $RepoRoot "tests\golden\optimizations_cfg.dot"
    },
    @{
        Source = Join-Path $RepoRoot "benchmarks\loop_invariant.src"
        Prefix = Join-Path $BuildDir "golden_loop_invariant"
        ExpectedIR = Join-Path $RepoRoot "tests\golden\loop_invariant_ir.txt"
        ExpectedCfg = Join-Path $RepoRoot "tests\golden\loop_invariant_cfg.dot"
    }
)

foreach ($case in $cases) {
    & $CompilerPath --no-png $case.Source $case.Prefix | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Golden output compile failed for $($case.Source)"
    }

    $actualIr = Normalize-Text (Get-Content -Raw -Path ($case.Prefix + "_ir.txt"))
    $expectedIr = Normalize-Text (Get-Content -Raw -Path $case.ExpectedIR)
    if ($actualIr -ne $expectedIr) {
        throw "IR golden mismatch for $($case.Source)"
    }

    $actualCfg = Normalize-Text (Get-Content -Raw -Path ($case.Prefix + "_cfg.dot"))
    $expectedCfg = Normalize-Text (Get-Content -Raw -Path $case.ExpectedCfg)
    if ($actualCfg -ne $expectedCfg) {
        throw "CFG golden mismatch for $($case.Source)"
    }
}
