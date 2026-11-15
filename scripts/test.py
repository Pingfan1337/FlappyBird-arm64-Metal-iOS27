#!/usr/bin/env python3
"""Host-side regression checks. Run on macOS with the Xcode toolchain."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
build=ROOT/'build/tests'
build.mkdir(parents=True,exist_ok=True)
for source in sorted((ROOT/'tests').glob('*')):
    if source.suffix not in ('.cpp','.mm'):continue
    output=build/source.stem
    command=['xcrun','clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off']
    if source.suffix=='.mm':command+=['-fobjc-arc','-framework','Foundation','-framework','AVFoundation']
    subprocess.run(command+[str(source),'-o',str(output)],cwd=ROOT,check=True)
    subprocess.run([str(output)],cwd=ROOT,check=True)
print('All host regression checks passed.')
