import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ManifestWriterTest(unittest.TestCase):
    def test_manifest_describes_actual_payload(self):
        spec = importlib.util.spec_from_file_location('update_manifest', ROOT / 'scripts/update-manifest.py')
        self.assertTrue(spec.origin and Path(spec.origin).exists(), 'manifest writer is not implemented')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR')) as temporary:
            payload = Path(temporary) / 'payload'
            (payload / 'runtime').mkdir(parents=True)
            library = payload / 'runtime/libexample.so.2.7'
            library.write_bytes(b'library fixture')
            (payload / 'runtime/libexample.so.2').symlink_to(library.name)
            (payload / 'runtime/ignored.txt').write_text('not a library')
            installer = Path(temporary) / 'release.run'
            installer.write_bytes(b'installer fixture')
            result = module.build_manifest(payload, installer, '1.2.3', 'https://example.test/releases/1.2.3', 'notes')
            self.assertEqual(result['version'], '1.2.3')
            self.assertEqual(result['installer'], {'url': 'https://example.test/releases/1.2.3/release.run', 'size': 17,
                                                   'sha256': hashlib.sha256(installer.read_bytes()).hexdigest()})
            self.assertEqual(result['libraries'], [{'name': 'runtime/libexample.so.2.7', 'version': '2.7',
                                                    'sha256': hashlib.sha256(library.read_bytes()).hexdigest()}])
            self.assertEqual(result['notes'], 'notes')
            output = Path(temporary) / 'latest.json'
            import subprocess
            import sys
            subprocess.run([sys.executable, '-B', str(ROOT / 'scripts/update-manifest.py'), '--payload', str(payload),
                            '--installer', str(installer), '--version', '1.2.3', '--release-url', 'https://example.test',
                            '--output', str(output)], check=True)
            self.assertTrue(output.with_suffix('.json.sha256').exists(), 'manifest checksum must be published beside latest.json')
            self.assertEqual(output.with_suffix('.json.sha256').read_text(), hashlib.sha256(output.read_bytes()).hexdigest() + '  latest.json\n')
            library.unlink()
            (payload / 'runtime/libexample.so.2').unlink()
            self.assertEqual(module.build_manifest(payload, installer, '1.2.3', 'https://example.test')['libraries'], [])


if __name__ == '__main__':
    unittest.main()
