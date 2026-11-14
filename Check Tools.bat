@echo off
cd /d "%~dp0"
if not exist Build mkdir Build
set O=Build\toolcheck.txt
echo TOOLCHECK %DATE% %TIME%> %O%
echo --- VS folders>> %O%
dir /b "%ProgramFiles%\Microsoft Visual Studio" >> %O% 2>&1
dir /b "%ProgramFiles(x86)%\Microsoft Visual Studio" >> %O% 2>&1
dir /b "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer" >> %O% 2>&1
echo --- MSBuild search>> %O%
where /r "%ProgramFiles%\Microsoft Visual Studio" MSBuild.exe >> %O% 2>&1
where /r "%ProgramFiles(x86)%\Microsoft Visual Studio" MSBuild.exe >> %O% 2>&1
where /r "%ProgramFiles(x86)%\MSBuild" MSBuild.exe >> %O% 2>&1
echo --- cl search>> %O%
where /r "%ProgramFiles%\Microsoft Visual Studio" cl.exe >> %O% 2>&1
where /r "%ProgramFiles(x86)%\Microsoft Visual Studio" cl.exe >> %O% 2>&1
echo --- VS Code / other>> %O%
where code >> %O% 2>&1
where cl msbuild g++ gcc clang cmake mysql >> %O% 2>&1
dir /b "%ProgramFiles%\MySQL" "%ProgramFiles%\MariaDB*" "%ProgramFiles(x86)%\MariaDB" >> %O% 2>&1
echo --- vswhere>> %O%
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -all -products * -format text >> %O% 2>&1
echo --- mariadb service>> %O%
sc query MariaDB >> %O% 2>&1
dir /b "%ProgramFiles%\MariaDB 11.8\bin\mysql.exe" >> %O% 2>&1
echo TOOLCHECK DONE>> %O%
