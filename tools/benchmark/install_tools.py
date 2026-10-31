#!/usr/bin/env python3
"""Install only benchmark tools; preserve results and every differing old file."""
from pathlib import Path
from datetime import datetime, timezone
import argparse, hashlib, json, os, shutil, tempfile

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def reject_links(p):
    for part in [p, *p.parents]:
        if part.is_symlink():
            raise ValueError('Refusing symlink destination: '+str(part))

def install(package, home):
    package=Path(package).resolve(); home=Path(home).resolve()
    source=package/'TOOLS'; manifest=package/'SHA256SUMS.txt'
    if not source.is_dir() or not manifest.is_file():
        raise ValueError('Extract the complete tool ZIP first; TOOLS/ or SHA256SUMS.txt missing')
    listed=set()
    for line in manifest.read_text().splitlines():
        expected,name=line.split('  ',1); relative=Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Unsafe manifest path')
        item=package/relative
        if item.is_symlink() or not item.is_file() or digest(item)!=expected:
            raise ValueError('Package checksum mismatch: '+name)
        listed.add(name)
    files=[p for p in source.rglob('*') if p.is_file()]
    if any(str(p.relative_to(package)) not in listed or p.is_symlink() for p in files):
        raise ValueError('Unlisted/symlink tool payload')
    root=home/'Desktop/AirportItlwm'; target=root/'TOOLS'
    for folder in [root,target,root/'TESTS',root/'BACKUPS']:
        reject_links(folder)
        if folder.exists() and not folder.is_dir():
            raise ValueError('Expected directory, preserving existing file: '+str(folder))
    for p in files:
        dest=target/p.relative_to(source); reject_links(dest)
        for ancestor in dest.parents:
            if ancestor.exists() and not ancestor.is_dir():
                raise ValueError('Parent is not a directory, preserving it: '+str(ancestor))
            if ancestor == target:
                break
        if dest.exists() and not dest.is_file():
            raise ValueError('Expected regular file, preserving destination: '+str(dest))
    # Stage complete verified copies before changing a destination. All differing
    # old bytes are backed up before the first replacement; unrelated files and
    # TESTS are never touched. No shell, sudo, network, EFI or kext operation.
    for folder in [target,root/'TESTS',root/'BACKUPS']:folder.mkdir(parents=True,exist_ok=True)
    backup=Path(tempfile.mkdtemp(prefix='benchmark-r11-'+datetime.now().strftime('%Y%m%d_%H%M%S')+'-',dir=root/'BACKUPS'))
    staging=Path(tempfile.mkdtemp(prefix='.benchmark-stage-',dir=root))
    changed=[]; installed=[]
    try:
        for p in files:
            rel=p.relative_to(source); dest=target/rel; staged=staging/rel
            staged.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(p,staged)
            if digest(staged)!=digest(p):raise ValueError('Staging checksum mismatch')
            if dest.exists() and (digest(dest)!=digest(p) or (dest.stat().st_mode & 0o777)!=(p.stat().st_mode & 0o777)):
                old=backup/'TOOLS'/rel;old.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dest,old)
                changed.append({'path':str(rel),'previous_sha256':digest(dest),'backup':str(old)})
        for p in files:
            rel=p.relative_to(source);dest=target/rel;dest.parent.mkdir(parents=True,exist_ok=True)
            os.replace(staging/rel,dest)
            assert digest(dest)==digest(p)
            installed.append({'path':str(rel),'sha256':digest(dest)})
        report={'revision':'shared-2026-10-31-r11-public-be200','time_utc':datetime.now(timezone.utc).isoformat(),
                'package_manifest_sha256':digest(manifest),'destination':str(target),'backup':str(backup),
                'saved_previous_files':changed,'installed':installed,'results_preserved':True,
                'kext_or_boot_changes':False}
        (backup/'INSTALLATION.json').write_text(json.dumps(report,indent=2)+'\n')
        return report
    finally:
        shutil.rmtree(staging)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package',type=Path,default=Path(__file__).resolve().parent)
    ap.add_argument('--home',type=Path,default=Path.home(),help='Destination home (test fixture override)')
    args=ap.parse_args()
    try:
        result=install(args.package,args.home)
    except (ValueError,OSError) as e:
        raise SystemExit('STOP: '+str(e))
    print('Tools ready: '+result['destination'])
    print('Previous differing files preserved: '+result['backup'])
    print('Read: '+str(Path(result['destination'])/'README.md'))
    print('No kext was installed. No traffic or network change was started.')

if __name__=='__main__':main()
