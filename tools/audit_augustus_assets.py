"""Compare the installed distribution with final upstream asset blobs without extracting them."""
import argparse
import hashlib
import json
import subprocess
import zipfile
import io
import xml.etree.ElementTree as ET
from functools import lru_cache
from pathlib import Path
from PIL import Image


def git(*args):
    return subprocess.check_output(['git', *args])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pack', required=True)
    parser.add_argument('--target', default='upstream/master')
    parser.add_argument('--base', default='caa61f5ceca8cf19e1caa785e9af2a55859c52cb')
    parser.add_argument('--report', default='out/augustus-asset-audit.json')
    args = parser.parse_args()
    # Include changed-then-reverted files and side-parent commits. A net diff
    # alone cannot establish coverage of every queued asset commit.
    paths = list(dict.fromkeys(path for path in git('log', '--format=', '--name-only', args.base + '..' + args.target, '--', 'res/assets').decode().splitlines() if path))
    source_paths = {path.lower(): path for path in git('ls-tree', '-r', '--name-only', args.target, '--', 'res/assets').decode().splitlines()}
    removed_paths = [path for path in paths if path.lower() not in source_paths]
    paths = [source_paths[path.lower()] for path in paths if path.lower() in source_paths]
    paths = list(dict.fromkeys(paths + [path for path in source_paths.values() if path.startswith('res/assets/Graphics/') and path.endswith('.xml') and path.count('/') == 3]))
    rows = [dict(path=path, status='removed-upstream') for path in removed_paths]
    with zipfile.ZipFile(args.pack) as pack:
        entries = {name.lower(): name for name in pack.namelist()}
        @lru_cache(None)
        def source_image(path):
            path = source_paths[path.lower()]
            return Image.open(io.BytesIO(git('show', args.target + ':' + path))).convert('RGBA')

        def visual(image, root, atlas=None):
            layers = list(image.findall('layer'))
            if 'src' in image.attrib and not layers:
                layers.insert(0, ET.Element('layer', {'src': image.get('src')}))
            elif 'group' in image.attrib and not layers:
                layers.insert(0, ET.Element('layer', {key:value for key,value in image.attrib.items() if key in ('group','image','rotate','invert','x','y')}))
            operations, pending = [], []
            def flush():
                if not pending:
                    return
                left = min(x for x, y, pixels in pending)
                top = min(y for x, y, pixels in pending)
                right = max(x + pixels.width for x, y, pixels in pending)
                bottom = max(y + pixels.height for x, y, pixels in pending)
                canvas = Image.new('RGBA', (right-left, bottom-top))
                for x, y, pixels in pending:
                    canvas.alpha_composite(pixels, (x-left, y-top))
                bounds = canvas.getbbox()
                if bounds:
                    crop = canvas.crop(bounds)
                    # RGB under fully transparent pixels is not rendered.
                    crop.putdata([(r,g,b,a) if a else (0,0,0,0) for r,g,b,a in crop.getdata()])
                    operations.append(('pixels', left+bounds[0], top+bounds[1], crop.size, hashlib.sha256(crop.tobytes()).hexdigest()))
                pending.clear()
            for layer in layers:
                x,y = int(layer.get('x',0)),int(layer.get('y',0))
                if 'group' in layer.attrib:
                    flush()
                    attributes = {key:value for key,value in layer.attrib.items() if key not in ('width','height') and not (key in ('x','y') and value == '0')}
                    operations.append(('reference', tuple(sorted(attributes.items()))))
                elif atlas is not None:
                    sx,sy = int(layer.get('src_x',0)),int(layer.get('src_y',0))
                    w,h = int(layer.get('width',0)),int(layer.get('height',0))
                    pixels = atlas.crop((sx,sy,sx+w,sy+h))
                    if layer.get('rotate'): pixels = pixels.rotate(-int(layer.get('rotate')), expand=True)
                    pending.append((x,y,pixels))
                elif 'src' in layer.attrib:
                    pending.append((x,y,source_image('res/assets/Graphics/' + root + '/' + layer.get('src') + '.png')))
            flush()
            return operations

        packed_sources = set()
        referenced_sources = set()
        for path in paths:
            entry = entries.get(path.removeprefix('res/').lower())
            expected = git('rev-parse', args.target + ':' + path).decode().strip()
            actual = None
            if entry:
                data = pack.read(entry)
                actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            row = {'path': path, 'entry': entry, 'expected': expected, 'actual': actual, 'status': 'match' if actual == expected else 'missing' if not entry else 'different'}
            if path.endswith('.xml') and '/Graphics/' in path and entry:
                source = ET.fromstring(git('show', args.target + ':' + path))
                previous = ET.fromstring(git('show', args.base + ':' + path))
                def structure(node):
                    return node.tag, tuple(sorted((key,value) for key,value in node.attrib.items() if not (key in ('x','y') and value == '0'))), tuple(structure(child) for child in node)
                old_images = {node.get('id', '#'+str(index)): structure(node) for index,node in enumerate(previous.findall('image'))}
                packed = ET.fromstring(pack.read(entry))
                root = source.get('name')
                for node in source.iter():
                    if node.get('src'):
                        reference = source_paths.get(('res/assets/Graphics/'+root+'/'+node.get('src')+'.png').lower())
                        if reference: referenced_sources.add(reference)
                atlas = Image.open(io.BytesIO(pack.read(entries[('assets/Graphics/'+root+'.png').lower()]))).convert('RGBA')
                lookup = {node.get('id', '#'+str(index)): node for index,node in enumerate(packed.findall('image'))}
                failures = []
                for index,node in enumerate(source.findall('image')):
                    identity = node.get('id', '#'+str(index))
                    changed_pixels = any(source_paths.get(('res/assets/Graphics/'+root+'/'+layer.get('src','')+'.png').lower()) in paths for layer in (node.findall('layer') or [node]))
                    if old_images.get(identity) == structure(node) and not changed_pixels:
                        continue
                    for layer in (node.findall('layer') or [node]):
                        if layer.get('src'):
                            packed_sources.add(source_paths[('res/assets/Graphics/'+root+'/'+layer.get('src')+'.png').lower()])
                    target = lookup.get(identity)
                    metadata_matches = target is not None and all(target.get(key) == value for key,value in node.attrib.items() if key not in ('src', 'width', 'height', 'group', 'image', 'rotate', 'invert'))
                    animation_matches = target is not None and tuple(structure(child) for child in node.findall('animation')) == tuple(structure(child) for child in target.findall('animation'))
                    if not metadata_matches or not animation_matches or visual(node,root) != visual(target,root,atlas):
                        failures.append(identity)
                row.update(status='packed-equivalent' if not failures else 'packed-different', image_failures=failures)
            rows.append(row)
        for row in rows:
            if row['status'] == 'missing' and row['path'] in packed_sources:
                row['status'] = 'packed-source'
            elif row['status'] == 'missing' and row['path'].endswith('.png') and row['path'] not in referenced_sources:
                row['status'] = 'unused-source'
    Path(args.report).write_text(json.dumps({'target': git('rev-parse', args.target).decode().strip(), 'pack': args.pack, 'assets': rows}, indent=2) + '\n')
    for status in ('match', 'packed-equivalent', 'packed-source', 'unused-source', 'removed-upstream', 'packed-different', 'missing', 'different'):
        print(status, sum(row['status'] == status for row in rows))
    for row in rows:
        if row['status'] in ('missing', 'different', 'packed-different'):
            print(row['status'], row['path'])
            if row.get('image_failures'):
                print(' images:', row['image_failures'])
    raise SystemExit(any(row['status'] in ('missing', 'different', 'packed-different') for row in rows))


if __name__ == '__main__':
    main()
