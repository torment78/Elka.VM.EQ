#ifndef AppVersion
  #define AppVersion "0.3.1"
#endif
#ifndef PayloadDir
  #error Supply PayloadDir with the validated portable payload.
#endif
#ifndef ReleaseDir
  #error Supply ReleaseDir for the installer output.
#endif

[Setup]
AppId={{FD5AE90B-B4D7-4D97-AF2B-8D1B9561A42A}
AppName=Elka VM EQ
AppVersion={#AppVersion}
AppPublisher=ElkaSoft
AppPublisherURL=https://github.com/torment78/Elka.VM.EQ
AppSupportURL=https://github.com/torment78/Elka.VM.EQ/issues
DefaultDirName={localappdata}\Programs\ElkaSoft\Elka VM EQ
DefaultGroupName=ElkaSoft\Elka VM EQ
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#ReleaseDir}
OutputBaseFilename=Elka-VM-EQ-{#AppVersion}-Setup-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
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

[Icons]
Name: "{group}\Elka VM EQ"; Filename: "{app}\ElkaVmEq.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Elka VM EQ"; Filename: "{app}\ElkaVmEq.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ElkaVmEq.exe"; Description: "Launch Elka VM EQ"; Flags: nowait postinstall skipifsilent
