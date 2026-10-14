#!/usr/bin/env python3
"""Release manifest validation and updater (Python standard library only)."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener

MANIFEST_URL = 'https://github.com/GitHubStefan13/LumaFrameVR/releases/latest/download/latest.json'
MANIFEST_LIMIT = 1024 * 1024


def https_url(value):
    if not isinstance(value, str):
        raise ValueError('URL must be a string')
    url = urlsplit(value)
    if url.scheme != 'https' or not url.hostname or url.username or url.password:
        raise ValueError('URL must use HTTPS without credentials')
    return value


def version_parts(value):
    if not isinstance(value, str) or not re.fullmatch(r'[0-9]+(?:\.[0-9]+)*', value) or len(value) > 256:
        raise ValueError('version must contain dot-separated numbers')
    return tuple(int(part) for part in value.split('.'))


def compare_versions(left, right):
    left, right = version_parts(left), version_parts(right)
    count = max(len(left), len(right))
    left += (0,) * (count - len(left))
    right += (0,) * (count - len(right))
    return (left > right) - (left < right)


def checksum(value):
    if not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value):
        raise ValueError('sha256 must contain 64 lowercase hexadecimal digits')
    return value


def parse_manifest(text):
    manifest = json.loads(text)
    if not isinstance(manifest, dict):
        raise ValueError('manifest must be an object')
    version_parts(manifest.get('version'))
    installer = manifest.get('installer')
    if not isinstance(installer, dict):
        raise ValueError('manifest must describe an installer')
    https_url(installer.get('url'))
    checksum(installer.get('sha256'))
    size = installer.get('size')
    if type(size) is not int or size <= 0 or size > 4 * 1024**3:
        raise ValueError('installer size must be positive and at most 4 GiB')
    libraries = manifest.get('libraries')
    if not isinstance(libraries, list):
        raise ValueError('libraries must be an array')
    for library in libraries:
        if not isinstance(library, dict) or not isinstance(library.get('name'), str) or not library['name']:
            raise ValueError('library must have a name')
        name = Path(library['name'])
        if name.is_absolute() or '..' in name.parts:
            raise ValueError('library name escapes the payload')
        if not isinstance(library.get('version'), str) or not library['version']:
            raise ValueError('library must have a version')
        checksum(library.get('sha256'))
    if not isinstance(manifest.get('notes', ''), str):
        raise ValueError('notes must be a string')
    return manifest


class HttpsRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        https_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def https_open(url):
    request = Request(https_url(url), headers={'User-Agent': 'LumaFrameVR-updater', 'Accept-Encoding': 'identity'})
    return build_opener(HttpsRedirect()).open(request, timeout=30)


def progress(text):
    print(' '.join(str(text).split()), flush=True)


def summary(manifest, root):
    old = {}
    changes = []
    for library in manifest['libraries']:
        path = root / library['name']
        if not path.is_relative_to(root) or '..' in path.parts:
            raise ValueError('library name escapes the payload')
        if path.is_file():
            digest = hashlib.sha256()
            with path.open('rb') as source:
                for block in iter(lambda: source.read(1024 * 1024), b''):
                    digest.update(block)
            old[library['name']] = digest.hexdigest()
        if old.get(library['name']) != library['sha256']:
            changes.append(library['name'] + ' ' + library['version'])
    changes.append(manifest.get('notes', ''))
    return ' '.join('; '.join(filter(None, changes)).split())


def verify_installed(app, version):
    current = app / 'current'
    expected = (app / 'releases' / version).resolve()
    if not current.is_symlink() or current.resolve() != expected:
        raise RuntimeError('installer did not move current to the requested release')
    actual = json.loads((current / 'BUILD_INFO.json').read_text(encoding='utf-8'))
    if actual.get('version') != version:
        raise RuntimeError('current release version does not match the manifest')


def run_update(root, action):
    if action not in ('--check', '--install'):
        raise ValueError('usage: update.sh --check | --install')
    root = root.resolve()
    installed = json.loads((root / 'BUILD_INFO.json').read_text(encoding='utf-8'))['version']
    version_parts(installed)
    progress('Checking for updates')
    with https_open(MANIFEST_URL) as source:
        raw = source.read(MANIFEST_LIMIT + 1)
    if len(raw) > MANIFEST_LIMIT:
        raise ValueError('manifest is too large')
    with https_open(MANIFEST_URL + '.sha256') as source:
        published = source.read(256).decode('ascii')
    match = re.fullmatch(r'([0-9a-f]{64})  latest\.json\n?', published)
    if not match or hashlib.sha256(raw).hexdigest() != match[1]:
        raise ValueError('manifest checksum does not match its published SHA-256')
    manifest = parse_manifest(raw)
    if compare_versions(manifest['version'], installed) <= 0:
        progress('LumaFrameVR ' + installed + ' is the latest release')
        return 3
    if action == '--check':
        print('AVAILABLE\t' + manifest['version'] + '\t' + summary(manifest, root), flush=True)
        return 0
    if root.parent.name != 'releases' or root.parents[1].name != 'lumaframevr' or root.parents[2].name != 'opt':
        raise ValueError('silent update requires an installed release under <prefix>/opt/lumaframevr/releases')
    prefix = root.parents[3]
    with (root.parents[1] / '.update.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError('another update is already running') from error
        with tempfile.TemporaryDirectory(prefix='lumaframevr-update-', dir=os.environ.get('TMPDIR')) as temporary:
            installer = Path(temporary) / 'installer.run'
            progress('Downloading LumaFrameVR ' + manifest['version'])
            digest = hashlib.sha256()
            size = 0
            with https_open(manifest['installer']['url']) as source, installer.open('wb') as destination:
                for block in iter(lambda: source.read(1024 * 1024), b''):
                    size += len(block)
                    if size > manifest['installer']['size']:
                        raise ValueError('installer exceeds its published size')
                    digest.update(block)
                    destination.write(block)
            progress('Verifying installer checksum')
            if size != manifest['installer']['size']:
                raise ValueError('installer size does not match manifest')
            if digest.hexdigest() != manifest['installer']['sha256']:
                raise ValueError('installer checksum does not match manifest')
            progress('Installing LumaFrameVR ' + manifest['version'])
            with (Path(temporary) / 'installer.log').open('w+b') as log:
                result = subprocess.run(['/bin/sh', str(installer), '--update', '--yes', '--no-ui', '--no-steam', '--prefix', str(prefix)],
                                        stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, timeout=300)
                if result.returncode:
                    log.seek(max(0, log.tell() - 8192))
                    lines = log.read().decode('utf-8', errors='replace').splitlines()
                    raise RuntimeError('installer failed: ' + (lines[-1] if lines else str(result.returncode)))
            verify_installed(root.parents[1], manifest['version'])
            progress('Installed LumaFrameVR ' + manifest['version'] + '; restart the player')
            return 0


def main():
    try:
        if sys.version_info < (3, 9):
            raise RuntimeError("Python 3.9 or newer is required")
        if len(sys.argv) != 2:
            raise ValueError('usage: update.sh --check | --install')
        return run_update(Path(__file__).resolve().parent, sys.argv[1])
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        progress('Update failed: ' + str(error))
        return 1


if __name__ == '__main__':
    sys.exit(main())
