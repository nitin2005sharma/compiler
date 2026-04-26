param(
    [string]$CompilerPath,
    [string]$RepoRoot,
    [string]$BuildDir
)

$ErrorActionPreference = "Stop"

function Get-PositiveCount([string]$text, [string]$pattern, [string]$label) {
    $match = [regex]::Match($text, $pattern)
    if (-not $match.Success) {
        throw "Could not find $label in summary.`n$text"
    }

    $count = [int]$match.Groups[1].Value
    if ($count -le 0) {
        throw "Expected $label to be greater than 0. Got $count.`n$text"
    }

    return $count
}

$cases = @(
    @{
        Name = "loop_invariant"
        Source = Join-Path $RepoRoot "benchmarks\loop_invariant.src"
        Prefix = Join-Path $BuildDir "transform_loop_invariant"
        SummaryChecks = @(
            @{ Pattern = '(\d+) loop-invariant hoists'; Label = "loop-invariant hoists" },
            @{ Pattern = '(\d+) loop preheaders'; Label = "loop preheaders" }
        )
        Validate = {
            param([string]$irText)
            if ($irText -notmatch 't1 = 10') {
                throw "Expected LICM benchmark IR to contain the hoisted invariant computation.`n$irText"
            }
            if ($irText -notmatch 'L\d+:\s*\r?\n\d+: t1 = 10') {
                throw "Expected LICM benchmark IR to place the hoisted computation in a preheader label block.`n$irText"
            }
        }
    },
    @{
        Name = "loop_unroll"
        Source = Join-Path $RepoRoot "benchmarks\loop_unroll.src"
        Prefix = Join-Path $BuildDir "transform_loop_unroll"
        SummaryChecks = @(
            @{ Pattern = '(\d+) loop unrolls'; Label = "loop unrolls" }
        )
        Validate = {
            param([string]$irText)
            $ifFalseCount = [regex]::Matches($irText, 'ifFalse ').Count
            if ($ifFalseCount -lt 2) {
                throw "Expected unrolled loop IR to contain two guarded condition checks per iteration group.`n$irText"
            }
        }
    },
    @{
        Name = "loop_peel"
        Source = Join-Path $RepoRoot "benchmarks\loop_peel.src"
        Prefix = Join-Path $BuildDir "transform_loop_peel"
        SummaryChecks = @(
            @{ Pattern = '(\d+) loop peels'; Label = "loop peels" }
        )
        Validate = {
            param([string]$irText)
            if ($irText -notmatch 'ret 13') {
                throw "Expected peeled loop benchmark to collapse to the known constant result after optimization.`n$irText"
            }
        }
    },
    @{
        Name = "loop_unswitch"
        Source = Join-Path $RepoRoot "benchmarks\loop_unswitch.src"
        Prefix = Join-Path $BuildDir "transform_loop_unswitch"
        SummaryChecks = @(
            @{ Pattern = '(\d+) loop unswitches'; Label = "loop unswitches" }
        )
        Validate = {
            param([string]$irText)
            if ($irText -notmatch 'ifFalse flag goto') {
                throw "Expected unswitched IR to contain the outer invariant branch dispatch.`n$irText"
            }
            $loopHeaderCount = [regex]::Matches($irText, 'L\d+:').Count
            if ($loopHeaderCount -lt 4) {
                throw "Expected unswitched IR to duplicate the loop structure into separate variants.`n$irText"
            }
        }
    },
    @{
        Name = "strength_reduction"
        Source = Join-Path $RepoRoot "benchmarks\strength_reduction.src"
        Prefix = Join-Path $BuildDir "transform_strength_reduction"
        SummaryChecks = @(
            @{ Pattern = '(\d+) strength reductions'; Label = "strength reductions" },
            @{ Pattern = '(\d+) induction variables optimized'; Label = "induction variables optimized" }
        )
        Validate = {
            param([string]$irText)
            if ($irText -match 'i \* 4') {
                throw "Expected strength reduction to remove the loop-body multiply by i.`n$irText"
            }
            if ($irText -notmatch 't\d+ = t\d+ \+ 4') {
                throw "Expected strength reduction to maintain the scaled induction value with addition.`n$irText"
            }
        }
    }
)

foreach ($case in $cases) {
    & $CompilerPath --no-png $case.Source $case.Prefix | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Transform check compile failed for $($case.Source)"
    }

    $summaryText = Get-Content -Raw -Path ($case.Prefix + "_summary.txt")
    foreach ($check in $case.SummaryChecks) {
        [void](Get-PositiveCount -text $summaryText -pattern $check.Pattern -label $check.Label)
    }

    $irText = Get-Content -Raw -Path ($case.Prefix + "_ir.txt")
    & $case.Validate $irText

    foreach ($suffix in @("_opt_report.txt", "_ssa_ir.txt", "_symbols.txt", "_liveness.txt", "_cfg_analysis.txt", "_cfg_analysis.dot")) {
        $artifact = $case.Prefix + $suffix
        if (-not (Test-Path $artifact)) {
            throw "Expected analysis artifact to exist: $artifact"
        }
    }
}
