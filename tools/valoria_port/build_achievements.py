#!/usr/bin/env python3
"""Ports VALORIA's achievements into Helbreath Reimagined.

Run from the repo root:  python3 tools/valoria_port/build_achievements.py

Reads   tools/valoria_port/valoria_data/achievements.json   (VALORIA's 709 achievements)
Writes  Source/Shared/ValoriaAchieveData.h                  (compiled into the client and the game server)

The conversion rules (the same for every achievement):
  * Each one is a counter reaching a number, exactly as in VALORIA: kills of one monster, total kills,
    level, enemy players defeated, quests done, side quests done. Its name and title are VALORIA's.
  * Monster kills count Helbreath's monster of the same kind (by its Npc.cfg name, MONSTERS below).
    VALORIA monsters Helbreath doesn't have are left out (UNMATCHED below).
  * Helbreath monsters VALORIA doesn't have (EXTRA below) get the same 13 tiers and title pattern.
  * VALORIA's story quests are Helbreath's City Hall quests ("Complete N quests").
  * Kill counters live in fixed slots (the order of MONSTERS then EXTRA): only ever add to the end of
    those lists, or saved kill counts move to another monster.
"""
import json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'tools', 'valoria_port', 'valoria_data', 'achievements.json')
OUT = os.path.join(ROOT, 'Source', 'Shared', 'ValoriaAchieveData.h')

# VALORIA target -> Helbreath Npc.cfg name
MONSTERS = {
    'slime': 'Slime', 'rabbit': 'Rabbit', 'cat': 'Cat', 'giant_ant': 'Giant-Ant', 'giant_frog': 'Giant-Frog',
    'orc': 'Orc', 'scorpion': 'Scorpion', 'zombie': 'Zombie', 'skeleton': 'Skeleton', 'orc_mage': 'Orc-Mage',
    'clay_golem': 'Clay-Golem', 'giant_lizard': 'Giant-Lizard', 'giant_plant': 'Giant-Plant',
    'cannibal_plant': 'Cannibal-Plant', 'giant_crayfish': 'Giant-Crayfish', 'stone_golem': 'Stone-Golem',
    'hellbound': 'Hellbound', 'troll': 'Troll', 'ogre': 'Orge', 'cyclops': 'Cyclops', 'dire_boar': 'DireBoar',
    'werewolf': 'WereWolf', 'mastermage_orc': 'MasterMage-Orc', 'beholder': 'Beholder', 'dark_elf': 'Dark-Elf',
    'ettin': 'Ettin', 'minotaur': 'Minotaurs', 'unicorn': 'Unicorn', 'stalker': 'Stalker', 'frost': 'Frost',
    'ice_golem': 'Ice-Golem', 'claw_turtle': 'Claw-Turtle', 'centaurus': 'Centaurus', 'tentocle': 'Tentocle',
    'mountain_giant': 'Mountain-Giant', 'liche': 'Liche', 'gargoyle': 'Gagoyle', 'demon': 'Demon', 'nizie': 'Nizie',
    'hellclaw': 'Hellclaw', 'tigerworm': 'Tigerworm', 'wyvern': 'Wyvern', 'fire_wyvern': 'Fire-Wyvern',
    'barlog': 'Barlog', 'abaddon': 'Abaddon',
}
UNMATCHED = {'giant_tree', 'barbarian', 'black_dragon', 'korgath', 'gorvash', 'starless_king'}
# Helbreath-only monsters: (Npc.cfg name, shown name, plural, category)
EXTRA = [('Amphis', 'Amphis', 'Amphises', 'monsters'), ('Ghost', 'Ghost', 'Ghosts', 'monsters'),
         ('Vampire', 'Vampire', 'Vampires', 'monsters'), ('Ice-Pixie', 'Ice Pixie', 'Ice Pixies', 'monsters'),
         ('Air-Pixie', 'Air Pixie', 'Air Pixies', 'monsters'), ('Rudolph', 'Rudolph', 'Rudolphs', 'monsters')]
KILL_SLOTS = 64
COUNTER = {'total_kills': 64, 'enemy_kills': 65, 'story': 66, 'side': 67, 'level': 68}
CATS = ['monsters', 'bosses', 'hunter', 'adventure', 'quests', 'war']

def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main():
    ach = json.load(open(SRC, encoding='utf-8'))
    seen = set(x['target'] for x in ach if x['type'] == 'kill')
    missing = seen - set(MONSTERS) - UNMATCHED
    if missing: sys.exit('VALORIA monsters with no rule: %s' % sorted(missing))

    targets, slot = [], {}			# (npc, name, cat)
    defs = []						# (counter, cat, tier, count, name, title)
    tiers = {}						# group -> [(count, title pattern)] from slime, for EXTRA
    for x in ach:
        t = x['type']
        if t == 'kill':
            if x['target'] in UNMATCHED: continue
            if x['target'] not in slot:
                slot[x['target']] = len(targets)
                short = x['name'].split(' ', 2)[2] if x['count'] == 1 else None
                targets.append((MONSTERS[x['target']], short, x['cat']))
            c = slot[x['target']]
            if x['target'] == 'slime': tiers.setdefault('p', []).append((x['count'], x['title'].replace('Slime', '{m}')))
        else:
            c = COUNTER[t]
        name = x['name']
        if t == 'story': name = name.replace('story quest', 'quest')
        defs.append((c, CATS.index(x['cat']), x['tier'], x['count'], name, x['title']))
    for npc, short, plural, cat in EXTRA:
        c = len(targets)
        targets.append((npc, short, cat))
        for i, (n, pat) in enumerate(tiers['p']):
            defs.append((c, CATS.index(cat), i + 1, n, 'Kill %s %s' % ('{:,}'.format(n), short if n == 1 else plural),
                         pat.format(m=short)))
    assert len(targets) <= KILL_SLOTS
    # kills first by slot, then the rest as VALORIA lists them
    defs.sort(key=lambda d: (d[0] >= KILL_SLOTS, d[0] if d[0] < KILL_SLOTS else 0))

    L = ['// ValoriaAchieveData.h: VALORIA\'s achievements in Helbreath Reimagined.',
         '// Generated by tools/valoria_port/build_achievements.py from VALORIA data/achievements.json: do not edit.',
         '// The rules are in ValoriaEnchant.h (Ach); the client draws them in UI\\AchievePanel.cpp, the game',
         '// server counts them in HG Server\\Achievements.cpp.',
         '#pragma once', '', 'namespace Ach {', '',
         'struct Target { const char * npc; const char * name; unsigned char cat; };	// npc = its Npc.cfg name',
         'const int TARGET_COUNT = %d;' % len(targets),
         'static const Target kTargets[TARGET_COUNT] = {']
    for npc, short, cat in targets:
        L.append('\t{ %s, %s, %d },' % (cstr(npc), cstr(short), CATS.index(cat)))
    L += ['};', '',
          'struct Def { unsigned char counter, cat, tier; unsigned int count; const char * name; const char * title; };',
          'const int DEF_COUNT = %d;' % len(defs),
          'static const Def kDefs[DEF_COUNT] = {']
    for c, cat, tier, n, name, title in defs:
        L.append('\t{ %d, %d, %d, %d, %s, %s },' % (c, cat, tier, n, cstr(name), cstr(title)))
    L += ['};', '', '} // namespace Ach', '']
    open(OUT, 'w', encoding='latin-1', newline='\r\n').write('\n'.join(L))
    print('%d achievements, %d monsters (%d from VALORIA, %d Helbreath-only), %d left out' %
          (len(defs), len(targets), len(targets) - len(EXTRA), len(EXTRA), sum(1 for x in ach if x.get('target') in UNMATCHED)))

if __name__ == '__main__':
    main()
