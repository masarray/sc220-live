#ifndef AppVersion
  #error AppVersion must be defined by package-sc220-installer.ps1
#endif
#ifndef AppExeSource
  #error AppExeSource must be defined by package-sc220-installer.ps1
#endif
#ifndef EulaSource
  #error EulaSource must be defined by package-sc220-installer.ps1
#endif
#ifndef PurchaseTermsSource
  #error PurchaseTermsSource must be defined by package-sc220-installer.ps1
#endif
#ifndef ThirdPartySource
  #error ThirdPartySource must be defined by package-sc220-installer.ps1
#endif
#ifndef VBNoticeSource
  #error VBNoticeSource must be defined by package-sc220-installer.ps1
#endif
#ifndef VBPackageSource
  #error VBPackageSource must be defined by package-sc220-installer.ps1
#endif
#ifndef VCRedistSource
  #error VCRedistSource must be defined by package-sc220-installer.ps1
#endif
#ifndef PrerequisiteProbeSource
  #error PrerequisiteProbeSource must be defined by package-sc220-installer.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be defined by package-sc220-installer.ps1
#endif

#define AppName "SC220 Live"
#define AppPublisher "Recording Tech / MasArray"
#define AppExeName "SC220 Live.exe"
#define VBSetupName "VBCABLE_Setup_x64.exe"
#define VBTempDir "{tmp}\sc220-vbcable"
#define PrerequisiteTempDir "{tmp}\sc220-prerequisites"
#define VCRedistName "VC_redist.x64.exe"
#define PrerequisiteProbeName "SC220PrerequisiteProbe.ps1"

[Setup]
AppId={{A7A6126D-0574-45DE-8921-1937A41C0E4F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://masarray.github.io/sc220-download/
AppSupportURL=https://github.com/masarray/sc220-download/releases
AppUpdatesURL=https://github.com/masarray/sc220-download/releases/latest
DefaultDirName={autopf}\SC220 Live
DefaultGroupName=SC220 Live
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir={#OutputDir}
OutputBaseFilename=SC220-Live-v{#AppVersion}-Setup-win-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.10240
LicenseFile={#EulaSource}
UninstallDisplayIcon={app}\{#AppExeName}
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes
UsePreviousTasks=yes
ChangesAssociations=no
ChangesEnvironment=no
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=SC220 Live smart Windows installer with verified prerequisites
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "vbcable"; Description: "Install or repair VB-Audio VB-CABLE (donationware)"; GroupDescription: "Virtual audio routing:"; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; Keep the preflight probe first so solid compression does not delay early extraction.
Source: "{#PrerequisiteProbeSource}"; DestName: "{#PrerequisiteProbeName}"; Flags: dontcopy noencryption

; The Microsoft runtime is executed only when its x64 registry state is absent.
Source: "{#VCRedistSource}"; DestDir: "{#PrerequisiteTempDir}"; DestName: "{#VCRedistName}"; Flags: ignoreversion deleteafterinstall

; Preserve a diagnostic copy with the installed application.
Source: "{#PrerequisiteProbeSource}"; DestDir: "{app}\Tools"; DestName: "{#PrerequisiteProbeName}"; Flags: ignoreversion

; VB-Audio requires the setup executable to remain beside its INF, SYS, CAT,
; icon, and support files. Copy the complete reviewed package to a temporary
; directory only when the optional driver task is selected.
Source: "{#VBPackageSource}\*"; DestDir: "{#VBTempDir}"; Flags: ignoreversion recursesubdirs createallsubdirs deleteafterinstall; Tasks: vbcable
Source: "{#AppExeSource}"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion
Source: "{#EulaSource}"; DestDir: "{app}\Legal"; DestName: "EULA.txt"; Flags: ignoreversion
Source: "{#PurchaseTermsSource}"; DestDir: "{app}\Legal"; DestName: "PURCHASE_TERMS.txt"; Flags: ignoreversion
Source: "{#ThirdPartySource}"; DestDir: "{app}\Legal"; DestName: "THIRD_PARTY_NOTICES.md"; Flags: ignoreversion
Source: "{#VBNoticeSource}"; DestDir: "{app}\Legal"; DestName: "VB_AUDIO_NOTICE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\SC220 Live"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\SC220 Live"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch SC220 Live"; Flags: nowait postinstall skipifsilent; Check: CanLaunchApp

[Code]
var
  SystemCheckPage: TWizardPage;
  SystemCheckText: TNewStaticText;
  VBCableLink: TNewStaticText;
  InitialCoreProbeCode: Integer;
  InitialVBCableProbeCode: Integer;
  VBCableInstallSucceeded: Boolean;
  CorePrerequisitesReady: Boolean;
  InstallerRestartRequired: Boolean;

procedure OpenVBCableWebsite(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('open', 'https://vb-audio.com/Cable/', '', '', SW_SHOWNORMAL,
    ewNoWait, ErrorCode);
end;

function PowerShellPath: String;
begin
  Result := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
end;

function RunPrerequisiteProbe(const Mode: String): Integer;
var
  ScriptPath: String;
  Parameters: String;
  ResultCode: Integer;
begin
  Result := 99;
  try
    ExtractTemporaryFile('{#PrerequisiteProbeName}');
    ScriptPath := ExpandConstant('{tmp}\{#PrerequisiteProbeName}');
    Parameters := '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ' +
      AddQuotes(ScriptPath) + ' -Mode ' + AddQuotes(Mode);

    if Exec(PowerShellPath, Parameters, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Result := ResultCode
    else
      Log('SC220 prerequisite probe could not be started.');
  except
    Log('SC220 prerequisite probe failed: ' + GetExceptionMessage);
  end;
end;

function IsVCRuntimeInstalled: Boolean;
var
  Installed: Cardinal;
  Version: String;
begin
  Result := False;
  Installed := 0;
  Version := '';

  if RegQueryDWordValue(HKLM64,
       'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
       'Installed', Installed) then
  begin
    RegQueryStringValue(HKLM64,
      'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
      'Version', Version);
    Result := Installed = 1;
  end;

  Log('Microsoft VC++ x64 runtime installed=' + IntToStr(Installed) +
      ' version=' + Version);
end;

function CoreStatusText: String;
begin
  case InitialCoreProbeCode of
    0: Result := 'Ready - Microsoft Visual C++ Runtime and Windows Audio services detected.';
    11: Result := 'Will install - Microsoft Visual C++ Runtime x64 is missing or incomplete.';
    12: Result := 'Needs repair - one or more Windows Audio services are not running.';
  else
    Result := 'Will verify during installation - the initial system probe was inconclusive.';
  end;
end;

function VBCableStatusText: String;
begin
  case InitialVBCableProbeCode of
    0: Result :=
      'Detected - CABLE Input and CABLE Output are available. Existing VB-CABLE will be left untouched.';
    21: Result :=
      'Repair recommended - VB-CABLE is partially detected, but its driver or one endpoint is incomplete.';
    22: Result :=
      'Not detected - the optional verified VB-CABLE package is selected for installation.';
  else
    Result :=
      'Detection was inconclusive. You may keep the VB-CABLE task selected to install or repair it.';
  end;
end;

procedure UpdateSystemCheckText;
var
  VBAction: String;
begin
  if WizardIsTaskSelected('vbcable') then
    VBAction := 'Installer action: install or repair VB-CABLE.'
  else
    VBAction := 'Installer action: keep the existing audio routing unchanged.';

  SystemCheckText.Caption :=
    'SC220 LIVE SYSTEM CHECK' + #13#10 + #13#10 +
    'Microsoft runtime and Windows Audio' + #13#10 +
    CoreStatusText + #13#10 + #13#10 +
    'VB-Audio VB-CABLE' + #13#10 +
    VBCableStatusText + #13#10 +
    VBAction + #13#10 + #13#10 +
    'The installer verifies these components again before SC220 Live is launched. ' +
    'A restart is requested when a driver or runtime installer requires it.';
end;

procedure InitializeWizard;
begin
  VBCableInstallSucceeded := False;
  CorePrerequisitesReady := False;
  InstallerRestartRequired := False;

  InitialCoreProbeCode := RunPrerequisiteProbe('Core');
  InitialVBCableProbeCode := RunPrerequisiteProbe('VB-Cable');

  // Interactive users receive the smart default. Silent automation keeps any
  // explicit /TASKS or /MERGETASKS selection unchanged.
  if not WizardSilent then
  begin
    if InitialVBCableProbeCode = 0 then
      WizardSelectTasks('!vbcable')
    else
      WizardSelectTasks('vbcable');
  end;

  SystemCheckPage := CreateCustomPage(
    wpSelectTasks,
    'SC220 Live system check',
    'Required runtime, Windows Audio, and virtual-cable readiness');

  SystemCheckText := TNewStaticText.Create(SystemCheckPage);
  SystemCheckText.Parent := SystemCheckPage.Surface;
  SystemCheckText.Left := ScaleX(0);
  SystemCheckText.Top := ScaleY(8);
  SystemCheckText.Width := SystemCheckPage.SurfaceWidth;
  SystemCheckText.Height := ScaleY(244);
  SystemCheckText.AutoSize := False;
  SystemCheckText.WordWrap := True;

  VBCableLink := TNewStaticText.Create(SystemCheckPage);
  VBCableLink.Parent := SystemCheckPage.Surface;
  VBCableLink.Left := ScaleX(0);
  VBCableLink.Top := ScaleY(258);
  VBCableLink.Caption := 'VB-Audio information, donation, and licensing';
  VBCableLink.Font.Color := clBlue;
  VBCableLink.Font.Style := [fsUnderline];
  VBCableLink.Cursor := crHand;
  VBCableLink.OnClick := @OpenVBCableWebsite;

  UpdateSystemCheckText;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = SystemCheckPage.ID then
    UpdateSystemCheckText;
end;

procedure TryStartAudioServices;
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\sc.exe'), 'start AudioEndpointBuilder', '', SW_HIDE,
    ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\sc.exe'), 'start Audiosrv', '', SW_HIDE,
    ewWaitUntilTerminated, ResultCode);
end;

procedure InstallVCRuntimeIfNeeded;
var
  ResultCode: Integer;
  SetupPath: String;
  LogPath: String;
begin
  if IsVCRuntimeInstalled then
  begin
    Log('Microsoft VC++ x64 runtime is already installed; skipping redistributable setup.');
    Exit;
  end;

  SetupPath := ExpandConstant('{#PrerequisiteTempDir}\{#VCRedistName}');
  LogPath := ExpandConstant('{tmp}\SC220-vc-redist.log');
  WizardForm.StatusLabel.Caption :=
    'Installing Microsoft Visual C++ Runtime required by SC220 Live...';

  if not FileExists(SetupPath) then
  begin
    MsgBox(
      'The verified Microsoft Visual C++ Runtime package was not extracted. ' +
      'SC220 Live cannot be launched safely.', mbError, MB_OK);
    Exit;
  end;

  if not Exec(SetupPath,
      '/install /passive /norestart /log ' + AddQuotes(LogPath),
      ExtractFileDir(SetupPath), SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    MsgBox(
      'Microsoft Visual C++ Runtime setup could not be started. ' +
      'SC220 Live was installed, but launch has been disabled.', mbError, MB_OK);
    Exit;
  end;

  Log('Microsoft VC++ Redistributable exit code: ' + IntToStr(ResultCode));
  if (ResultCode = 3010) or (ResultCode = 1641) then
    InstallerRestartRequired := True;

  if not ((ResultCode = 0) or (ResultCode = 1638) or
          (ResultCode = 3010) or (ResultCode = 1641)) then
    MsgBox(
      'Microsoft Visual C++ Runtime setup returned exit code ' +
      IntToStr(ResultCode) + '. SC220 Live will not be launched automatically.' +
      Chr(13) + Chr(10) + Chr(13) + Chr(10) +
      'Diagnostic log: ' + LogPath,
      mbError, MB_OK);
end;

procedure VerifyCorePrerequisites;
var
  ProbeCode: Integer;
begin
  ProbeCode := RunPrerequisiteProbe('Core');
  if ProbeCode = 12 then
  begin
    WizardForm.StatusLabel.Caption := 'Starting Windows Audio services...';
    TryStartAudioServices;
    ProbeCode := RunPrerequisiteProbe('Core');
  end;

  CorePrerequisitesReady := ProbeCode = 0;
  if not CorePrerequisitesReady then
  begin
    if ProbeCode = 11 then
      MsgBox(
        'Microsoft Visual C++ Runtime is still not detected. SC220 Live was ' +
        'installed, but automatic launch has been disabled to prevent a missing-DLL error.',
        mbError, MB_OK)
    else if ProbeCode = 12 then
      MsgBox(
        'Windows Audio or Audio Endpoint Builder is not running. SC220 Live was ' +
        'installed, but automatic launch has been disabled. Restart Windows or repair the audio services.',
        mbError, MB_OK)
    else
      MsgBox(
        'SC220 Live could not confirm its core Windows prerequisites. Automatic ' +
        'launch has been disabled. Run Tools\SC220PrerequisiteProbe.ps1 for diagnostics.',
        mbError, MB_OK);
  end;
end;

procedure InstallVBCableIfSelected;
var
  ResultCode: Integer;
  SetupPath: String;
begin
  if not WizardIsTaskSelected('vbcable') then
    Exit;

  SetupPath := ExpandConstant('{#VBTempDir}\{#VBSetupName}');
  Log('Launching complete VB-CABLE package from: ' + SetupPath);

  if not FileExists(SetupPath) then
  begin
    MsgBox(
      'The complete VB-CABLE package was not extracted correctly. ' +
      'SC220 Live was installed, but Stream Output is not ready.',
      mbError, MB_OK);
    Exit;
  end;

  WizardForm.StatusLabel.Caption :=
    'Installing or repairing VB-Audio VB-CABLE. Approve the Windows driver prompt if shown...';

  if not Exec(SetupPath, '-i -h', ExtractFileDir(SetupPath), SW_SHOWNORMAL,
    ewWaitUntilTerminated, ResultCode) then
  begin
    MsgBox(
      'VB-CABLE setup could not be started. SC220 Live was installed, but ' +
      'the virtual cable must be installed separately from vb-audio.com.',
      mbError, MB_OK);
  end
  else if ResultCode <> 0 then
  begin
    MsgBox(
      'VB-CABLE setup returned exit code ' + IntToStr(ResultCode) + '. ' +
      'SC220 Live was installed, but VB-CABLE was not confirmed as installed. ' +
      'Do not use Stream Output until a corrected installation succeeds.',
      mbError, MB_OK);
  end
  else
  begin
    VBCableInstallSucceeded := True;
    InstallerRestartRequired := True;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    InstallVCRuntimeIfNeeded;
    VerifyCorePrerequisites;
    InstallVBCableIfSelected;
  end;
end;

function NeedRestart: Boolean;
begin
  Result := InstallerRestartRequired;
end;

function CanLaunchApp: Boolean;
begin
  Result := CorePrerequisitesReady and
            (not InstallerRestartRequired) and
            (not VBCableInstallSucceeded);
end;
