"""Remove identical inherited fields, proving resolved XML stays the same.

Macro-bearing source files are preserved. Enumerating their setting values tracks
which individual fields can vary, so unrelated inherited fields can still be
deduplicated safely. Graphics/UI/localization keep their own composition contracts.
"""
from pathlib import Path
from copy import deepcopy
import argparse
import re
import itertools
import xml.etree.ElementTree as ET

def canonical(node):
    return node.tag, tuple(sorted(node.attrib.items())), (node.text or '').strip(), tuple(canonical(n) for n in node)

def field(node):
    return node.tag, node.get('id', '')

def groups(node):
    result = {}
    for n in node:
        result.setdefault(field(n), []).append(n)
    return result

def merge(base, upper):
    if base.get('disabled') == 'true' or upper.get('disabled') == 'true':
        return deepcopy(upper)
    result = deepcopy(base)
    result.attrib.update(upper.attrib)
    replaced = groups(upper)
    for n in list(result):
        if field(n) in replaced:
            result.remove(n)
    result.extend(deepcopy(list(upper)))
    # Field ordering is not a semantic part of registry definitions.
    result[:] = sorted(result, key=lambda n: field(n))
    return result

def expand(text, settings):
    text = re.sub(r'\$(!?)([A-Za-z_][A-Za-z_0-9]*)\{([^{}]*)\}', lambda m: m[3] if (settings[m[2]] == 'true') != bool(m[1]) else '', text)
    return re.sub(r'\$([A-Za-z_][A-Za-z_0-9]*)', lambda m: settings[m[1]], text)

def run(root, apply):
    merged, dynamic = {}, {}
    changed = removed = fields = 0
    for mod in ['Julius', 'Augustus', 'Vespasian']:
        layer = root / 'Mods' / mod
        manifest = ET.parse(layer / 'mod.xml').getroot()
        settings = {s.get('id'): s.get('default') for s in manifest.findall('settings/setting')}
        choices = {}
        for setting in manifest.findall('settings/setting'):
            choices[setting.get('id')] = ['false', 'true'] if setting.get('type') == 'bool' else list(set([setting.get('default')] + setting.get('type')[4:-1].split(',')))
        for path in sorted(layer.rglob('*.xml')):
            relative = path.relative_to(layer)
            if relative.parts[0] in ['Graphics', 'UI', 'Localization'] or relative.name == 'mod.xml':
                continue
            text = path.read_text(encoding='utf-8-sig')
            node = ET.fromstring(expand(text, settings))
            key = ('BuildingType' if node.tag == 'building' else relative.parts[0], node.tag, node.get('type', node.get('id', node.get('name', path.stem))))
            base = merged.get(key)
            dynamic_source = '$' in text
            inherited_dynamic = dynamic.get(key, set())
            source_dynamic = set()
            if dynamic_source:
                identifiers = sorted(set(re.findall(r'\$!?([A-Za-z_][A-Za-z_0-9]*)', text)))
                original_groups = groups(node)
                for values in itertools.product(*(choices[name] for name in identifiers)):
                    variant = ET.fromstring(expand(text, {**settings, **dict(zip(identifiers, values))}))
                    variant_groups = groups(variant)
                    for group in original_groups.keys() | variant_groups.keys():
                        if [canonical(n) for n in original_groups.get(group, [])] != [canonical(n) for n in variant_groups.get(group, [])]:
                            source_dynamic.add(group)
                    for attribute in node.attrib.keys() | variant.attrib.keys():
                        if node.get(attribute) != variant.get(attribute):
                            source_dynamic.add(('@attribute', attribute))
            if base is not None and not dynamic_source and node.get('disabled') != 'true' and base.get('disabled') != 'true':
                sparse = deepcopy(node)
                inherited = groups(base)
                for group, entries in groups(node).items():
                    if group not in inherited_dynamic and [canonical(n) for n in entries] == [canonical(n) for n in inherited.get(group, [])]:
                        for n in list(sparse):
                            if field(n) == group:
                                sparse.remove(n); fields += 1
                for attribute in list(sparse.attrib):
                    if ('@attribute', attribute) not in inherited_dynamic and attribute not in ['type', 'id', 'name'] and sparse.get(attribute) == base.get(attribute):
                        del sparse.attrib[attribute]
                assert canonical(merge(base, sparse)) == canonical(merge(base, node)), path
                if canonical(sparse) != canonical(node):
                    changed += 1
                    if apply:
                        if not len(sparse) and set(sparse.attrib) <= {'type', 'id', 'name'}:
                            path.unlink(); removed += 1
                        else:
                            ET.indent(sparse, space='    ')
                            path.write_text('<?xml version="1.0" encoding="utf-8"?>\n' + ET.tostring(sparse, encoding='unicode') + '\n', encoding='utf-8')
            merged[key] = merge(base, node) if base is not None else deepcopy(node)
            replaced = set(groups(node)) | {('@attribute', name) for name in node.attrib}
            dynamic[key] = (inherited_dynamic - replaced) | source_dynamic
    print(f'{"Applied" if apply else "Preview"}: {fields} inherited fields in {changed} files; {removed} empty overlays removed. Resolved content equivalence verified.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--apply', action='store_true'); args = parser.parse_args()
    run(Path(__file__).resolve().parents[1], args.apply)
