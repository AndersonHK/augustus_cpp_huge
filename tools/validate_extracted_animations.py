#!/usr/bin/env python3
"""Validate generated animation contracts without copying source graphics into Mods.

Pass generated Julius and Augustus Graphics directories. --baseline-julius may
point at a v12 installation to verify that extraction kept every source PNG.
"""
import argparse
import hashlib
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

DIRECTIONS = ('ne', 'e', 'se', 's', 'sw', 'w', 'nw', 'n')


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def documents(root):
    result = {}
    for path in root.rglob('*.xml'):
        if 'Renderer_Seam_Climate' in path.parts:
            continue
        doc = ET.parse(path).getroot()
        if doc.tag == 'assetlist' and doc.get('explicit_animations') == 'true':
            result[doc.get('name').lower()] = (path, doc)
    return result


def validate(julius, augustus, baseline=None, animations_only=False):
    sources = [documents(julius), documents(augustus)]
    animations = references = checked_groups = 0
    unresolved = set()
    for source_index, source in enumerate(sources):
        for group, (path, doc) in source.items():
            if animations_only and source_index == 1 and not doc.findall('image/animation/frame'):
                continue
            checked_groups += 1
            for animation in doc.findall('image/animation'):
                frames = animation.findall('frame')
                declared = int(animation.get('frames', str(len(frames))))
                require(declared == len(frames), f'{path}: incomplete animation ({declared}/{len(frames)})')
                # Offset-only source metadata may remain; it is not a playback sequence.
                animations += bool(frames)
            for element in doc.iter():
                if element.tag not in ('image', 'layer', 'frame'):
                    continue
                if element.get('src'):
                    png = path.with_suffix('') / (element.get('src') + '.png')
                    require(png.is_file(), f'{path}: missing PNG {png}')
                if not element.get('group'):
                    continue
                target_group = element.get('group').lower()
                if target_group == 'system':  # Engine-provided UI primitives, outside the extracted atlases.
                    continue
                if target_group == 'this':
                    target_group = group
                image = element.get('image', 'Image_0000')
                target = None
                for candidate_source in reversed(sources[:source_index + 1]):
                    if target_group in candidate_source:
                        target_doc = candidate_source[target_group][1]
                        target = next((e for e in target_doc.findall('image') if e.get('id') == image), None)
                        if target is not None:
                            break
                if target is None:
                    unresolved.add(f'{path}: unresolved {target_group}/{image}')
                    continue
                if element.get('frame'):
                    frame = int(element.get('frame'))
                    require(0 < frame <= len(target.findall('animation/frame')), f'{path}: out-of-range frame {frame}')
                references += 1

    doc = sources[0]['walkers\\group_115'][1]
    entries = {e.get('id'): e for e in doc.findall('image')}
    require(set(entries) == {'move_' + d for d in DIRECTIONS} | {'corpse', 'gesture'}, 'Group_115 must expose exactly ten sequences')
    expected = {'move_' + d: list(range(i, 96, 8)) for i, d in enumerate(DIRECTIONS)}
    expected['corpse'] = list(range(96, 104))
    expected['gesture'] = [104, 104, 105, 106, 107, 108, 109, 110, 111, 111, 110, 109, 108, 107, 106, 105]
    covered = set()
    for name, indices in expected.items():
        entry = entries[name]
        actual = [int(frame.get('src').removeprefix('Image_')) for frame in entry.findall('animation/frame')]
        require(actual == indices, f'Group_115/{name}: frame order changed')
        require(entry.get('sprite_offset_x') == entry.get('sprite_offset_y') == '0', f'Group_115/{name}: placement changed')
        animation = entry.find('animation')
        require(animation.get('reversible', 'false') == 'false' and animation.get('speed', '0') == '0', f'{name}: playback flags changed')
        covered.update(actual)
    require(covered == set(range(112)), 'Group_115 lost source sprites')
    for name, length in [('dog', 8), ('thief', 12), ('architect', 12)]:
        model = sources[1]['walkers\\' + name][1]
        for direction in DIRECTIONS:
            entry = next((e for e in model.findall('image') if e.get('id') == 'move_' + direction), None)
            require(entry is not None and len(entry.findall('animation/frame')) == length, f'{name}: incomplete movement {direction}')
        require(not any(e.get('id', '').startswith('default_') for e in model.findall('image')), f'{name}: obsolete per-frame aliases')

    def sprite_offset(group, image, visiting=None):
        visiting = set() if visiting is None else visiting
        key = (group.lower(), image)
        require(key not in visiting, f'Anchor reference cycle: {key}')
        visiting = visiting | {key}
        entry = None
        for source in reversed(sources):
            if key[0] in source:
                entry = next((e for e in source[key[0]][1].findall('image') if e.get('id') == image), None)
                if entry is not None:
                    break
        if entry is None:
            return None
        if entry.get('sprite_offset_x') is not None or entry.get('sprite_offset_y') is not None:
            return int(entry.get('sprite_offset_x', '0')), int(entry.get('sprite_offset_y', '0'))
        animation = entry.find('animation')
        if animation is not None:
            return int(animation.get('x', '0')), int(animation.get('y', '0'))
        for reference in [entry] + entry.findall('layer'):
            target = reference.get('group')
            if not target:
                continue
            anchor = sprite_offset(group if target == 'this' else target, reference.get('image', 'Image_0000'), visiting)
            if anchor is not None:
                return anchor
        return None

    anchors = 0
    for group, (_, doc) in sources[1].items():
        if not group.startswith('walkers\\'):
            continue
        for entry in doc.findall('image'):
            if entry.find('animation/frame') is None or not entry.get('group'):
                continue
            expected_anchor = (19, 29) if group == 'walkers\\dog' else sprite_offset(entry.get('group'), entry.get('image'))
            require(sprite_offset(group, entry.get('id')) == (expected_anchor or (0, 0)), f'{group}/{entry.get("id")}: source anchor changed')
            anchors += 1

    compared = 0
    if baseline:
        for png in baseline.rglob('*.png'):
            relative = png.relative_to(baseline)
            if 'Renderer_Seam_Climate' in relative.parts:
                continue
            candidate = julius / relative
            # Authored overlays and old deployment backups are not extraction inputs.
            if not candidate.is_file():
                require(not png.stem.startswith('Image_'), f'Extraction lost {relative}')
                continue
            require(hashlib.sha256(png.read_bytes()).digest() == hashlib.sha256(candidate.read_bytes()).digest(), f'PNG pixels changed: {relative}')
            compared += 1
    require(not unresolved, '\n'.join(sorted(unresolved)))
    print(f'Validated {checked_groups} groups, {animations} explicit animations, {references} references, {anchors} model anchors; {compared} unchanged source PNGs.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--julius', required=True, type=Path)
    parser.add_argument('--augustus', required=True, type=Path)
    parser.add_argument('--baseline-julius', type=Path)
    parser.add_argument('--animations-only', action='store_true', help='Restrict Augustus checks to groups containing playback animations; Julius remains fully checked.')
    args = parser.parse_args()
    try:
        validate(args.julius, args.augustus, args.baseline_julius, args.animations_only)
    except (ValueError, ET.ParseError, OSError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
