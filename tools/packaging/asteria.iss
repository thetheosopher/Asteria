; Asteria Inno Setup Script
; Copyright (C) 2026 Michael A. McCloskey
; Licensed under the GNU General Public License v2 or later.
;
; Packages Asteria desktop app with all runtime dependencies and data files.
; Supports per-user or machine-wide installation.
;
; Prerequisites:
;   - Build Asteria in Release mode: cmake --build build/default --config Release
;   - Inno Setup 6.x installed
;
; Usage:
;   iscc.exe asteria.iss
;   or open in Inno Setup Compiler GUI and click Compile

#define MyAppName "Asteria"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Michael A. McCloskey"
#define MyAppURL "https://github.com/thetheosopher/Asteria"
#define MyAppCoffeeURL "https://buymeacoffee.com/theosopher"
#define MyAppExeName "asteria_app.exe"
#define MyAppCliName "asteria_cli.exe"

; Paths relative to this .iss file location
#define BuildRoot "..\..\build\default"
#define SourceRoot "..\.."
#define ThirdPartyRoot "..\..\third_party\astrolog"
#define AssetsRoot "..\..\assets"
#define PackagingRoot "."

[Setup]
AppId={{A5E7R1A0-C2F6-4B8A-9D3E-1A0B2C3D4E5F}
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
; Allow user to choose per-user or machine install
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile={#PackagingRoot}\LICENSE.txt
SetupIconFile={#PackagingRoot}\assets\asteria.ico
OutputDir={#PackagingRoot}\Output
OutputBaseFilename=AsteriaSetup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Asteria - Astrological Charting Application
VersionInfoCopyright=Copyright (C) 2026 {#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "installcli"; Description: "Install command-line tool (asteria_cli)"; GroupDescription: "Optional Components:"

[Files]
; Main application
Source: "{#BuildRoot}\src\app\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; CLI tool (optional)
Source: "{#BuildRoot}\src\automation\Release\{#MyAppCliName}"; DestDir: "{app}"; Flags: ignoreversion; Tasks: installcli

; Atlas data (next to exe, as the app expects)
Source: "{#AssetsRoot}\atlasbig.as"; DestDir: "{app}"; Flags: ignoreversion

; Astrology font
Source: "{#AssetsRoot}\fonts\Astro.ttf"; DestDir: "{app}"; Flags: ignoreversion

; Application icon PNG (for About screen)
Source: "{#AssetsRoot}\asteria_icon.png"; DestDir: "{app}"; Flags: ignoreversion

; Astrolog data files
Source: "{#ThirdPartyRoot}\timezone.as"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\astrolog.as"; DestDir: "{app}\astrolog_data"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\sefstars.txt"; DestDir: "{app}\astrolog_data"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\seorbel.txt"; DestDir: "{app}\astrolog_data"; Flags: ignoreversion
Source: "{#ThirdPartyRoot}\ephem\*"; DestDir: "{app}\astrolog_data\ephem"; Flags: ignoreversion recursesubdirs

; License and notices
Source: "{#PackagingRoot}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackagingRoot}\NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Set ASTROLOG environment variable so the engine finds its data files.
; Use HKCU for per-user installs, HKLM for machine installs.
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "ASTROLOG"; ValueData: "{app}\astrolog_data"; Flags: uninsdeletevalue; Check: not IsAdminInstallMode
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: "ASTROLOG"; ValueData: "{app}\astrolog_data"; Flags: uninsdeletevalue; Check: IsAdminInstallMode

[UninstallDelete]
; Clean up user data directory on uninstall (database, logs)
; Uncomment if desired:
; Type: filesandirs; Name: "{app}\data"
