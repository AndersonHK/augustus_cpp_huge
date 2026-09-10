"""Append licensed dog source sprites to an installed Augustus walker atlas.

Input PNG masters remain in res/assets. Packaged atlases and runtime extraction
remain outside the checkout. Repeated deployment is idempotent.
"""
from pathlib import Path
import argparse
import json
import os
import xml.etree.ElementTree as ET
from PIL import Image

def pack(game_root):
    repo = Path(__file__).resolve().parents[1]
    folder = game_root / 'assets/Graphics'
    xml_path = folder / 'walkers.xml'
    png_path = folder / 'Walkers.png'
    marker_path = folder / 'vespasian-dog-pack.json'
    node = ET.parse(xml_path).getroot()
    if any(n.get('id') == 'dog_walk_ne_01' for n in node) and not marker_path.exists():
        print('Installed walker pack already includes upstream dogs'); return
    atlas = Image.open(png_path).convert('RGBA')
    original_height = json.loads(marker_path.read_text())['base_height'] if marker_path.exists() else atlas.height
    for n in list(node):
        if n.get('id', '').startswith('dog_'):
            node.remove(n)
    sources = sorted((repo / 'res/assets/Graphics/Walkers').glob('dog_*.png'))
    assert len(sources) == 65, 'Expected all dog sprite frames and portrait'
    images = [(p, Image.open(p).convert('RGBA')) for p in sources]
    width = atlas.width
    x = y = row_height = 0
    placements = []
    for path, image in images:
        if x + image.width > width:
            x = 0; y += row_height + 1; row_height = 0
        placements.append((path, image, x, y)); x += image.width + 1; row_height = max(row_height, image.height)
    packed = Image.new('RGBA', (width, original_height + y + row_height + 1))
    packed.paste(atlas.crop((0, 0, width, original_height)), (0, 0))
    for path, image, x, y in placements:
        packed.paste(image, (x, original_height + y))
        image_node = ET.SubElement(node, 'image', id=path.stem)
        ET.SubElement(image_node, 'layer', src_x=str(x), src_y=str(original_height + y), x='0', y='0', width=str(image.width), height=str(image.height))
    ET.indent(node, space='    ')
    xml_temp = xml_path.with_suffix('.xml.tmp'); png_temp = png_path.with_suffix('.png.tmp')
    packed.save(png_temp, format='PNG'); xml_temp.write_text('<?xml version="1.0"?>\n' + ET.tostring(node, encoding='unicode') + '\n', encoding='utf-8')
    marker_path.write_text(json.dumps({'base_height': original_height}) + '\n')
    os.replace(png_temp, png_path); os.replace(xml_temp, xml_path)
    print('Packed 65 dog sprites into installed Augustus walker atlas')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--game-root', type=Path, required=True); args = parser.parse_args()
    pack(args.game_root.resolve())
