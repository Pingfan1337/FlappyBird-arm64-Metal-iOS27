#!/usr/bin/env python3
"""Build one universal device IPA: armv7/iOS 5 + arm64/iOS 7, optional APIs."""
from pathlib import Path
import hashlib,json,plistlib,shutil,subprocess,zipfile,os,argparse
from legacy_toolchain import prepare
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--modern-only',action='store_true',help='Build only the iOS 17+ arm64 slice without legacy dependencies');args=parser.parse_args()
BUILD=Path(os.environ.get('FLAPPY_BUILD_DIR',str(ROOT/'build/device')))
APP=BUILD/'Payload/FlappyBird.app'
if APP.exists():shutil.rmtree(APP)
APP.mkdir(parents=True,exist_ok=True)
current=Path(subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip())
legacy,headers=(None,None) if args.modern_only else prepare()
slices=[]
for arch,minimum,sdk in ([('arm64','17.0',current)] if args.modern_only else [('armv7','5.0',legacy),('arm64','7.0',current)]):
    target=f'{arch}-apple-ios{minimum}';main=BUILD/(arch+'-main.o');availability=BUILD/(arch+'-availability.o');binary=BUILD/('FlappyBird-'+arch)
    command=['xcrun','clang++','-target',target,'-isysroot',str(sdk),'-std=c++17','-O2','-fobjc-arc','-fno-sized-deallocation','-ffp-contract=off','-Wall','-Wextra','-Wno-unused-parameter','-Wno-deprecated-declarations','-Werror=unguarded-availability-new']
    if headers:command+=['-nostdinc++','-isystem',str(headers)]
    subprocess.run(command+['-c',str(ROOT/'src/App/main.mm'),'-o',str(main)],check=True)
    subprocess.run(['xcrun','clang','-target',target,'-isysroot',str(sdk),'-c',str(ROOT/'src/App/Availability.m'),'-o',str(availability)],check=True)
    # Compile ARC separately, then link objects without ARC driver injection.
    # All ARC runtime calls used here are provided by iOS 5+ libobjc itself.
    link=['xcrun','clang++','-target',target,'-isysroot',str(sdk),str(main),str(availability),'-Wl,-no_fixup_chains']
    for framework in ['UIKit','Foundation','CoreGraphics','ImageIO','QuartzCore','AVFoundation']:link+=['-framework',framework]
    link+=['-weak_framework','Metal','-weak_framework','OpenGLES'] if arch=='arm64' else ['-framework','OpenGLES']
    link+=['-lobjc','-lc++abi','-o',str(binary)]
    subprocess.run(link,check=True);slices.append(binary)
if len(slices)>1:subprocess.run(['xcrun','lipo','-create',*map(str,slices),'-output',str(APP/'FlappyBird')],check=True)
else:shutil.copy2(slices[0],APP/'FlappyBird')
resources=ROOT/'resources';manifest=json.loads((resources/'sha256.json').read_text())
for name,digest in manifest.items():
    source=resources/name;assert hashlib.sha256(source.read_bytes()).hexdigest()==digest,name;shutil.copy2(source,APP/name)
subprocess.run(['xcrun','ibtool','--compile',str(APP/'LaunchScreen.storyboardc'),str(ROOT/'src/App/LaunchScreen.storyboard'),'--minimum-deployment-target','8.0','--target-device','iphone','--target-device','ipad'],check=True)
info={
 'CFBundleDevelopmentRegion':'en','CFBundleDisplayName':'Flappy Bird','CFBundleExecutable':'FlappyBird','CFBundleIdentifier':'com.dotgears.flap','CFBundleInfoDictionaryVersion':'6.0','CFBundleName':'Flappy Bird','CFBundlePackageType':'APPL',
 'CFBundleShortVersionString':'1.2','CFBundleVersion':'12006','CFBundleSupportedPlatforms':['iPhoneOS'],'LSRequiresIPhoneOS':True,'MinimumOSVersion':'17.0' if args.modern_only else '5.0','UIDeviceFamily':[1,2],
 'UISupportedInterfaceOrientations':['UIInterfaceOrientationPortrait'],'UIRequiresFullScreen':True,'UIStatusBarHidden':True,'CADisableMinimumFrameDurationOnPhone':True,'UIViewControllerBasedStatusBarAppearance':True,
 'UILaunchScreen':{},'UILaunchStoryboardName':'LaunchScreen',
 'CFBundleIconFiles':['AppIcon57x57','AppIcon60x60','AppIcon72x72'],'CFBundleIcons':{'CFBundlePrimaryIcon':{'CFBundleIconFiles':['AppIcon57x57','AppIcon60x60'],'UIPrerenderedIcon':True}},'CFBundleIcons~ipad':{'CFBundlePrimaryIcon':{'CFBundleIconFiles':['AppIcon72x72'],'UIPrerenderedIcon':True}},
 'UIApplicationSceneManifest':{'UIApplicationSupportsMultipleScenes':False,'UISceneConfigurations':{'UIWindowSceneSessionRoleApplication':[{'UISceneConfigurationName':'Game','UISceneDelegateClassName':'SceneDelegate'}]}}
}
(APP/'Info.plist').write_bytes(plistlib.dumps(info));(APP/'PkgInfo').write_bytes(b'APPL????')
destination=ROOT/'dist';destination.mkdir(exist_ok=True)
ipa=destination/('FlappyBird-iOS17-arm64-unsigned.ipa' if args.modern_only else 'FlappyBird-iOS5-27-Universal-unsigned.ipa')
with zipfile.ZipFile(ipa,'w',zipfile.ZIP_DEFLATED) as z:
    for f in sorted(APP.rglob('*')):
        if f.is_file():z.write(f,f.relative_to(BUILD))
digest=hashlib.sha256(ipa.read_bytes()).hexdigest();(destination/(ipa.stem+'.sha256')).write_text(digest+'  '+ipa.name+'\n')
print(ipa);print('SHA256',digest)
