"""Inventory pinned upstream archive producers, including changes without version bumps.

Reads Git objects only. The inventory retains source expressions and enclosing version
gates instead of guessing record layouts from sizeof(runtime structs).
"""
import hashlib
import json
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = 'caa61f5ceca8cf19e1caa785e9af2a55859c52cb'
TIP = '719f4860a6b57617fa323e84488e603e34d66911'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT).decode('utf-8')


def function(source, name):
    match = re.search(r'(?m)^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source)
    if not match:
        raise ValueError('Missing function: ' + name)
    start = source.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    paths = [p for p in git('ls-tree', '-r', '--name-only', TIP, 'src').splitlines()
             if p.endswith(('.c', '.h'))]
    tracked = []
    for path in paths:
        source = git('show', TIP + ':' + path)
        if (path.endswith('.h') or 'buffer_write_' in source or 'buffer_read_' in source or
                path in ('src/game/save_version.h', 'src/game/resource.h',
                         'src/building/type.h', 'src/figure/type.h', 'src/core/buffer.c')):
            tracked.append(path)
    commits = [BASE] + git('rev-list', '--reverse', '--first-parent', BASE + '..' + TIP).splitlines()
    schemas = {}
    declarations = {}
    producers = []
    previous = None
    for commit in commits:
        tree = git('ls-tree', '-r', commit, '--', *tracked)
        entries = {line.split('\t')[1]: line.split()[2] for line in tree.splitlines()}
        for path, blob in entries.items():
            if not path.endswith('.h') or blob in declarations:
                continue
            source = git('show', blob)
            declarations[blob] = {
                'path': path,
                'constants': re.findall(r'(?m)^#define\s+\w+\s+[^\n]+(?:\\\n[^\n]+)*', source),
                'enums': re.findall(r'(?:typedef\s+)?enum(?:\s+\w+)?\s*\{.*?\}\s*\w*\s*;', source, re.S)}
        identity = hashlib.sha256(json.dumps(entries, sort_keys=True).encode()).hexdigest()
        if identity == previous:
            continue
        previous = identity
        version_source = git('show', commit + ':src/game/save_version.h')
        versions = dict(re.findall(r'\b(SAVE_GAME_CURRENT_VERSION|SCENARIO_CURRENT_VERSION)\s*=\s*(0x[0-9a-fA-F]+|\d+)', version_source))
        io = git('show', commit + ':src/game/file_io.c')
        layout = {name: function(io, name) for name in ('get_version_data', 'init_savegame_data', 'init_scenario_data')}
        layout_id = hashlib.sha256(json.dumps(layout, sort_keys=True).encode()).hexdigest()
        schemas.setdefault(layout_id, layout)
        producers.append({'commit': commit, 'subject': git('show', '-s', '--format=%s', commit).strip(),
                          'save_version': versions['SAVE_GAME_CURRENT_VERSION'],
                          'scenario_version': versions['SCENARIO_CURRENT_VERSION'],
                          'source_identity': identity, 'layout': layout_id, 'source_blobs': entries})
    result = {'base': BASE, 'tip': TIP,
              'interpretation': 'Ordered piece expressions retain compression flags (1 compressed), dynamic size 0, feature gates and exact producer Git blobs. A shared numeric version does not imply a shared record schema. Blob identities cover serialization readers/writers and enum tables; resolve with git show <blob>. Runtime struct sizes are not serialized sizes.',
              'layouts': schemas, 'record_declarations': declarations, 'producers': producers}
    target = ROOT / 'docs/augustus_save_schema_inventory.json'
    target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(producers)} producer identities; {len(schemas)} ordered layouts; {len(tracked)} serialization source paths')


if __name__ == '__main__':
    main()
