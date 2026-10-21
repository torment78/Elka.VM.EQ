#ifndef AppVersion
  #define AppVersion "0.3.2"
#endif
#ifndef VersionNumbers
  #define VersionNumbers "0.3.2.0"
#endif
#if Ver < EncodeVer(6, 6, 0)
  #error Inno Setup 6.6 or newer is required for the dark installer.
#endif
#define DonateURL "https://ko-fi.com/msffixit"
#ifndef PayloadDir
  #error Supply PayloadDir with the validated portable payload.
#endif
#ifndef ReleaseDir
  #error Supply ReleaseDir for the installer output.
#endif

[Setup]
#ifdef InstallerTest
AppId=ElkaVmEqInstallerSmokeTest
#else
AppId={{FD5AE90B-B4D7-4D97-AF2B-8D1B9561A42A}
#endif
AppName=Elka VM EQ
AppVersion={#AppVersion}
VersionInfoVersion={#VersionNumbers}
AppPublisher=ElkaSoft
AppPublisherURL=https://github.com/torment78/Elka.VM.EQ
AppSupportURL=https://github.com/torment78/Elka.VM.EQ/issues
DefaultDirName={localappdata}\Programs\ElkaSoft\Elka VM EQ
#ifdef InstallerTest
DefaultGroupName=ElkaVmEqInstallerSmokeTest
#else
DefaultGroupName=ElkaSoft\Elka VM EQ
#endif
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#ReleaseDir}
#ifdef InstallerTest
OutputBaseFilename=InstallerSmokeTest
#else
OutputBaseFilename=Elka-VM-EQ-{#AppVersion}-Setup-x64
#endif
Compression=lzma2
SolidCompression=yes
WizardStyle=modern dark polar includetitlebar
WizardSizePercent=120,120
DisableWelcomePage=no
SetupIconFile=..\graphics\ElkaVmEq.ico
WizardImageFile=..\graphics\vm-eq-vertical.png
WizardSmallImageFile=..\graphics\vm-eq-icon-256.png
UninstallDisplayIcon={app}\ElkaVmEq.exe
CloseApplications=no
RestartApplications=no
SetupLogging=yes

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\ElkaVmEq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\Notices\*"; DestDir: "{app}\Notices"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "Assets\Donate.png"; Flags: dontcopy

[Icons]
#ifdef InstallerTest
Name: "{group}\Elka VM EQ Test"; Filename: "{app}\ElkaVmEq.exe"; WorkingDir: "{app}"
#else
Name: "{group}\Elka VM EQ"; Filename: "{app}\ElkaVmEq.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Elka VM EQ"; Filename: "{app}\ElkaVmEq.exe"; WorkingDir: "{app}"; Tasks: desktopicon
#endif

[Run]
#ifndef InstallerTest
Filename: "{app}\ElkaVmEq.exe"; Description: "Launch Elka VM EQ"; Flags: nowait postinstall skipifsilent
#endif

[Code]
var
  DonateButton: TBitmapButton;

procedure DonateClick(Sender: TObject);
var ErrorCode: Integer;
begin
  if not ShellExecAsOriginalUser('open', '{#DonateURL}', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode) then
    MsgBox('Open {#DonateURL} in your browser to support ElkaSoft.', mbInformation, MB_OK);
end;

procedure FitPortrait(Image: TBitmapImage);
var ArtworkHeight: Integer;
begin
  { Match VoicePlay: show the entire 2:3 artwork without cropping its footer. }
  ArtworkHeight := (Image.Width * 3) div 2;
  Image.Top := Image.Top + (Image.Height - ArtworkHeight) div 2;
  Image.Height := ArtworkHeight;
end;

procedure InitializeWizard;
begin
  FitPortrait(WizardForm.WizardBitmapImage);
  FitPortrait(WizardForm.WizardBitmapImage2);
  WizardForm.WelcomeLabel1.Caption := 'Elka VM EQ';
  WizardForm.WelcomeLabel2.Caption :=
    'Six-band EQ control for VoiceMeeter' + #13#10#13#10 +
    'Control strip and bus EQ, with a live level readout and optional spectrum display.' + #13#10#13#10 +
    'VoiceMeeter must be installed separately. Leave RTA off while another application is using the audio callback.' + #13#10#13#10 +
    'Windows x64  |  {#AppVersion}  |  Unsigned build';
  ExtractTemporaryFile('Donate.png');
  DonateButton := TBitmapButton.Create(WizardForm);
  DonateButton.Parent := WizardForm;
  DonateButton.Caption := 'Donate to ElkaSoft on Ko-fi';
  DonateButton.SetBounds(ScaleX(16), WizardForm.NextButton.Top, ScaleX(104), WizardForm.NextButton.Height);
  DonateButton.Anchors := [akLeft, akBottom];
  DonateButton.TabStop := True;
  DonateButton.Stretch := True;
  DonateButton.BackColor := WizardForm.Color;
  DonateButton.PngImage.LoadFromFile(ExpandConstant('{tmp}\Donate.png'));
  DonateButton.OnClick := @DonateClick;
end;
