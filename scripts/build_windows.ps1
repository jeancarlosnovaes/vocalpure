param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    if (-not $SkipTests) {
        cargo test --locked --all-targets --target-dir target/validation
        if ($LASTEXITCODE -ne 0) { throw 'Os testes falharam.' }
    }
    cargo build --locked --release
    if ($LASTEXITCODE -ne 0) { throw 'A compilacao falhou.' }
    $bundle = Join-Path (Get-Location) 'dist/VocalPure.vst3/Contents/x86_64-win'
    New-Item -ItemType Directory -Force -Path $bundle | Out-Null
    Copy-Item -LiteralPath 'target/release/vocalpure.dll' -Destination (Join-Path $bundle 'VocalPure.vst3') -Force
    Copy-Item -LiteralPath 'target/release/vocalpure.dll' -Destination 'dist/VocalPure.clap' -Force
    Copy-Item -LiteralPath 'target/release/VocalPure.exe' -Destination 'dist/VocalPure.exe' -Force
    Copy-Item -LiteralPath 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'RELEASE_NOTES.md' -Destination 'dist' -Force
    Copy-Item -LiteralPath 'vendor/rnnoise/COPYING' -Destination 'dist/RNNoise-COPYING.txt' -Force
    & (Join-Path $PSScriptRoot 'package_licenses.ps1')
    Get-ChildItem -LiteralPath 'dist' -Recurse -File | Where-Object Name -ne 'SHA256SUMS.txt' | ForEach-Object {
        $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
        '{0}  {1}' -f $hash.Hash, $_.FullName.Substring((Join-Path (Get-Location) 'dist').Length + 1)
    } | Set-Content -LiteralPath 'dist/SHA256SUMS.txt' -Encoding utf8
    Write-Host 'Pronto. VST3, CLAP e aplicativo em dist/.'
} finally { Pop-Location }
