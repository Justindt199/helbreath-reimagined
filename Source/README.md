# Helbreath Unrivaled

Source code for the Helbreath Unrivaled client and servers.

| Folder | What it is |
|---|---|
| `Client3/` | Game client (software RGB565 renderer presented through SFML, optional 2x HD sprite mirror) |
| `HG Server/` | Game server |
| `Login Server/` | Login server (MySQL) |
| `Shared/` | Headers shared between client and servers (net messages, items, maps, etc.) |
| `LegionLauncher/` | Launcher / updater |
| `Pak Editor/` | C# tool for editing `.pak` sprite archives |
| `AnMap/`, `CRCGenerate/` | Map and checksum utilities |
| `Database Setup.sql` | Database schema |
| `server configs/` | Server runtime data: `HG Server` (GServer.cfg, MAPDATA maps, GameData), `Login Server` (LServer.cfg, item/NPC/magic/quest configs), drop lists |

`All Build.sln` builds the main projects in Visual Studio.

Client game data (sprites, sounds, client maps) is not in this repository. Server build output, executables, DLLs and logs are also left out.
