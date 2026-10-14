import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

COMMIT = 'f4992299601614adbfefd398dc8e281556bb7444'
SOURCE = 'https://github.com/immersive-web/webxr-input-profiles/tree/' + COMMIT

def multiply(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]

def transform(node):
    if 'matrix' in node:
        return [[node['matrix'][c * 4 + r] for c in range(4)] for r in range(4)]
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    length = math.sqrt(x*x + y*y + z*z + w*w)
    x, y, z, w = [v / length for v in (x, y, z, w)]
    rotation = [[1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w, 0],
                [2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*z-2*x*w, 0],
                [2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y, 0], [0, 0, 0, 1]]
    scale = node.get('scale', [1, 1, 1])
    for r in range(3):
        for c in range(3):
            rotation[r][c] *= scale[c]
        rotation[r][3] = node.get('translation', [0, 0, 0])[r]
    return rotation

def decode_png(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not PNG')
    width, height, depth, color, compression, filtering, interlace = struct.unpack_from('>IIBBBBB', data, 16)
    if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
        raise ValueError('fixture requires noninterlaced RGB8 PNG')
    compressed = b''
    offset = 8
    while offset < len(data):
        length = struct.unpack_from('>I', data, offset)[0]
        kind, chunk = data[offset+4:offset+8], data[offset+8:offset+8+length]
        if zlib.crc32(kind + chunk) != struct.unpack_from('>I', data, offset+8+length)[0]:
            raise ValueError('PNG CRC mismatch')
        if kind == b'IDAT':
            compressed += chunk
        offset += length + 12
    raw = zlib.decompress(compressed)
    stride = width * 3
    if len(raw) != height * (stride + 1):
        raise ValueError('PNG payload size mismatch')
    rows, previous = [], bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        mode = raw[start]
        values = bytearray(raw[start+1:start+1+stride])
        for i in range(stride):
            left, up, diagonal = (values[i-3] if i >= 3 else 0), previous[i], (previous[i-3] if i >= 3 else 0)
            if mode == 1:
                prediction = left
            elif mode == 2:
                prediction = up
            elif mode == 3:
                prediction = (left + up) // 2
            elif mode == 4:
                p = left + up - diagonal
                distances = [abs(p-left), abs(p-up), abs(p-diagonal)]
                prediction = (left, up, diagonal)[distances.index(min(distances))]
            elif mode == 0:
                prediction = 0
            else:
                raise ValueError('unknown PNG filter')
            values[i] = (values[i] + prediction) & 255
        rows.append(values)
        previous = values
    return width, height, rows

def convert(path, output):
    source = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', source)
    if magic != 0x46546c67 or version != 2 or length != len(source):
        raise ValueError('invalid GLB')
    size, kind = struct.unpack_from('<II', source, 12)
    if kind != 0x4e4f534a:
        raise ValueError('missing GLB JSON')
    document = json.loads(source[20:20+size])
    binsize, binkind = struct.unpack_from('<II', source, 20+size)
    binary = source[28+size:28+size+binsize]
    if binkind != 0x004e4942:
        raise ValueError('missing GLB binary')
    def view(index):
        value = document['bufferViews'][index]
        start = value.get('byteOffset', 0)
        return binary[start:start+value['byteLength']]
    def accessor(index):
        value = document['accessors'][index]
        if 'sparse' in value or value.get('normalized', False):
            raise ValueError('unsupported accessor')
        formats = {5126: 'f', 5123: 'H', 5125: 'I'}
        counts = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}
        fmt = '<' + formats[value['componentType']] * counts[value['type']]
        size = struct.calcsize(fmt)
        stride = document['bufferViews'][value['bufferView']].get('byteStride', size)
        data = view(value['bufferView'])
        start = value.get('byteOffset', 0)
        return [struct.unpack_from(fmt, data, start+i*stride) for i in range(value['count'])]
    identity = [[float(r == c) for c in range(4)] for r in range(4)]
    vertices, indices, rendered = [], [], []
    def traverse(index, parent):
        node = document['nodes'][index]
        matrix = multiply(parent, transform(node))
        if 'mesh' in node:
            rendered.append(node['name'])
            for primitive in document['meshes'][node['mesh']]['primitives']:
                if primitive.get('mode', 4) != 4 or primitive['material'] != 0:
                    raise ValueError('fixture requires triangles and one material')
                attrs = primitive['attributes']
                positions, normals, uv = [accessor(attrs[k]) for k in ('POSITION', 'NORMAL', 'TEXCOORD_0')]
                base = len(vertices)
                for p, n, t in zip(positions, normals, uv, strict=True):
                    pos = [sum(matrix[r][c] * p[c] for c in range(3)) + matrix[r][3] for r in range(3)]
                    normal = [sum(matrix[r][c] * n[c] for c in range(3)) for r in range(3)]
                    norm = math.sqrt(sum(v*v for v in normal))
                    vertices.append(pos + [v/norm for v in normal] + list(t))
                indices.extend(base + i[0] for i in accessor(primitive['indices']))
        for child in node.get('children', []):
            traverse(child, matrix)
    for root in document['scenes'][document.get('scene', 0)]['nodes']:
        traverse(root, identity)
    if len(vertices) > 65536:
        raise ValueError('16-bit vertex limit')
    material = document['materials'][0]['pbrMetallicRoughness']
    texture = document['textures'][material['baseColorTexture']['index']]['source']
    width, height, rows = decode_png(view(document['images'][texture]['bufferView']))
    rgba = bytearray()
    for y in range(512):
        row = rows[y*height//512]
        for x in range(512):
            at = (x*width//512)*3
            rgba.extend(row[at:at+3])
            rgba.append(255)
    provenance = (SOURCE + '/packages/assets/profiles/valve-index; MIT; source-sha256=' + hashlib.sha256(source).hexdigest()).encode()
    name = ('Valve Index ' + path.stem + ' WebXR fixture').encode()
    with output.open('xb') as stream:
        stream.write(b'LFCMODEL' + struct.pack('<4I', len(vertices), len(indices), 512, 512))
        for text in (provenance, name):
            stream.write(struct.pack('<I', len(text)) + text)
        stream.write(struct.pack('<I16f', 1, *[identity[r][c] for c in range(4) for r in range(4)]))
        for vertex in vertices:
            stream.write(struct.pack('<8f', *vertex))
        stream.write(struct.pack('<' + 'H'*len(indices), *indices))
        stream.write(rgba)
    print(json.dumps({'file': str(output), 'vertices': len(vertices), 'indices': len(indices), 'rgba': len(rgba), 'rendered_nodes': rendered, 'source_sha256': hashlib.sha256(source).hexdigest(), 'fixture_sha256': hashlib.sha256(output.read_bytes()).hexdigest()}))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    convert(args.input, args.output)
