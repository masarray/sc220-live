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
#ifndef VBSetupSource
  #error VBSetupSource must be defined by package-sc220-installer.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be defined by package-sc220-installer.ps1
#endif

#define AppName "SC220 Live"
#define AppPublisher "Recording Tech / MasArray"
#define AppExeName "SC220 Live.exe"
#define VBSetupName "VBCABLE_Setup_x64.exe"

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
VersionInfoDescription=SC220 Live Windows installer with optional VB-Audio VB-CABLE
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "vbcable"; Description: "Install or repair VB-Audio VB-CABLE (donationware)"; GroupDescription: "Virtual audio routing:"; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#AppExeSource}"; DestDir: "{app}"; DestName: "{#AppExeName}"; Flags: ignoreversion
Source: "{#EulaSource}"; DestDir: "{app}\Legal"; DestName: "EULA.txt"; Flags: ignoreversion
Source: "{#PurchaseTermsSource}"; DestDir: "{app}\Legal"; DestName: "PURCHASE_TERMS.txt"; Flags: ignoreversion
Source: "{#ThirdPartySource}"; DestDir: "{app}\Legal"; DestName: "THIRD_PARTY_NOTICES.md"; Flags: ignoreversion
Source: "{#VBNoticeSource}"; DestDir: "{app}\Legal"; DestName: "VB_AUDIO_NOTICE.txt"; Flags: ignoreversion
Source: "{#VBSetupSource}"; Flags: dontcopy

[Icons]
Name: "{group}\SC220 Live"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\SC220 Live"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch SC220 Live"; Flags: nowait postinstall skipifsilent; Check: CanLaunchApp

[Code]
var
  VBCablePage: TWizardPage;
  VBCableText: TNewStaticText;
  VBCableLink: TNewStaticText;
  VBCableInstallSucceeded: Boolean;

procedure OpenVBCableWebsite(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('open', 'https://vb-audio.com/Cable/', '', '', SW_SHOWNORMAL,
    ewNoWait, ErrorCode);
end;

procedure InitializeWizard;
begin
  VBCableInstallSucceeded := False;

  VBCablePage := CreateCustomPage(
    wpSelectTasks,
    'VB-Audio VB-CABLE',
    'Optional donationware driver for SC220 Live Stream Output');

  VBCableText := TNewStaticText.Create(VBCablePage);
  VBCableText.Parent := VBCablePage.Surface;
  VBCableText.Left := ScaleX(0);
  VBCableText.Top := ScaleY(8);
  VBCableText.Width := VBCablePage.SurfaceWidth;
  VBCableText.Height := ScaleY(150);
  VBCableText.AutoSize := False;
  VBCableText.WordWrap := True;
  VBCableText.Caption :=
    'VB-CABLE is developed and owned by VB-Audio Software. It is included ' +
    'unchanged as an optional donationware component. You remain free to ' +
    'support VB-Audio if the driver is useful.' + #13#10 + #13#10 +
    'Windows may display a driver-security confirmation. A restart is required ' +
    'before CABLE Input and CABLE Output become fully available.' + #13#10 + #13#10 +
    'Deselect the VB-CABLE task when it is already installed or when you prefer ' +
    'to install it separately.';

  VBCableLink := TNewStaticText.Create(VBCablePage);
  VBCableLink.Parent := VBCablePage.Surface;
  VBCableLink.Left := ScaleX(0);
  VBCableLink.Top := ScaleY(174);
  VBCableLink.Caption := 'VB-Audio information, donation, and licensing';
  VBCableLink.Font.Color := clBlue;
  VBCableLink.Font.Style := [fsUnderline];
  VBCableLink.Cursor := crHand;
  VBCableLink.OnClick := @OpenVBCableWebsite;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = VBCablePage.ID) and
    (not WizardIsTaskSelected('vbcable'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  SetupPath: String;
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('vbcable') then
  begin
    ExtractTemporaryFile('{#VBSetupName}');
    SetupPath := ExpandConstant('{tmp}\{#VBSetupName}');

    WizardForm.StatusLabel.Caption :=
      'Installing VB-Audio VB-CABLE. Approve the Windows driver prompt if shown...';

    if not Exec(SetupPath, '-i -h', '', SW_SHOWNORMAL,
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
        'SC220 Live was installed, but verify the VB-CABLE installation and ' +
        'restart Windows before using Stream Output.',
        mbError, MB_OK);
    end
    else
    begin
      VBCableInstallSucceeded := True;
    end;
  end;
end;

function NeedRestart: Boolean;
begin
  Result := VBCableInstallSucceeded;
end;

function CanLaunchApp: Boolean;
begin
  Result := not VBCableInstallSucceeded;
end;
