; CENTRIX installer - Designed by Reyaan.
#define MyName "CENTRIX"
#define MyVersion "1.0.0"
#define MyVendor "Reyaan."
#define BuildDir "..\build\CENTRIX_artefacts\Release\VST3\CENTRIX.vst3"

[Setup]
AppId={{6F1D7C3A-4B8E-4C5A-9E21-CE17A1C0D001}
AppName={#MyName}
AppVersion={#MyVersion}
AppPublisher={#MyVendor}
DefaultDirName={commoncf64}\VST3\CENTRIX.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DefaultGroupName={#MyName}
OutputDir=Output
OutputBaseFilename=CENTRIX_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#MyName} (VST3)

[Files]
Source: "{#BuildDir}\*"; DestDir: "{commoncf64}\VST3\CENTRIX.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=CENTRIX has been installed.%n%nIn FL Studio open Options > Manage plugins, click "Find installed plugins", then add CENTRIX as an effect.%n%nDesigned by Reyaan.
