#!/usr/bin/env python3
"""Add, remove or query the LumaFrameVR entry in Steam's shortcuts file.

Steam keeps non-Steam games in a binary VDF file,
<steam>/userdata/<account>/config/shortcuts.vdf. Editing it needs structured
reads and length prefixed writes, which is why this step - and only this step -
is Python. Everything else about the installation is plain shell.

Only entries this project owns are touched: an entry whose executable is the
LumaFrameVR launcher and whose name is LumaFrameVR. Steam is asked to reload the
file on its next start; a timestamped backup is written before every change.
"""
import argparse
import os
import re
import struct
import sys
import time
import zlib
from dataclasses import dataclass
from pathlib import Path

APP_NAME = 'LumaFrameVR'
LEGACY_NAMES = (APP_NAME, 'Devkit Game: ' + APP_NAME)
STEAM_ROOTS = ('~/.steam/steam', '~/.local/share/Steam',
               '~/.var/app/com.valvesoftware.Steam/.local/share/Steam')


@dataclass(frozen=True)
class Node:
    kind: int
    key: str
    value: object
    raw: bytes = b''


def encode(node):
    if node.raw:
        return node.raw
    key = node.key.encode('utf-8', 'surrogateescape') + b'\0'
    if node.kind == 0:
        value = dump(node.value)
    elif node.kind == 1:
        value = node.value.encode('utf-8', 'surrogateescape') + b'\0'
    elif node.kind in (2, 7):
        value = struct.pack('<I' if node.kind == 2 else '<Q', node.value)
    else:
        value = node.value
    return bytes([node.kind]) + key + value


def dump(nodes):
    return b''.join(map(encode, nodes)) + b'\x08'


def parse(data):
    if not isinstance(data, bytes) or len(data) > 16 * 1024 * 1024:
        raise ValueError('invalid or oversized binary VDF')
    position = 0
    count = 0

    def take(size):
        nonlocal position
        if position + size > len(data):
            raise ValueError('truncated binary VDF')
        value = data[position:position + size]
        position += size
        return value

    def string():
        nonlocal position
        end = data.find(b'\0', position, position + 1024 * 1024 + 1)
        if end < 0:
            raise ValueError('unterminated VDF string')
        value = data[position:end].decode('utf-8', 'surrogateescape')
        position = end + 1
        return value

    def fields(depth):
        nonlocal count
        if depth > 32:
            raise ValueError('VDF nesting exceeds the safety limit')
        nodes, keys = [], set()
        while True:
            start = position
            kind = take(1)[0]
            if kind == 8:
                return nodes
            count += 1
            if count > 100000:
                raise ValueError('too many VDF fields')
            key = string()
            if key in keys:
                raise ValueError('duplicate VDF key')
            keys.add(key)
            if kind == 0:
                value = fields(depth + 1)
            elif kind == 1:
                value = string()
            elif kind in (2, 7):
                value = int.from_bytes(take(4 if kind == 2 else 8), 'little')
            elif kind in (3, 4, 6):
                value = take(4)
            elif kind == 5:
                value = bytearray()
                while len(value) <= 1024 * 1024:
                    pair = take(2)
                    value.extend(pair)
                    if pair == b'\0\0':
                        break
                else:
                    raise ValueError('oversized wide VDF string')
                value = bytes(value)
            else:
                raise ValueError('unsupported binary VDF type')
            nodes.append(Node(kind, key, value, data[start:position]))

    nodes = fields(0)
    if position != len(data):
        raise ValueError('trailing binary VDF data')
    return nodes


def mapping(nodes):
    result = {}
    for node in nodes:
        key = node.key.lower()
        if node.kind == 1:
            result[key] = node.value
        elif node.kind == 2:
            result[key] = node.value
    return result


def quote(path):
    if not isinstance(path, str) or not path.startswith('/') or any(c in path for c in '\0\n\r"$`\\'):
        raise ValueError('unsafe or non absolute path: ' + repr(path))
    return '"' + path.replace('\\', '\\\\') + '"'


def shortcuts_tree(data):
    roots = parse(data) if data else [Node(0, 'shortcuts', [])]
    matches = [n for n in roots if n.key.lower() == 'shortcuts']
    if len(matches) != 1 or matches[0].kind != 0:
        raise ValueError('invalid shortcuts root')
    shortcuts = matches[0]
    if any(n.kind != 0 or not n.key.isascii() or not n.key.isdigit() for n in shortcuts.value):
        raise ValueError('invalid shortcut entry')
    return roots, shortcuts


def appid_for(executable):
    return zlib.crc32((executable + APP_NAME).encode()) | 0x80000000


def owned_indices(rows, launcher, quoted):
    allowed = {launcher, quoted}
    return [i for i, row in enumerate(rows)
            if row.get('appname') in LEGACY_NAMES and row.get('exe') in allowed]


def plan_add(data, launcher, start_dir, icon, options):
    executable = quote(launcher)
    quote(start_dir)
    if not isinstance(icon, str) or '\0' in icon:
        raise ValueError('invalid icon path')
    roots, shortcuts = shortcuts_tree(data)
    entries, rows = shortcuts.value, [mapping(n.value) for n in shortcuts.value]
    owned = owned_indices(rows, launcher, executable)
    if any(row.get('appname') == APP_NAME and i not in owned for i, row in enumerate(rows)):
        raise ValueError('another Steam shortcut is already called ' + APP_NAME)
    appid = appid_for(executable)
    if any(row.get('appid') == appid for i, row in enumerate(rows) if i not in owned):
        raise ValueError('the Steam AppID %d is already used by another shortcut' % appid)
    if len({rows[i].get('appid') for i in owned}) > 1:
        raise ValueError('several LumaFrameVR shortcuts with different AppIDs exist')
    winner = next((i for i in owned if rows[i].get('appid') == appid), owned[0] if owned else None)
    values = {'appid': appid, 'AppName': APP_NAME, 'Exe': executable, 'StartDir': quote(start_dir),
              'icon': icon, 'ShortcutPath': '', 'LaunchOptions': options, 'IsHidden': 0,
              'AllowDesktopConfig': 0, 'AllowOverlay': 1, 'openvr': 1, 'Devkit': 0,
              'DevkitGameID': '', 'DevkitOverrideAppID': 0}
    nodes = list(entries[winner].value) if winner is not None else []
    for key, value in values.items():
        node = Node(2 if isinstance(value, int) else 1, key, value)
        position = next((i for i, n in enumerate(nodes) if n.key.lower() == key.lower()), None)
        if position is None:
            nodes.append(node)
        elif nodes[position].kind != node.kind:
            raise ValueError('unexpected shortcut field type for ' + key)
        else:
            nodes[position] = Node(node.kind, nodes[position].key, value)
    if winner is None:
        nodes += [Node(2, 'LastPlayTime', 0), Node(0, 'tags', [])]
        index = str(max((int(n.key) for n in entries), default=-1) + 1)
        entries = entries + [Node(0, index, nodes)]
    else:
        entries = [Node(0, n.key, nodes) if i == winner else n
                   for i, n in enumerate(entries) if i not in owned or i == winner]
    updated = Node(0, shortcuts.key, entries)
    return dump([updated if n is shortcuts else n for n in roots]), appid


def plan_remove(data, launcher):
    executable = quote(launcher)
    roots, shortcuts = shortcuts_tree(data)
    entries = shortcuts.value
    rows = [mapping(n.value) for n in entries]
    owned = owned_indices(rows, launcher, executable)
    if not owned:
        return data, []
    removed = sorted({rows[i].get('appid') for i in owned})
    entries = [n for i, n in enumerate(entries) if i not in owned]
    updated = Node(0, shortcuts.key, entries)
    return dump([updated if n is shortcuts else n for n in roots]), removed


def has_shortcut(data, launcher):
    roots, shortcuts = shortcuts_tree(data)
    rows = [mapping(n.value) for n in shortcuts.value]
    return bool(owned_indices(rows, launcher, quote(launcher)))


def recent_steam_id(steam_root):
    path = steam_root / 'config' / 'loginusers.vdf'
    if not path.is_file():
        return None
    inside, result = False, None
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        match = re.search(r'"(\d{17})"', line)
        if match and not line.strip().startswith('//'):
            result, inside = match.group(1), False
        if '{' in line:
            inside = True
        if inside and re.search(r'"MostRecent"\s*"1"', line, re.IGNORECASE):
            return result
        if '}' in line:
            inside = False
    return None


def locate_shortcut_file(explicit=None):
    if explicit:
        path = Path(explicit).expanduser()
        return path if path.name.endswith('.vdf') else path / 'config' / 'shortcuts.vdf'
    candidates = []
    seen = {}
    for root in STEAM_ROOTS:
        steam_root = Path(root).expanduser()
        accounts = (steam_root / 'userdata').glob('*') if (steam_root / 'userdata').is_dir() else []
        for account in sorted(accounts):
            if account.name.isdigit() and (account / 'config').is_dir():
                # On the frame ~/.steam/steam is a symlink to ~/.local/share/Steam, so the
                # same account turns up under both roots. Compare resolved paths, or the
                # installer sees two accounts and refuses to guess which one is meant.
                resolved = os.path.realpath(str(account / 'config' / 'shortcuts.vdf'))
                if resolved not in seen:
                    seen[resolved] = (steam_root, account)
    candidates = list(seen.values())
    if not candidates:
        raise SystemExit('error: no Steam installation with a userdata directory was found')
    if len(candidates) > 1:
        for steam_root, account in candidates:
            if recent_steam_id(steam_root) == account.name:
                return account / 'config' / 'shortcuts.vdf'
        listing = '\n  '.join(str(account / 'config' / 'shortcuts.vdf') for _, account in candidates)
        raise SystemExit('error: several Steam accounts found, pass --userdata:\n  ' + listing)
    return candidates[0][1] / 'config' / 'shortcuts.vdf'


def load(path):
    return path.read_bytes() if path.is_file() else b''


def store(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file():
        stamp = time.strftime('%Y%m%d%H%M%S') + '-' + str(os.getpid())
        backup = path.with_name(path.name + '.lumaframevr-' + stamp)
        backup.write_bytes(path.read_bytes())
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(data)
    os.replace(temporary, path)


def steam_pid(steam_root):
    """The PID Steam recorded for itself, when the file is there and that process is alive."""
    for name in ('steam.pid', 'root/steam.pid'):
        candidate = Path(steam_root).expanduser() / name
        try:
            pid = int(candidate.read_text().strip())
        except (OSError, ValueError):
            continue
        if Path('/proc/%d' % pid).exists():
            return pid
    return None


def steam_running(shortcut_path):
    """Steam keeps shortcuts.vdf in memory and writes it back when it exits, so an edit made
    while it runs is lost. Its pid file and the processes it leaves around are enough to tell."""
    roots = {str(Path(shortcut_path).resolve().parents[3])}
    roots.update(str(Path(root).expanduser()) for root in STEAM_ROOTS)
    for root in roots:
        pid = steam_pid(root)
        if pid:
            return pid
    try:
        entries = list(Path('/proc').iterdir())
    except OSError:
        return None
    for entry in entries:
        if not entry.name.isdigit():
            continue
        try:
            name = (entry / 'comm').read_text().strip()
        except OSError:
            continue
        if name in ('steam', 'steamwebhelper', 'steamvr'):
            return int(entry.name)
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description='Manage the LumaFrameVR Steam shortcut')
    parser.add_argument('action', choices=('add', 'remove', 'status', 'appid'))
    parser.add_argument('--exec', dest='executable', required=True, help='LumaFrameVR launcher path')
    parser.add_argument('--start-dir', dest='start_dir', default='')
    parser.add_argument('--icon', default='')
    parser.add_argument('--launch-options', dest='options', default='')
    parser.add_argument('--userdata', default=None, help='Steam userdata directory')
    args = parser.parse_args(argv)
    executable = str(Path(args.executable).expanduser())
    # The AppID is deterministic, so it can be reported without touching Steam.
    # The installer bakes it into the launcher so SteamAppId/SteamGameId are set
    # even when the player is started outside Steam.
    if args.action == 'appid':
        print(appid_for(quote(executable)))
        return 0
    if args.userdata is None:
        args.userdata = os.environ.get('LUMAFRAMEVR_STEAM_USERDATA')
    start_dir = args.start_dir or str(Path(executable).parent)
    path = locate_shortcut_file(args.userdata)
    data = load(path)
    if args.action == 'status':
        print(('installed' if has_shortcut(data, executable) else 'absent') + ' ' + str(path))
        return 0 if has_shortcut(data, executable) else 1
    if args.action == 'add':
        running = steam_running(path)
        if running:
            print('warning: Steam is running (pid %d) and rewrites this file when it exits, '
                  'which would undo the shortcut.' % running)
            print('warning: close Steam, run this again, then start Steam.')
        updated, appid = plan_add(data, executable, start_dir, args.icon, args.options)
        if updated != data:
            store(path, updated)
        # Read the file back. Announcing a ready shortcut when the entry did not land is how a
        # missing entry in the Steam library goes unnoticed.
        if not has_shortcut(load(path), executable):
            raise SystemExit('error: the shortcut did not land in %s' % path)
        print('Steam shortcut ready in %s (AppID %d, verified)' % (path, appid))
        print('Start Steam (or restart it) for the entry to appear under Non-Steam Games.')
        return 0
    updated, removed = plan_remove(data, executable)
    if updated != data:
        store(path, updated)
    print('Removed %s from %s' % (', '.join(str(a) for a in removed) or 'nothing', path))
    return 0


if __name__ == '__main__':
    sys.exit(main())
