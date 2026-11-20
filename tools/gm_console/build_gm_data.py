#!/usr/bin/env python3
"""Writes Source/Client3/UI/GmData.h: the GM console's monster list (from Npc.cfg) and map list
(from the game server's GServer.cfg). Run again after changing either.

    python3 tools/gm_console/build_gm_data.py
"""
import os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
BOSSES = {'Hellclaw', 'Tigerworm', 'Wyvern', 'Fire-Wyvern', 'Abaddon'}
# friendly names for the maps that have one; the rest show their map name
MAP_NAMES = {
    'aresden': 'Aresden', 'elvine': 'Elvine', 'middleland': 'Middleland', '2ndmiddle': '2nd Middleland',
    'default': 'Traveller island', 'default2': 'Traveller island 2', 'bisle': 'Bleeding Isle',
    'huntzone1': 'Hunt zone 1', 'huntzone2': 'Hunt zone 2', 'huntzone3': 'Hunt zone 3', 'huntzone4': 'Hunt zone 4',
    'arefarm': 'Aresden farm', 'elvfarm': 'Elvine farm', 'aresdend1': 'Aresden dungeon', 'elvined1': 'Elvine dungeon',
    'middled1n': 'Middleland dungeon (N)', 'middled1x': 'Middleland dungeon (X)', 'dglv2': 'Dungeon 2', 'dglv3': 'Dungeon 3',
    'dglv4': 'Dungeon 4', 'toh1': 'Tower of Hell 1', 'toh2': 'Tower of Hell 2', 'toh3': 'Tower of Hell 3',
    'inferniaA': 'Infernia A', 'inferniaB': 'Infernia B', 'maze': 'Maze', 'GodH': "God's Hall", 'HRampart': 'Heldenian rampart',
    'BtField': 'Battlefield', 'astoria': 'Astoria', 'areuni': 'Aresden university', 'elvuni': 'Elvine university',
    'cityhall_1': 'Aresden city hall', 'cityhall_2': 'Elvine city hall', 'gshop_1': 'Aresden shop', 'gshop_2': 'Elvine shop',
    'bsmith_1': 'Aresden blacksmith', 'bsmith_2': 'Elvine blacksmith', 'wzdtwr_1': 'Aresden magic tower', 'wzdtwr_2': 'Elvine magic tower',
    'cath_1': 'Aresden church', 'cath_2': 'Elvine church', 'wrhus_1': 'Aresden warehouse', 'wrhus_2': 'Elvine warehouse',
    'gldhall_1': 'Aresden guild hall', 'gldhall_2': 'Elvine guild hall', 'arejail': 'Aresden jail', 'elvjail': 'Elvine jail',
}
FIRST = ['aresden', 'elvine', 'middleland', '2ndmiddle', 'default', 'huntzone1', 'huntzone2', 'huntzone3', 'huntzone4',
         'aresdend1', 'elvined1', 'middled1n', 'middled1x', 'dglv2', 'dglv3', 'dglv4', 'toh1', 'toh2', 'toh3', 'inferniaA', 'inferniaB', 'maze']

def c(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

mons = []
for line in open(os.path.join(ROOT, 'Server', 'Login Server', 'Config', 'Npc.cfg'), encoding='latin-1'):
    t = line.split()
    if len(t) > 13 and t[0] == 'Npc' and t[1] == '=':
        name, hd, xp, side = t[2], int(t[4]), int(t[8]), int(t[12])
        if hd <= 0 or side in (1, 2): continue          # town guards and soldiers are left out
        mons.append((name, hd, xp, name in BOSSES))
mons.sort(key=lambda m: (m[1], m[0]))

maps = []
for line in open(os.path.join(ROOT, 'Server', 'HG Server', 'GServer.cfg'), encoding='latin-1'):
    m = re.match(r'\s*game-server-map\s*=\s*(\S+)', line)
    if m: maps.append(m.group(1))
maps = [m for m in FIRST if m in maps] + sorted((m for m in maps if m not in FIRST), key=str.lower)

out = ['// GmData.h: the GM console\'s lists. Written by tools/gm_console/build_gm_data.py - do not edit.', '#pragma once', '',
       'namespace GmData {', '', 'struct Monster { const char * name; int hp, xp; bool boss; };',
       'static const Monster kMonsters[] = {']
out += ['\t{ %s, %d, %d, %s },' % (c(n), hd, xp, 'true' if b else 'false') for n, hd, xp, b in mons]
out += ['};', 'const int MONSTER_COUNT = (int)(sizeof(kMonsters) / sizeof(kMonsters[0]));', '',
        'struct Map { const char * id, * name; };', 'static const Map kMaps[] = {']
out += ['\t{ %s, %s },' % (c(m), c(MAP_NAMES.get(m, m))) for m in maps]
out += ['};', 'const int MAP_COUNT = (int)(sizeof(kMaps) / sizeof(kMaps[0]));', '', '} // namespace GmData', '']
p = os.path.join(ROOT, 'Source', 'Client3', 'UI', 'GmData.h')
open(p, 'w', newline='\r\n').write('\n'.join(out))
print('wrote', p, len(mons), 'monsters,', len(maps), 'maps')
