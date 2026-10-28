#!/usr/bin/env python3
"""Run analytic mip-bias checks through the approved real native backend."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3];local=root/'runtime/legacy'
p=argparse.ArgumentParser();p.add_argument('--plain',action='store_true');p.add_argument('--config',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
out=a.output.resolve() if a.output else local/('c3-lod-plain' if a.plain else 'c3-lod-sanitized');out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['DYLD_LIBRARY_PATH']=os.pathsep.join(map(str,[local/'c3-runtime/MoltenVK-1.4.2/MoltenVK/MoltenVK/dynamic/dylib/macOS',local/'c3-sdk/1.4.363.0/macOS/lib',local/'c3-prefix/lib',local/'c3-build/dxvk-r2/src/d3d9']))
env['VK_DRIVER_FILES']=str(local/'c3-runtime/MoltenVK-1.4.2/local-icd.json');env['DXVK_WSI_DRIVER']='SDL3';env['DXVK_STATE_CACHE_PATH']=str(out);env.pop('DXVK_CONFIG',None);(out/'dxvk.conf').write_text(a.config.read_text() if a.config else '# approved defaults\n');env['DXVK_CONFIG_FILE']=str(out/'dxvk.conf')
exe=local/'c3-build'/('plain' if a.plain else 'sanitized')/'c3-lod'
with (out/'run.log').open('w') as log:r=subprocess.run([str(exe),str(out/'lod'),str(root/'native/client/c3/lod-shaders')],env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT)
text=(out/'run.log').read_text();valid=r.returncode==0 and 'PASS LOD oracle 112896 interior pixels;' in text and 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
(out/'result.json').write_text(json.dumps({'pass':valid,'exit':r.returncode,'binary_sha256':sha(exe),'backend_sha256':sha(local/'c3-build/dxvk-r2/src/d3d9/libdxvk_d3d9.0.dylib'),'config_sha256':sha(out/'dxvk.conf'),'scope':'FFP0/1; PS0/15 implicit/texldb/texldl/texldd; VS0/3 texldl; bias -1/0/+1'},indent=2)+'\n')
print('PASS LOD analytic oracle' if valid else text[-2000:]);raise SystemExit(0 if valid else 1)
