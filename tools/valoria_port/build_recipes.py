#!/usr/bin/env python3
"""Ports VALORIA's recipes into Helbreath Reimagined. They replace Helbreath's own recipes.

Run from the repo root after build_items.py:  python3 tools/valoria_port/build_recipes.py

Reads   tools/valoria_port/valoria_data/recipes.json  (VALORIA's recipes)
        tools/valoria_port/item_map.json              (VALORIA item -> Helbreath item name(s), from build_items.py)
        Server/Login Server/Config/Item*.cfg, ItemValoria*.cfg (item numbers and icons)
Writes  Server/Login Server/Config/BuildItem.cfg      Blacksmith recipes  -> Manufacturing (the forging tool)
        Server/Login Server/Config/Potion.cfg         potions and food    -> Alchemy (the alchemy pot)
        Server/Login Server/Config/CraftItem.cfg      everything else from the Magic Tower -> Crafting
        Client/CONTENTS/BItemcfg.txt                  the Manufacturing window's recipe list
        Server/configs/valoria_recipes.txt            level, gold and quantity of every recipe (game server)

The rules (the same for every recipe):
  * Which window: VALORIA's own split. Blacksmith = Manufacturing; potions, elixirs and food = Alchemy;
    the rest of the Magic Tower = Crafting.
  * Alchemy and Crafting work out the recipe from what is placed. If two recipes use the same
    ingredients (Frostweave Skirt and Frostweave Robe), the bigger one the stacks can pay for is made;
    to make the smaller one, place stacks that only cover it.
  * Required character level = VALORIA level x 140 / 80, rounded (the same rule as item levels).
  * Gold fee = VALORIA gold / 4, rounded (the same rule as item prices).
  * Ingredients and output quantity are VALORIA's. A placed stack offers its whole count, so 30 Forge
    Dust is one stack in one slot; the game server takes exactly what the recipe needs.
  * Crafting always succeeds, as in VALORIA. Manufacturing and Alchemy still train their Helbreath skill.
  * Armour made in a male and a female version is listed once, as the male (M) version; the game
    server hands a woman the female (W) version.
  * A recipe is left out when its result or an ingredient isn't ported yet (elixirs, scrolls,
    enhancement stones, runes: later stages; gloves: Helbreath has no glove slot).
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from build_items import CFG_DIR, COLS, MAX_HB_LEVEL, MAX_VAL_LEVEL, ROOT  # noqa: E402

DATA = os.path.join(HERE, 'valoria_data')
ALCHEMY_TYPES = {'potion', 'elixir', 'food'}
SLOTS = 6


# Items Justin wants found, not made: the Blessed Upgrade Stone is a super-rare boss drop (build_passives.py).
DROP_ONLY = {'blessed_stone'}

# Monster parts that VALORIA and Helbreath both have (Slime Gel and Slime Jelly...). One rule for every
# pair: VALORIA's part is a Smithing / Crafting material, Helbreath's is an Alchemy reagent.
#   VALORIA part   Helbreath part
DUPLICATES = {
    'slime_gel':        'SlimeJelly',
    'orc_tusk':         'OrcTeeth',
    'scorpion_stinger': 'ScorpionSting',
    'ogre_hide':        'OgreLeather',
    'bone_dust':        'SkeletonBones',
    'wolf_pelt':        'WerewolfLeather',
    'mithril_ore':      'Mithral',
}
# So each part of a pair has its own use: what is added to VALORIA's recipes (ingredient: count).
# Gel binds the gem settings of the plain rings; the tusk, stinger, hide and pelt go into early gear;
# the Helbreath parts into potions and elixirs.
RECIPE_ADD = {
    'ruby_ring':        {'slime_gel': 1},
    'sapphire_ring':    {'slime_gel': 1},
    'emerald_ring':     {'slime_gel': 1},
    'amethyst_ring':    {'slime_gel': 1},
    'broadsword':       {'orc_tusk': 2},
    'studded_leggings': {'scorpion_stinger': 2},
    'studded_leather':  {'ogre_hide': 1},
    'wind_boots':       {'wolf_pelt': 2},
    'potion_sp_x':      {'ScorpionSting': 1},
    'potion_hp_x':      {'OgreLeather': 1},
    'potion_mp_l':      {'SkeletonBones': 1},
    'elixir_swiftness': {'WerewolfLeather': 1},
    'elixir_ironskin':  {'Mithral': 1},
}


def apply_purposes(recipes, items):
    """Alchemy recipes take the Helbreath part of a pair in place of VALORIA's; RECIPE_ADD is added."""
    for r in recipes:
        alch = r['station'] != 'blacksmith' and items.get(r['out'], {}).get('type') in ALCHEMY_TYPES
        ins = {}
        for k, n in r['in'].items():
            if alch and k in DUPLICATES: k = DUPLICATES[k]
            ins[k] = ins.get(k, 0) + n
        for k, n in RECIPE_ADD.get(r['out'], {}).items():
            ins[k] = ins.get(k, 0) + n
        r['in'] = ins
        for k in ins:   # check the rule holds
            if alch and k in DUPLICATES: sys.exit('alchemy recipe %s uses %s' % (r['out'], k))
            if not alch and k in DUPLICATES.values(): sys.exit('%s recipe %s uses %s' % (r['station'], r['out'], k))

def all_items():
    rows = {}
    for f in sorted(os.listdir(CFG_DIR)):
        if not re.match(r'Item(\d*|Valoria\d+)\.cfg$', f): continue
        for line in open(os.path.join(CFG_DIR, f), encoding='latin-1'):
            if re.match(r'\s*Item\s*=', line, re.I):
                c = line.split('=', 1)[1].split()
                rows[c[1]] = dict(zip(COLS, c))
    return rows


def main():
    recipes = json.load(open(os.path.join(DATA, 'recipes.json')))
    items = json.load(open(os.path.join(DATA, 'items.json')))
    items = items if isinstance(items, dict) else {x['id']: x for x in items}
    imap = json.load(open(os.path.join(HERE, 'item_map.json')))
    hb = all_items()
    apply_purposes(recipes, items)
    for name in DUPLICATES.values(): imap.setdefault(name, [name])   # Helbreath parts, by their own name

    build, potion, craft, extra, skipped = [], [], [], [], []
    for r in recipes:
        if r['out'] in DROP_ONLY:
            skipped.append((r['out'], ['boss drop only'])); continue
        missing = [k for k in [r['out']] + list(r['in']) if k not in imap]
        if missing:
            skipped.append((r['out'], missing)); continue
        if len(r['in']) > SLOTS: sys.exit('more than 6 ingredients: ' + r['out'])
        ins = []
        for k, n in r['in'].items():
            name = imap[k][0]                       # an ingredient is never gendered armour
            ins.append((name, int(hb[name]['id']), int(n)))
        level = max(1, int(round(r.get('level', 1) * MAX_HB_LEVEL / MAX_VAL_LEVEL)))
        gold = int(round(r.get('gold', 0) / 4))
        qty = int(r.get('qty', 1))
        if r['station'] == 'blacksmith': dest = build
        elif items.get(r['out'], {}).get('type') in ALCHEMY_TYPES: dest = potion
        else: dest = craft
        out = imap[r['out']][0]                     # (M) when gendered; the server swaps in (W)
        dest.append((out, ins))
        extra.append((out, level, gold, qty))

    # Alchemy and Crafting find the recipe from the ingredients alone. When two use the same ingredients,
    # the game server makes the one whose amounts exactly match the placed stacks, otherwise the bigger
    # one the stacks can pay for; so two recipes may not have exactly the same amounts.
    for lst, what in ((potion, 'Alchemy'), (craft, 'Crafting')):
        seen = {}
        for out, ins in lst:
            key = frozenset((i, n) for _, i, n in ins)
            if key in seen: sys.exit('%s recipes %s and %s use the same ingredients and amount' % (what, seen[key], out))
            seen[key] = out

    def pairs(ins):
        p = ['%d %d' % (i, n) for _, i, n in ins] + ['-1 0'] * (SLOTS - len(ins))
        return '  '.join(p)

    head = '// VALORIA recipes: generated by tools/valoria_port/build_recipes.py, do not edit by hand.\n'

    with open(os.path.join(CFG_DIR, 'BuildItem.cfg'), 'w', newline='\r\n') as f:
        f.write(head + '//\t\tName\tSkillLimit\t(material id, count, value) x6\taverage value\tmax skill\tattribute\n')
        for out, ins in build:
            mats = ['%d\t%d\t0' % (i, n) for _, i, n in ins] + ['0\t0\t0'] * (SLOTS - len(ins))
            f.write('BuildItem\t=\t%s\t0\t%s\t0\t100\t0\n' % (out, '\t'.join(mats)))

    with open(os.path.join(CFG_DIR, 'Potion.cfg'), 'w', newline='\r\n') as f:
        f.write(head + '//-------Num--Name--(ingredient id, count) x6--SkillLimit--Difficulty\n')
        for n, (out, ins) in enumerate(potion, 1):
            f.write('potion = %d  %s  %s  0  0\n' % (n, out, pairs(ins)))

    with open(os.path.join(CFG_DIR, 'CraftItem.cfg'), 'w', newline='\r\n') as f:
        f.write(head + '//------Num--Name--(ingredient id, count) x6--SkillLimit--Difficulty\n')
        for n, (out, ins) in enumerate(craft, 1):
            f.write('crafting = %d  %s  %s  0  0\n' % (n, out, pairs(ins)))

    with open(os.path.join(ROOT, 'Client', 'CONTENTS', 'BItemcfg.txt'), 'w', newline='\r\n') as f:
        f.write(head + '//----------Name---------------SkillLimit-----Elements (name count) x6----SprH---SprFrame--MaxSkill\n')
        for out, ins in build:
            els = ['%s %d' % (nm, n) for nm, _, n in ins] + ['xxx 0'] * (SLOTS - len(ins))
            f.write('BuildItem = %s\t0\t%s\t%s\t%s\t100\n' % (out, ' '.join(els), hb[out]['spr'], hb[out]['sfrm']))

    with open(os.path.join(ROOT, 'Server', 'configs', 'valoria_recipes.txt'), 'w', newline='\r\n') as f:
        f.write(head + '// name\tlevel\tgold\tquantity\n')
        for out, level, gold, qty in extra:
            f.write('%s\t%d\t%d\t%d\n' % (out, level, gold, qty))

    print('Manufacturing %d, Alchemy %d, Crafting %d recipes; left out %d:' % (len(build), len(potion), len(craft), len(skipped)))
    for out, miss in skipped: print('   ', out, '(needs', ', '.join(miss) + ')')


if __name__ == '__main__':
    main()
