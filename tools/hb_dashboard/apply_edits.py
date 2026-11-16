#!/usr/bin/env python3
"""Puts the dashboard's saved edits onto the server data files.

    python3 tools/hb_dashboard/apply_edits.py EDITS.json RESULT.json

EDITS.json: the dashboard's pending edit documents, a list of {"doc": id, "kind", "id", "fields": {field: value}}.
Each edit is applied whole or not at all. RESULT.json lists, per document, "applied" or "failed" and why,
for marking the documents on the dashboard. Run the VALORIA generators (build_items.py,
build_recipes.py, build_passives.py, build_achievements.py, build_world.py) before this, never after:
they rewrite their files. Changes to achievements, side quests, shrines and chests remake their
headers (build_content.py): the game needs rebuilding for those.
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hbdata


def main(src, dst):
    edits = json.load(open(src))
    out = []
    for e in edits:
        files = {}
        done = []
        try:
            # check every field first, so an edit with one bad field changes nothing
            have = {rid: f for rid, f, _, _ in hbdata.rows(e['kind'])}
            if e['id'] not in have:
                raise KeyError(f'no {e["kind"]} "{e["id"]}" on the server any more')
            for k in e['fields']:
                if k not in have[e['id']]:
                    raise KeyError(f'no field "{k}"')
            for path in set(p for rid, _, p, _ in hbdata.rows(e['kind']) if rid == e['id']):
                files[path] = hbdata.read(path)
            for k, v in e['fields'].items():
                old = hbdata.set_field(e['kind'], e['id'], k, v)
                done.append(f'{k}: {old} -> {v}')
            out.append({'doc': e['doc'], 'status': 'applied', 'note': '; '.join(done)})
        except (KeyError, ValueError, IndexError) as ex:
            for path, text in files.items():
                hbdata.write(path, text)
            out.append({'doc': e['doc'], 'status': 'failed', 'note': str(ex.args[0]) if ex.args else str(ex)})
    # the VALORIA content is compiled in: make its headers again from the changed data
    if any(r['status'] == 'applied' and e['kind'] in hbdata.CONTENT for r, e in zip(out, edits)):
        import subprocess
        subprocess.run([sys.executable, os.path.join(hbdata.ROOT, 'tools', 'valoria_port', 'build_content.py')], check=True)
    json.dump(out, open(dst, 'w'), indent=1)
    for r in out:
        print(r['status'], r['doc'], r['note'])


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
