#!/usr/bin/env python3
"""Build the LumaFrameVR desktop installer.

Two distribution shapes come out of one source tree:

* a self extracting .run file - the installer script, the release payload and the
  Steam helper in one executable file. This is the file to download, double click
  in the file manager or copy to another machine and run over SSH.
* an optional directory bundle - the same parts as separate files plus a double
  clickable desktop entry, for users who prefer to unpack first.

Both are verified before they are written: the payload must carry the release
name, launch.sh, the application binary and a SHA256SUMS.txt, and the binary must
be an AArch64 executable.
"""
import argparse
import hashlib
import io
import json
import re
import struct
import tarfile
from pathlib import Path

SCRIPT_NAME = 'install.sh'
# The script is a template: the version and the payload it embeds come from the caller, which
# CMake drives from the project() version in CMakeLists.txt, so there is one place to bump.
SCRIPT_TEMPLATE = 'install.sh.in'
HELPER_NAME = 'steam_shortcut.py'
ENTRY_NAME = 'install-lumaframevr.desktop'
SUMS_NAME = 'SHA256SUMS.txt'
EM_AARCH64 = 0xB7


def arguments():
    parser = argparse.ArgumentParser(description='Build the LumaFrameVR desktop installer')
    parser.add_argument('--payload', type=Path, required=True, help='release payload tar.gz')
    parser.add_argument('--version', required=True, help='application version; the project version in CMakeLists.txt')
    parser.add_argument('--build', type=int, default=1, help='installer build number')
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, help='write the self extracting .run here')
    parser.add_argument('--directory', type=Path, help='additionally write a directory bundle here')
    parser.add_argument('--record', type=Path, help='write a JSON build record here')
    parser.add_argument('--allow-unverified', action='store_true',
                        help='skip the payload integrity checks (tests only)')
    parser.add_argument('--allow-host-architecture', action='store_true',
                        help='accept a non-AArch64 payload for local packaging tests')
    return parser.parse_args()


def inspect_payload(path, version, strict, allow_host=False):
    data = path.read_bytes()
    sha256 = hashlib.sha256(data).hexdigest()
    release = 'lumaframevr-steam-frame-arm64-' + version
    members, binary = [], None
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        for member in archive:
            members.append(member.name)
            if member.name == release + '/lumaframevr':
                binary = archive.extractfile(member).read(20)
    if strict:
        names = set(members)
        for required in (release + '/launch.sh', release + '/lumaframevr', release + '/' + SUMS_NAME):
            if required not in names:
                raise SystemExit('error: the payload does not contain ' + required)
        if binary is None or len(binary) < 20:
            raise SystemExit('error: the payload application is not a Linux ELF64 executable')
        machine = struct.unpack('<H', binary[18:20])[0]
        if machine != EM_AARCH64 and not allow_host:
            raise SystemExit('error: the payload application is not an AArch64 executable')
    return {'sha256': sha256, 'bytes': len(data), 'members': len(members), 'release': release}


def render_script(script, version, payload, offsets):
    text = script.read_text(encoding='utf-8')
    replacements = {'VERSION': version, 'PAYLOAD_NAME': payload['name'], 'RELEASE_NAME': payload['release'],
                    'PAYLOAD_SHA256': payload['sha256'], 'PAYLOAD_BYTES': str(payload['bytes'])}
    for name in replacements:
        text, count = re.subn(r'(?m)^' + name + r'=.*$', lambda match, value=replacements[name]: name + '=' + str(value), text)
        if count != 1:
            raise SystemExit('error: %s= was not found exactly once in %s' % (name, script.name))
    for name, value in offsets.items():
        text, count = re.subn(r'(?m)^' + name + r'=0$', name + '=' + str(value), text)
        if count != 1:
            raise SystemExit('error: %s=0 was not found exactly once in %s' % (name, script.name))
    return text


def solve_header(script, version, payload, data):
    offsets = {'PAYLOAD_OFFSET': 0, 'HELPER_OFFSET': 0}
    for _ in range(8):
        text = render_script(script, version, payload, offsets) + '\n'
        header = text.encode('utf-8')
        payload_offset = len(header)
        helper_offset = payload_offset + len(data)
        if offsets == {'PAYLOAD_OFFSET': payload_offset, 'HELPER_OFFSET': helper_offset}:
            return header
        offsets = {'PAYLOAD_OFFSET': payload_offset, 'HELPER_OFFSET': helper_offset}
    raise SystemExit('error: the header offsets did not stabilise')


def write_sums(directory, names):
    lines = []
    for name in names:
        digest = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        lines.append('%s  %s\n' % (digest, name))
    (directory / SUMS_NAME).write_text(''.join(lines), encoding='utf-8')


def build_directory(target, args, script, helper, entry, payload, data):
    (target / 'payload').mkdir(parents=True, exist_ok=True)
    (target / SCRIPT_NAME).write_text(render_script(script, args.version, payload, {}), encoding='utf-8')
    (target / SCRIPT_NAME).chmod(0o755)
    (target / HELPER_NAME).write_bytes(helper.read_bytes())
    (target / HELPER_NAME).chmod(0o644)
    (target / ENTRY_NAME).write_bytes(entry.read_bytes())
    (target / 'payload' / args.payload.name).write_bytes(data)
    (target / 'payload' / (args.payload.name + '.sha256')).write_text(
        payload['sha256'] + '  ' + args.payload.name + '\n', encoding='utf-8')
    (target / 'README.user.md').write_text(
        '# LumaFrameVR %s installer\n\n'
        'Run `./install.sh`, or double click "Install LumaFrameVR", to install LumaFrameVR for\n'
        'the current user. No root and no Python are needed for that step. Over SSH use\n'
        '`./install.sh --install --no-steam`.\n\n'
        'If the target cannot execute `install.sh` (a missing execute bit, or a noexec\n'
        'filesystem), execute `install-lumaframevr.desktop`, which this bundle ships beside\n'
        '`install.sh` for exactly that case, or hand the installer to `sh`, which never needs\n'
        'the file itself to be executable:\n\n'
        '    sh ./install.sh --install --no-steam --yes\n\n'
        'Actions: --install, --repair, --uninstall [--purge], --steam-add, --steam-remove,\n'
        '--plan. Run `./install.sh --help` for every option.\n' % args.version,
        encoding='utf-8')
    write_sums(target, [SCRIPT_NAME, HELPER_NAME, ENTRY_NAME, 'README.user.md',
                        'payload/' + args.payload.name])


def main():
    args = arguments()
    if not args.output and not args.directory:
        raise SystemExit('error: pass --output, --directory or both')
    script = args.source / 'installer' / SCRIPT_TEMPLATE
    helper = args.source / 'installer' / HELPER_NAME
    entry = args.source / 'installer' / ENTRY_NAME
    for required in (script, helper, entry):
        if not required.is_file():
            raise SystemExit('error: missing installer file ' + str(required))
    payload = inspect_payload(args.payload, args.version, not args.allow_unverified,
                              args.allow_host_architecture)
    payload['name'] = args.payload.name
    data = args.payload.read_bytes()
    record = {'version': args.version, 'build': args.build, 'source': str(args.source),
              'payload': str(args.payload), 'payload_sha256': payload['sha256'],
              'payload_bytes': payload['bytes'], 'payload_members': payload['members'],
              'release': payload['release']}
    if args.directory:
        build_directory(args.directory, args, script, helper, entry, payload, data)
        record['directory'] = str(args.directory)
    if args.output:
        header = solve_header(script, args.version, payload, data)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('wb') as stream:
            stream.write(header)
            stream.write(data)
            stream.write(helper.read_bytes())
        args.output.chmod(0o755)
        digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
        args.output.with_name(args.output.name + '.sha256').write_text(
            digest + '  ' + args.output.name + '\n', encoding='utf-8')
        record.update(run=str(args.output), run_sha256=digest, run_bytes=args.output.stat().st_size,
                      payload_offset=len(header), helper_offset=len(header) + len(data))
    if args.record:
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
