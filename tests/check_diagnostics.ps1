param(
    [string]$CompilerPath,
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

$parseFile = Join-Path $RepoRoot "tests\bad_parse_missing_semi.src"
$semanticFile = Join-Path $RepoRoot "tests\bad_semantic_undeclared.src"

function Invoke-CompilerExpectFailure {
    param(
        [string]$SourcePath,
        [string]$Prefix
    )

    $previousNativePreference = $PSNativeCommandUseErrorActionPreference
    $previousErrorAction = $ErrorActionPreference
    $PSNativeCommandUseErrorActionPreference = $false
    $ErrorActionPreference = "Continue"
    try {
        $output = & $CompilerPath $SourcePath $Prefix 2>&1
        return @{
            ExitCode = $LASTEXITCODE
            Text = ($output | Out-String)
        }
    }
    finally {
        $PSNativeCommandUseErrorActionPreference = $previousNativePreference
        $ErrorActionPreference = $previousErrorAction
    }
}

$parseResult = Invoke-CompilerExpectFailure -SourcePath $parseFile -Prefix "diag_parse"
if ($parseResult.ExitCode -eq 0) {
    throw "Expected parse diagnostics run to fail."
}
$parseText = $parseResult.Text
if ($parseText -notmatch "Parse error at line 4, column 5") {
    throw "Expected parse diagnostic to include line and column. Got:`n$parseText"
}
if ($parseText -notmatch "\^\^\^\^\^\^") {
    throw "Expected parse diagnostic to include caret highlighting. Got:`n$parseText"
}
if ($parseText -notmatch "expected ;") {
    throw "Expected parse diagnostic to mention the expected token. Got:`n$parseText"
}

$semanticResult = Invoke-CompilerExpectFailure -SourcePath $semanticFile -Prefix "diag_semantic"
if ($semanticResult.ExitCode -eq 0) {
    throw "Expected semantic diagnostics run to fail."
}
$semanticText = $semanticResult.Text
if ($semanticText -notmatch "Semantic error at line 3, column 5") {
    throw "Expected semantic diagnostic to include line and column. Got:`n$semanticText"
}
if ($semanticText -notmatch "undeclared variable 'y'") {
    throw "Expected semantic diagnostic to mention the undeclared variable. Got:`n$semanticText"
}
