import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class UpdaterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = ROOT / 'update.py'
        if not path.exists():
            return
        spec = importlib.util.spec_from_file_location('updater', path)
        cls.updater = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.updater)

    def setUp(self):
        blocker = patch('socket.getaddrinfo', side_effect=AssertionError('network access is forbidden in updater tests'))
        blocker.start()
        self.addCleanup(blocker.stop)

    def test_check_and_verified_install_offline(self):
        self.assertTrue(hasattr(self.updater, 'run_update'), 'update execution is not implemented')
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temporary:
            prefix = Path(temporary)
            root = prefix / 'opt/lumaframevr/releases/1.0.0'
            root.mkdir(parents=True)
            (root / 'BUILD_INFO.json').write_text('{"version":"1.0.0"}')
            installer = (b'#!/bin/sh\nprintf "%s\\n" "$@" > "$LUMAFRAMEVR_TEST_ARGS"\n'
                         b'for prefix do :; done\napp="$prefix/opt/lumaframevr"\n'
                         b'mkdir -p "$app/releases/1.10.0"\n'
                         b'printf \'{"version":"1.10.0"}\' > "$app/releases/1.10.0/BUILD_INFO.json"\n'
                         b'ln -snf releases/1.10.0 "$app/current"\n')
            manifest = {'version': '1.10.0', 'installer': {'url': 'https://example.test/i.run',
                        'sha256': hashlib.sha256(installer).hexdigest(), 'size': len(installer)},
                        'libraries': [{'name': 'runtime/libnew.so.1', 'version': '1', 'sha256': 'a' * 64}],
                        'notes': 'New decoder'}
            bad_manifest_checksum = False
            def response(url):
                raw = json.dumps(manifest).encode()
                if url.endswith('latest.json.sha256'):
                    value = 'b' * 64 if bad_manifest_checksum else hashlib.sha256(raw).hexdigest()
                    return io.BytesIO((value + '  latest.json\n').encode())
                return io.BytesIO(raw if url.endswith('latest.json') else installer)
            output = io.StringIO()
            args = prefix / 'args'
            with patch.object(self.updater, 'https_open', side_effect=response), patch.dict(os.environ, {'LUMAFRAMEVR_TEST_ARGS': str(args)}), contextlib.redirect_stdout(output):
                self.assertEqual(self.updater.run_update(root, '--check'), 0)
                self.assertIn('AVAILABLE\t1.10.0\t', output.getvalue())
                self.assertIn('New decoder', output.getvalue())
                self.assertFalse(args.exists())
                self.assertEqual(self.updater.run_update(root, '--install'), 0)
                self.assertEqual(args.read_text().splitlines(), ['--update', '--yes', '--no-ui', '--no-steam', '--prefix', str(prefix)])
                args.unlink()
                manifest['installer']['sha256'] = 'b' * 64
                with self.assertRaisesRegex(ValueError, 'checksum'):
                    self.updater.run_update(root, '--install')
                self.assertFalse(args.exists(), 'checksum failure must not run installer')
                bad_manifest_checksum = True
                with self.assertRaisesRegex(ValueError, 'manifest checksum'):
                    self.updater.run_update(root, '--check')
                bad_manifest_checksum = False
                manifest['version'] = '1.0.0'
                self.assertEqual(self.updater.run_update(root, '--check'), 3)
                manifest['version'] = '0.9.0'
                self.assertEqual(self.updater.run_update(root, '--install'), 3)
                manifest['version'] = '1.10.0'
                manifest['installer']['sha256'] = hashlib.sha256(installer).hexdigest()
                self.assertTrue(hasattr(self.updater, 'verify_installed'), 'install success must read back current')
                (root.parents[1] / 'current').unlink()
                with self.assertRaisesRegex(RuntimeError, 'current'):
                    self.updater.verify_installed(root.parents[1], '1.10.0')

    def test_https_redirect_cannot_downgrade(self):
        self.assertTrue(hasattr(self.updater, 'HttpsRedirect'), 'HTTPS redirect protection is not implemented')
        with self.assertRaises(ValueError):
            self.updater.HttpsRedirect().redirect_request(None, None, 302, 'redirect', {}, 'http://example.test/i')

    def test_manifest_parser_from_string(self):
        self.assertTrue(hasattr(self, 'updater'), 'updater parser is not implemented')
        raw = json.dumps({'version': '1.10.0', 'installer': {'url': 'https://example.test/install.run',
                         'sha256': 'a' * 64, 'size': 42}, 'libraries': [], 'notes': 'A release'})
        self.assertEqual(self.updater.parse_manifest(raw)['version'], '1.10.0')

    def test_numeric_version_comparison(self):
        self.assertTrue(hasattr(self.updater, 'compare_versions'), 'version comparison is not implemented')
        for left, right, expected in [('1.10.0', '1.9.9', 1), ('1.0', '1.0.0', 0), ('2', '10', -1),
                                      ('1.0.1', '1', 1), ('01.002', '1.2', 0)]:
            with self.subTest(left=left, right=right):
                self.assertEqual(self.updater.compare_versions(left, right), expected)
        for invalid in ['', 'v1.0', '1.0-beta', '1..0', '1.0\n', '-1.0']:
            with self.assertRaises(ValueError):
                self.updater.compare_versions(invalid, '1')

    def test_manifest_rejects_unsafe_inputs(self):
        valid = {'version': '1.2.3', 'installer': {'url': 'https://example.test/i.run',
                 'sha256': 'a' * 64, 'size': 42}, 'libraries': []}
        for field, value in [('url', 'http://example.test/i.run'), ('url', 'https://user@example.test/i'),
                             ('sha256', 'wrong'), ('size', True), ('size', 0), ('size', -1)]:
            data = json.loads(json.dumps(valid))
            data['installer'][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                self.updater.parse_manifest(json.dumps(data))
        for text in ['{}', '[]', 'not json', json.dumps({**valid, 'libraries': [{}]}),
                     json.dumps({**valid, 'notes': 1})]:
            with self.assertRaises(ValueError):
                self.updater.parse_manifest(text)


if __name__ == '__main__':
    unittest.main()
