; Instalador de Barullo 7FM para Windows (Inno Setup 6)
; Se compila desde el workflow con:  iscc /DAppVersion=0.4.0 /DArtDir=<artefactos Release> /DOutDir=<salida> packaging\windows.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef ArtDir
  #define ArtDir "..\build\Barullo7FM_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\dist"
#endif

[Setup]
AppId={{B7F3A1D4-2C6E-4E8B-A15D-7F9C0E2B4A61}
AppName=Barullo 7FM
AppVersion={#AppVersion}
AppPublisher=Escoita
AppPublisherURL=https://www.escoita.link
DefaultDirName={autopf}\Barullo 7FM
DefaultGroupName=Barullo 7FM
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=Barullo7FM-{#AppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Barullo 7FM

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Todo (VST3 e aplicación)"
Name: "custom"; Description: "Escoller"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Plugin VST3 (para o DAW)"; Types: full custom
Name: "app";  Description: "Aplicación independente"; Types: full custom

[Files]
; VST3: va a la carpeta estándar C:\Program Files\Common Files\VST3
Source: "{#ArtDir}\VST3\Barullo 7FM.vst3\*"; DestDir: "{commoncf64}\VST3\Barullo 7FM.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtDir}\Standalone\Barullo 7FM.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{group}\Barullo 7FM"; Filename: "{app}\Barullo 7FM.exe"; Components: app
Name: "{autodesktop}\Barullo 7FM"; Filename: "{app}\Barullo 7FM.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Icona no escritorio"; Components: app; Flags: unchecked

[Run]
Filename: "{app}\Barullo 7FM.exe"; Description: "Abrir Barullo 7FM"; Components: app; Flags: nowait postinstall skipifsilent
