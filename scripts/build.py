#!/usr/bin/env python3
"""Build a device-only IPA; no simulator, signing identity or third-party packages."""
from pathlib import Path
import hashlib,json,plistlib,shutil,subprocess,zipfile,os
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT
BUILD=Path(os.environ.get('FLAPPY_BUILD_DIR',str(ROOT/'build/device')))
APP=BUILD/'Payload/FlappyBird.app'
APP.mkdir(parents=True,exist_ok=True)
sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
subprocess.run(['xcrun','--sdk','iphoneos','clang++','-target','arm64-apple-ios17.0','-isysroot',sdk,'-std=c++17','-O2','-fobjc-arc','-ffp-contract=off','-Wall','-Wextra','-Wno-unused-parameter','-framework','UIKit','-framework','CoreGraphics','-framework','Metal','-framework','MetalKit','-framework','AVFoundation','-framework','QuartzCore',str(ROOT/'src/App/main.mm'),'-o',str(APP/'FlappyBird')],check=True)
resources=ROOT/'resources'
manifest=json.loads((resources/'sha256.json').read_text())
required=['atlas.png','atlas.txt','sfx_point.aif','sfx_swooshing.caf','sfx_die.caf','sfx_hit.caf','sfx_wing.caf','AppIcon60x60@2x.png']
for name in required:
    source=resources/name
    assert hashlib.sha256(source.read_bytes()).hexdigest()==manifest[name],name
    shutil.copy2(source,APP/name)
info={
 'CFBundleDevelopmentRegion':'en','CFBundleDisplayName':'Flappy Bird','CFBundleExecutable':'FlappyBird',
 'CFBundleIdentifier':'com.dotgears.flap','CFBundleInfoDictionaryVersion':'6.0','CFBundleName':'Flappy Bird',
 'CFBundlePackageType':'APPL','CFBundleShortVersionString':'1.2','CFBundleVersion':'12005',
 'CFBundleSupportedPlatforms':['iPhoneOS'],'LSRequiresIPhoneOS':True,'MinimumOSVersion':'17.0',
 'UIDeviceFamily':[1],'UIRequiredDeviceCapabilities':['arm64','metal'],
 'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait'],'UIStatusBarHidden':True,
 'CADisableMinimumFrameDurationOnPhone':True,'UIViewControllerBasedStatusBarAppearance':True,'UILaunchScreen':{},
 'CFBundleIcons':{'CFBundlePrimaryIcon':{'CFBundleIconFiles':['AppIcon60x60'],'UIPrerenderedIcon':True}},
 'UIApplicationSceneManifest':{'UIApplicationSupportsMultipleScenes':False,'UISceneConfigurations':{
   'UIWindowSceneSessionRoleApplication':[{'UISceneConfigurationName':'Game','UISceneDelegateClassName':'SceneDelegate'}]}},
}
(APP/'Info.plist').write_bytes(plistlib.dumps(info))
(APP/'PkgInfo').write_bytes(b'APPL????')
# Deliberately unsigned: the user's on-device tool supplies signing assets.
destination=ROOT/'dist'
destination.mkdir(exist_ok=True)
ipa=destination/'FlappyBird-1.2-Metal-smooth-unsigned.ipa'
with zipfile.ZipFile(ipa,'w',zipfile.ZIP_DEFLATED) as z:
    for f in sorted(APP.rglob('*')):
        if f.is_file(): z.write(f,f.relative_to(BUILD))
print(ipa)
digest=hashlib.sha256(ipa.read_bytes()).hexdigest()
(ipa.parent/'SHA256SUMS.txt').write_text(digest+'  '+ipa.name+'\n')
print('SHA256',digest)
