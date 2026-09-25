# build_ico.py
import subprocess
import sys
import os
from pathlib import Path
from PIL import Image


svg_path = 'f.svg'
ico_path = 'f.ico'
tmp_path = '/tmp/tmp_pngs'

sizes = [16, 24, 32, 48, 64, 128]


svg_path = Path(svg_path)
ico_path = Path(ico_path)
tmp_path = Path(tmp_path)
temp_pngs = []

os.makedirs(tmp_path, exist_ok=True)

try:
    for size in sizes:
        png_path = tmp_path / f'tmp_{size}.png'
        
        cmd = [
            "inkscape", str(svg_path),
            "--export-type=png",
            f"--export-width={size}",
            f"--export-height={size}",
            f"--export-filename={png_path}"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Inkscape error for {size}px:\n{result.stderr}", file=sys.stderr)
            sys.exit(1)

        temp_pngs.append(png_path)


    images = []
    for png_path in temp_pngs:
        img = Image.open(png_path)
        
        # 转为 1-bit 黑白
        # 先转灰度再二值化，比直接 convert("1") 阈值控制更好
        #img = img.convert("L").point(lambda x: 255 if x > 127 else 0, "1")
    
        # 转为调色板模式，自动减少颜色数
        # colors=16 即 4-bit，colors=256 即 8-bit
        img = img.convert("RGBA").quantize(colors=8, method=Image.Quantize.FASTOCTREE)
        
        images.append(img)

    # Pillow 检测到 mode="1" 或 mode="P" 时，会自动使用 BMP 编码写入 ICO
    images[-1].save(
        str(ico_path), format="ICO",
        sizes=[(s, s) for s in sizes],
        append_images=images[:-1]
    )

    final_size = ico_path.stat().st_size
    print(f"Generated {ico_path} ({final_size:,} bytes)")

finally:
    # 清理临时 PNG
    '''
    for png_path in temp_pngs:
        if png_path.exists():
            png_path.unlink()
    '''
