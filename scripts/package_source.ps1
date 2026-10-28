$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    $workspace = (Get-Location).Path
    $manifest = Get-Content -LiteralPath Cargo.toml -Raw
    $version = [regex]::Match($manifest, '(?m)^version\s*=\s*"([^"]+)"').Groups[1].Value
    $stage = Join-Path $workspace ('target/source-' + [Guid]::NewGuid().ToString('N'))
    $folder = Join-Path $stage "VocalPure-$version-Source"
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    foreach ($name in @('src','tests','scripts','installer','assets','examples','vendor','.cargo')) {
        Copy-Item -LiteralPath (Join-Path $workspace $name) -Destination (Join-Path $folder $name) -Recurse
    }
    foreach ($name in @('Cargo.toml','Cargo.lock','build.rs','README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE_NOTES.md','.gitignore')) {
        Copy-Item -LiteralPath (Join-Path $workspace $name) -Destination $folder
    }
    New-Item -ItemType Directory -Force -Path releases | Out-Null
    $zip = Join-Path $workspace "releases/VocalPure-$version-Source.zip"
    Compress-Archive -LiteralPath $folder -DestinationPath $zip -CompressionLevel Optimal -Force
    $hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
    '{0}  {1}' -f $hash.Hash, (Split-Path -Leaf $zip) | Set-Content -LiteralPath ($zip + '.sha256') -Encoding utf8
    Write-Host "Codigo-fonte: $zip"
} finally { Pop-Location }
