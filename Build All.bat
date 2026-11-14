@echo off
setlocal EnableDelayedExpansion
rem Builds the login server, game server and client (Release, 32-bit) with Visual Studio 2022.
rem Results: Build\build_status.txt (summary) and Build\<project>.log (full compiler output).
cd /d "%~dp0"
if not exist Build mkdir Build
set STATUS=Build\build_status.txt
echo BUILD STARTED %DATE% %TIME%> "%STATUS%"

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
  echo ERROR: vswhere.exe not found - is Visual Studio installed?>> "%STATUS%"
  goto :end
)
set MSBUILD=
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
if "%MSBUILD%"=="" (
  echo ERROR: MSBuild.exe not found - install the "Desktop development with C++" workload.>> "%STATUS%"
  goto :end
)
echo MSBuild: %MSBUILD%>> "%STATUS%"
"%VSWHERE%" -latest -products * -property catalog_productDisplayVersion >> "%STATUS%"
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > Build\vctools.txt
for %%A in (Build\vctools.txt) do if %%~zA==0 echo WARNING: C++ build tools (MSVC x86/x64) do not appear to be installed.>> "%STATUS%"

call :build HGserver "Source\HG Server\HGserver.vcxproj"
call :build Client "Source\Client3\Client.vcxproj"

rem --- MariaDB Connector/C (MySQL client library), built from source as 32-bit
set MYSQL_CONNECTOR_DIR=%~dp0deps\mariadb-connector
if not exist "%MYSQL_CONNECTOR_DIR%\lib\libmariadb.lib" (
  echo Building MariaDB Connector/C ...
  cmake -S deps\mariadb-connector-c-3.4.11 -B Build\mcc -G "Visual Studio 17 2022" -A Win32 -DWITH_UNIT_TESTS=OFF -DWITH_CURL=OFF -DWITH_SSL=SCHANNEL -DWITH_DOCS=OFF > Build\Connector.log 2>&1
  cmake --build Build\mcc --config Release --target libmariadb >> Build\Connector.log 2>&1
  if exist Build\mcc\libmariadb\Release\libmariadb.lib (
    mkdir "%MYSQL_CONNECTOR_DIR%\include" "%MYSQL_CONNECTOR_DIR%\lib" 2>nul
    xcopy /y /e /q deps\mariadb-connector-c-3.4.11\include "%MYSQL_CONNECTOR_DIR%\include\" >nul
    xcopy /y /e /q Build\mcc\include "%MYSQL_CONNECTOR_DIR%\include\" >nul
    copy /y Build\mcc\libmariadb\Release\libmariadb.lib "%MYSQL_CONNECTOR_DIR%\lib\" >nul
    copy /y Build\mcc\libmariadb\Release\libmariadb.dll "%MYSQL_CONNECTOR_DIR%\lib\" >nul
    echo Connector: OK>> "%STATUS%"
  ) else (
    echo Connector: FAILED - see Build\Connector.log>> "%STATUS%"
  )
)
if exist "%MYSQL_CONNECTOR_DIR%\include\mysql.h" (
  call :build LoginServer "Source\Login Server\LoginServer.vcxproj"
) else (
  echo LoginServer: SKIPPED - connector not built>> "%STATUS%"
)
goto :end

:build
echo Building %~1 ...
"%MSBUILD%" %2 -nologo -m -v:m -p:Configuration=Release -p:Platform=Win32 -flp:LogFile=Build\%~1.log;Verbosity=minimal;Encoding=UTF-8
if errorlevel 1 (echo %~1: FAILED>> "%STATUS%") else (echo %~1: OK>> "%STATUS%")
exit /b 0

:end
echo BUILD FINISHED %DATE% %TIME%>> "%STATUS%"
type "%STATUS%"
echo.
echo This window can be closed.
pause
