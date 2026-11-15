#!/usr/bin/env python3
"""Builds the Helbreath Reimagined Server Data dashboard page from the server's data files.

    python3 tools/hb_dashboard/export.py OUT_DIR

writes OUT_DIR/index.html (template.html with the data). Publish it with the fonts the page uses
(fonts/CinzelDecorative-Bold.ttf). Edits saved on the page come back through
apply_edits.py.
"""
import datetime, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hbdata


def main(out_dir):
    data = {'rows': {}, 'built': datetime.date.today().isoformat()}
    for k in hbdata.KINDS:
        data['rows'][k] = [[rid, f] for rid, f, _, _ in hbdata.rows(k)]
    data['itemNames'] = {f['id']: f['name'] for _, f in data['rows']['items']}
    html = open(os.path.join(os.path.dirname(__file__), 'template.html'), encoding='utf-8').read()
    blob = json.dumps(data, separators=(',', ':')).replace('</', '<\\/')
    os.makedirs(out_dir, exist_ok=True)
    open(os.path.join(out_dir, 'index.html'), 'w', encoding='utf-8').write(html.replace('__DATA__', blob))
    print('wrote', os.path.join(out_dir, 'index.html'), {k: len(v) for k, v in data['rows'].items()})


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'dashboard_out')
