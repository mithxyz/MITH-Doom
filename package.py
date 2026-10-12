"""Build shareable ZIPs with source and executable permissions preserved."""
from pathlib import Path
import argparse, hashlib, json, zipfile

ROOT = Path(__file__).resolve().parent
VERSION = '0.1.1'
parser = argparse.ArgumentParser()
parser.add_argument('--platform', choices=['mac', 'windows', 'all'], default='all')
args = parser.parse_args()
DIST = ROOT/'dist'; DIST.mkdir(exist_ok=True)

def write_tree(archive, folder, prefix):
    for path in sorted(folder.rglob('*')):
        if path.is_file() and '__pycache__' not in path.parts and path.name != '.DS_Store':
            archive.write(path, prefix/path.relative_to(folder))

source = DIST/f'MITH Doom {VERSION} Source.zip'
with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as archive:
    prefix = Path(f'MITH Doom {VERSION} Source')
    for folder in ['src', 'tests', 'vendor', '.github', 'packaging']:
        if (ROOT/folder).exists(): write_tree(archive, ROOT/folder, prefix/folder)
    for name in ['README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'DEPENDENCIES.lock',
                 'build.sh', 'build-windows.sh', 'package.py', '.gitignore']:
        archive.write(ROOT/name, prefix/name)

packages = [source]
platforms = ['mac', 'windows'] if args.platform == 'all' else [args.platform]
for platform in platforms:
    label = 'Mac' if platform == 'mac' else 'Windows'
    target = DIST/f'MITH Doom {VERSION} {label}.zip'
    prefix = Path(f'MITH Doom {VERSION} {label}')
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        if platform == 'mac':
            bundle = DIST/'MITH Doom.bundle'
            if not bundle.exists(): raise SystemExit('Build the Mac bundle first.')
            write_tree(archive, bundle, prefix/bundle.name)
        else:
            dll = DIST/'windows/MITH Doom.dll'
            if not dll.exists(): raise SystemExit('Build the Windows DLL first.')
            archive.write(dll, prefix/dll.name)
            write_tree(archive, DIST/'windows/MITH Doom Resources', prefix/'MITH Doom Resources')
        for name in ['LICENSE', 'THIRD_PARTY_NOTICES.md']:
            archive.write(ROOT/name, prefix/name)
        archive.write(ROOT/'README.md', prefix/'README.md')
        archive.write(ROOT/f'packaging/{platform}-INSTALL.txt', prefix/'START HERE.txt')
        archive.write(source, prefix/'Source'/source.name)
        verification = ROOT/'previews'/('validation.json' if platform == 'mac' else 'windows-validation.json')
        if verification.exists(): archive.write(verification, prefix/'Verification'/verification.name)
    packages.append(target)

for path in packages:
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert not any(name.lower().endswith(('.wad', 'doom19s.zip', 'doom95.zip')) for name in archive.namelist())
manifest = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in packages}
(DIST/'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name, digest in manifest.items()))
(DIST/'release-manifest.json').write_text(json.dumps({'version': VERSION, 'brand': 'MITH.xyz', 'sha256': manifest}, indent=2)+'\n')
for path in packages: print(path)
