param([Parameter(Mandatory = $true)][string]$Python)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$local = Join-Path $root 'tools/local'
$envDir = Join-Path $local 'onnx-eval'
$envPython = Join-Path $envDir 'Scripts/python.exe'
if (!(Test-Path -LiteralPath $envPython)) {
    & $Python -m venv $envDir
    if ($LASTEXITCODE -ne 0) { throw 'Could not create isolated Python environment' }
}
& $envPython -m pip install --only-binary=:all: -r (Join-Path $root 'tools/onnx-eval-requirements.txt')
if ($LASTEXITCODE -ne 0) { throw 'Could not install evaluation dependencies' }
$model = Get-Content -Raw (Join-Path $root 'tools/onnx_model.json') | ConvertFrom-Json
$modelDir = Join-Path $local 'onnx-models'
New-Item -ItemType Directory -Force -Path $modelDir | Out-Null
$destination = Join-Path $modelDir $model.filename
if (!(Test-Path -LiteralPath $destination)) {
    $url = "$($model.repository)/resolve/$($model.revision)/onnx/$($model.filename)?download=true"
    $partial = "$destination.part"
    Invoke-WebRequest -Uri $url -OutFile $partial
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $model.sha256) {
        throw "Model checksum failed. Unused partial file retained at $partial"
    }
    Move-Item -LiteralPath $partial -Destination $destination
}
if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $model.sha256) {
    throw 'Existing model has an unexpected checksum; no inference will be performed'
}
Write-Output "Evaluation environment ready. Model: $destination"
