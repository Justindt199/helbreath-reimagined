HELBREATH REIMAGINED
====================

TO PLAY
  1. Start Servers.bat     starts the login server and game server (no password needed)
  2. Client\Client.exe     sign in or create an account, create a character, play
  Stop Servers.bat         stops both servers (and any running client)
  Alt+Enter in the client  switches between fullscreen and windowed

  A test account made during setup: see Build\test_account.txt

FOLDERS
  Source\     C++ source (from the lmunaco/Helbreath-Unrivaled repo, Nov 13 version), level cap 140
  Server\     login server + game server (configs, maps, exes)
  Client\     game client and its data
  deps\       SFML 3.0.2 and the MariaDB connector used to compile
  Build\      build logs and temporary compiler files
  Source_old_github\, Server_old\   the older Helbreath-Unrivaled-main version (not used)

REBUILDING AFTER CODE CHANGES
  Stop Servers.bat, then Build All.bat (results in Build\build_status.txt), then Start Servers.bat

DATABASE
  MariaDB 11.8 (Windows service "MariaDB"), database server_db.
  The game logs in as its own user 'helbreath' (set in Server\Login Server\LServer.cfg).
