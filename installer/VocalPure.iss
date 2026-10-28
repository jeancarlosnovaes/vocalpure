#ifndef AppVersion
  #define AppVersion "0.5.1"
#endif
#ifndef PackageRoot
  #define PackageRoot "..\dist"
#endif

[Setup]
#ifdef TestBuild
AppId=VocalPureRustInstallerTest
OutputBaseFilename=VocalPure-{#AppVersion}-Installer-Test
OutputDir=..\target\installer-test-build
PrivilegesRequired=lowest
#else
AppId={{AF1DFE0A-9243-4AF2-9CB3-77856C2D3FE8}
OutputBaseFilename=VocalPure-{#AppVersion}-Windows-x64-Setup
OutputDir=..\releases
PrivilegesRequired=admin
UsePreviousPrivileges=no
#endif
AppName=VocalPure
AppVersion={#AppVersion}
AppVerName=VocalPure {#AppVersion}
AppPublisher=VocalPure
AppPublisherURL=https://github.com/jeancarlosnovaes/vocalpure
DefaultDirName={autopf}\VocalPure
DefaultGroupName=VocalPure
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern dynamic polar includetitlebar
WizardSizePercent=115,110
WizardImageFile=..\assets\wizard-sidebar.bmp
WizardSmallImageFile=..\assets\wizard-small.bmp
SetupIconFile=..\assets\vocalpure.ico
UninstallDisplayIcon={app}\VocalPure.ico
LicenseFile=..\LICENSE
InfoBeforeFile=before-install.txt
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=VocalPure
SetupLogging=yes
VersionInfoDescription=Instalador do VocalPure
VersionInfoProductName=VocalPure
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"; InfoBeforeFile: "before-install.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoBeforeFile: "before-install-en.txt"

[CustomMessages]
brazilianportuguese.Full=Aplicativo e plugins
english.Full=Application and plugins
brazilianportuguese.AppOnly=Somente o aplicativo
english.AppOnly=Application only
brazilianportuguese.Custom=Escolher os componentes
english.Custom=Choose components
brazilianportuguese.Application=Aplicativo VocalPure
english.Application=VocalPure application
brazilianportuguese.VST3=Plugin VST3 para editores de áudio
english.VST3=VST3 plugin for audio editors
brazilianportuguese.CLAP=Plugin CLAP
english.CLAP=CLAP plugin
brazilianportuguese.Desktop=Criar atalho na área de trabalho
english.Desktop=Create a desktop shortcut
brazilianportuguese.Shortcuts=Atalhos:
english.Shortcuts=Shortcuts:
brazilianportuguese.Uninstall=Desinstalar VocalPure
english.Uninstall=Uninstall VocalPure
brazilianportuguese.Launch=Abrir o VocalPure
english.Launch=Launch VocalPure
brazilianportuguese.Folders=Pastas dos plugins
english.Folders=Plugin folders
brazilianportuguese.FoldersSubtitle=Confirme onde seus editores procuram os plugins.
english.FoldersSubtitle=Choose where your audio editors scan for plugins.
brazilianportuguese.FoldersInfo=Os plugins selecionados serão instalados nas pastas abaixo. A pasta compartilhada requer permissão de administrador.
english.FoldersInfo=Selected plugins will be installed in the folders below. Shared folders require administrator approval.
brazilianportuguese.VST3Folder=Pasta VST3:
english.VST3Folder=VST3 folder:
brazilianportuguese.CLAPFolder=Pasta CLAP:
english.CLAPFolder=CLAP folder:
brazilianportuguese.WriteError=Sem permissão para gravar nesta pasta. Feche o instalador e execute a nova versão como administrador: 
english.WriteError=Cannot write to this folder. Close setup and run the updated installer as administrator: 
brazilianportuguese.BackupError=Não foi possível criar uma cópia do plugin anterior. Feche seu editor de áudio e tente novamente.
english.BackupError=Could not back up the existing plugin. Close your audio editor and try again.

[Types]
Name: "full"; Description: "{cm:Full}"
Name: "app"; Description: "{cm:AppOnly}"
Name: "custom"; Description: "{cm:Custom}"; Flags: iscustom

[Components]
Name: "app"; Description: "{cm:Application}"; Types: full app
Name: "vst3"; Description: "{cm:VST3}"; Types: full
Name: "clap"; Description: "{cm:CLAP}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:Desktop}"; GroupDescription: "{cm:Shortcuts}"; Flags: unchecked; Components: app

[Files]
Source: "{#PackageRoot}\VocalPure.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#PackageRoot}\VocalPure.vst3\*"; DestDir: "{code:GetVST3Dir}\VocalPure.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs; BeforeInstall: BackupVST3
Source: "{#PackageRoot}\VocalPure.clap"; DestDir: "{code:GetCLAPDir}"; Components: clap; Flags: ignoreversion
Source: "{#PackageRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\RELEASE_NOTES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\DEPENDENCIES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\RNNoise-COPYING.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\assets\vocalpure.ico"; DestDir: "{app}"; DestName: "VocalPure.ico"; Flags: ignoreversion

[Icons]
Name: "{group}\VocalPure"; Filename: "{app}\VocalPure.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VocalPure.ico"; Components: app
Name: "{autodesktop}\VocalPure"; Filename: "{app}\VocalPure.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VocalPure.ico"; Tasks: desktopicon; Components: app
Name: "{group}\{cm:Uninstall}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\VocalPure.exe"; Description: "{cm:Launch}"; Flags: nowait postinstall skipifsilent unchecked; Components: app

[Code]
var
  PluginDirs: TInputDirWizardPage;

function DefaultPluginDir(FormatName: String): String;
begin
#ifdef TestBuild
  Result := ExpandConstant('{param:TESTROOT|}') + '\Plugins\' + FormatName;
#else
  Result := ExpandConstant('{commoncf64}') + '\' + FormatName;
#endif
end;

function GetVST3Dir(Param: String): String;
begin
  Result := PluginDirs.Values[0];
end;

function GetCLAPDir(Param: String): String;
begin
  Result := PluginDirs.Values[1];
end;

procedure InitializeWizard;
begin
  PluginDirs := CreateInputDirPage(wpSelectComponents,
    CustomMessage('Folders'),
    CustomMessage('FoldersSubtitle'),
    CustomMessage('FoldersInfo'),
    False, '');
  PluginDirs.Add(CustomMessage('VST3Folder'));
  PluginDirs.Add(CustomMessage('CLAPFolder'));
  PluginDirs.Values[0] := DefaultPluginDir('VST3');
  PluginDirs.Values[1] := DefaultPluginDir('CLAP');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = PluginDirs.ID) and
    (not WizardIsComponentSelected('vst3')) and (not WizardIsComponentSelected('clap'));
end;

function InitializeSetup: Boolean;
begin
  Result := True;
#ifdef TestBuild
  Result := ExpandConstant('{param:TESTROOT|}') <> '';
  if not Result then
    MsgBox('Este instalador de teste precisa de uma pasta TESTROOT.', mbError, MB_OK);
#endif
end;


function CheckWritable(Directory: String): String;
var Probe: String; Index: Integer;
begin
  Result := '';
  Index := 0;
  repeat
    Probe := AddBackslash(Directory) + 'VocalPure-setup-write-check-' + IntToStr(Index) + '.tmp';
    Index := Index + 1;
  until not FileExists(Probe);
  if (not ForceDirectories(Directory)) or (not SaveStringToFile(Probe, 'VocalPure setup', False)) then
    Result := CustomMessage('WriteError') + Directory
  else
    DeleteFile(Probe);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if WizardIsComponentSelected('vst3') then Result := CheckWritable(GetVST3Dir(''));
  if (Result = '') and WizardIsComponentSelected('clap') then Result := CheckWritable(GetCLAPDir(''));
end;

procedure BackupVST3;
var Existing, BackupDir, Backup: String; Index: Integer;
begin
  Existing := AddBackslash(GetVST3Dir('')) + 'VocalPure.vst3\Contents\x86_64-win\VocalPure.vst3';
  if not FileExists(Existing) then Exit;
  BackupDir := ExpandConstant('{app}\Backups');
  if not ForceDirectories(BackupDir) then RaiseException(CustomMessage('BackupError'));
  Index := 0;
  repeat
    Backup := BackupDir + '\VocalPure-' + GetDateTimeString('yyyymmdd-hhnnss', '-', ':') + '-' + IntToStr(Index) + '.vst3.previous';
    Index := Index + 1;
  until not FileExists(Backup);
  if not FileCopy(Existing, Backup, True) then RaiseException(CustomMessage('BackupError'));
end;
