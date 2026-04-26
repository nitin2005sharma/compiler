param(
    [string]$CompilerPath,
    [int]$Iterations = 25
)

$ErrorActionPreference = "Stop"
$random = [System.Random]::new(20260426)
$runRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("compiler-random-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $runRoot | Out-Null

function New-RandomLiteral() {
    return [string]($random.Next(1, 10))
}

function New-RandomExpr([int]$depth) {
    if ($depth -le 0 -or $random.NextDouble() -lt 0.35) {
        if ($random.NextDouble() -lt 0.4) {
            return "x"
        }
        return (New-RandomLiteral)
    }

    $left = New-RandomExpr ($depth - 1)
    $right = New-RandomExpr ($depth - 1)
    $ops = @("+", "-", "*", "<", "<=", ">", ">=", "==", "!=")
    $op = $ops[$random.Next(0, $ops.Count)]
    return "(" + $left + " " + $op + " " + $right + ")"
}

function New-RandomBlock([int]$depth) {
    $lines = @()
    $statementCount = $random.Next(1, 4)
    for ($i = 0; $i -lt $statementCount; ++$i) {
        $roll = $random.NextDouble()
        if ($depth -gt 0 -and $roll -lt 0.25) {
            $cond = New-RandomExpr 1
            $thenExpr = New-RandomExpr 1
            $elseExpr = New-RandomExpr 1
            $lines += "if ($cond) { x = $thenExpr; } else { x = $elseExpr; }"
        } elseif ($depth -gt 0 -and $roll -lt 0.45) {
            $limit = $random.Next(1, 5)
            $increment = $random.Next(1, 3)
            $lines += "while (x < $limit) { x = x + $increment; }"
        } else {
            $lines += "x = " + (New-RandomExpr $depth) + ";"
        }
    }
    return $lines
}

try {
    for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
        $programLines = @(
            "int main() {",
            "    int x;",
            "    x = " + (New-RandomLiteral) + ";"
        )

        foreach ($line in (New-RandomBlock 2)) {
            $programLines += "    " + $line
        }

        $programLines += "    return x;"
        $programLines += "}"
        $program = ($programLines -join "`n") + "`n"

        $sourcePath = Join-Path $runRoot ("random_" + $iteration + ".src")
        $prefix = Join-Path $runRoot ("random_" + $iteration)
        Set-Content -Path $sourcePath -Value $program

        & $CompilerPath --no-png $sourcePath $prefix | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Random parser smoke test failed on iteration $iteration.`n$program"
        }
    }
}
finally {
    if (Test-Path $runRoot) {
        Remove-Item -LiteralPath $runRoot -Recurse -Force
    }
}
