; Cryptographic Triangles NSIS Installer
; Produces a single setup.exe with wallet + Tor bundled
; Uses per-user install (no UAC elevation) so network drives stay visible

!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif

!define APPNAME "Cryptographic Triangles"
!define COMPANYNAME "Cryptographic Triangles"
!define EXENAME "triangles-qt.exe"

Name "${APPNAME} v${VERSION}"
OutFile "Cryptographic-Triangles-${VERSION}-win-x64-setup.exe"
InstallDir "$LOCALAPPDATA\${APPNAME}"
InstallDirRegKey HKCU "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel user

; UI — icons and bitmaps are relative to THIS .nsi file
!define MUI_ICON "..\..\src\qt\res\icons\triangles.ico"
!define MUI_UNICON "..\..\src\qt\res\icons\triangles.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "..\..\share\pixmaps\nsis-header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\..\share\pixmaps\nsis-wizard.bmp"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APPNAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR"

  ; Wallet + Qt DLLs (prepared by the Package step into dist/)
  File /r "..\..\dist\*.*"

  ; Tor binary + data (prepared by Download Tor step into tor-files/)
  SetOutPath "$INSTDIR\tor"
  File /r "..\..\tor-files\*.*"

  ; Create data directory
  CreateDirectory "$APPDATA\Triangles"

  ; Uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Start menu
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Desktop shortcut
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0

  ; Add/Remove Programs (per-user)
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$INSTDIR\${EXENAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${COMPANYNAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${VERSION}"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1
  WriteRegStr HKCU "Software\${APPNAME}" "InstallDir" "$INSTDIR"

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "EstimatedSize" "$0"
SectionEnd

Section "Uninstall"
  ; Stop running processes
  nsExec::ExecToLog 'taskkill /F /IM triangles-qt.exe'
  nsExec::ExecToLog 'taskkill /F /IM trianglesd.exe'
  nsExec::ExecToLog 'taskkill /F /IM tor.exe'

  ; Remove installation
  RMDir /r "$INSTDIR"

  ; Remove shortcuts
  RMDir /r "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"

  ; Remove registry
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
  DeleteRegKey HKCU "Software\${APPNAME}"
SectionEnd
