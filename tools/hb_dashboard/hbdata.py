"""Reads and writes Helbreath Reimagined's server data for the dashboard (tools/hb_dashboard).

Every editable thing is a row: (kind, id) -> named fields. export.py turns the rows into the dashboard
page; apply_edits.py writes the dashboard's edits back into the same files through set_field, which
changes only the one value and keeps every tab, space and comment in the file as it was.

Kinds and where they live (paths from the repo root):
  items      Server/Login Server/Config/Item*.cfg        one 'Item = ...' line per item, id = item number
  monsters   Server/Login Server/Config/Npc.cfg          id = monster name
  spells     Server/Login Server/Config/Magic.cfg        id = spell number
  quests     Server/Login Server/Config/Quest.cfg        id = quest number
  alchemy    Server/Login Server/Config/Potion.cfg       id = recipe number   (VALORIA recipes)
  crafting   Server/Login Server/Config/CraftItem.cfg    id = recipe number   (VALORIA recipes)
  forging    Server/Login Server/Config/BuildItem.cfg    id = item made       (VALORIA recipes)
  teleports  Server/Login Server/Config/Teleport.cfg     id = row number
  drops      Server/configs/droplist.xml                 id = monster|tier number|item, field = chance
  login      Client/CONTENTS/login_layout.cfg            id = piece of the sign-in screen ([logo], [panel] ...),
                                                         fields = its 'key = value' lines (the game reads the file itself)
"""
import glob, os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CFG = os.path.join(ROOT, 'Server', 'Login Server', 'Config')
DROPS = os.path.join(ROOT, 'Server', 'configs', 'droplist.xml')

# Field names, in file order after the '='. Values past the list get "col N".
FIELDS = {
    'items': ['id', 'name', 'item type', 'equip slot', 'effect', 'value 1', 'value 2', 'value 3', 'value 4', 'value 5',
              'value 6', 'durability', 'special effect', 'sprite', 'sprite frame', 'price', 'weight', 'appearance',
              'speed', 'level needed', 'gender', 'special value 1', 'special value 2', 'skill', 'category', 'colour',
              'logged'],
    'monsters': ['name', 'type', 'hit dice', 'defense', 'hit ratio', 'min bravery', 'exp dice', 'attack dice',
                 'attack range dice', 'size', 'side', 'action limit', 'action time', 'magic resist', 'magic level',
                 'day of week', 'chat', 'search range', 'regen time', 'element', 'absorb damage', 'max mana',
                 'magic hit ratio', 'attack range', 'area'],
    'spells': ['id', 'name', 'type', 'delay', 'duration', 'mana cost', 'h range', 'v range', 'value 1', 'value 2',
               'value 3', 'value 4', 'value 5', 'value 6', 'value 7', 'value 8', 'int needed', 'gold cost',
               'category', 'element'],
    'alchemy': ['id', 'name'] + [f'{w} {i}' for i in range(1, 7) for w in ('ingredient', 'amount')] + ['skill', 'difficulty'],
    'crafting': ['id', 'name'] + [f'{w} {i}' for i in range(1, 7) for w in ('ingredient', 'amount')] + ['skill', 'difficulty'],
    'forging': ['name', 'skill'] + [f'{w} {i}' for i in range(1, 7) for w in ('material', 'amount', 'material value')]
               + ['max skill', 'chance'],
    'teleports': ['npc', 'name', 'map', 'x', 'y', 'cost', 'min level', 'max level', 'side', 'hunt', 'night only'],
    'quests': None,     # from Quest.cfg's own header line
}
FILES = {'monsters': ['Npc.cfg'], 'spells': ['Magic.cfg'], 'quests': ['Quest.cfg'], 'alchemy': ['Potion.cfg'],
         'crafting': ['CraftItem.cfg'], 'forging': ['BuildItem.cfg'], 'teleports': ['Teleport.cfg']}
KEYWORD = {'items': 'item', 'monsters': 'npc', 'spells': 'magic', 'quests': 'quest', 'alchemy': 'potion',
           'crafting': 'crafting', 'forging': 'builditem', 'teleports': 'teleport'}
ID_FIELD = {'items': 0, 'monsters': 0, 'spells': 0, 'quests': 0, 'alchemy': 0, 'crafting': 0, 'forging': 0}
LINE = re.compile(r'^(\s*)([A-Za-z]+)(\s*=\s*)(.*?)(\s*)$')


def files_for(kind):
    if kind == 'items':
        return sorted(glob.glob(os.path.join(CFG, 'Item*.cfg')))
    return [os.path.join(CFG, f) for f in FILES[kind]]


def read(path):
    return open(path, encoding='latin-1', newline='').read()


def write(path, text):
    open(path, 'w', encoding='latin-1', newline='').write(text)


def quest_fields():
    for line in read(os.path.join(CFG, 'Quest.cfg')).splitlines():
        if line.startswith('//') and '\t' in line and 'type' in line:
            names = [t.strip() for t in line[2:].split('\t') if t.strip()]
            return names[1:] if names and names[0].lower().startswith('quest') else names
    return []


def tokens(rest):
    """The values of a line (after '=') with the whitespace between them, so a line can be rebuilt as it was."""
    return re.split(r'(\s+)', rest)


def rows(kind):
    """Yields (id, fields dict, file, line number) for every row of a kind."""
    if kind == 'login':
        yield from login_rows()
        return
    if kind == 'drops':
        yield from drop_rows()
        return
    names = FIELDS[kind] if FIELDS[kind] is not None else quest_fields()
    n = 0
    for path in files_for(kind):
        for ln, line in enumerate(read(path).splitlines()):
            m = LINE.match(line)
            if not m or m.group(2).lower() != KEYWORD[kind]:
                continue
            vals = tokens(m.group(4))[0::2]
            f = {}
            for i, v in enumerate(vals):
                f[names[i] if i < len(names) else f'col {i + 1}'] = v
            rid = str(n) if kind == 'teleports' else vals[ID_FIELD[kind]]
            n += 1
            yield rid, f, path, ln


def set_field(kind, rid, field, value):
    """Changes one value of one row in its file. Returns the old value, or raises KeyError."""
    if kind == 'login':
        return set_login(rid, field, value)
    if kind == 'drops':
        return set_drop(rid, field, value)
    names = FIELDS[kind] if FIELDS[kind] is not None else quest_fields()
    for r, f, path, ln in rows(kind):
        if r != rid:
            continue
        keys = list(f)
        if field not in keys:
            raise KeyError(f'{kind} {rid}: no field "{field}"')
        idx = keys.index(field)
        value = str(value).strip()
        if not value or re.search(r'\s', value):
            raise ValueError(f'{kind} {rid} {field}: a value can\'t be empty or contain spaces')
        text = read(path)
        lines = text.split('\n')
        line = lines[ln].rstrip('\r')
        cr = '\r' if lines[ln].endswith('\r') else ''
        m = LINE.match(line)
        parts = tokens(m.group(4))
        old = parts[idx * 2]
        parts[idx * 2] = value
        lines[ln] = m.group(1) + m.group(2) + m.group(3) + ''.join(parts) + m.group(5) + cr
        write(path, '\n'.join(lines))
        return old
    raise KeyError(f'{kind}: no row "{rid}"')


# ---- droplist.xml: <npc_t name=".."> <tier_t type=".." prob=".."> <item_t name=".." prob=".."/>
NPC_RE = re.compile(r'<npc_t name="([^"]+)"[^>]*>(.*?)</npc_t>', re.S)
TIER_RE = re.compile(r'<tier_t type="([^"]+)" prob="(\d+)">(.*?)</tier_t>', re.S)
ITEM_RE = re.compile(r'<item_t name="([^"]+)" prob="(\d+)"/>')


def drop_rows():
    text = read(DROPS)
    for nm in NPC_RE.finditer(text):
        for ti, t in enumerate(TIER_RE.finditer(nm.group(2)), 1):
            total = sum(int(p) for _, p in ITEM_RE.findall(t.group(3))) or 1
            for it in ITEM_RE.finditer(t.group(3)):
                yield (f'{nm.group(1)}|{ti}|{it.group(1)}',
                       {'monster': nm.group(1), 'tier': f'{ti} ({t.group(1)})', 'tier chance': t.group(2),
                        'item': it.group(1), 'weight': it.group(2),
                        'share of tier': f'{100 * int(it.group(2)) / total:.1f}%'}, DROPS, 0)


def set_drop(rid, field, value):
    if field not in ('weight', 'tier chance'):
        raise KeyError(f'drops {rid}: only "weight" and "tier chance" can be changed')
    npc, tier, item = rid.split('|')
    value = str(int(value))
    text = read(DROPS)
    nm = next((m for m in NPC_RE.finditer(text) if m.group(1) == npc), None)
    if not nm:
        raise KeyError(f'drops: no monster "{npc}"')
    t = list(TIER_RE.finditer(nm.group(2)))[int(tier) - 1]
    base = nm.start(2) + t.start()
    if field == 'tier chance':
        a, b = nm.start(2) + t.start(2), nm.start(2) + t.end(2)
        old = text[a:b]
        write(DROPS, text[:a] + value + text[b:])
        return old
    it = next((m for m in ITEM_RE.finditer(t.group(3)) if m.group(1) == item), None)
    if not it:
        raise KeyError(f'drops {rid}: no item "{item}" in that tier')
    a = nm.start(2) + t.start(3) + it.start(2)
    b = nm.start(2) + t.start(3) + it.end(2)
    old = text[a:b]
    write(DROPS, text[:a] + value + text[b:])
    return old


# ---- login_layout.cfg: [piece] sections of 'key = value' lines (Source/Client3/UI/LoginLayout.cpp reads it)
LOGIN = os.path.join(ROOT, 'Client', 'CONTENTS', 'login_layout.cfg')
ANCHORS = ['top-left', 'top', 'top-right', 'left', 'center', 'right', 'bottom-left', 'bottom', 'bottom-right']
KV = re.compile(r'^(\s*)([a-z][a-z0-9 ]*?)(\s*=[ \t]*)(.*?)(\s*)$')


def login_rows():
    cur, f, at = None, {}, 0
    for ln, line in enumerate(read(LOGIN).splitlines()):
        t = line.strip()
        if t.startswith('[') and t.endswith(']'):
            if cur:
                yield cur, f, LOGIN, at
            cur, f, at = t[1:-1].strip(), {}, ln
        elif cur and not t.startswith('//'):
            m = KV.match(line)
            if m:
                f[m.group(2)] = m.group(4)
    if cur:
        yield cur, f, LOGIN, at


def set_login(rid, field, value):
    value = str(value).strip()
    if field in ('x', 'y', 'width', 'height'):
        value = str(int(value))
        if field in ('width', 'height') and int(value) < 1:
            raise ValueError(f'login {rid} {field}: must be 1 or more')
    elif field == 'anchor' and value not in ANCHORS:
        raise ValueError(f'login {rid}: anchor must be one of {", ".join(ANCHORS)}')
    elif field == 'show' and value not in ('yes', 'no'):
        raise ValueError(f'login {rid}: show must be yes or no')
    elif '\n' in value or '\r' in value or len(value) > 120:
        raise ValueError(f'login {rid} {field}: one line of up to 120 characters')
    lines = read(LOGIN).split('\n')
    cur = None
    for i, raw in enumerate(lines):
        line = raw.rstrip('\r')
        t = line.strip()
        if t.startswith('[') and t.endswith(']'):
            cur = t[1:-1].strip()
        elif cur == rid and not t.startswith('//'):
            m = KV.match(line)
            if m and m.group(2) == field:
                lines[i] = m.group(1) + m.group(2) + (' = ' + value if value else ' =') + ('\r' if raw.endswith('\r') else '')
                write(LOGIN, '\n'.join(lines))
                return m.group(4)
    raise KeyError(f'login {rid}: no field "{field}"')


KINDS = ['items', 'monsters', 'drops', 'spells', 'alchemy', 'crafting', 'forging', 'quests', 'teleports', 'login']
