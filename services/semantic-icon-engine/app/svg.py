"""Small strict SVG subset. Reject unsupported content instead of unsafe repair."""
import hashlib
import math
import re
from xml.etree import ElementTree as ET
from defusedxml.ElementTree import fromstring
PART = r'[a-z0-9]+(?:-[a-z0-9]+)*'
ID = re.compile(rf'(?:lucide:{PART}|simple-icons:[a-z0-9]+(?:[-_][a-z0-9]+)*|iconify:{PART}:{PART})\Z')
TAGS = set('svg g path circle ellipse rect line polyline polygon defs linearGradient radialGradient stop clipPath mask title desc'.split())
ATTRS = set('role viewBox width height x y x1 x2 y1 y2 cx cy r rx ry d points fill stroke stroke-width stroke-linecap stroke-linejoin stroke-miterlimit stroke-dasharray stroke-dashoffset fill-rule clip-rule opacity fill-opacity stroke-opacity transform id clip-path mask gradientUnits gradientTransform offset stop-color stop-opacity fx fy fr'.split())
REF = re.compile(r'url\(#[A-Za-z_][A-Za-z0-9_-]*\)\Z')

def valid_id(value):
    return isinstance(value, str) and len(value) <= 200 and ID.fullmatch(value) is not None

def sanitize(svg):
    if len(svg.encode()) > 131072 or re.search(r'<!|<\?', svg.replace('<?xml version="1.0" encoding="UTF-8"?>', ''), re.I):
        raise ValueError('XML declarations/entities or oversized SVG')
    root = fromstring(svg)
    ids, refs = set(), []
    count = 0
    def visit(node, depth=0):
        nonlocal count
        count += 1
        if depth > 24 or count > 4096:
            raise ValueError('SVG complexity limit')
        if node.tag.startswith('{'):
            if not node.tag.startswith('{http://www.w3.org/2000/svg}'):
                raise ValueError('foreign namespace')
            node.tag = node.tag.split('}', 1)[1]
        if node.tag not in TAGS:
            raise ValueError('unsupported element')
        for key, value in node.attrib.items():
            if key not in ATTRS or len(value) > 100000 or re.search(r'[<>\\]|javascript|data:|https?:|@|expression', value, re.I):
                raise ValueError('unsafe attribute')
            if 'url' in value.lower():
                if not REF.fullmatch(value):
                    raise ValueError('external resource')
                refs.append(value[5:-1])
            if key == 'id':
                if not re.fullmatch('[A-Za-z_][A-Za-z0-9_-]*', value) or value in ids:
                    raise ValueError('invalid local ID')
                ids.add(value)
        node.attrib = dict(sorted(node.attrib.items()))
        if node.tag not in ('title', 'desc') and node.text and node.text.strip():
            raise ValueError('unexpected SVG text')
        for child in node:
            visit(child, depth + 1)
    visit(root)
    if root.tag != 'svg' or any(r not in ids for r in refs):
        raise ValueError('invalid SVG root/reference')
    try:
        box = [float(x) for x in root.attrib['viewBox'].replace(',', ' ').split()]
        if len(box) != 4 or not all(math.isfinite(x) and abs(x) <= 100000 for x in box) or min(box[2:]) <= 0:
            raise ValueError()
    except (KeyError, ValueError):
        raise ValueError('invalid viewBox')
    root.set('width', '24'); root.set('height', '24'); root.set('xmlns', 'http://www.w3.org/2000/svg')
    root.attrib = dict(sorted(root.attrib.items()))
    result = ET.tostring(root, encoding='unicode', short_empty_elements=True)
    return result, hashlib.sha256(result.encode()).hexdigest()

def iconify_svg(pack, name):
    chain, seen = [], set()
    while name in pack.get('aliases', {}):
        if name in seen or len(chain) >= 32:
            raise ValueError('alias cycle/depth')
        seen.add(name); alias = pack['aliases'][name]; chain.append(alias); name = alias['parent']
    if name not in pack['icons']:
        raise ValueError('missing alias parent')
    data = {k: pack.get(k, v) for k, v in [('width',16), ('height',16), ('left',0), ('top',0)]}
    data.update(pack['icons'][name])
    for alias in reversed(chain):
        for k, v in alias.items():
            if k in ('hFlip', 'vFlip'): data[k] = bool(data.get(k, False)) ^ bool(v)
            elif k == 'rotate': data[k] = (data.get(k, 0) + v) % 4
            elif k != 'parent': data[k] = v
    w, h, left, top = [float(data[k]) for k in ('width', 'height', 'left', 'top')]
    if min(w,h) <= 0 or not all(math.isfinite(x) and abs(x)<=100000 for x in (w,h,left,top)):
        raise ValueError('invalid dimensions')
    body = data['body']
    if left or top: body = f'<g transform="translate({-left:g} {-top:g})">{body}</g>'
    transforms = []
    if data.get('hFlip'): transforms.append(f'translate({w:g} 0) scale(-1 1)')
    if data.get('vFlip'): transforms.append(f'translate(0 {h:g}) scale(1 -1)')
    if transforms: body = '<g transform="' + ' '.join(transforms) + '">' + body + '</g>'
    r = data.get('rotate', 0) % 4
    if r:
        rotation = {1:f'translate({h:g} 0) rotate(90)',2:f'translate({w:g} {h:g}) rotate(180)',3:f'translate(0 {w:g}) rotate(270)'}[r]
        body = f'<g transform="{rotation}">{body}</g>'
        if r % 2: w, h = h, w
    return f'<svg viewBox="0 0 {w:g} {h:g}" fill="currentColor">{body}</svg>'
