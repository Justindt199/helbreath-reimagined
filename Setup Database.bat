@echo off
rem One-time: rebuilds the empty game database from the new schema and creates the game's own DB account.
cd /d "%~dp0"
set MYSQLEXE=
for /d %%d in ("%ProgramFiles%\MariaDB*") do if exist "%%d\bin\mysql.exe" set MYSQLEXE=%%d\bin\mysql.exe
echo Enter your MariaDB root password (this is the last time it is needed):
"%MYSQLEXE%" -u root -p --default-character-set=utf8 "--init-command=SET SESSION sql_mode=''" < Build\db_reset.sql 2> Build\db_error.txt
if errorlevel 1 (echo FAILED> Build\db_status.txt& type Build\db_error.txt) else (echo OK2> Build\db_status.txt& echo Database ready.)
pause
