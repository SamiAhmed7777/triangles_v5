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

; Bootstrap page
Page custom BootstrapPage

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Bootstrap selection variable
Var BootstrapChoice

; Bootstrap page function
Function BootstrapPage
  !insertmacro MUI_HEADER_TEXT "Blockchain Sync" "Choose how to synchronize the blockchain"
  
  nsDialogs::Create 1018
  Pop $0
  
  ${NSD_CreateLabel} 0 10u 100% 20u "The Triangles blockchain requires ~1GB of data. Choose sync method:"
  Pop $0
  
  ${NSD_CreateRadioButton} 10u 40u 100% 12u "Download bootstrap (~1.3GB) — Recommended (fast)"
  Pop $1
  ${NSD_Check} $1
  
  ${NSD_CreateRadioButton} 10u 60u 100% 12u "Sync from network — Slow (may take days)"
  Pop $2
  
  ${NSD_CreateLabel} 10u 80u 100% 30u "Bootstrap will download a recent blockchain snapshot, saving hours or days of sync time. Network bandwidth required: ~1.3GB."
  Pop $0
  
  nsDialogs::Show
  
  ${NSD_GetState} $1 $BootstrapChoice
FunctionEnd

Section "Install"
  SetOutPath "$INSTDIR"

  ; Wallet + Qt DLLs (prepared by the Package step into dist/)
  File /r "..\..\dist\*.*"

  ; Tor binary + data (prepared by Download Tor step into tor-files/)
  SetOutPath "$INSTDIR\tor"
  File /r "..\..\tor-files\*.*"

  ; Create data directory
  CreateDirectory "$APPDATA\Triangles"

  ; Download blockchain bootstrap if selected
  ${If} $BootstrapChoice == ${BST_CHECKED}
    DetailPrint "Downloading blockchain bootstrap..."
    inetc::get /CAPTION "Downloading Blockchain" /CANCELTEXT "Skip" \
      "http://bootstrap.cryptographic-triangles.org/tri-blockchain.tar.gz" \
      "$TEMP\tri-blockchain.tar.gz" /END
    Pop $0
    ${If} $0 == "OK"
      DetailPrint "Extracting blockchain..."
      nsExec::ExecToLog '"$INSTDIR\7z.exe" x "$TEMP\tri-blockchain.tar.gz" -o"$TEMP" -y'
      nsExec::ExecToLog '"$INSTDIR\7z.exe" x "$TEMP\tri-blockchain.tar" -o"$APPDATA\Triangles" -y'
      Delete "$TEMP\tri-blockchain.tar.gz"
      Delete "$TEMP\tri-blockchain.tar"
      DetailPrint "Blockchain bootstrap installed!"
    ${Else}
      DetailPrint "Bootstrap download failed or skipped — will sync from network"
    ${EndIf}
  ${EndIf}

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
