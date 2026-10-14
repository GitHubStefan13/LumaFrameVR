#!/usr/bin/env python3
"""Describe the installer and the real, versioned shared objects in its payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import quote, urlsplit


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def build_manifest(payload, installer, version, release_url, notes=''):
    if urlsplit(release_url).scheme != 'https' or not urlsplit(release_url).hostname:
        raise ValueError('release URL must use HTTPS')
    libraries = []
    for path in sorted(payload.rglob('*')):
        match = re.fullmatch(r'(.+)\.so(?:\.([0-9.]+))?', path.name)
        if match and path.is_file() and not path.is_symlink():
            libraries.append({'name': path.relative_to(payload).as_posix(),
                              'version': match[2] or 'unknown', 'sha256': sha256(path)})
    return {'version': version,
            'installer': {'url': release_url.rstrip('/') + '/' + quote(installer.name),
                          'sha256': sha256(installer), 'size': installer.stat().st_size},
            'libraries': libraries, 'notes': notes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--installer', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--release-url', required=True)
    parser.add_argument('--notes', default='')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = build_manifest(args.payload, args.installer, args.version, args.release_url, args.notes)
    args.output.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    args.output.with_name(args.output.name + '.sha256').write_text(sha256(args.output) + '  ' + args.output.name + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
