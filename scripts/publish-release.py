#!/usr/bin/env python3
"""Validate the complete build set before publishing a GitHub release."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

PLATFORMS = ['macos-universal', 'windows-x86_64', 'linux-x86_64', 'switch', 'web']


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def prepare(version, commit, directory):
    if version != 'development' and not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', version):
        raise ValueError('Use development or a version tag such as v0.1.1 or v0.2.0-rc.1')
    expected = {f'BadToys3D-{version}-{platform}.zip' for platform in PLATFORMS}
    if {p.name for p in directory.glob('*.zip')} != expected:
        raise ValueError('Release must contain exactly one ZIP for each of the five platforms')
    for platform in PLATFORMS:
        name = f'BadToys3D-{version}-{platform}'
        with zipfile.ZipFile(directory / (name + '.zip')) as archive:
            if archive.testzip() is not None:
                raise ValueError(f'Corrupt archive: {name}')
            info = json.loads(archive.read(name + '/BUILD_INFO.json'))
            if (info['version'], info['source_commit'], info['platform']) != (version, commit, platform):
                raise ValueError(f'Mismatched build provenance: {name}')
            if info['game_data_included'] or info['embedded_menu_midi']:
                raise ValueError(f'Game assets declared in {name}')
            if any(Path(p).name.lower() in {'data.pck', 'm1.dat', 'config.dat', 'savegame.dat'} for p in archive.namelist()):
                raise ValueError(f'Game assets found in {name}')
    manifest = directory / 'BUILD_INFO.json'
    manifest.write_text(json.dumps({
        'version': version, 'source_commit': commit, 'platforms': PLATFORMS,
        'game_data_included': False, 'embedded_menu_midi': False,
        'validation': 'All platforms compiled; native macOS/Linux CTests and build script checks passed. Archives validated.',
        'limitations': 'No automated interactive gameplay testing. macOS is ad-hoc signed, not notarized. Windows is unsigned.',
    }, indent=2) + '\n')
    assets = sorted(directory.glob('*.zip')) + [manifest]
    checksums = directory / 'SHA256SUMS.txt'
    checksums.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in assets))
    return assets + [checksums]


def publish(version, commit):
    directory = Path('dist/releases')
    assets = prepare(version, commit, directory)
    development = version == 'development'
    if development:
        # A manually retried old run must not replace the latest development build.
        head = run('git', 'ls-remote', 'origin', 'refs/heads/main').split()[0]
        if head != commit:
            print('Skipping outdated development build; main has moved.')
            return
    existing = json.loads(run('gh', 'release', 'list', '--limit', '1000', '--json', 'tagName,isDraft'))
    release = next((r for r in existing if r['tagName'] == version), None)
    if release and not development and not release['isDraft']:
        raise ValueError('Numbered release already published; push a new version tag instead')
    if development and not release:
        run('git', 'push', 'origin', f'{commit}:refs/tags/development', '--force')
    notes = directory / 'release-notes.md'
    notes.write_text(f'''Built from `{commit}`.

**Game data is not included. Provide your own `data.pck` and a valid license for the original game.** Extract the ZIP for your platform before playing.

| Platform | How to run |
| --- | --- |
| macOS universal | macOS 11+. Put `data.pck` beside **Bad Toys 3D.app** and open the app. |
| Windows x86-64 | Put `data.pck` beside `bt3d_raylib.exe` and launch it. |
| Linux x86-64 | Ubuntu 22.04/glibc 2.35+. Put `data.pck` beside `bt3d_raylib` and run it from a graphical desktop. |
| Switch | Put the NRO and `data.pck` in `sdmc:/switch/bt3d/`; launch through Homebrew Menu. |
| Web | Serve the extracted folder with `python3 -m http.server 8000`, open http://localhost:8000/, select `data.pck` and click Start. |

Each ZIP includes instructions and source provenance. Web Start requests fullscreen; click during play to capture the mouse, Esc to release it. Browser saves use IndexedDB.

All five builds and native macOS/Linux tests passed. Interactive gameplay is not automatically tested. macOS is ad-hoc signed, not notarized; Windows is unsigned. Menu music is unavailable because the separate original MIDI asset is excluded. Android is not included.

This is an unofficial fan port, not affiliated with or endorsed by Tibo Software. [Official game website](https://www.tibosoftware.com/bad-toys.htm).
''')
    title = 'Development' if development else version
    prerelease = development or '-' in version
    if not release:
        run('gh', 'release', 'create', version, '--verify-tag', '--draft', '--title', title, '--notes-file', str(notes))
    elif development:
        # Hide the rolling release until every asset and checksum has been replaced.
        run('gh', 'release', 'edit', version, '--draft=true')
    run('gh', 'release', 'upload', version, *map(str, assets), '--clobber')
    if development and release:
        run('git', 'push', 'origin', f'{commit}:refs/tags/development', '--force')
    run('gh', 'release', 'edit', version, '--draft=false', f'--prerelease={str(prerelease).lower()}',
        '--latest=false' if prerelease else '--latest', '--title', title, '--notes-file', str(notes))
    print(run('gh', 'release', 'view', version, '--json', 'url', '--jq', '.url'))


if __name__ == '__main__':
    publish(*sys.argv[1:])
