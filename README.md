# Helbreath Reimagined

Justin's Helbreath client, login server and game server, built on the Helbreath Unrivaled source, with the max level set to 115. VALORIA's items and skills (forging, crafting, alchemy, enchanting, passive tree, empowering) are being added on top.

| Folder | What it is |
|---|---|
| `Source/` | C++ source: `Client3` (client), `HG Server` (game server), `Login Server`, `Shared` |
| `Server/` | Runtime configs for both servers (maps, item/NPC/magic configs, drop list) |
| `Client/` | The client's small data files (CONTENTS, fonts). Sprites, sounds and music are not in the repo |
| `Database/` | Database setup template |
| `*.bat` | Build, install, start/stop and database tools for Windows |

## Setup on a new PC

1. `Install Prerequisites.bat`: Visual Studio 2022 Build Tools (C++) and MariaDB.
2. Put SFML 3.0.2 (VS2022, x86) in `deps/SFML-3.0.2` and the MariaDB Connector/C 3.4.11 source in `deps/mariadb-connector-c-3.4.11`.
3. In `Database/db_reset.template.sql` and `Server/Login Server/LServer.cfg`, replace `CHANGE_ME` with the same new password, save the SQL as `Build/db_reset.sql`, then run `Setup Database.bat`.
4. `Build All.bat`, then `Start Servers.bat`, then `Client/Client.exe`.

The client's sprites, sounds, music and maps come from the full game folder, not this repo.
