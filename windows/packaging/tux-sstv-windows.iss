#ifndef AppVersion
  #define AppVersion "1.15.3"
#endif
#ifndef BundleDir
  #define BundleDir "..\dist\windows\Tux-sstv"
#endif
#ifndef AppIcon
  #define AppIcon "..\resources\tux-sstv.ico"
#endif
#ifndef InstallerOutput
  #define InstallerOutput "..\dist\windows"
#endif
[Setup]
AppId={{C1498C64-C60B-4772-9627-AC7C39264E35}
AppName=Tux-sstv
AppVersion={#AppVersion}
AppPublisher=Eric Richards (KI5BZO)
DefaultDirName={localappdata}\Programs\Tux-sstv
DefaultGroupName=Tux-sstv
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
MinVersion=10.0.17763
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile={#AppIcon}
UninstallDisplayIcon={app}\tux-sstv.ico
VersionInfoDescription=Tux-sstv amateur radio SSTV installer
VersionInfoCompany=Eric Richards (KI5BZO)
WizardStyle=modern
OutputDir={#InstallerOutput}
OutputBaseFilename=Tux-sstv-{#AppVersion}-windows-x64-setup
Compression=lzma2/normal
SolidCompression=yes
[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}"; Excludes: ".tux-windows-staging,tux-sstv.ico,WINDOWS-START-HERE.txt"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AppIcon}"; DestDir: "{app}"; DestName: "tux-sstv.ico"; Flags: ignoreversion
Source: "..\docs\WINDOWS-INSTALL.txt"; DestDir: "{app}"; DestName: "WINDOWS-START-HERE.txt"; Flags: ignoreversion
[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked
[Icons]
Name: "{autoprograms}\Tux-sstv"; Filename: "{app}\tux-sstv.exe"; WorkingDir: "{app}"; IconFilename: "{app}\tux-sstv.ico"; Comment: "Send and receive SSTV pictures over amateur radio"
Name: "{autodesktop}\Tux-sstv"; Filename: "{app}\tux-sstv.exe"; WorkingDir: "{app}"; IconFilename: "{app}\tux-sstv.ico"; Tasks: desktopicon
