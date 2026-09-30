; Airwindows Chain installer. Puts the VST3 and CLAP in the standard folders.
; Build: makensis /DVERSION=0.2.0 tools\installer.nsi

!include "MUI2.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif

Name "Airwindows Chain"
OutFile "..\build\AirwindowsChain-${VERSION}-setup.exe"
Unicode True
RequestExecutionLevel admin
SetCompressor /SOLID lzma
InstallDir "$PROGRAMFILES64\Airwindows Chain"
BrandingText "Airwindows Chain ${VERSION}"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Airwindows Chain ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This installs the VST3 and CLAP versions of Airwindows Chain into the standard plugin folders, where FL Studio, Ableton Live, Reaper, Bitwig and the rest will find them.$\r$\n$\r$\nClose your DAW first if it already has the plugin loaded."
!define MUI_FINISHPAGE_TITLE "Installed"
!define MUI_FINISHPAGE_TEXT "Open your DAW and rescan plugins once. It shows up as Airwindows Chain, under Kani."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Airwindows Chain"
  SetOutPath "$COMMONFILES64\VST3\Airwindows Chain.vst3"
  File /r "..\build\AirwindowsChain_artefacts\Release\VST3\Airwindows Chain.vst3\*.*"

  SetOutPath "$COMMONFILES64\CLAP"
  File "..\build\AirwindowsChain_artefacts\Release\CLAP\Airwindows Chain.clap"

  SetOutPath "$INSTDIR"
  File "..\LICENSE"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "DisplayName" "Airwindows Chain"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "Publisher" "Kani"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "URLInfoAbout" "https://github.com/clotheshoesandwoes/airwindows-chain"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain" "NoRepair" 1
SectionEnd

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\Airwindows Chain.vst3"
  Delete "$COMMONFILES64\CLAP\Airwindows Chain.clap"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AirwindowsChain"
SectionEnd
