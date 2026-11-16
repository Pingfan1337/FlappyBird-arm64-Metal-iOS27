#!/usr/bin/env python3
"""Inspect real Mach-O slices and imports, not just Info.plist version claims."""
from pathlib import Path
import struct,json,zipfile,plistlib,hashlib,sys
from legacy_toolchain import OLD_CXX_EXPORTS
ROOT=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'dist/FlappyBird-iOS5-27-Universal-unsigned.ipa'
def unpack(fmt,b,off):return struct.unpack_from(fmt,b,off)
with zipfile.ZipFile(p) as archive:
    assert archive.testzip() is None
    info=plistlib.loads(archive.read('Payload/FlappyBird.app/Info.plist'))
    assert info['MinimumOSVersion']=='5.0'
    assert info['UIDeviceFamily']==[1,2]
    assert 'UIRequiredDeviceCapabilities' not in info
    assert any('LaunchScreen.storyboardc/' in n for n in archive.namelist())
    assert 'Payload/FlappyBird.app/Default-568h@2x.png' in archive.namelist()
    data=archive.read('Payload/FlappyBird.app/FlappyBird')
    assert unpack('>I',data,0)[0]==0xcafebabe,'Must use the legacy-compatible FAT32 container'
    count=unpack('>I',data,4)[0];assert count==2
    reports=[]
    for i in range(count):
        cpu,sub,offset,size,align=unpack('>IIIII',data,8+20*i);b=data[offset:offset+size]
        is64=cpu==0x100000c;name='arm64' if is64 else 'armv7';assert cpu in (12,0x100000c)
        expected=0x70000 if is64 else 0x50000
        assert unpack('<I',b,0)[0]==(0xfeedfacf if is64 else 0xfeedface)
        ncmds=unpack('<I',b,16)[0];pos=32 if is64 else 28;imports={};providers={};libraries=[];commands=[];version=None;symtab=None
        for _ in range(ncmds):
            cmd,length=unpack('<II',b,pos);commands.append(cmd)
            assert cmd!=0x80000034,'Chained fixups cannot be used by legacy dyld'
            if cmd==0x25:version=unpack('<I',b,pos+8)[0]
            if cmd==2:symtab=unpack('<IIII',b,pos+8)
            if cmd in (0xc,0x80000018):
                n=unpack('<I',b,pos+8)[0];text=b[pos+n:pos+length].split(b'\0')[0].decode();libraries.append({'path':text,'weak':cmd==0x80000018})
            pos+=length
        assert version==expected,(name,version)
        assert (0x80000028 in commands) if is64 else (5 in commands),'Incorrect legacy entry-point format'
        symoff,nsyms,stroff,strsize=symtab
        for n in range(nsyms):
            strx,typ,section,desc,value=unpack('<IBBHQ' if is64 else '<IBBHI',b,symoff+n*(16 if is64 else 12))
            if typ&0xe==0 and strx:
                symbol=b[stroff+strx:stroff+strsize].split(b'\0')[0].decode();imports[symbol]=bool(desc&0x40)
                ordinal=desc>>8
                if 0<ordinal<=len(libraries):providers[symbol]=libraries[ordinal-1]['path']
        for symbol in ['___isPlatformVersionAtLeast','___isOSVersionAtLeast','__availability_version_check','_objc_claimAutoreleasedReturnValue','___chkstk_darwin','_clock_gettime']:
            assert symbol not in imports,(name,'new runtime dependency',symbol)
        optional=['_OBJC_CLASS_$_AVAudioEngine','_OBJC_CLASS_$_AVAudioFile','_dispatch_queue_attr_make_with_qos_class']
        if is64:optional+=['_OBJC_CLASS_$_CAMetalDisplayLink','_OBJC_CLASS_$_CAMetalLayer','_CAFrameRateRangeMake','_MTLCreateSystemDefaultDevice','_OBJC_CLASS_$_UIWindowScene']
        for symbol in optional:assert imports.get(symbol) is True,(name,'optional API must be weak',symbol)
        if not is64:
            assert not any('Metal' in lib['path'] for lib in libraries)
            for symbol in OLD_CXX_EXPORTS:assert providers.get(symbol)=='/usr/lib/libc++.1.dylib',(symbol,'must bind to iOS 5 provider')
            for symbol in ['_OBJC_CLASS_$_NSObject','_OBJC_METACLASS_$_NSObject']:assert providers.get(symbol)=='/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation',(symbol,'must bind to iOS 5 provider')
        assert b'/Users/' not in b,'No personal source paths in executable'
        reports.append({'arch':name,'minimum':f'{version>>16}.{version>>8&255}','entry':'LC_MAIN' if is64 else 'LC_UNIXTHREAD','undefined_symbols':len(imports),'weak_imports':sum(imports.values()),'libraries':libraries})
    for name,digest in json.loads((ROOT/'resources/sha256.json').read_text()).items():assert hashlib.sha256(archive.read('Payload/FlappyBird.app/'+name)).hexdigest()==digest
result={'ipa':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'slices':reports,'status':'static checks passed; legacy device execution pending'}
(ROOT/'dist/compatibility-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
