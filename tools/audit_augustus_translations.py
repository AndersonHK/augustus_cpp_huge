"""Compare changed upstream translations with the authored Augustus catalogs.

Only unchanged ancestral translations and missing keys can be applied automatically.
Fork-authored conflicts require an explicit disposition in the audit record.
"""
import argparse
import ast
import json
import re
import subprocess
from pathlib import Path

LANGUAGES = dict(zip(
    ('cs', 'de', 'el', 'en', 'es', 'fr', 'it', 'ja', 'ko', 'pl', 'pt', 'ru', 'sv', 'uk', 'zh-Hans', 'zh-Hant'),
    ('czech', 'german', 'greek', 'english', 'spanish', 'french', 'italian', 'japanese', 'korean', 'polish', 'portuguese', 'russian', 'swedish', 'ukrainian', 'simplified_chinese', 'traditional_chinese')))
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/|[A-Za-z_][A-Za-z_0-9]*|\S')


def source(ref, path):
    return subprocess.check_output(['git', 'show', f'{ref}:{path}']).decode('utf-8-sig')


def catalog(ref, language):
    common = source(ref, 'src/translation/common.h')
    macros = {key: ast.literal_eval(value) for key, value in re.findall(r'#define\s+(\w+)\s+("[^\n]+")', common)}
    translation = source(ref, f'src/translation/{language}.c').replace('\\\n', '')
    tokens = [token for token in TOKENS.findall(translation) if not token.startswith(('//', '/*'))]
    values = {}
    for index, token in enumerate(tokens):
        if token != '{' or index + 2 >= len(tokens) or not tokens[index + 1].startswith('TR_') or tokens[index + 2] != ',':
            continue
        key = tokens[index + 1]
        parts = []
        for value in tokens[index + 3:]:
            if value == '}':
                break
            if value == ',':
                continue
            if value.startswith('"'):
                parts.append(ast.literal_eval(value))
            elif value in macros:
                parts.append(macros[value])
            else:
                raise ValueError(f'{ref}/{language}/{key}: unsupported C token {value!r}')
        # Upstream set_strings retains the first occurrence of duplicate keys.
        values.setdefault(key, ''.join(parts))
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='caa61f5ceca8cf19e1caa785e9af2a55859c52cb')
    parser.add_argument('--target', default='upstream/master')
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--report', default='out/augustus-translation-audit.json')
    parser.add_argument('--resolutions', default='docs/augustus_translation_resolutions_2026_09_07.json')
    args = parser.parse_args()
    report = {'base': args.base, 'target': subprocess.check_output(['git', 'rev-parse', args.target]).decode().strip(), 'languages': {}}
    resolution_path = Path(args.resolutions)
    resolutions = json.loads(resolution_path.read_text(encoding='utf-8')) if resolution_path.exists() else {}
    for code, language in LANGUAGES.items():
        base, target = catalog(args.base, language), catalog(args.target, language)
        path = Path('Mods/Augustus/Localization') / f'{code}.json'
        text = path.read_text(encoding='utf-8-sig')
        payload = json.loads(text)
        ours = payload['project_keys']
        entries = []
        replacements = {}
        additions = {}
        for key in sorted(set(base) | set(target)):
            if base.get(key) == target.get(key):
                continue
            status = 'same' if ours.get(key) == target.get(key) else 'removed-upstream' if key not in target else 'apply' if key not in ours or ours[key] == base.get(key) else 'conflict'
            resolution = resolutions.get(code + '/' + key)
            desired = target.get(key)
            if resolution:
                desired = ours.get(key) if resolution['action'] == 'keep' else resolution.get('value', desired)
                status = 'retained' if resolution['action'] == 'keep' else 'same' if ours.get(key) == desired else 'apply'
            entries.append({'key': key, 'status': status, 'base': base.get(key), 'ours': ours.get(key), 'target': target.get(key), 'resolution': resolution})
            if status == 'apply':
                (replacements if key in ours else additions)[key] = desired
        if args.apply:
            for key, value in replacements.items():
                pattern = re.compile(r'("' + re.escape(key) + r'"\s*:\s*)"(?:\\.|[^"\\])*"')
                text, count = pattern.subn(lambda m: m[1] + json.dumps(value, ensure_ascii=False), text)
                if count != 1:
                    raise ValueError(f'{path}: expected one occurrence of {key}, got {count}')
            if additions:
                indent_match = re.search(r'\n( +)"TR_', text)
                indent = indent_match[1] if indent_match else '    '
                start = re.search(r'"project_keys"\s*:\s*\{', text).end()
                addition = ''.join('\n' + indent + json.dumps(key) + ': ' + json.dumps(value, ensure_ascii=False) + ',' for key, value in additions.items())
                text = text[:start] + addition + text[start:]
            json.loads(text)
            path.write_text(text, encoding='utf-8')
        report['languages'][code] = entries
        counts = {status: sum(entry['status'] == status for entry in entries) for status in ('same', 'apply', 'retained', 'conflict', 'removed-upstream')}
        print(code, counts)
    Path(args.report).write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
