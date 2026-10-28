#!/usr/bin/env python3
"""Audit the fixed C3 selections with the existing independent asset reader."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
p=argparse.ArgumentParser()
p.add_argument('reader',type=Path)
p.add_argument('assets',type=Path)
p.add_argument('output',type=Path)
a=p.parse_args()
spec=importlib.util.spec_from_file_location('asset_data',a.reader)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
s=m.Store(a.assets)
scenes={'static':(2000460,[(24478,24479,21136,2000726)]),
        'character':(1005220,[(21881,21892,22062,1088947),(21882,21893,22084,1005537),
                             (21883,21894,22182,1057356),(21884,21895,22178,1008809),(21885,21896,22087,1009320)])}
result={'reader_sha256':hashlib.sha256(a.reader.read_bytes()).hexdigest(),'scenes':{}}
for name,(obj,selections) in scenes.items():
    parts=s.object(obj);meshes=[]
    for kind,cloth,selection,mesh in selections:
        matches=[x for x in parts if (x['kind'],x['cloth'],x['selection'],x['mesh'])==(kind,cloth,selection,mesh)]
        if len(matches)!=1:raise RuntimeError('selection does not uniquely name original cloth/mesh')
        pos,norm,uv,indices=s.mesh(mesh) # Rejects the known nonfinite geometry.
        raw=s.read('mesh',mesh);groups,nodes,vb,lods=raw.get('IIBI');raw.skip(16+nodes*64)
        count=raw.get('I');data=raw.take(count*(48 if nodes else 40))
        if bool(nodes)!=(name=='character'):raise RuntimeError('unexpected skinning class')
        for at in range(0,len(data),4):
            if nodes and at%48==24:continue
            if not math.isfinite(struct.unpack_from('<f',data,at)[0]):raise RuntimeError('nonfinite selected vertex data')
        if nodes:
            weights=[struct.unpack_from('<3f',data,i*48+12) for i in range(count)]
            if not all(all(math.isfinite(v) for v in w) for w in weights):raise RuntimeError('nonfinite skinning weights')
            maximum=max(max(data[i*48+24:i*48+28]) for i in range(count))
        else:maximum=None
        meshes.append({'kind':kind,'cloth':cloth,'selection':selection,'mesh':mesh,
                       'nodes':nodes,'vertices':count,'triangles':sum(len(x) for x in indices),
                       'position_min':pos.min(0).tolist(),'position_max':pos.max(0).tolist(),
                       'vertex_bytes_sha256':hashlib.sha256(data).hexdigest(),
                       'max_packed_bone_index':maximum,'finite_geometry':True,
                       'textures':[x['texture'] for x in matches[0]['materials']]})
    result['scenes'][name]={'object':obj,'meshes':meshes}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(result,indent=2)+'\n')
print('PASS fixed C3 original asset IDs and finite selected geometry')
