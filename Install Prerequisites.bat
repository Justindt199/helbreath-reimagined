@echo off
rem Installs the C++ compiler (Visual Studio 2022 Build Tools) and MariaDB Server.
cd /d "%~dp0"
if not exist Build mkdir Build
set LOG=Build\install_status.txt
echo INSTALL STARTED %DATE% %TIME%> %LOG%

echo.
echo === Step 1 of 2: Visual Studio 2022 Build Tools (C++) ===
echo Approve the Windows prompt. This downloads about 2-3 GB and can take 10-30 minutes.
"%USERPROFILE%\Downloads\vs_BuildTools.exe" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart --wait
echo BuildTools exit code %ERRORLEVEL%>> %LOG%

echo.
echo === Step 2 of 2: MariaDB Server ===
echo In the MariaDB setup: keep the defaults, tick "Modify password for database user 'root'",
echo choose a root password you will remember, and keep "Install as service" checked.
msiexec /i "%USERPROFILE%\Downloads\mariadb-11.8.9-winx64.msi"
echo MariaDB exit code %ERRORLEVEL%>> %LOG%

echo INSTALL FINISHED %DATE% %TIME%>> %LOG%
type %LOG%
echo.
echo Done. This window can be closed.
pause
