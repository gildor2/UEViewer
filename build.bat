@echo off
setlocal enabledelayedexpansion

:: 1. SET PATHS
set "SRC=umodel_64.exe"
set "DST=C:\p4v\laurynas_RocketLeague\Plugins\ProjectPlugins\MapImporter\Source\MapImporter\UModel\umodel_64.exe"
set "PATH=C:\Users\lauri\Documents\Github\UEViewer\BuildTools\bin;%PATH%"
set "VERSION_FILE=UmodelTool\Version.h"

:: 2. Increment version (Ultra-Safe No-Script Version)
echo Incrementing version...
for /f "tokens=3" %%A in ('type "%VERSION_FILE%"') do set /a "VER=%%A+1"
echo #define GIT_REVISION %VER% > "%VERSION_FILE%"

:: Verify visually
type "%VERSION_FILE%"


:: 3. BUILD
bash build.sh --64
if %ERRORLEVEL% NEQ 0 (
    echo Build failed, aborting.
    exit /b 1
)

:: 4. VERIFY SRC AND COPY
:: Using quotes around everything to prevent ". was unexpected"
if not exist "%SRC%" (
    echo ERROR: Compiled file "%SRC%" not found!
    exit /b 1
)

if exist "%DST%" (
    echo Deleting old destination...
    del /F /Q "%DST%"
)

echo Copying "%SRC%" to "%DST%"...
copy /Y "%SRC%" "%DST%" >nul

if %ERRORLEVEL% EQU 0 (
    echo Copy succeeded.
) else (
    echo ERROR: Copy failed. Check if the file is locked by another program.
    exit /b 1
)

endlocal
exit /b 0
