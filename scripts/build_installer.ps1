param([string]$IsccPath, [switch]$SkipBuild, [switch]$TestBuild)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build_windows.ps1')
    }
    if (-not $IsccPath) {
        $candidates = @(
            'target/build-tools/inno/ISCC.exe',
            'C:/Program Files (x86)/Inno Setup 6/ISCC.exe',
            'C:/Program Files/Inno Setup 6/ISCC.exe'
        )
        $IsccPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $IsccPath) {
            $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
            if ($command) { $IsccPath = $command.Source }
        }
    }
    if (-not $IsccPath) { throw 'Instale o Inno Setup 6.7 ou informe -IsccPath.' }
    foreach ($required in @('dist/VocalPure.exe','dist/VocalPure.clap','dist/VocalPure.vst3/Contents/x86_64-win/VocalPure.vst3','assets/vocalpure.ico')) {
        if (-not (Test-Path -LiteralPath $required)) { throw "Arquivo ausente: $required" }
    }
    $manifest = Get-Content -LiteralPath Cargo.toml -Raw
    $version = [regex]::Match($manifest, '(?m)^version\s*=\s*"([^"]+)"').Groups[1].Value
    $arguments = @('/Qp',('/DAppVersion=' + $version))
    if ($TestBuild) { $arguments += '/DTestBuild' }
    $arguments += 'installer/VocalPure.iss'
    & $IsccPath @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o instalador.' }
    if (-not $TestBuild) {
        $file = Join-Path (Get-Location) "releases/VocalPure-$version-Windows-x64-Setup.exe"
        $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
        '{0}  {1}' -f $hash.Hash, (Split-Path -Leaf $file) | Set-Content -LiteralPath ($file + '.sha256') -Encoding utf8
        Write-Host "Instalador pronto: $file"
    }
} finally { Pop-Location }
