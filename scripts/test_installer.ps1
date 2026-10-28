param([string]$InstallerPath)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    $workspace = (Get-Location).Path
    if (-not $InstallerPath) { $InstallerPath = 'target/installer-test-build/VocalPure-0.5.1-Installer-Test.exe' }
    $installer = (Resolve-Path -LiteralPath $InstallerPath).Path
    $testRoot = [IO.Path]::GetFullPath((Join-Path $workspace ('target/installer-qa-' + [Guid]::NewGuid().ToString('N'))))
    if (-not $testRoot.StartsWith($workspace + [IO.Path]::DirectorySeparatorChar)) { throw 'Pasta de teste fora do projeto.' }
    $registration = 'HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/VocalPureRustInstallerTest_is1'
    if (Test-Path -LiteralPath $registration) { throw 'Ja existe uma instalacao de teste registrada. Preserve-a e verifique antes de continuar.' }
    $app = Join-Path $testRoot 'App'
    $vst = Join-Path $testRoot 'Plugins/VST3/VocalPure.vst3/Contents/x86_64-win/VocalPure.vst3'
    $clap = Join-Path $testRoot 'Plugins/CLAP/VocalPure.clap'
    New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
    $original = Join-Path $testRoot 'Plugins/VST3/OutroPlugin.vst3/original.txt'
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $original) | Out-Null
    Set-Content -LiteralPath $original -Value 'Versao C++ preservada.'

    function Assert-EqualFile([string]$Source, [string]$Destination) {
        if (-not (Test-Path -LiteralPath $Destination)) { throw "Arquivo instalado ausente: $Destination" }
        if ((Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash) {
            throw "Arquivo instalado divergente: $Destination"
        }
    }
    function Install-Test([string]$Components, [string]$LogName) {
        $arguments = @('/SP-','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',
            ('/DIR="' + $app + '"'), ('/TESTROOT="' + $testRoot + '"'),
            '/TYPE=custom', ('/COMPONENTS=' + $Components), ('/LOG="' + (Join-Path $testRoot $LogName) + '"'))
        $process = Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Instalacao falhou: codigo $($process.ExitCode). Veja $testRoot" }
    }
    function Uninstall-Test {
        $uninstaller = Join-Path $app 'unins000.exe'
        if (-not (Test-Path -LiteralPath $uninstaller)) { throw 'Desinstalador ausente.' }
        $process = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="' + (Join-Path $testRoot 'uninstall.log') + '"')) -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Desinstalacao falhou: $($process.ExitCode)" }
        for ($n = 0; $n -lt 50 -and (Test-Path -LiteralPath $registration); $n++) { Start-Sleep -Milliseconds 100 }
        if (Test-Path -LiteralPath $registration) { throw 'O registro do instalador de teste permaneceu.' }
        foreach ($path in @((Join-Path $app 'VocalPure.exe'),$vst,$clap)) {
            if (Test-Path -LiteralPath $path) { throw "Arquivo do produto permaneceu apos desinstalar: $path" }
        }
        if (-not (Test-Path -LiteralPath $original)) { throw 'A versao C++ foi afetada.' }
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $vst) | Out-Null
    Set-Content -LiteralPath $vst -Value 'Previous plugin binary'
    Install-Test 'app,vst3,clap' 'install-full.log'
    Assert-EqualFile 'dist/VocalPure.exe' (Join-Path $app 'VocalPure.exe')
    Assert-EqualFile 'dist/VocalPure.vst3/Contents/x86_64-win/VocalPure.vst3' $vst
    Assert-EqualFile 'dist/VocalPure.clap' $clap
    if (-not (Test-Path -LiteralPath $registration)) { throw 'Registro de desinstalacao ausente.' }
    $backups = @(Get-ChildItem -LiteralPath (Join-Path $app 'Backups') -Filter '*.previous' -File)
    if (-not ($backups | Where-Object { (Get-Content -LiteralPath $_.FullName -Raw).Trim() -eq 'Previous plugin binary' })) { throw 'Backup do plugin anterior ausente.' }
    $userFile = Join-Path $app 'meus-ajustes.txt'
    Set-Content -LiteralPath $userFile -Value 'Preservar dados do usuario.'
    Add-Content -LiteralPath (Join-Path $app 'README.md') -Value 'Marcador de versao antiga para o teste de atualizacao.'
    Install-Test 'app,vst3,clap' 'upgrade.log'
    Assert-EqualFile 'dist/README.md' (Join-Path $app 'README.md')
    Assert-EqualFile 'dist/VocalPure.exe' (Join-Path $app 'VocalPure.exe')
    Uninstall-Test
    if (-not (Test-Path -LiteralPath $userFile)) { throw 'Dados do usuario removidos.' }

    Install-Test 'app' 'install-app.log'
    Assert-EqualFile 'dist/VocalPure.exe' (Join-Path $app 'VocalPure.exe')
    if ((Test-Path -LiteralPath $vst) -or (Test-Path -LiteralPath $clap)) { throw 'Plugins instalados sem serem selecionados.' }
    Uninstall-Test

    Install-Test 'vst3,clap' 'install-plugins.log'
    Assert-EqualFile 'dist/VocalPure.clap' $clap
    Assert-EqualFile 'dist/VocalPure.vst3/Contents/x86_64-win/VocalPure.vst3' $vst
    if (Test-Path -LiteralPath (Join-Path $app 'VocalPure.exe')) { throw 'Aplicativo instalado sem ser selecionado.' }
    Uninstall-Test
    Write-Host "PASS: instalacao completa, atualizacao, componentes, desinstalacao e preservacao dos arquivos do usuario e da versao C++. Logs: $testRoot"
} finally { Pop-Location }
