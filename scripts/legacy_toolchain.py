#!/usr/bin/env python3
"""Prepare a pinned, device-only legacy SDK/header cache inside build/."""
from pathlib import Path
import hashlib,re,tarfile,urllib.request,shutil
ROOT=Path(__file__).resolve().parents[1]
CACHE=ROOT/'build/toolchains'
SDK_URL='https://github.com/theos/sdks/releases/download/master-146e41f/iPhoneOS9.3.sdk.tar.xz'
SDK_SHA='39df195271aadab894e740ae957005b9af8090593399dffc9aa6204008de3cc1'
LIBCXX_COMMIT='de80a7d886d75188de210eb4d6f0768fdc4d4d0b'
LIBCXX_URL='https://api.github.com/repos/llvm-mirror/libcxx/tarball/'+LIBCXX_COMMIT
LIBCXX_SHA='bc8e6a02fbcf99556d6f09c0ef41977e0f2dd9a7040da473619fe9895cff5e08'
# These exports lived in libc++ on iOS 5 and moved to libc++abi later.
# Preserve their original two-level namespace provider when linking armv7.
OLD_CXX_EXPORTS=['__ZNSt8bad_castC1Ev','__ZNSt8bad_castD1Ev','__ZSt9terminatev','__ZTISt8bad_cast','__ZdaPv','__ZdlPv','__Znam','__Znwm']
def fetch(name,url,digest):
    CACHE.mkdir(parents=True,exist_ok=True);p=CACHE/name
    if not p.exists():
        print('Downloading legacy build dependency:',name,flush=True)
        with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'FlappyBird-compat-builder'})) as response,p.open('wb') as output:shutil.copyfileobj(response,output)
    if hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise RuntimeError('Download hash mismatch: '+str(p))
    return p

def extract(archive,destination,strip=False):
    destination.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive) as tar:
        for member in tar.getmembers():
            if strip:
                parts=Path(member.name).parts
                if len(parts)<2:continue
                member.name=str(Path(*parts[1:]))
            target=(destination/member.name).resolve()
            if not target.is_relative_to(destination.resolve()):raise RuntimeError('Unsafe archive path')
            tar.extract(member,destination,filter='data')

def prepare():
    sdk=CACHE/'iPhoneOS9.3.sdk';libcxx=CACHE/'libcxx'
    if not (CACHE/'prepared-v3').exists():
        archive=fetch('iPhoneOS9.3.sdk.tar.xz',SDK_URL,SDK_SHA)
        headers=fetch('libcxx37.tar.gz',LIBCXX_URL,LIBCXX_SHA)
        if sdk.exists():shutil.rmtree(sdk)
        if libcxx.exists():shutil.rmtree(libcxx)
        extract(archive,CACHE);extract(headers,libcxx,strip=True)
        # Old TBD v1 files combine simulator/device archs, which recent TAPI
        # misclassifies. Preserve device symbols and remove simulator sections.
        for p in sdk.rglob('*.tbd'):
            output=[]
            for part in re.split(r'(?m)(?=^  - archs:)',p.read_text()):
                match=re.search(r'archs:\s*\[([^\]]+)\]',part)
                if match:
                    arm=[a.strip() for a in match[1].split(',') if a.strip() in ('armv7','armv7s','arm64')]
                    if not arm and part.startswith('  - archs:'):continue
                    part=part[:match.start()]+'archs: [ '+', '.join(arm)+' ]'+part[match.end():]
                output.append(part)
            p.write_text(''.join(output))
        # The curated SDK omits these longstanding libSystem memory exports.
        p=sdk/'usr/lib/libSystem.B.tbd';p.write_text(p.read_text().replace('symbols:            [','symbols:            [ _memcmp, _memcpy, _memmove, _memset, _bzero, _bcopy,',1))
        p=sdk/'usr/lib/libc++abi.tbd';text=p.read_text()
        for symbol in OLD_CXX_EXPORTS:text=re.sub(r'(?<!\w)'+re.escape(symbol)+r'\s*,','',text)
        p.write_text(text)
        p=sdk/'usr/lib/libc++.1.tbd';p.write_text(p.read_text().replace('symbols:            [','symbols:            [ '+', '.join(OLD_CXX_EXPORTS)+',',1))
        # NSObject was exported by CoreFoundation before moving to libobjc.
        p=sdk/'usr/lib/libobjc.A.tbd';p.write_text(p.read_text().replace('objc-classes:       [ _NSObject,','objc-classes:       ['))
        p=sdk/'System/Library/Frameworks/CoreFoundation.framework/CoreFoundation.tbd';p.write_text(p.read_text().replace('objc-classes:       [','objc-classes:       [ _NSObject,',1))
        # Match the historical string constructor declaration under modern clang.
        p=libcxx/'include/string';text=p.read_text();text=text.replace('basic_string<_CharT, _Traits, _Allocator>::basic_string(const allocator_type& __a)\n    :','basic_string<_CharT, _Traits, _Allocator>::basic_string(const allocator_type& __a)\n#if _LIBCPP_STD_VER <= 14\n    _NOEXCEPT_(is_nothrow_copy_constructible<allocator_type>::value)\n#else\n    _NOEXCEPT\n#endif\n    :');p.write_text(text)
        (CACHE/'prepared-v3').write_text('Pinned SDK and headers; compatibility normalization version 3\n')
    return sdk,libcxx/'include'
if __name__=='__main__':
    print(*prepare(),sep='\n')
