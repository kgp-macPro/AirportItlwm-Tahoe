#!/usr/bin/env python3
"""Clean standalone Release build of the separate experimental BlueToolFixup."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lilu-sdk', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--compile-check', action='store_true', help='Non-deliverable scratch compilation only')
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=False)
sdk = root/'MacKernelSDK'
lilu = args.lilu_sdk/'Contents/Resources'
src = root/'bluetooth/BlueToolFixup-Intel'
for source in list(src.glob('*.cpp'))+list(src.glob('*.hpp'))+list((root/'bluetooth/Common').glob('*.hpp'))+[Path(__file__).resolve(),root/'tools/bluetooth-intel/BLE_QUALIFY.cpp']:
    relative = str(source.relative_to(root))
    pinned = subprocess.run(['git','show','HEAD:'+relative],cwd=root,capture_output=True)
    if not args.compile_check and (pinned.returncode or pinned.stdout != source.read_bytes()):
        raise RuntimeError('Final build requires committed, unchanged source: '+relative)
command_log = []
def run(command):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    command_log.append(dict(command=command, exit_code=result.returncode,
                            stdout=result.stdout, stderr=result.stderr))
    (args.out/'BUILD_COMMANDS.json').write_text(json.dumps(command_log, indent=2)+'\n')
    if result.returncode:
        print(result.stderr[-5000:])
        result.check_returncode()
    return result.stdout
flags = ['xcrun','clang++','-arch','x86_64','-std=c++17','-O2','-g',
         '-fapple-kext','-fno-exceptions','-fno-rtti','-fno-stack-protector',
         '-fno-builtin','-fno-asynchronous-unwind-tables','-mno-red-zone','-nostdinc',
         '-DKERNEL','-DKERNEL_PRIVATE','-D__APPLE__','-D__MACH__',
         '-DPRODUCT_NAME=BlueToolFixup','-DMODULE_VERSION=2.7.6','-DLILU_CUSTOM_IOKIT_INIT',
         '-DAPPLE_KEXT_ASSERTIONS=1','-DDRIVER_PRIVATE','-DAPPLE','-DNeXT',
         '-mmacosx-version-min=12.0','-fno-common','-fasm-blocks',
         '-Wall','-Wextra','-Wno-unused-parameter','-Wno-deprecated-declarations',
         '-Wno-vla','-Wno-ossharedptr-misuse',
         '-I'+str(sdk/'Headers'),'-I'+str(lilu),'-I'+str(src),
         '-fdebug-compilation-dir=.', '-ffile-prefix-map='+str(root)+'=/Intel-Bluetooth',
         '-ffile-prefix-map='+str(lilu)+'=/Lilu-1.7.2']
objects = []
inputs = [src/'BlueToolFixup.cpp',src/'KextEntry.cpp',lilu/'Library/plugin_start.cpp']
for source in inputs:
    obj = args.out/(source.stem+'.o')
    run(flags+['-MMD','-MF',str(obj.with_suffix('.d')),'-c',source,'-o',obj])
    objects.append(obj)
bundle = args.out/'BlueToolFixup.kext/Contents'
(bundle/'MacOS').mkdir(parents=True)
exe = bundle/'MacOS/BlueToolFixup'
run(['xcrun','ld','-arch','x86_64','-kext','-undefined','dynamic_lookup',
     '-no_fixup_chains','-macos_version_min','12.0','-o',exe,*objects,
     sdk/'Library/x86_64/libkmod.a'])
run(['xcrun','strip','-S',exe])
libraries = {key:'10.0.0' for key in ['com.apple.kpi.bsd','com.apple.kpi.dsep',
    'com.apple.kpi.iokit','com.apple.kpi.libkern','com.apple.kpi.mach','com.apple.kpi.unsupported']}
libraries['as.vit9696.Lilu'] = '1.7.2'
info = dict(CFBundleIdentifier='as.acidanthera.BlueToolFixup',CFBundleExecutable='BlueToolFixup',
            CFBundleName='BlueToolFixup',CFBundlePackageType='KEXT',CFBundleVersion='2.7.6',
            CFBundleShortVersionString='2.7.6',OSBundleCompatibleVersion='2.7.2',
            OSBundleRequired='Root',OSBundleLibraries=libraries,
            KGPExperimentalRevision='Intel AX/BE relocation-normalized Tahoe layouts; no HCI preflight',
            KGPSourceRole='Separate experimental Bluetooth component; no AirportItlwm change',
            IOKitPersonalities={'BlueToolFixup':dict(CFBundleIdentifier='as.acidanthera.BlueToolFixup',
                IOClass='BlueToolFixup',IOMatchCategory='BlueToolFixup',IOProviderClass='IOResources',
                IOResourceMatch='IOKit')})
(bundle/'Info.plist').write_bytes(plistlib.dumps(info))
symbols = run(['xcrun','nm','-n',exe])
undefined = run(['xcrun','nm','-u',exe])
assert ' _kmod_info' in symbols and ' __realmain' in symbols
assert 'BlueToolFixup_kern_start' in symbols
assert 'kern_start' not in undefined and 'kern_stop' not in undefined
(args.out/'SYMBOLS.txt').write_text(symbols)
(args.out/'UNDEFINED.txt').write_text(undefined)
uuid = run(['xcrun','dwarfdump','--uuid',exe]).strip()
run(['xcrun','clang++','-std=c++17','-arch','x86_64','-O2','-Wno-deprecated-declarations',
     '-I'+str(src),root/'tools/bluetooth-intel/BLE_QUALIFY.cpp','-o',args.out/'BLE_QUALIFY'])
record = dict(status='COMPILE_CHECK_ONLY' if args.compile_check else 'BUILD_SUCCEEDED', role='Intel AX/BE native patches; no HCI preflight, separate from AirportItlwm',
    source_head=run(['git','rev-parse','HEAD']).strip(),
    source_branch=run(['git','branch','--show-current']).strip(),
    compiler=run(['xcrun','clang++','--version']).strip(),
    xcode=run(['xcodebuild','-version']).strip(),
    sdk_version=run(['xcrun','--sdk','macosx','--show-sdk-version']).strip(),
    sdk_build=run(['xcrun','--sdk','macosx','--show-sdk-build-version']).strip(),
    uuid=uuid,executable_bytes=exe.stat().st_size,
    executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
    classifier_sha256=hashlib.sha256((args.out/'BLE_QUALIFY').read_bytes()).hexdigest(),
    info_sha256=hashlib.sha256((bundle/'Info.plist').read_bytes()).hexdigest(),
    inputs={str(path.relative_to(root)) if path.is_relative_to(root) else str(path):
        hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs+list(src.glob('*.hpp'))+list((root/'bluetooth/Common').glob('*.hpp'))},
    limitation='Clean software build, no installation or hardware advertising/visibility result.')
dependencies = set(inputs+list(src.glob('*.hpp'))+list((root/'bluetooth/Common').glob('*.hpp'))+[sdk/'Library/x86_64/libkmod.a',root/'tools/bluetooth-intel/BLE_QUALIFY.cpp'])
import shlex
for depfile in args.out.glob('*.d'):
    text = depfile.read_text().replace('\\\n',' ')
    for path in shlex.split(text.split(':',1)[1]):
        file = Path(path)
        if not file.is_absolute():file=root/file
        if file.is_file():dependencies.add(file)
inventory = {str(file):hashlib.sha256(file.read_bytes()).hexdigest()for file in sorted(dependencies)}
(args.out/'BUILD_INPUTS.json').write_text(json.dumps(inventory,indent=2)+'\n')
record['build_inputs_count']=len(inventory)
record['build_inputs_manifest_sha256']=hashlib.sha256((args.out/'BUILD_INPUTS.json').read_bytes()).hexdigest()
(args.out/'BUILD_RESULT.json').write_text(json.dumps(record,indent=2)+'\n')
print(record['status'],uuid,record['executable_sha256'])
