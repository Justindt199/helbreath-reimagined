@echo off
rem Stops the Helbreath login server, game server and any running client.
taskkill /f /im HGserver.exe >nul 2>&1
taskkill /f /im LoginServer.exe >nul 2>&1
taskkill /f /im Client.exe >nul 2>&1
taskkill /f /im Client_Unrivaled.exe >nul 2>&1
echo stopped> "%~dp0Build\stop_status.txt"
