@echo off
cd /d "%~dp0Server\Login Server"
start "" LoginServer.exe
timeout /t 5 >nul
cd /d "%~dp0Server\HG Server"
start "" HGserver.exe
