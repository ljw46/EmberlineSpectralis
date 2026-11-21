; Inno Setup script. Build with:  iscc /DAppVersion=1.0.0 /DArtefacts=<path to build\EmberlineSpectralis_artefacts\Release> installer\windows\EmberlineSpectralis.iss
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\build\EmberlineSpectralis_artefacts\Release"
#endif

[Setup]
AppId={{7B1F6C0E-4B7A-4C55-9E2C-5E6A1D3C8A41}
AppName=Emberline Spectralis
AppVersion={#AppVersion}
AppPublisher=Emberline
AppPublisherURL=https://github.com/ljw46/EmberlineSpectralis
AppSupportURL=https://github.com/ljw46/EmberlineSpectralis/issues
DefaultDirName={autopf}\Emberline Spectralis
DefaultGroupName=Emberline Spectralis
UninstallDisplayIcon={app}\Emberline Spectralis.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=EmberlineSpectralis-{#AppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin

[Types]
Name: "full"; Description: "Plugin (VST3) and Standalone app"
Name: "plugin"; Description: "Plugin (VST3) only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (for your DAW)"; Types: full plugin custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full custom

[Files]
; VST3 goes in the system VST3 folder every DAW scans
Source: "{#Artefacts}\VST3\Emberline Spectralis.vst3\*"; DestDir: "{commoncf64}\VST3\Emberline Spectralis.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Artefacts}\Standalone\Emberline Spectralis.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE-CONTENT.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Emberline Spectralis"; Filename: "{app}\Emberline Spectralis.exe"; Components: standalone
Name: "{group}\Uninstall Emberline Spectralis"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Emberline Spectralis.exe"; Description: "Start the Standalone app"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
