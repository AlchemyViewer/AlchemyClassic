start /WAIT %1 /SKIP_DIALOGS /AUTOSTART %4 %5 %6
IF ERRORLEVEL 1 ECHO %3 > %2
DEL %1