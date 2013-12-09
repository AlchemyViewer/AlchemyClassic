;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; secondlife setup.nsi
;; Copyright 2004-2011, Linden Research, Inc.
;;
;; This library is free software; you can redistribute it and/or
;; modify it under the terms of the GNU Lesser General Public
;; License as published by the Free Software Foundation;
;; version 2.1 of the License only.
;;
;; This library is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; Lesser General Public License for more details.
;;
;; You should have received a copy of the GNU Lesser General Public
;; License along with this library; if not, write to the Free Software
;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
;;
;; Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
;;
;; NSIS Unicode 2.38.1 or higher required
;; http://www.scratchpaper.com/
;;
;; Author: James Cook, Don Kjer, Callum Prentice
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Include flags
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
!include "x64.nsh"
!include "LogicLib.nsh"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compiler flags
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SetOverwrite on				; overwrite files
SetCompress auto			; compress iff saves space
SetCompressor /solid lzma	; compress whole installer as one block
SetDatablockOptimize off	; only saves us 0.1%, not worth it
XPStyle on                  ; add an XP manifest to the installer
RequestExecutionLevel admin	; on Vista we must be admin because we write to Program Files

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Project flags
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%%VERSION%%
%%WIN64_BIN_BUILD%%

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; - language files - one for each language (or flavor thereof)
;; (these files are in the same place as the nsi template but the python script generates a new nsi file in the 
;; application directory so we have to add a path to these include files)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
!include "%%SOURCE%%\installers\windows\lang_da.nsi"
!include "%%SOURCE%%\installers\windows\lang_de.nsi"
!include "%%SOURCE%%\installers\windows\lang_en-us.nsi"
!include "%%SOURCE%%\installers\windows\lang_es.nsi"
!include "%%SOURCE%%\installers\windows\lang_fr.nsi"
!include "%%SOURCE%%\installers\windows\lang_ja.nsi"
!include "%%SOURCE%%\installers\windows\lang_it.nsi"
!include "%%SOURCE%%\installers\windows\lang_pl.nsi"
!include "%%SOURCE%%\installers\windows\lang_pt-br.nsi"
!include "%%SOURCE%%\installers\windows\lang_ru.nsi"
!include "%%SOURCE%%\installers\windows\lang_tr.nsi"
!include "%%SOURCE%%\installers\windows\lang_zh.nsi"

# *TODO: Move these into the language files themselves
LangString LanguageCode ${LANG_DANISH}   "da"
LangString LanguageCode ${LANG_GERMAN}   "de"
LangString LanguageCode ${LANG_ENGLISH}  "en"
LangString LanguageCode ${LANG_SPANISH}  "es"
LangString LanguageCode ${LANG_FRENCH}   "fr"
LangString LanguageCode ${LANG_JAPANESE} "ja"
LangString LanguageCode ${LANG_ITALIAN}  "it"
LangString LanguageCode ${LANG_POLISH}   "pl"
LangString LanguageCode ${LANG_PORTUGUESEBR} "pt"
LangString LanguageCode ${LANG_RUSSIAN}  "ru"
LangString LanguageCode ${LANG_TURKISH}  "tr"
LangString LanguageCode ${LANG_TRADCHINESE}  "zh"

;; this placeholder is replaced by viewer_manifest.py
%%INST_VARS%%

Name ${INSTNAME}

SubCaption 0 $(LicenseSubTitleSetup)	; override "license agreement" text

BrandingText " "						; bottom of window text
Icon          %%SOURCE%%\installers\windows\install_icon.ico
UninstallIcon %%SOURCE%%\installers\windows\uninstall_icon.ico
WindowIcon on							; show our icon in left corner
BGGradient off							; no big background window
CRCCheck on								; make sure CRC is OK
InstProgressFlags smooth colored		; new colored smooth look
ShowInstDetails nevershow				; no details, no "show" button
SetOverwrite on							; stomp files by default
AutoCloseWindow true					; after all files install, close window

!ifdef WIN64_BIN_BUILD
InstallDir "$PROGRAMFILES64\${INSTNAME}"
!else
InstallDir "$PROGRAMFILES\${INSTNAME}"
!endif
;InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\${INSTNAME}" ""
DirText $(DirectoryChooseTitle) $(DirectoryChooseSetup)
Page directory dirPre
Page instfiles

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Variables
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Var INSTPROG
Var INSTEXE
Var INSTSHORTCUT
Var COMMANDLINE         ; command line passed to this installer, set in .onInit
Var SHORTCUT_LANG_PARAM ; "--set InstallLanguage de", passes language to viewer
Var SKIP_DIALOGS        ; set from command line in  .onInit. autoinstall 
                        ; GUI and the defaults.
Var DO_UNINSTALL_V2     ; If non-null, path to a previous Viewer 2 installation that will be uninstalled.

;;; Function definitions should go before file includes, because calls to
;;; DLLs like LangDLL trigger an implicit file include, so if that call is at
;;; the end of this script NSIS has to decompress the whole installer before 
;;; it can call the DLL function. JC

!include "FileFunc.nsh"     ; For GetParameters, GetOptions
!insertmacro GetParameters
!insertmacro GetOptions

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; After install completes, launch app
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInstSuccess
    Push $R0	# Option value, unused

    StrCmp $SKIP_DIALOGS "true" label_launch 

    ${GetOptions} $COMMANDLINE "/AUTOSTART" $R0
    # If parameter was there (no error) just launch
    # Otherwise ask
    IfErrors label_ask_launch label_launch
    
label_ask_launch:
    # Don't launch by default when silent
    IfSilent label_no_launch
	MessageBox MB_YESNO $(InstSuccesssQuestion) \
        IDYES label_launch IDNO label_no_launch
        
label_launch:
	# Assumes SetOutPath $INSTDIR
	Exec '"$INSTDIR\$INSTEXE" $SHORTCUT_LANG_PARAM'
label_no_launch:
	Pop $R0
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Pre-directory page callback
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function dirPre
    StrCmp $SKIP_DIALOGS "true" 0 +2
	Abort
FunctionEnd    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Make sure we're not on Windows 98 / ME
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckWindowsVersion
	DetailPrint "Checking Windows version..."
	Call GetWindowsVersion
	Pop $R0
	; Just get first two characters, ignore 4.0 part of "NT 4.0"
	StrCpy $R0 $R0 2
	; Blacklist certain OS versions
	StrCmp $R0 "95" win_ver_bad
	StrCmp $R0 "98" win_ver_bad
	StrCmp $R0 "ME" win_ver_bad
	StrCmp $R0 "NT" win_ver_bad
	Return
win_ver_bad:
	StrCmp $SKIP_DIALOGS "true" +2 ; If skip_dialogs is set just install
            MessageBox MB_YESNO $(CheckWindowsVersionMB) IDNO win_ver_abort
	Return
win_ver_abort:
	Quit
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Make sure the user can install/uninstall
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckIfAdministrator
    DetailPrint $(CheckAdministratorInstDP)
    UserInfo::GetAccountType
    Pop $R0
    StrCmp $R0 "Admin" lbl_is_admin
        MessageBox MB_OK $(CheckAdministratorInstMB)
        Quit
lbl_is_admin:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CheckIfAdministrator
    DetailPrint $(CheckAdministratorUnInstDP)
    UserInfo::GetAccountType
    Pop $R0
    StrCmp $R0 "Admin" lbl_is_admin
        MessageBox MB_OK $(CheckAdministratorUnInstMB)
        Quit
lbl_is_admin:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Checks to see if the current version has already been installed (according to the registry).
; If it has, allow user to bail out of install process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckIfAlreadyCurrent
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif
    Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG" "Version"
    StrCmp $0 ${VERSION_LONG} 0 continue_install
    StrCmp $SKIP_DIALOGS "true" continue_install
    MessageBox MB_OKCANCEL $(CheckIfCurrentMB) /SD IDOK IDOK continue_install
    Quit
continue_install:
    Pop $0
    Return
FunctionEnd
	
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Checks for CPU valid (must have SSE2 support)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckCPUFlags
    Call GetWindowsVersion
    Pop $R0
    StrCmp $R0 "2000" OK_SSE  ; sse check not available on win2k.

    Push $1
    System::Call 'kernel32::IsProcessorFeaturePresent(i) i(10) .r1'
    IntCmp $1 1 OK_SSE
    MessageBox MB_OKCANCEL $(MissingSSE2) /SD IDOK IDOK OK_SSE
    Quit

  OK_SSE:
    Pop $1
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of install process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CloseSecondLife
  Push $0
  FindWindow $0 "Alchemy" ""
  IntCmp $0 0 DONE
  
  StrCmp $SKIP_DIALOGS "true" CLOSE
    MessageBox MB_OKCANCEL $(CloseSecondLifeInstMB) IDOK CLOSE IDCANCEL CANCEL_INSTALL

  CANCEL_INSTALL:
    Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeInstDP)
    SendMessage $0 16 0 0

  LOOP:
	  FindWindow $0 "Alchemy" ""
	  IntCmp $0 0 DONE
	  Sleep 500
	  Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Test our connection to secondlife.com
; Also allows us to count attempted installs by examining web logs.
; *TODO: Return current SL version info and have installer check
; if it is up to date.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckNetworkConnection
    ; Uneeded - LD
    Return 
    Push $0
    Push $1
    Push $2	# Option value for GetOptions
    DetailPrint $(CheckNetworkConnectionDP)
    ; Look for a tag value from the stub installer, used for statistics
    ; to correlate installs.  Default to "" if not found on command line.
    StrCpy $2 ""
    ${GetOptions} $COMMANDLINE "/STUBTAG=" $2
    GetTempFileName $0
    !define HTTP_TIMEOUT 5000 ; milliseconds
    ; Don't show secondary progress bar, this will be quick.
    NSISdl::download_quiet \
        /TIMEOUT=${HTTP_TIMEOUT} \
        "http://install.secondlife.com/check/?stubtag=$2&version=${VERSION_LONG}" \
        $0
    Pop $1 ; Return value, either "success", "cancel" or an error message
    ; MessageBox MB_OK "Download result: $1"
    ; Result ignored for now
	; StrCmp $1 "success" +2
	;	DetailPrint "Connection failed: $1"
    Delete $0 ; temporary file
    Pop $2
    Pop $1
    Pop $0
    Return
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Function CheckWillUninstallV2               
;
; If we are being called through auto-update, we need to uninstall any
; existing V2 installation. Otherwise, we wind up with
; SecondLifeViewer2 and SecondLifeViewer installations existing side
; by side no indication which to use.
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckWillUninstallV2

  StrCpy $DO_UNINSTALL_V2 ""

  ; Uneeded - LD
  Return
  StrCmp $SKIP_DIALOGS "true" 0 CHECKV2_DONE
  StrCmp $INSTDIR "$PROGRAMFILES\${INSTNAME}2" CHECKV2_DONE ; don't uninstall our own install dir.
  IfFileExists "$PROGRAMFILES\${INSTNAME}2\uninst.exe" CHECKV2_FOUND CHECKV2_DONE

CHECKV2_FOUND:
  StrCpy $DO_UNINSTALL_V2 "true"

CHECKV2_DONE:

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Save user files to temp location
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function PreserveUserFiles
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif
Push $0
Push $1
Push $2

    RMDir /r "$TEMP\AlchemySettingsBackup"
    CreateDirectory "$TEMP\AlchemySettingsBackup"
    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    CreateDirectory "$TEMP\AlchemySettingsBackup\$0"
    CopyFiles /SILENT "$2\Application Data\Alchemy\*" "$TEMP\AlchemySettingsBackup\$0"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\Alchemy
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    CreateDirectory "$TEMP\AlchemySettingsBackup\AllUsers\"
    CopyFiles /SILENT "$2\Application Data\Alchemy\*" "$TEMP\AlchemySettingsBackup\AllUsers\"
Pop $0

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Restore user files from temp location
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RestoreUserFiles
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif

Push $0
Push $1
Push $2

    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    CreateDirectory "$2\Application Data\Alchemy\"
    CopyFiles /SILENT "$TEMP\AlchemySettingsBackup\$0\*" "$2\Application Data\Alchemy\" 

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\Alchemy
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    CreateDirectory "$2\Application Data\Alchemy\"
    CopyFiles /SILENT "$TEMP\AlchemySettingsBackup\AllUsers\*" "$2\Application Data\Alchemy\" 
Pop $0

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Remove temp dirs
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveTempUserFiles
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif

Push $0
Push $1
Push $2

    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    RMDir /r "$TEMP\AlchemySettingsBackup\$0\*"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\Alchemy
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    RMDir /r "$TEMP\AlchemySettingsBackup\AllUsers\*"
Pop $0

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Clobber user files - TEST ONLY
; This is here for testing, generally not desirable to call it.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Function ClobberUserFilesTESTONLY

;Push $0
;Push $1
;Push $2
;
;    StrCpy $0 0 ; Index number used to iterate via EnumRegKey
;
;  LOOP:
;    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
;    StrCmp $1 "" DONE               ; no more users
;
;    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
;    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing
;
;    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
;    ExpandEnvStrings $2 $2
;
;    RMDir /r "$2\Application Data\Alchemy\"
;
;  CONTINUE:
;    IntOp $0 $0 + 1
;    Goto LOOP
;  DONE:
;
;Pop $2
;Pop $1
;Pop $0
;
;; Copy files in Documents and Settings\All Users\Alchemy
;Push $0
;    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
;    StrCmp $0 "" +2
;    RMDir /r "$2\Application Data\Alchemy\"
;Pop $0
;
;FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed shader files
;;; Since shaders are in active development, we'll likely need to shuffle them
;;; around a bit from build to build.  This ensures that shaders that we move
;;; or rename in the dev tree don't get left behind in the install.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldShaders

;; Remove old shader files first so fallbacks will work. see DEV-5663
RMDir /r "$INSTDIR\app_settings\shaders\*"

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed XUI files
;;; We've changed the directory hierarchy for skins, putting all XUI and texture
;;; files under a specific skin directory, i.e. skins/default/xui/en-us as opposed
;;; to skins/xui/en-us.  Need to clean up the old path when upgrading
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldXUI

;; remove old XUI and texture files
RmDir /r "$INSTDIR\skins\html"
RmDir /r "$INSTDIR\skins\xui"
RmDir /r "$INSTDIR\skins\textures"
Delete "$INSTDIR\skins\*.txt"

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Remove any releasenotes files.
;;; We are no longer including release notes with the viewer. This will delete
;;; any that were left behind by an older installer. Delete will not fail if
;;; the files do not exist
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldReleaseNotes

;; remove releasenotes.txt file from application directory, and the shortcut
;; from the start menu.
Delete "$SMPROGRAMS\$INSTSHORTCUT\SL Release Notes.lnk"
Delete "$INSTDIR\releasenotes.txt"

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Delete files in Documents and Settings\<user>\Alchemy
; Delete files in Documents and Settings\All Users\Alchemy
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.DocumentsAndSettingsFolder
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif

; Delete files in Documents and Settings\<user>\Alchemy
Push $0
Push $1
Push $2

  DetailPrint "Deleting files in Documents and Settings folder"

  StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

        ; Remove all cache and settings files but leave any other .txt files to preserve the chat logs
;    RMDir /r "$2\Application Data\Alchemy\logs"
    RMDir /r "$2\Application Data\Alchemy\browser_profile"
    RMDir /r "$2\Application Data\Alchemy\user_settings"
    Delete  "$2\Application Data\Alchemy\*.xml"
    Delete  "$2\Application Data\Alchemy\*.bmp"
    Delete  "$2\Application Data\Alchemy\search_history.txt"
    Delete  "$2\Application Data\Alchemy\plugin_cookies.txt"
    Delete  "$2\Application Data\Alchemy\typed_locations.txt"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Delete files in Documents and Settings\All Users\Alchemy
Push $0
  ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
  StrCmp $0 "" +2
  RMDir /r "$0\Alchemy"
Pop $0

; Delete files in C:\Windows\Application Data\Alchemy
; If the user is running on a pre-NT system, Application Data lives here instead of
; in Documents and Settings.
RMDir /r "$WINDIR\Application Data\Alchemy"

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of uninstall process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CloseSecondLife
  Push $0
  FindWindow $0 "Alchemy" ""
  IntCmp $0 0 DONE
  MessageBox MB_OKCANCEL $(CloseSecondLifeUnInstMB) IDOK CLOSE IDCANCEL CANCEL_UNINSTALL

  CANCEL_UNINSTALL:
    Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeUnInstDP)
    SendMessage $0 16 0 0

  LOOP:
	  FindWindow $0 "Alchemy" ""
	  IntCmp $0 0 DONE
	  Sleep 500
	  Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   Delete the stored password for the current Windows user
;   DEV-10821 -- Unauthorised user can gain access to an SL account after a real user has uninstalled
;
Function un.RemovePassword

DetailPrint "Removing Alchemy password"

SetShellVarContext current
Delete "$APPDATA\Alchemy\user_settings\password.dat"
SetShellVarContext all

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed files
;;; This deletes the uninstall executable, but it works 
;;; because it is copied to temp directory before running
;;;
;;; Note:  You must list all files here, because we only
;;; want to delete our files, not things users left in the
;;; application directories.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.ProgramFiles

;; Remove mozilla file first so recursive directory deletion doesn't get hung up
Delete "$INSTDIR\app_settings\mozilla\components"

;; This placeholder is replaced by the complete list of files to uninstall by viewer_manifest.py
%%DELETE_FILES%%

;; Optional/obsolete files.  Delete won't fail if they don't exist.
Delete "$INSTDIR\dronesettings.ini"
Delete "$INSTDIR\message_template.msg"
Delete "$INSTDIR\newview.pdb"
Delete "$INSTDIR\newview.map"
Delete "$INSTDIR\Alchemy.pdb"
Delete "$INSTDIR\Alchemy.map"
Delete "$INSTDIR\comm.dat"
Delete "$INSTDIR\*.glsl"
Delete "$INSTDIR\motions\*.lla"
Delete "$INSTDIR\trial\*.html"
Delete "$INSTDIR\newview.exe"
;; Remove entire help directory
Delete "$INSTDIR\help\Advanced\*"
RMDir  "$INSTDIR\help\Advanced"
Delete "$INSTDIR\help\basics\*"
RMDir  "$INSTDIR\help\basics"
Delete "$INSTDIR\help\Concepts\*"
RMDir  "$INSTDIR\help\Concepts"
Delete "$INSTDIR\help\welcome\*"
RMDir  "$INSTDIR\help\welcome"
Delete "$INSTDIR\help\*"
RMDir  "$INSTDIR\help"

Delete "$INSTDIR\uninst.exe"
RMDir "$INSTDIR"

IfFileExists "$INSTDIR" FOLDERFOUND NOFOLDER

FOLDERFOUND:
  ; Silent uninstall always removes all files (/SD IDYES)
  MessageBox MB_YESNO $(DeleteProgramFilesMB) /SD IDYES IDNO NOFOLDER
  RMDir /r "$INSTDIR"

NOFOLDER:

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Uninstall settings
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
UninstallText $(UninstallTextMsg)
ShowUninstDetails show

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Uninstall section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Section Uninstall
!ifdef WIN64_BIN_BUILD
SetRegView 64
!endif

; Start with some default values.
StrCpy $INSTPROG "${INSTNAME}"
StrCpy $INSTEXE "${INSTEXE}"
StrCpy $INSTSHORTCUT "${SHORTCUT}"
Call un.CheckIfAdministrator		; Make sure the user can install/uninstall

; uninstall for all users (if you change this, change it in the install as well)
SetShellVarContext all			

; Make sure we're not running
Call un.CloseSecondLife

; Clean up registry keys and subkeys (these should all be !defines somewhere)
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG"

; Clean up shortcuts
Delete "$SMPROGRAMS\$INSTSHORTCUT\*.*"
RMDir  "$SMPROGRAMS\$INSTSHORTCUT"

Delete "$DESKTOP\$INSTSHORTCUT.lnk"
Delete "$INSTDIR\$INSTSHORTCUT.lnk"
Delete "$INSTDIR\Uninstall $INSTSHORTCUT.lnk"

; Clean up cache and log files.
; Leave them in-place for non AGNI installs.

!ifdef UNINSTALL_SETTINGS
Call un.DocumentsAndSettingsFolder
!endif

; remove stored password on uninstall
Call un.RemovePassword

Call un.ProgramFiles

SectionEnd 				; end of uninstall section


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; (From the NSIS documentation, JC)
; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;   ; at this point $R0 is "NT 4.0" or whatnot
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function GetWindowsVersion
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif
   Push $R0
   Push $R1
 
   ReadRegStr $R0 HKLM \
   "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

   IfErrors 0 lbl_winnt
   
   ; we are not NT
   ReadRegStr $R0 HKLM \
   "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber
 
   StrCpy $R1 $R0 1
   StrCmp $R1 '4' 0 lbl_error
 
   StrCpy $R1 $R0 3
 
   StrCmp $R1 '4.0' lbl_win32_95
   StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98
 
   lbl_win32_95:
     StrCpy $R0 '95'
   Goto lbl_done
 
   lbl_win32_98:
     StrCpy $R0 '98'
   Goto lbl_done
 
   lbl_win32_ME:
     StrCpy $R0 'ME'
   Goto lbl_done
 
   lbl_winnt:
 
   StrCpy $R1 $R0 1
 
   StrCmp $R1 '3' lbl_winnt_x
   StrCmp $R1 '4' lbl_winnt_x
 
   StrCpy $R1 $R0 3
 
   StrCmp $R1 '5.0' lbl_winnt_2000
   StrCmp $R1 '5.1' lbl_winnt_XP
   StrCmp $R1 '5.2' lbl_winnt_2003 lbl_error
 
   lbl_winnt_x:
     StrCpy $R0 "NT $R0" 6
   Goto lbl_done
 
   lbl_winnt_2000:
     Strcpy $R0 '2000'
   Goto lbl_done
 
   lbl_winnt_XP:
     Strcpy $R0 'XP'
   Goto lbl_done
 
   lbl_winnt_2003:
     Strcpy $R0 '2003'
   Goto lbl_done
 
   lbl_error:
     Strcpy $R0 ''
   lbl_done:
 
   Pop $R1
   Exch $R0
 
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Note: to add new languages, add a language file include to the list 
;;  at the top of this file, add an entry to the menu and then add an 
;;  entry to the language ID selector below
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInit
!ifdef WIN64_BIN_BUILD
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "This version requires a 64 bit operating system."
        Quit
    ${EndIf}
    SetRegView 64
!endif
    Push $0
    ${GetParameters} $COMMANDLINE              ; get our command line

    ${GetOptions} $COMMANDLINE "/SKIP_DIALOGS" $0   
    IfErrors +2 0 ; If error jump past setting SKIP_DIALOGS
        StrCpy $SKIP_DIALOGS "true"

    ${GetOptions} $COMMANDLINE "/LANGID=" $0   ; /LANGID=1033 implies US English
    ; If no language (error), then proceed
    IfErrors lbl_configure_default_lang
    ; No error means we got a language, so use it
    StrCpy $LANGUAGE $0
    Goto lbl_return

lbl_configure_default_lang:
    ; If we currently have a version of SL installed, default to the language of that install
    ; Otherwise don't change $LANGUAGE and it will default to the OS UI language.
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\${INSTNAME}" "InstallerLanguage"
    IfErrors +2 0 ; If error skip the copy instruction 
	StrCpy $LANGUAGE $0

    ; For silent installs, no language prompt, use default
    IfSilent lbl_return
    StrCmp $SKIP_DIALOGS "true" lbl_return
  
lbl_build_menu:
	Push ""
    # Use separate file so labels can be UTF-16 but we can still merge changes
    # into this ASCII file. JC
    !include "%%SOURCE%%\installers\windows\language_menu.nsi"
    
	Push A ; A means auto count languages for the auto count to work the first empty push (Push "") must remain
	LangDLL::LangDialog $(InstallerLanguageTitle) $(SelectInstallerLanguage)
	Pop $0
	StrCmp $0 "cancel" 0 +2
		Abort
    StrCpy $LANGUAGE $0

	; save language in registry		
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\${INSTNAME}" "InstallerLanguage" $LANGUAGE
lbl_return:
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.onInit
!ifdef WIN64_BIN_BUILD
    SetRegView 64
!endif
	; read language from registry and set for uninstaller
    ; Key will be removed on successful uninstall
	ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\${INSTNAME}" "InstallerLanguage"
    IfErrors lbl_end
	StrCpy $LANGUAGE $0
lbl_end:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; MAIN SECTION
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Section ""						; (default section)
!ifdef WIN64_BIN_BUILD
SetRegView 64
!endif
SetShellVarContext all			; install for all users (if you change this, change it in the uninstall as well)

; Start with some default values.
StrCpy $INSTPROG "${INSTNAME}"
StrCpy $INSTEXE "${INSTEXE}"
StrCpy $INSTSHORTCUT "${SHORTCUT}"

Call CheckWindowsVersion		; warn if on Windows 98/ME
Call CheckCPUFlags			; Make sure we have SSE2 support
Call CheckIfAdministrator		; Make sure the user can install/uninstall
Call CheckIfAlreadyCurrent		; Make sure that we haven't already installed this version
Call CloseSecondLife			; Make sure we're not running
Call CheckNetworkConnection		; ping secondlife.com
Call CheckWillUninstallV2               ; See if a V2 install exists and will be removed.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
StrCmp $DO_UNINSTALL_V2 "" PRESERVE_DONE
  Call PreserveUserFiles
PRESERVE_DONE:

;;; Don't remove cache files during a regular install, removing the inventory cache on upgrades results in lots of damage to the servers.
;Call RemoveCacheFiles			; Installing over removes potentially corrupted
								; VFS and cache files.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Need to clean out shader files from previous installs to fix DEV-5663
Call RemoveOldShaders

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Need to clean out old XUI files that predate skinning
Call RemoveOldXUI

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Clear out old releasenotes.txt files. These are now on the public wiki.
Call RemoveOldReleaseNotes

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Files
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This placeholder is replaced by the complete list of all the files in the installer, by viewer_manifest.py
%%INSTALL_FILES%%

# Pass the installer's language to the client to use as a default
StrCpy $SHORTCUT_LANG_PARAM "--set InstallLanguage $(LanguageCode)"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Shortcuts in start menu
CreateDirectory	"$SMPROGRAMS\$INSTSHORTCUT"
SetOutPath "$INSTDIR"
CreateShortCut	"$SMPROGRAMS\$INSTSHORTCUT\$INSTSHORTCUT.lnk" \
				"$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"


WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\SL Create Account.url" \
				"InternetShortcut" "URL" \
				"http://join.secondlife.com/"
WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\SL Your Account.url" \
				"InternetShortcut" "URL" \
				"http://www.secondlife.com/account/"
WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\SL Scripting Language Help.url" \
				"InternetShortcut" "URL" \
                "http://wiki.secondlife.com/wiki/LSL_Portal"
CreateShortCut	"$SMPROGRAMS\$INSTSHORTCUT\Uninstall $INSTSHORTCUT.lnk" \
				'"$INSTDIR\uninst.exe"' ''

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Other shortcuts
SetOutPath "$INSTDIR"
CreateShortCut "$DESKTOP\$INSTSHORTCUT.lnk" \
        "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
CreateShortCut "$INSTDIR\$INSTSHORTCUT.lnk" \
        "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
CreateShortCut "$INSTDIR\Uninstall $INSTSHORTCUT.lnk" \
				'"$INSTDIR\uninst.exe"' ''


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write registry
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG" "" "$INSTDIR"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG" "Version" "${VERSION_LONG}"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG" "Shortcut" "$INSTSHORTCUT"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Alchemy Viewer Project\$INSTPROG" "Exe" "$INSTEXE"
!ifdef WIN64_BIN_BUILD
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayName" "$INSTPROG x64 (remove only)"
!else
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayName" "$INSTPROG (remove only)"
!endif
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "Publisher" "Alchemy Viewer Project"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "URLInfoAbout" "http://www.alchemyviewer.org"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "URLUpdateInfo" "http://www.alchemyviewer.org/p/downloads.html"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayIcon" '"$INSTDIR\$INSTEXE"'
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayVersion" "${VERSION_LONG}"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "UninstallString" '"$INSTDIR\uninst.exe"'

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write URL registry info
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "(default)" "URL:Second Life"
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "URL Protocol" ""
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\DefaultIcon" "" '"$INSTDIR\$INSTEXE"'
;; URL param must be last item passed to viewer, it ignores subsequent params
;; to avoid parameter injection attacks.
WriteRegExpandStr HKEY_CLASSES_ROOT "${URLNAME}\shell\open\command" "" '"$INSTDIR\$INSTEXE" -url "%1"'
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info"(default)" "URL:Second Life"
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info" "URL Protocol" ""
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\DefaultIcon" "" '"$INSTDIR\$INSTEXE"'
;; URL param must be last item passed to viewer, it ignores subsequent params
;; to avoid parameter injection attacks.
WriteRegExpandStr HKEY_CLASSES_ROOT "x-grid-location-info\shell\open\command" "" '"$INSTDIR\$INSTEXE" -url "%1"'

; write out uninstaller
WriteUninstaller "$INSTDIR\uninst.exe"

; Uninstall existing "Second Life Viewer 2" install if needed.
StrCmp $DO_UNINSTALL_V2 "" REMOVE_SLV2_DONE
  ExecWait '"$PROGRAMFILES\${INSTNAME}2\uninst.exe" /S _?=$PROGRAMFILES\${INSTNAME}2'
  Delete "$PROGRAMFILES\${INSTNAME}2\uninst.exe" ; with _? option above, uninst.exe will be left behind.
  RMDir "$PROGRAMFILES\${INSTNAME}2" ; will remove only if empty.

  Call RestoreUserFiles
  Call RemoveTempUserFiles
REMOVE_SLV2_DONE:

; end of default section
SectionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; EOF  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
