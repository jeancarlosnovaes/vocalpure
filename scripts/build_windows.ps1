param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cmake = Join-Path $root 'tools/local/cmake-3.31.6-windows-x86_64/bin/cmake.exe'
if (!(Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
& $cmake -S $root -B "$root/build" -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed' }
& $cmake --build "$root/build" --config $Configuration --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir "$root/build" -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
