; MFR-303 - Copyright (C) 2026 Music For Robots
; SPDX-License-Identifier: AGPL-3.0-or-later
;
; Inno Setup script. Built by scripts/package.ps1, which passes:
;   /DAppVersion=1.0.0 /DBuildDir=<...\Release> /DOutDir=<dist>

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6F2C1B7A-3D4E-4A8B-9C1D-5E7F80A1B303}
AppName=MFR-303
AppVersion={#AppVersion}
AppVerName=MFR-303 {#AppVersion}
AppPublisher=Music For Robots
AppPublisherURL=https://musicforrobots.com
AppSupportURL=https://github.com/music-for-robots/MFR-303
DefaultDirName={autopf}\Music For Robots\MFR-303
DefaultGroupName=Music For Robots
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir={#OutDir}
OutputBaseFilename=MFR-303-{#AppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=MFR-303
UninstallDisplayIcon={app}\MFR-303.exe

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3)"; Types: full vst3 custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#BuildDir}\VST3\MFR-303.vst3\*"; DestDir: "{commoncf64}\VST3\MFR-303.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\MFR-303.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\MFR-303"; Filename: "{app}\MFR-303.exe"; Components: standalone
Name: "{group}\Uninstall MFR-303"; Filename: "{uninstallexe}"

[UninstallDelete]
; The bundle is ours alone, so remove the whole folder (Inno leaves empty bundle dirs otherwise).
Type: filesandordirs; Name: "{commoncf64}\VST3\MFR-303.vst3"
