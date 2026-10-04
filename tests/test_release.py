#!/usr/bin/env python3
"""Release provenance, asset exclusion and publication regression checks."""
import hashlib
import json
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
RELEASE = runpy.run_path(str(ROOT / 'scripts/publish-release.py'))
PREPARE = RELEASE['prepare']
PUBLISH = RELEASE['publish']
COMMIT = 'a' * 40


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        for platform in RELEASE['PLATFORMS']:
            name = 'BadToys3D-development-' + platform
            with zipfile.ZipFile(self.directory / (name + '.zip'), 'w') as archive:
                archive.writestr(name + '/BUILD_INFO.json', json.dumps({
                    'version': 'development', 'source_commit': COMMIT,
                    'platform': platform, 'game_data_included': False,
                    'embedded_menu_midi': False,
                }))

    def test_complete_set_and_checksums(self):
        assets = PREPARE('development', COMMIT, self.directory)
        self.assertEqual(len(assets), 7)
        for line in (self.directory / 'SHA256SUMS.txt').read_text().splitlines():
            digest, name = line.split('  ')
            self.assertEqual(digest, hashlib.sha256((self.directory / name).read_bytes()).hexdigest())

    def test_missing_platform_rejected(self):
        next(self.directory.glob('*.zip')).unlink()
        with self.assertRaisesRegex(ValueError, 'five platforms'):
            PREPARE('development', COMMIT, self.directory)

    def test_mismatched_commit_rejected(self):
        with self.assertRaisesRegex(ValueError, 'provenance'):
            PREPARE('development', 'b' * 40, self.directory)

    def test_game_assets_rejected(self):
        with zipfile.ZipFile(next(self.directory.glob('*.zip')), 'a') as archive:
            archive.writestr('nested/DATA.PCK', b'private data')
        with self.assertRaisesRegex(ValueError, 'assets found'):
            PREPARE('development', COMMIT, self.directory)

    def test_invalid_version_rejected(self):
        with self.assertRaisesRegex(ValueError, 'version tag'):
            PREPARE('../unexpected', COMMIT, self.directory)

    def test_numbered_release_never_overwritten(self):
        calls = []
        def command(*args):
            calls.append(args)
            return json.dumps([{'tagName': 'v0.1.0', 'isDraft': False}])
        with patch.dict(PUBLISH.__globals__, prepare=lambda *args: [], run=command):
            with self.assertRaisesRegex(ValueError, 'already published'):
                PUBLISH('v0.1.0', COMMIT)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][:3], ('gh', 'release', 'list'))

    def test_old_main_run_cannot_publish(self):
        calls = []
        def command(*args):
            calls.append(args)
            return 'b' * 40 + '\trefs/heads/main'
        with patch.dict(PUBLISH.__globals__, prepare=lambda *args: [], run=command):
            PUBLISH('development', COMMIT)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][:2], ('git', 'ls-remote'))

    def test_development_tag_moves_only_after_upload(self):
        calls = []
        def command(*args):
            calls.append(args)
            if args[:2] == ('git', 'ls-remote'):
                return COMMIT + '\trefs/heads/main'
            if args[:3] == ('gh', 'release', 'list'):
                return json.dumps([{'tagName': 'development', 'isDraft': False}])
            return ''
        with patch.dict(PUBLISH.__globals__, prepare=lambda *args: [], run=command), patch.object(Path, 'write_text'):
            PUBLISH('development', COMMIT)
        upload = next(i for i, c in enumerate(calls) if c[:3] == ('gh', 'release', 'upload'))
        move = next(i for i, c in enumerate(calls) if c[:2] == ('git', 'push'))
        hide = next(i for i, c in enumerate(calls) if '--draft=true' in c)
        show = next(i for i, c in enumerate(calls) if '--draft=false' in c)
        self.assertLess(hide, upload)
        self.assertLess(upload, move)
        self.assertLess(move, show)


if __name__ == '__main__':
    unittest.main()
