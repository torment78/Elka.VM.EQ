"""Format-only exports, following ElkaVoicePlay; no cropping or repainting."""
from pathlib import Path
from PIL import Image
import hashlib
import json
import zipfile

repo = Path(__file__).resolve().parents[1]
graphics = repo / 'graphics'
manifest = []
for orientation in ('horizontal', 'vertical'):
    with Image.open(graphics / f'vm-eq-{orientation}.png') as original:
        image = original.convert('RGB')
        target = graphics / f'vm-eq-{orientation}-under-1mb.jpg'
        for quality in range(94, 39, -2):
            image.save(target, 'JPEG', quality=quality, optimize=True, progressive=True)
            if target.stat().st_size < 1_000_000:
                break
        assert target.stat().st_size < 1_000_000, target
        manifest.append({'file': target.name, 'bytes': target.stat().st_size,
                         'pixels': image.size, 'jpeg_quality': quality})

sizes = [(n, n) for n in (16, 24, 32, 48, 64, 128, 256)]
with Image.open(graphics / 'vm-eq-icon.png') as original:
    icon = original.convert('RGBA')
    assert icon.width == icon.height
    assert icon.getchannel('A').getextrema()[0] == 0, 'Icon must have transparency'
    icon.save(graphics / 'ElkaVmEq.ico', format='ICO', sizes=sizes)
    icon.resize((256, 256), Image.Resampling.LANCZOS).save(graphics / 'vm-eq-icon-256.png')
with Image.open(graphics / 'ElkaVmEq.ico') as ico:
    assert ico.ico.sizes() == set(sizes)
    for size in sizes:
        assert ico.ico.getimage(size).size == size

for item in manifest:
    print(item)
print('ICO verified:', sizes)
for path in sorted(graphics.iterdir()):
    if path.suffix.lower() not in ('.png', '.jpg', '.ico'):
        continue
    if path.suffix.lower() != '.ico':
        with Image.open(path) as image:
            image.verify()
    print(path.name, path.stat().st_size)

(graphics / 'exports.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
files = sorted(p for p in graphics.iterdir() if p.is_file() and p.name != 'SHA256SUMS.txt')
(graphics / 'SHA256SUMS.txt').write_text(''.join(
    f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in files), encoding='ascii')
archive = repo / 'artifacts' / 'Elka-VM-EQ-artwork.zip'
archive.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
    for path in sorted(graphics.iterdir()):
        if path.is_file():
            bundle.write(path, 'Elka VM EQ Artwork/' + path.name)
with zipfile.ZipFile(archive) as bundle:
    assert bundle.testzip() is None
    for path in graphics.iterdir():
        if path.is_file():
            assert bundle.read('Elka VM EQ Artwork/' + path.name) == path.read_bytes()
print('Verified ZIP:', archive, archive.stat().st_size, 'bytes')
