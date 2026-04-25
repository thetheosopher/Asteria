; Asteria Inno Setup Script
; Packages Asteria desktop app with runtime dependencies and data files.
;
; Prerequisites:
;   - Build Asteria in Release mode before compiling this script
;   - Inno Setup 6.x installed
;
; Usage:
;   iscc.exe asteria.iss
;   or open in Inno Setup Compiler GUI and click Compile

#define MyAppName "Asteria"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Asteria Project"
#define MyAppURL "https://github.com/asteria-project/asteria"
#define MyAppExeName "asteria_app.exe"
#define MyAppCliName "asteria_cli.exe"

; Paths relative to this .iss file location
#define BuildRoot "..\..\build\default"
#define ThirdPartyRoot "..\..\third_party\astrolog"
#define PackagingRoot "."

[Setup]
AppId={{A5E7R1A0-1234-5678-9ABC-DEF012345678}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile={#PackagingRoot}\LICENSE.txt
; Uncomment when icon assets are available:
; SetupIconFile={#PackagingRoot}\assets\asteria.ico
; WizardImageFile={#PackagingRoot}\assets\asteria-banner.bmp
; WizardSmallImageFile={#PackagingRoot}\assets\asteria-small.bmp
OutputDir={#PackagingRoot}\Output
OutputBaseFilename=AsteriaSetup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "installcli"; Description: "Install command-line tool (asteria-cli)"; GroupDescription: "Optional Components:"

[Files]
; Main application
Source: "{#BuildRoot}\src\app\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; CLI tool (optional)
Source: "{#BuildRoot}\src\automation\Release\{#MyAppCliName}"; DestDir: "{app}"; Flags: ignoreversion; Tasks: installcli

; Astrolog data files
Source: "{#ThirdPartyRoot}\astrolog.as"; DestDir: "{app}\astrolog"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\atlas.as"; DestDir: "{app}\astrolog"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\timezone.as"; DestDir: "{app}\astrolog"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\sefstars.txt"; DestDir: "{app}\astrolog"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\seorbel.txt"; DestDir: "{app}\astrolog"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\ephem\*"; DestDir: "{app}\astrolog\ephem"; Flags: ignoreversion recursesubdirs

; License and notices
Source: "{#PackagingRoot}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackagingRoot}\NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Set ASTROLOG environment variable for the current user so the engine
; can find its data files at runtime.
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "ASTROLOG"; ValueData: "{app}\astrolog"; Flags: uninsdeletevalue

[UninstallDelete]
; Clean up user data directory on uninstall (optional — user may want to keep)
; Uncomment if desired:
; Type: filesandirs; Name: "{localappdata}\Asteria"
