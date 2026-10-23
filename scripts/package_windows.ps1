param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_windows.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build/regression failed' }
} else {
    $ctest = Join-Path $root 'tools/local/cmake-3.31.6-windows-x86_64/bin/ctest.exe'
    if (!(Test-Path -LiteralPath $ctest)) { $ctest = (Get-Command ctest -ErrorAction Stop).Source }
    & $ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Regression failed; package refused' }
}
$projectText = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')
$version = [regex]::Match($projectText, 'project\(VocalPure VERSION ([0-9.]+)').Groups[1].Value
if (!$version) { throw 'Cannot determine project version' }
$bundle = Join-Path $root 'build/VocalPure_artefacts/Release/VST3/VocalPure.vst3'
$exe = Join-Path $root 'build/VocalPure_artefacts/Release/Standalone/VocalPure.exe'
$binary = Join-Path $bundle 'Contents/x86_64-win/VocalPure.vst3'
$installer = Join-Path $root 'build/Installer/Release/Instalar VocalPure.exe'
foreach ($file in @($binary, $exe, $installer)) {
    if (!(Test-Path -LiteralPath $file)) { throw "Missing binary: $file" }
    if ((Get-Item -LiteralPath $file).VersionInfo.ProductVersion -ne $version) {
        throw "Binary version does not match $version : $file"
    }
}
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$name = "VocalPure-$version-Windows-x64-internal-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')"
$stage = Join-Path $dist $name
$archive = "$stage.zip"
New-Item -ItemType Directory -Path $stage | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage 'VST3'), (Join-Path $stage 'Standalone'), (Join-Path $stage 'Licenses') | Out-Null
Copy-Item -LiteralPath $bundle -Destination (Join-Path $stage 'VST3') -Recurse
Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'Standalone')
Copy-Item -LiteralPath $installer -Destination $stage
$stageBundle = Join-Path $stage 'VST3/VocalPure.vst3'
$payloadLines = @('# VocalPure VST3 SHA256') + @(Get-ChildItem -LiteralPath $stageBundle -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stageBundle.Length + 1).Replace('\','/')
    "$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $relative"
})
[IO.File]::WriteAllText((Join-Path $stage 'VST3/payload.sha256'), ($payloadLines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
& (Join-Path $root 'build/Release/VocalPureInstallerTests.exe') --verify-payload $stageBundle
if ($LASTEXITCODE -ne 0) { throw 'Native installer rejected the packaged payload' }
Copy-Item -LiteralPath (Join-Path $root 'docs/package-readme.txt') -Destination (Join-Path $stage 'LEIA-ME.txt')
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md'), (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'Licenses')
Copy-Item -LiteralPath (Join-Path $root 'external/JUCE/LICENSE.md') -Destination (Join-Path $stage 'Licenses/JUCE-LICENSE.md')
Copy-Item -LiteralPath (Join-Path $root 'external/rnnoise/COPYING') -Destination (Join-Path $stage 'Licenses/RNNoise-COPYING.txt')
# Preserve available dependency notices with their original relative paths.
$modules = Join-Path $root 'external/JUCE/modules'
$notices = Get-ChildItem -LiteralPath $modules -Recurse -File | Where-Object {
    $_.Name -match '(?i)licen[cs]e|copying|copyright|notice' -or
    $_.FullName.EndsWith('jpglib\README') -or $_.FullName.EndsWith('zlib\README')
}
foreach ($notice in $notices) {
    $relative = $notice.FullName.Substring($modules.Length + 1)
    $target = Join-Path $stage "Licenses/JUCE-dependencies/$relative"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $notice.FullName -Destination $target
}
$commit = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify source commit' }
$dirty = [bool](& git -C $root status --porcelain)
$files = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($stage.Length + 1).Replace('\','/'); bytes=$_.Length;
        sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$manifest = [ordered]@{product='VocalPure'; version=$version; platform='Windows x64';
    channel='internal-test'; signed=$false; source_commit=$commit; source_dirty=$dirty;
    backend='RNNoise 0.1.1'; onnx_included=$false; runtime_mode='static MSVC CRT';
    tests='foundation + installer transactions + exported VST3 smoke'; files=$files}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stage 'package.json') -Encoding utf8
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
# Verify the bytes INSIDE the final ZIP, not just the staging folder.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    $zipFiles = @($zip.Entries | Where-Object { $_.Name })
    if ($zipFiles.Count -ne $files.Count + 1) { throw 'ZIP file count mismatch' }
    foreach ($file in $files) {
        $entry = $zip.GetEntry($file.path)
        if (!$entry -or $entry.Length -ne $file.bytes) { throw "ZIP missing/wrong size: $($file.path)" }
        $stream = $entry.Open()
        $hasher = [System.Security.Cryptography.SHA256]::Create()
        try { $hash = -join ($hasher.ComputeHash($stream) | ForEach-Object { $_.ToString('x2') }) }
        finally { $hasher.Dispose(); $stream.Dispose() }
        if ($hash -ne $file.sha256) { throw "ZIP checksum mismatch: $($file.path)" }
    }
} finally { $zip.Dispose() }
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$archiveHash  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
Write-Output "Verified internal test package: $archive"
Write-Output "SHA256: $archiveHash"
