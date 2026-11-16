#!/usr/bin/env python3
"""Optional export/provider audit against an unpacked historical binary SDK.

No SDK is downloaded by this script. A stub/export check does not execute the
application or prove that Objective-C selectors and drivers behave correctly.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk',required=True,type=Path)
parser.add_argument('--binary',required=True,type=Path)
parser.add_argument('--arch',required=True,choices=['armv7','arm64'])
parser.add_argument('--output',type=Path)
args=parser.parse_args()
cache={}

def command(tool,*arguments):
    return subprocess.check_output(['xcrun',tool,'-arch',args.arch,*map(str,arguments)],text=True)

def exports(install_name):
    if install_name in cache:return cache[install_name]
    path=args.sdk/install_name.lstrip('/')
    if not path.is_file():raise RuntimeError('Missing historical SDK library: '+str(path))
    # N_INDR aliases print "I symbol (indirect for target)"; the last token
    # is not the exported name. Keep the alias as well as ordinary symbols.
    symbols=set(re.findall(r'^\s*(?:[0-9a-fA-F]+\s+)?[A-Za-z?]\s+(\S+)',command('nm','-gU',path),re.MULTILINE))
    cache[install_name]=symbols
    # Follow only re-export edges; unrelated loaded libraries do not satisfy
    # imports in Mach-O's two-level namespace.
    for line in command('otool','-L',path).splitlines():
        if '(compatibility version ' in line and 'reexport' in line:
            symbols.update(exports(line.strip().split(' (')[0]))
    return symbols

libraries={}
for line in command('otool','-L',args.binary).splitlines():
    if '(compatibility version ' not in line:continue
    path=line.strip().split(' (')[0];name=Path(path).name
    short={'libSystem.B.dylib':'libSystem','libobjc.A.dylib':'libobjc','libc++.1.dylib':'libc++','libc++abi.dylib':'libc++abi'}.get(name,name)
    libraries[short]=path
checked=weak=0;missing=[]
for line in command('nm','-m','-u',args.binary).splitlines():
    if 'weak external' in line:weak+=1;continue
    match=re.search(r'external (\S+) \(from ([^)]+)\)',line)
    if not match:continue
    symbol,provider=match.groups();checked+=1
    if symbol not in exports(libraries[provider]):missing.append({'provider':provider,'symbol':symbol})
report={'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),'arch':args.arch,'sdk':args.sdk.name,'strong_imports_checked':checked,'weak_imports_excluded':weak,'missing':missing,'scope':'export/provider checks only; device execution pending'}
output=json.dumps(report,indent=2)+'\n'
if args.output:args.output.write_text(output)
print(output,end='')
raise SystemExit(bool(missing))
