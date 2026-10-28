$ErrorActionPreference = 'Stop'
$metadataText = cargo metadata --format-version 1 --filter-platform x86_64-pc-windows-msvc --locked --offline
if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel consultar as licencas das dependencias.' }
$metadata = $metadataText | ConvertFrom-Json
$index = 0
$inventory = foreach ($package in $metadata.packages) {
    if ($package.name -eq 'vocalpure') { continue }
    $index++
    $folder = Join-Path 'dist/licenses' ('{0}-{1}-{2}' -f $package.name, $package.version, $index)
    $source = Split-Path -Parent $package.manifest_path
    $files = @(Get-ChildItem -LiteralPath $source -Recurse -File | Where-Object {
        $_.Name -match '^(LICENSE|LICENCE|COPYING|NOTICE|AUTHORS|OFL)([._-]|$)' -and
        $_.FullName -notmatch '[\\/]\.git[\\/]'
    })
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($source.Length + 1)
        $destination = Join-Path $folder $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
    }
    if ($package.license_file) {
        $license = Join-Path $source $package.license_file
        if (Test-Path -LiteralPath $license) {
            New-Item -ItemType Directory -Force -Path $folder | Out-Null
            Copy-Item -LiteralPath $license -Destination (Join-Path $folder 'DECLARED-LICENSE.txt') -Force
        }
    }
    '{0} {1}: {2} | {3}' -f $package.name, $package.version, $package.license, $package.source
}
$inventory | Set-Content -LiteralPath 'dist/DEPENDENCIES.txt' -Encoding utf8
