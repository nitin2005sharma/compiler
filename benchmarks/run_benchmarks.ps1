param(
    [string]$CompilerPath,
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputReport = (Join-Path $PSScriptRoot "BENCHMARK_REPORT.md")
)

$ErrorActionPreference = "Stop"

if (-not $CompilerPath) {
    throw "CompilerPath is required."
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseFullPath = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $baseFullPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $baseFullPath += [System.IO.Path]::DirectorySeparatorChar
    }

    $baseUri = [System.Uri]$baseFullPath
    $targetUri = [System.Uri]([System.IO.Path]::GetFullPath($TargetPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

$sourceEntries = @()
$sourceEntries += Get-ChildItem -Path (Join-Path $RepoRoot "examples") -Filter *.src |
    Sort-Object Name |
    ForEach-Object {
        [pscustomobject]@{
            Path = $_.FullName
            Category = "reduction"
        }
    }

$reductionBenchmarkNames = @("cse_block.src", "dead_store.src", "loop_invariant.src")
$showcaseBenchmarkNames = @("loop_peel.src", "loop_unroll.src", "loop_unswitch.src", "nested_loops.src", "strength_reduction.src")

$sourceEntries += Get-ChildItem -Path (Join-Path $RepoRoot "benchmarks") -Filter *.src |
    Sort-Object Name |
    ForEach-Object {
        $category = if ($showcaseBenchmarkNames -contains $_.Name) { "showcase" } else { "reduction" }
        [pscustomobject]@{
            Path = $_.FullName
            Category = $category
        }
    }

$sourceEntries += [pscustomobject]@{
    Path = (Join-Path $RepoRoot "tests\optimizations.src")
    Category = "reduction"
}

$sourceEntries = $sourceEntries |
    Sort-Object Path -Unique

$runRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("compiler-bench-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $runRoot | Out-Null

try {
    $rows = @()
    $totals = @{
        PreIr = 0
        OptIr = 0
        PreBlocks = 0
        OptBlocks = 0
        PreEdges = 0
        OptEdges = 0
        Folds = 0
        Propagations = 0
        Cse = 0
        DeadCode = 0
        DeadStores = 0
        Unreachable = 0
        LoopSimplifications = 0
        LoopHoists = 0
        LoopPreheaders = 0
        LoopPeels = 0
        LoopUnrolls = 0
        LoopUnswitches = 0
        StrengthReductions = 0
        InductionVariables = 0
    }

    for ($index = 0; $index -lt $sourceEntries.Count; $index++) {
        $entry = $sourceEntries[$index]
        $source = $entry.Path
        $category = $entry.Category
        $name = [System.IO.Path]::GetFileNameWithoutExtension($source)
        $relativeSource = Get-RelativePath -BasePath $RepoRoot -TargetPath $source
        $safeName = $relativeSource.Replace('\', '_').Replace('/', '_').Replace('.', '_')
        $prefix = Join-Path $runRoot ("{0:D2}_{1}" -f $index, $safeName)

        & $CompilerPath --no-png $source $prefix | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Benchmark compile failed for $source"
        }

        $preIrPath = "${prefix}_pre_opt_ir.txt"
        $optIrPath = "${prefix}_ir.txt"
        $summaryPath = "${prefix}_summary.txt"

        $preIrCount = @(Get-Content $preIrPath | Where-Object { $_.Trim() }).Count
        $optIrCount = @(Get-Content $optIrPath | Where-Object { $_.Trim() }).Count
        $summary = Get-Content -Raw -Path $summaryPath

        $preCfgMatch = [regex]::Match($summary, 'PRE-OPT CFG: Created a control-flow graph from the unoptimized IR with (\d+) basic blocks, (\d+) control-flow edges')
        $optCfgMatch = [regex]::Match($summary, 'CFG: Created an optimized control-flow graph with (\d+) basic blocks, (\d+) control-flow edges')
        $optMatch = [regex]::Match($summary, 'OPT: Applied (\d+) constant folds, (\d+) constant propagations, (\d+) common-subexpression eliminations, (\d+) dead-code removals, (\d+) dead-store removals, (\d+) unreachable-code removals, (\d+) loop simplifications, (\d+) loop-invariant hoists, (\d+) loop preheaders, (\d+) loop peels, (\d+) loop unrolls, and (\d+) loop unswitches, (\d+) strength reductions, and (\d+) induction variables optimized')

        if (-not $preCfgMatch.Success -or -not $optCfgMatch.Success -or -not $optMatch.Success) {
            throw "Could not parse summary metrics for $source"
        }

        $preBlocks = [int]$preCfgMatch.Groups[1].Value
        $preEdges = [int]$preCfgMatch.Groups[2].Value
        $optBlocks = [int]$optCfgMatch.Groups[1].Value
        $optEdges = [int]$optCfgMatch.Groups[2].Value

        $folds = [int]$optMatch.Groups[1].Value
        $propagations = [int]$optMatch.Groups[2].Value
        $cse = [int]$optMatch.Groups[3].Value
        $deadCode = [int]$optMatch.Groups[4].Value
        $deadStores = [int]$optMatch.Groups[5].Value
        $unreachable = [int]$optMatch.Groups[6].Value
        $loopSimplifications = [int]$optMatch.Groups[7].Value
        $loopHoists = [int]$optMatch.Groups[8].Value
        $loopPreheaders = [int]$optMatch.Groups[9].Value
        $loopPeels = [int]$optMatch.Groups[10].Value
        $loopUnrolls = [int]$optMatch.Groups[11].Value
        $loopUnswitches = [int]$optMatch.Groups[12].Value
        $strengthReductions = [int]$optMatch.Groups[13].Value
        $inductionVariables = [int]$optMatch.Groups[14].Value

        if ($category -eq "reduction") {
            $totals.PreIr += $preIrCount
            $totals.OptIr += $optIrCount
            $totals.PreBlocks += $preBlocks
            $totals.OptBlocks += $optBlocks
            $totals.PreEdges += $preEdges
            $totals.OptEdges += $optEdges
            $totals.Folds += $folds
            $totals.Propagations += $propagations
            $totals.Cse += $cse
            $totals.DeadCode += $deadCode
            $totals.DeadStores += $deadStores
            $totals.Unreachable += $unreachable
            $totals.LoopSimplifications += $loopSimplifications
            $totals.LoopHoists += $loopHoists
            $totals.LoopPreheaders += $loopPreheaders
            $totals.LoopPeels += $loopPeels
            $totals.LoopUnrolls += $loopUnrolls
            $totals.LoopUnswitches += $loopUnswitches
            $totals.StrengthReductions += $strengthReductions
            $totals.InductionVariables += $inductionVariables
        }

        $rows += [pscustomobject]@{
            Program = $relativeSource
            Category = $category
            PreIR = $preIrCount
            OptIR = $optIrCount
            PreBlocks = $preBlocks
            OptBlocks = $optBlocks
            PreEdges = $preEdges
            OptEdges = $optEdges
            LoopPreheaders = $loopPreheaders
            LoopPeels = $loopPeels
            LoopUnrolls = $loopUnrolls
            LoopUnswitches = $loopUnswitches
            StrengthReductions = $strengthReductions
            InductionVariables = $inductionVariables
        }
    }

    $irReduction = if ($totals.PreIr -gt 0) { [math]::Round((($totals.PreIr - $totals.OptIr) * 100.0) / $totals.PreIr, 2) } else { 0 }
    $blockReduction = if ($totals.PreBlocks -gt 0) { [math]::Round((($totals.PreBlocks - $totals.OptBlocks) * 100.0) / $totals.PreBlocks, 2) } else { 0 }
    $edgeReduction = if ($totals.PreEdges -gt 0) { [math]::Round((($totals.PreEdges - $totals.OptEdges) * 100.0) / $totals.PreEdges, 2) } else { 0 }

    $report = @()
    $report += "# Optimization Benchmark Report"
    $report += ""
    $report += "Total suite size: $($rows.Count) programs"
    $report += ""
    $report += "## Core Reduction Suite"
    $report += ""
    $report += "Programs counted in the aggregate reduction metrics: $((@($rows | Where-Object { $_.Category -eq 'reduction' })).Count)"
    $report += ""
    $report += "### Aggregate Results"
    $report += ""
    $report += "- IR instructions: $($totals.PreIr) -> $($totals.OptIr) ($irReduction% reduction)"
    $report += "- CFG basic blocks: $($totals.PreBlocks) -> $($totals.OptBlocks) ($blockReduction% reduction)"
    $report += "- CFG control-flow edges: $($totals.PreEdges) -> $($totals.OptEdges) ($edgeReduction% reduction)"
    $report += "- Constant folds: $($totals.Folds)"
    $report += "- Constant propagations: $($totals.Propagations)"
    $report += "- Common-subexpression eliminations: $($totals.Cse)"
    $report += "- Dead-code removals: $($totals.DeadCode)"
    $report += "- Dead-store removals: $($totals.DeadStores)"
    $report += "- Unreachable-code removals: $($totals.Unreachable)"
    $report += "- Loop simplifications: $($totals.LoopSimplifications)"
    $report += "- Loop-invariant hoists: $($totals.LoopHoists)"
    $report += "- Loop preheaders: $($totals.LoopPreheaders)"
    $report += "- Loop peels: $($totals.LoopPeels)"
    $report += "- Loop unrolls: $($totals.LoopUnrolls)"
    $report += "- Loop unswitches: $($totals.LoopUnswitches)"
    $report += "- Strength reductions: $($totals.StrengthReductions)"
    $report += "- Induction variables optimized: $($totals.InductionVariables)"
    $report += ""
    $report += "### Per-Program Results"
    $report += ""
    $report += "| Program | Pre-Opt IR | Opt IR | Pre Blocks | Opt Blocks | Pre Edges | Opt Edges |"
    $report += "|---|---:|---:|---:|---:|---:|---:|"
    foreach ($row in ($rows | Where-Object { $_.Category -eq 'reduction' })) {
        $report += "| $($row.Program) | $($row.PreIR) | $($row.OptIR) | $($row.PreBlocks) | $($row.OptBlocks) | $($row.PreEdges) | $($row.OptEdges) |"
    }
    $report += ""
    $report += "## Loop Transform Showcase"
    $report += ""
    $report += "These programs are designed to exercise structural loop transforms such as preheaders, peeling, unrolling, and unswitching. They are reported separately because these passes can increase IR size while still improving loop structure."
    $report += ""
    $report += "| Program | Pre-Opt IR | Opt IR | Loop Preheaders | Loop Peels | Loop Unrolls | Loop Unswitches | Strength Reductions | Induction Vars |"
    $report += "|---|---:|---:|---:|---:|---:|---:|---:|---:|"
    foreach ($row in ($rows | Where-Object { $_.Category -eq 'showcase' })) {
        $report += "| $($row.Program) | $($row.PreIR) | $($row.OptIR) | $($row.LoopPreheaders) | $($row.LoopPeels) | $($row.LoopUnrolls) | $($row.LoopUnswitches) | $($row.StrengthReductions) | $($row.InductionVariables) |"
    }

    $reportText = ($report -join "`n") + "`n"
    Set-Content -Path $OutputReport -Value $reportText
    Write-Output $reportText
}
finally {
    if (Test-Path $runRoot) {
        Remove-Item -LiteralPath $runRoot -Recurse -Force
    }
}
