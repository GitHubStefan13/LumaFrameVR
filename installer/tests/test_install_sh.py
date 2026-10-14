"""End to end tests for the shell desktop installer.

Every test runs the real installer in a throw away HOME with its own XDG
directories, so an assertion failure can never touch the developer's own
installation. The payload used by most tests is a miniature release: the layout,
the checksums and the AArch64 marker are real, the binary is a stub.
"""
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INSTALLER = ROOT / 'installer'
BUILDER = ROOT / 'scripts' / 'build-installer.py'
FIXTURE_VERSION = '0.0.0'
RELEASE_NAME = 'lumaframevr-steam-frame-arm64-' + FIXTURE_VERSION
sys.path.insert(0, str(ROOT / 'installer'))
import steam_shortcut  # noqa: E402  (the helper is shipped, importing it is intended)


def fake_binary():
    header = bytearray(64)
    header[0:4] = b'\x7fELF'
    header[4] = 2
    header[5] = 1
    struct.pack_into('<H', header, 16, 3)
    struct.pack_into('<H', header, 18, 0xB7)
    return bytes(header)


def write_payload(directory, name='payload.tar.gz', tamper_after_sums=None):
    files = {'lumaframevr': (fake_binary(), 0o755),
             'launch.sh': (b'#!/bin/sh\nexit 0\n', 0o755),
             'branding/icon.png': (b'\x89PNG\r\n\x1a\n' + b'\0' * 32, 0o644),
             'README.md': (b'LumaFrameVR\n', 0o644)}
    sums = []
    for relative, (data, mode) in files.items():
        sums.append('%s  %s\n' % (hashlib.sha256(data).hexdigest(), relative))
    files['SHA256SUMS.txt'] = (''.join(sums).encode(), 0o644)
    if tamper_after_sums:
        data, mode = files[tamper_after_sums]
        files[tamper_after_sums] = (data + b'# tampered\n', mode)
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
        for relative, (data, mode) in files.items():
            info = tarfile.TarInfo(RELEASE_NAME + '/' + relative)
            info.size = len(data)
            info.mode = mode
            archive.addfile(info, io.BytesIO(data))
    Path(directory).mkdir(parents=True, exist_ok=True)
    path = Path(directory) / name
    path.write_bytes(buffer.getvalue())
    return path


class InstallerCase(unittest.TestCase):
    def setUp(self):
        self.home = Path(tempfile.mkdtemp(prefix='lumaframevr-installer-'))
        self.addCleanup(shutil.rmtree, self.home, True)
        payload = write_payload(self.home / 'source', 'fixture-payload.tar.gz')
        self.bundle = self.home / 'bundle'
        self.build(bundle=self.bundle, payload=payload)
        self.prefix = self.home / '.local'
        self.app = self.prefix / 'opt/lumaframevr'
        self.release = self.app / 'releases' / FIXTURE_VERSION
        self.launcher = self.prefix / 'bin/lumaframevr'
        self.entry = self.prefix / 'share/applications/lumaframevr.desktop'
        self.icon = self.prefix / 'share/icons/hicolor/512x512/apps/lumaframevr.png'
        self.record = self.prefix / 'share/lumaframevr/install-record.txt'

    def build(self, bundle=None, payload=None, output=None):
        command = [sys.executable, str(BUILDER), '--version', FIXTURE_VERSION, '--allow-unverified',
                   '--payload', str(payload)]
        if bundle:
            command += ['--directory', str(bundle)]
        if output:
            command += ['--output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def environment(self, **extra):
        env = {'HOME': str(self.home), 'PATH': os.environ['PATH'], 'LANG': 'C',
               'LUMAFRAMEVR_VIEWER_STATE': ''}
        env.update(extra)
        return env

    def run_installer(self, *arguments, script=None, env=None, expect=0):
        script = script or (self.bundle / 'install.sh')
        result = subprocess.run(['sh', str(script)] + list(arguments),
                                capture_output=True, text=True, env=env or self.environment())
        if expect is not None:
            self.assertEqual(result.returncode, expect, result.stdout + result.stderr)
        return result

    def install(self, *arguments, env=None):
        return self.run_installer('--install', '--no-steam', '--yes', *arguments, env=env)

    def assert_installed(self):
        self.assertTrue(self.release.is_dir(), 'release directory missing')
        self.assertTrue((self.release / 'lumaframevr').is_file(), 'binary missing')
        self.assertTrue(self.launcher.is_file(), 'launcher missing')
        self.assertTrue(os.access(self.launcher, os.X_OK), 'launcher is not executable')
        self.assertTrue(self.entry.is_file(), 'desktop entry missing')
        self.assertTrue(self.icon.is_file(), 'icon missing')
        self.assertTrue(self.record.is_file(), 'install record missing')
        self.assertTrue((self.app / 'current').is_symlink(), 'current symlink missing')
        self.assertEqual(os.readlink(self.app / 'current'), 'releases/' + FIXTURE_VERSION)


class InstallTests(InstallerCase):
    def test_plan_changes_nothing(self):
        result = self.run_installer('--plan')
        self.assertIn('Would install', result.stdout)
        self.assertFalse(self.app.exists())
        self.assertFalse(self.entry.exists())

    def test_install_creates_the_standard_layout(self):
        result = self.install()
        self.assert_installed()
        self.assertIn('installed', result.stdout)
        recorded = self.record.read_text()
        self.assertIn('release ' + FIXTURE_VERSION, recorded)
        self.assertIn('created ' + str(self.launcher), recorded)

    def test_install_is_idempotent(self):
        self.install()
        first = (self.release / 'lumaframevr').read_bytes()
        self.install()
        self.assert_installed()
        self.assertEqual(first, (self.release / 'lumaframevr').read_bytes())

    def test_desktop_entry_points_at_the_launcher(self):
        self.install()
        text = self.entry.read_text()
        self.assertIn('Exec="' + str(self.launcher) + '" %f', text)
        self.assertIn('TryExec=' + str(self.launcher), text)
        self.assertIn('Icon=' + str(self.icon), text)
        self.assertNotIn('python', text.lower())

    def test_launcher_runs_the_current_release(self):
        self.install()
        text = self.launcher.read_text()
        self.assertIn('exec "$release/launch.sh" "$@"', text)
        self.assertIn('DISABLE_GAMESCOPE_WSI=1', text)
        self.assertIn('XR_RUNTIME_JSON', text)
        self.assertNotIn('python', text.lower())

    def test_desktop_entry_uses_registered_categories(self):
        self.install()
        text = self.entry.read_text()
        self.assertIn('Categories=AudioVideo;Video;Player;', text)
        self.assertNotIn(';VR;', text)

    def test_the_command_location_is_reported_when_it_is_not_on_path(self):
        result = self.run_installer('--install', '--no-steam', '--yes',
                                    env=self.environment(PATH='/usr/bin:/bin'))
        self.assertIn('is not in your PATH', result.stdout)
        quiet = self.prefix / 'bin'
        result = self.run_installer('--install', '--no-steam', '--yes',
                                    env=self.environment(PATH=str(quiet) + ':/usr/bin:/bin'))
        self.assertNotIn('is not in your PATH', result.stdout)

    def test_refuses_to_write_through_a_symlinked_launcher(self):
        elsewhere = self.home / 'elsewhere'
        elsewhere.mkdir()
        (self.prefix / 'bin').mkdir(parents=True)
        (self.prefix / 'bin/lumaframevr').symlink_to(elsewhere / 'target')
        result = self.run_installer('--install', '--no-steam', '--yes', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('refusing to write through the symlink', result.stdout + result.stderr)
        self.assertFalse((elsewhere / 'target').exists(), 'the link target must stay untouched')

    def test_refuses_a_bin_directory_that_leaves_the_prefix(self):
        elsewhere = self.home / 'elsewhere'
        elsewhere.mkdir()
        self.prefix.mkdir(parents=True)
        (self.prefix / 'bin').symlink_to(elsewhere)
        result = self.run_installer('--install', '--no-steam', '--yes', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('refusing to write outside the prefix', result.stdout + result.stderr)
        self.assertEqual([], list(elsewhere.iterdir()))

    def test_add_to_path_is_opt_in_idempotent_and_removed_on_uninstall(self):
        profile = self.home / '.profile'
        profile.write_text('# user profile\nkeep this line\n')
        self.run_installer('--install', '--no-steam', '--yes')
        self.assertNotIn('LumaFrameVR', profile.read_text())
        self.install('--add-to-path')
        text = profile.read_text()
        self.assertIn('keep this line', text)
        self.assertIn('# LumaFrameVR PATH', text)
        self.assertIn('export PATH="' + str(self.prefix / 'bin') + ':$PATH"', text)
        self.install('--add-to-path')
        self.assertEqual(text.count('# LumaFrameVR PATH'), profile.read_text().count('# LumaFrameVR PATH'))
        self.run_installer('--uninstall')
        after = profile.read_text()
        self.assertIn('keep this line', after)
        self.assertNotIn('LumaFrameVR', after)

    def test_no_path_note_when_the_command_directory_is_already_on_path(self):
        result = self.run_installer('--install', '--no-steam', '--yes',
                                    env=self.environment(PATH=str(self.prefix / 'bin') + ':/usr/bin:/bin'))
        self.assertNotIn('is not in your PATH', result.stdout)
        self.assertNotIn('Added', result.stdout)

    def test_running_the_script_without_a_payload_says_so(self):
        bare = self.home / 'bare'
        bare.mkdir()
        (bare / 'install.sh').write_bytes((INSTALLER / 'install.sh.in').read_bytes())
        result = self.run_installer('--install', '--no-steam', '--yes', script=bare / 'install.sh', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('no release payload found', result.stdout + result.stderr)
        self.assertFalse(self.app.exists())

    def test_install_into_a_custom_prefix(self):
        self.run_installer('--install', '--no-steam', '--yes', '--prefix', str(self.home / 'apps'))
        self.assertTrue((self.home / 'apps/opt/lumaframevr/releases' / FIXTURE_VERSION).is_dir())
        self.assertTrue((self.home / 'apps/bin/lumaframevr').is_file())
        self.assertFalse(self.app.exists())

    def test_system_prefix_is_refused(self):
        result = self.run_installer('--install', '--prefix', '/usr', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('refusing the system prefix', result.stdout + result.stderr)

    def test_install_without_a_second_copy_of_the_payload(self):
        output = self.home / 'lumaframevr-fixture.run'
        self.build(output=output, payload=self.home / 'source/fixture-payload.tar.gz')
        os.remove(self.home / 'bundle/payload/fixture-payload.tar.gz')
        result = self.run_installer('--install', '--no-steam', '--yes', script=output, expect=None)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_installed()

    def test_a_damaged_embedded_payload_is_refused(self):
        output = self.home / 'lumaframevr-fixture.run'
        record = self.build(output=output, payload=self.home / 'source/fixture-payload.tar.gz')
        data = bytearray(output.read_bytes())
        data[record['payload_offset'] + 64] ^= 0xFF
        output.write_bytes(data)
        result = self.run_installer('--install', '--no-steam', '--yes', script=output, expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum mismatch', result.stdout + result.stderr)
        self.assertFalse(self.app.exists(), 'a failed install must not leave a partial release')

    def test_a_damaged_payload_file_is_refused(self):
        payload = self.bundle / 'payload/fixture-payload.tar.gz'
        data = bytearray(payload.read_bytes())
        data[-40] ^= 0xFF
        payload.write_bytes(data)
        result = self.run_installer('--install', '--no-steam', '--yes', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum mismatch', result.stdout + result.stderr)
        self.assertFalse(self.app.exists())

    def test_a_damaged_payload_file_in_a_matching_run_is_refused(self):
        payload = self.home / 'source/fixture-payload.tar.gz'
        data = bytearray(payload.read_bytes())
        data[-40] ^= 0xFF
        payload.write_bytes(data)
        result = self.run_installer('--install', '--no-steam', '--yes', '--payload',
                                    str(payload), expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum mismatch', result.stdout + result.stderr)
        self.assertFalse(self.app.exists())

    def test_a_payload_whose_contents_do_not_match_its_checksums_is_refused(self):
        tampered = write_payload(self.home / 'source', 'tampered-payload.tar.gz',
                                 tamper_after_sums='launch.sh')
        bundle = self.home / 'tampered-bundle'
        self.build(bundle=bundle, payload=tampered)
        result = self.run_installer('--install', '--no-steam', '--yes', script=bundle / 'install.sh',
                                    expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('damaged or incomplete', result.stdout + result.stderr)
        self.assertFalse(self.app.exists())

    def test_a_foreign_release_directory_is_refused(self):
        self.release.mkdir(parents=True)
        (self.release / 'keep-me').write_text('user data')
        result = self.run_installer('--install', '--no-steam', '--yes', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('was not created by this installer', result.stdout + result.stderr)
        self.assertTrue((self.release / 'keep-me').is_file())

    def test_low_free_space_is_refused(self):
        env = self.environment(LUMAFRAMEVR_INSTALLER_FREE_KB='1')
        result = self.run_installer('--install', '--no-steam', '--yes', env=env, expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('not enough free space', result.stdout + result.stderr)


class RepairTests(InstallerCase):
    def test_repair_restores_missing_launcher_entry_and_icon(self):
        self.install()
        for path in (self.launcher, self.entry, self.icon):
            path.unlink()
        self.run_installer('--repair')
        self.assert_installed()

    def test_repair_restores_a_damaged_file_from_the_payload(self):
        self.install()
        text = (self.release / 'launch.sh').read_bytes()
        (self.release / 'launch.sh').write_bytes(b'corrupted\n')
        self.run_installer('--repair')
        self.assertEqual(text, (self.release / 'launch.sh').read_bytes())

    def test_repair_keeps_user_settings(self):
        self.install()
        settings = self.home / '.config/lumaframevr/viewer.state'
        settings.parent.mkdir(parents=True, exist_ok=True)
        settings.write_text('settings')
        self.run_installer('--repair')
        self.assertEqual('settings', settings.read_text())

    def test_repair_without_an_installation_fails(self):
        result = self.run_installer('--repair', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('nothing to repair', result.stdout + result.stderr)


class UninstallTests(InstallerCase):
    def test_uninstall_removes_what_was_installed_and_keeps_user_data(self):
        self.install()
        settings = self.home / '.config/lumaframevr/viewer.state'
        settings.parent.mkdir(parents=True, exist_ok=True)
        settings.write_text('settings')
        thumbnails = self.home / '.cache/lumaframevr/thumbnails-v1'
        thumbnails.mkdir(parents=True)
        (thumbnails / 'tile.bin').write_bytes(b'tile')
        result = self.run_installer('--uninstall')
        self.assertFalse(self.app.exists())
        self.assertFalse(self.launcher.exists())
        self.assertFalse(self.entry.exists())
        self.assertFalse(self.icon.exists())
        self.assertFalse(self.record.exists())
        self.assertTrue(settings.is_file(), 'settings must survive a plain uninstall')
        self.assertTrue((thumbnails / 'tile.bin').is_file())
        self.assertIn('kept', result.stdout)

    def test_purge_removes_user_data_too(self):
        self.install()
        settings = self.home / '.config/lumaframevr/viewer.state'
        settings.parent.mkdir(parents=True, exist_ok=True)
        settings.write_text('settings')
        self.run_installer('--uninstall', '--purge')
        self.assertFalse(settings.exists())
        self.assertFalse(self.app.exists())

    def test_uninstall_without_an_installation_fails(self):
        result = self.run_installer('--uninstall', expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('nothing to remove', result.stdout + result.stderr)

    def test_uninstall_leaves_foreign_files_in_the_prefix_alone(self):
        self.install()
        neighbour = self.prefix / 'bin/other-tool'
        neighbour.write_text('not ours')
        self.run_installer('--uninstall')
        self.assertEqual('not ours', neighbour.read_text())


class SteamTests(InstallerCase):
    def setUp(self):
        super().setUp()
        self.userdata = self.home / 'steam/userdata/12345'
        (self.userdata / 'config').mkdir(parents=True)
        self.shortcuts = self.userdata / 'config/shortcuts.vdf'
        other = [steam_shortcut.Node(2, 'appid', 1234),
                 steam_shortcut.Node(1, 'AppName', 'Something Else'),
                 steam_shortcut.Node(1, 'Exe', '"/usr/bin/other"')]
        self.shortcuts.write_bytes(steam_shortcut.dump(
            [steam_shortcut.Node(0, 'shortcuts', [steam_shortcut.Node(0, '0', other)])]))
        self.steam_env = self.environment(LUMAFRAMEVR_STEAM_USERDATA=str(self.userdata))

    def entries(self):
        roots = steam_shortcut.parse(self.shortcuts.read_bytes())
        shortcuts = [node for node in roots if node.key.lower() == 'shortcuts'][0]
        return [steam_shortcut.mapping(entry.value) for entry in shortcuts.value]

    def test_install_with_the_steam_option_adds_the_shortcut(self):
        self.run_installer('--install', '--yes', '--steam', env=self.steam_env)
        self.assert_installed()
        names = [entry.get('appname') for entry in self.entries()]
        self.assertIn('LumaFrameVR', names)
        self.assertIn('Something Else', names)
        mine = [entry for entry in self.entries() if entry.get('appname') == 'LumaFrameVR'][0]
        self.assertEqual('"' + str(self.launcher) + '"', mine['exe'])
        self.assertEqual(1, mine['openvr'])
        self.assertEqual(str(self.app / 'current'), mine['startdir'].strip('"'))

    def test_steam_add_then_remove_keeps_unrelated_shortcuts(self):
        self.install()
        before = self.entries()[0]
        self.run_installer('--steam-add', env=self.steam_env)
        self.assertEqual(2, len(self.entries()))
        self.run_installer('--steam-remove', env=self.steam_env)
        after = self.entries()
        self.assertEqual(1, len(after))
        self.assertEqual(before, after[0])

    def test_steam_add_is_stable_across_repeats(self):
        self.install(env=self.steam_env)
        self.run_installer('--steam-add', env=self.steam_env)
        first = self.shortcuts.read_bytes()
        self.run_installer('--steam-add', env=self.steam_env)
        self.assertEqual(first, self.shortcuts.read_bytes())
        self.assertEqual(2, len(self.entries()))

    def test_uninstall_removes_the_steam_shortcut(self):
        self.run_installer('--install', '--yes', '--steam', env=self.steam_env)
        self.run_installer('--uninstall', env=self.steam_env, expect=None)
        self.assertEqual(['Something Else'], [entry.get('appname') for entry in self.entries()])

    def test_a_backup_of_shortcuts_is_written(self):
        self.install()
        self.run_installer('--steam-add', env=self.steam_env)
        backups = list(self.shortcuts.parent.glob('shortcuts.vdf.lumaframevr-*'))
        self.assertEqual(1, len(backups))
        self.assertEqual(self.shortcuts.read_bytes()[0:4], backups[0].read_bytes()[0:4])

    def test_steam_names_are_not_hijacked(self):
        other = [steam_shortcut.Node(2, 'appid', 4321),
                 steam_shortcut.Node(1, 'AppName', 'LumaFrameVR'),
                 steam_shortcut.Node(1, 'Exe', '"/usr/bin/somebody-else"')]
        self.shortcuts.write_bytes(steam_shortcut.dump(
            [steam_shortcut.Node(0, 'shortcuts', [steam_shortcut.Node(0, '0', other)])]))
        self.install()
        result = self.run_installer('--steam-add', env=self.steam_env, expect=None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('already called LumaFrameVR', result.stdout + result.stderr)
        self.assertEqual(['LumaFrameVR'], [entry.get('appname') for entry in self.entries()])


if __name__ == '__main__':
    unittest.main()
