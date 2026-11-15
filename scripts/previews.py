#!/usr/bin/env python3
"""Render documentation previews with the actual Metal shaders; no simulator."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
output=ROOT/'build/render_previews'
output.parent.mkdir(exist_ok=True)
command=['xcrun','clang++','-std=c++17','-O2','-fobjc-arc','-ffp-contract=off']
for name in ['Foundation','Metal','MetalKit','CoreGraphics','ImageIO']:command+=['-framework',name]
subprocess.run(command+[str(ROOT/'scripts/render_previews.mm'),'-o',str(output)],cwd=ROOT,check=True)
subprocess.run([str(output)],cwd=ROOT,check=True)
