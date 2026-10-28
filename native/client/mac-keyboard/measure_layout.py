#!/usr/bin/env python3
"""Read only the TCML frame layout using TCMLParser::LoadFRAME's disk contract."""
import argparse,hashlib,json,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('layout',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
f=a.layout.open('rb');limit=a.layout.stat().st_size

def read(n):
    if n<0 or f.tell()+n>limit:raise ValueError('layout bounds')
    b=f.read(n)
    if len(b)!=n:raise ValueError('short layout read')
    return b

def integer():return struct.unpack('<i',read(4))[0]
def skip(n):
    if n<0 or f.tell()+n>limit:raise ValueError('layout bounds')
    f.seek(n,1)
def node(depth=0):
    if depth>100:raise ValueError('layout nesting')
    data=read(175)
    ident=struct.unpack_from('<I',data,0)[0]
    x,y,w,h=struct.unpack_from('<4i',data,89)
    for _ in range(2):skip(integer())
    count=integer()
    if not 0<=count<100000:raise ValueError('child count')
    kids=[node(depth+1) for _ in range(count)]
    return {'id':ident,'type':data[4],'x':x,'y':y,'width':w,'height':h,'children':kids}
count=integer()
if not 0<count<100000:raise ValueError('frame count')
frame=None
for _ in range(count):
    current=node()
    if current['id']==0x47d and frame is None:frame=current
if frame is None:raise ValueError('right-hand main frame not found')
# The same source ignores duplicate root frame IDs, keeping the first one.
digest=hashlib.sha256()
with a.layout.open('rb') as source:
    for chunk in iter(lambda:source.read(1024*1024),b''):digest.update(chunk)
result={'asset':str(a.layout),'sha256':digest.hexdigest(),'frame':frame}
s=json.dumps(result,indent=2)+'\n'
if a.output:a.output.write_text(s)
else:print(s)
