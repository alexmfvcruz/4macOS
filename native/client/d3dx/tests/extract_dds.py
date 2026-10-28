#!/usr/bin/env python3
"""Extract representative supplied DDS records for device-upload validation; no loaders run."""
from pathlib import Path
import hashlib
import json
import struct
import zlib
ROOT=Path(__file__).resolve().parents[4]
ASSETS=ROOT/'game'
OUT=ROOT/'runtime/legacy/d3dx-check/assets'
OUT.mkdir(parents=True,exist_ok=True)
CASES=[('Data/Img/LoginSrc.TIS',0,'ui-argb',True),('Data/Img/LoginSrc.TIS',4771564,'ui-dxt1',True),('Data/Img/LoginSrc.TIS',4783024,'ui-dxt3',True),('Data/Skin/2_UI.TTX',6598552,'texture-dxt1',False),('Data/Skin/2_UI.TTX',31097,'texture-dxt3',False),('Data/Skin/2_UI.TTX',4911922,'texture-argb',False),('Data/Skin/2_Effects.TTX',0,'conversion-to-dxt3',False),('Data/Skin/2_Effects.TTX',421750,'conversion-from-dxt3',False),('Data/Skin/2_Chakra.TTX',61898,'conversion-to-dxt1',False)]
manifest=[]
for relative,offset,name,is_ui in CASES:
    with (ASSETS/relative).open('rb') as source:
        source.seek(offset);header=source.read(8);original,size=struct.unpack('<II',header);record=zlib.decompress(source.read(size))
    assert len(record)==original
    if is_ui:
        id,tag,length,size=struct.unpack_from('<IBII',record);payload=record[13:13+size]
    else:
        id=None;pos=0
        refs=struct.unpack_from('<I',record,pos)[0];pos+=4+refs*4
        keys=struct.unpack_from('<I',record,pos)[0];pos+=4+keys*24
        tag=record[pos+12];pos+=14
        length,size=struct.unpack_from('<II',record,pos);pos+=8;payload=record[pos:pos+size]
    dds=zlib.decompress(payload) if length else payload
    if length: assert len(dds)==length
    assert dds[:4]==b'DDS '
    (OUT/(name+'.dds')).write_bytes(dds)
    manifest.append({'case':name,'source':relative,'offset':offset,'ui_id':id,'format_tag':tag,'dds_bytes':len(dds),'dds_sha256':hashlib.sha256(dds).hexdigest()})
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('PASS extracted',len(manifest),'representative DDS records to',OUT)
