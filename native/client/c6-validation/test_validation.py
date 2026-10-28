import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

import inventory as inv
import validate as val


def terrain():
    return struct.pack('<B7i3fB3fH',0,1,1,1,2,1,1,1,0,-1,0,0,35,256,.7,1)+struct.pack('<fI',1,123)+b'\1'


def unit(heights=(0.,1.,2.,3.)):
    return struct.pack('<i4f',4,*heights)+struct.pack('<iB',1,1)+struct.pack('<iH',1,0)+struct.pack('<iB',1,0)+struct.pack('<iii',0,0,0)+b'GPU-tail-not-decoded'


class InventoryTests(unittest.TestCase):
    def test_prefix_and_heights(self):
        h=inv.terrain_header(terrain());u=inv.unit_prefix(unit(),h,0)
        self.assertEqual(u['height_max'],3)
        self.assertEqual(u['unparsed_tail_bytes'],len(b'GPU-tail-not-decoded'))
        self.assertEqual(u['center_world'],[0,0.,0])

    def test_nonfinite_heights(self):
        with self.assertRaisesRegex(inv.DataError,'nonfinite'):
            inv.unit_prefix(unit((0.,float('nan'),2.,3.)),inv.terrain_header(terrain()),0)

    def test_height_count_and_truncation(self):
        for data in [struct.pack('<i',5)+unit()[4:],unit()[:18]]:
            with self.assertRaises(inv.DataError):
                inv.unit_prefix(data,inv.terrain_header(terrain()),0)

    def test_index_file_id_and_truncation(self):
        data=struct.pack('<iii',1,1,1)+b'A'+struct.pack('<III',10,0,0)
        self.assertEqual(inv.map_index(data)[0]['resource_id'],10)
        for corrupt in [data[:-1],data[:-8]+struct.pack('<II',1,0),data+b'x']:
            with self.assertRaises(inv.DataError):inv.map_index(corrupt)

    def test_safe_asset_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            for path in ['../outside','C:\\outside','/outside']:
                with self.assertRaises(inv.DataError):inv.safe_path(Path(tmp),path)

    def test_chart_record_size(self):
        data=struct.pack('<HII9fBB',1,0,2000001,1,1,1,0,0,0,0,0,0,0,1)
        self.assertEqual(inv.chart_maps(data)[0]['resource_id'],2000001)
        with self.assertRaises(inv.DataError):inv.chart_maps(data+b'x')


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.scene={'id':'terrain','dimensions':[2,2],'asset_hashes':{'unit':'a'*64},
                    'parameters':{'gameplay_map_id':0,'resource_map_id':2000001,'unit_id':37,
                                  'simulation_tick':0,'animation_tick':500,'texture_tick':0,'random_seed':1,
                                  'camera':{'eye':[0,3,0],'target':[0,0,1],'up':[0,1,0],'fov_y_radians':1.,'near':.5,'far':1000},
                                  'view_matrix':[1 if i%5 == 0 else 0 for i in range(16)],
                                  'projection_matrix':[1 if i%5 == 0 else 0 for i in range(16)],
                                  'lighting':{'ambient':0},'fog':{'enabled':False},
                                  'render_flags':{k:k=='terrain' for k in ('terrain','bsp','objects','characters','water','effects','minimap','glow','shadows','terrain_shader')}},
                    'required_categories':['terrain_base']}
        self.contract={'schema':1,'status':'APPROVED','scope':'preview','scenes':[self.scene]}
        for platform,dir in [('native','native'),('windows-x64','windows')]:
            r=self.root/dir/'terrain';r.mkdir(parents=True)
            ppm=b'P6\n2 2\n255\n'+bytes(range(12))
            names=[f'f{i}.ppm' for i in range(5)]
            for n in names:(r/n).write_bytes(ppm)
            log=b'C6_CAPTURE_COMPLETE\n';(r/'run.log').write_bytes(log)
            (r/'timing.tsv').write_text('frame\tcpu_draw_present_ms\n'+''.join(f'{i}\t1.25\n' for i in range(119)))
            self.meta={'platform':platform,'scene_sha256':val.scene_hash(self.scene),'asset_hashes':self.scene['asset_hashes'],
                       'actual_parameters':self.scene['parameters'],'build_sha256':'b'*64,'backend_sha256':'c'*64,'config_sha256':'d'*64,
                       'failed_device_calls':0,'nonfinite_meshes':0,'meshes_checked':0,'draws':{'terrain_base':1},
                       'runtime_log':'run.log','runtime_log_sha256':hashlib.sha256(log).hexdigest(),'readbacks':names,'timings':'timing.tsv'}
            (r/'capture.json').write_text(json.dumps(self.meta))

    def run_validation(self):
        return val.validate(self.contract,self.root/'native',self.root/'windows',self.root/'out')

    def mutate(self,fn):
        p=self.root/'native/terrain/capture.json';m=json.loads(p.read_text());fn(m);p.write_text(json.dumps(m))

    def test_match_never_completes_c6(self):
        r=self.run_validation();self.assertEqual(r['status'],'EXACT_CAPTURE_MATCH_REVIEW_REQUIRED');self.assertFalse(r['c6_completed'])

    def test_missing_category_and_wrong_identity(self):
        for mutation in [lambda m:m.update(draws={}),lambda m:m.update(scene_sha256='0'*64),lambda m:m.update(actual_parameters={})]:
            with self.subTest(mutation=mutation):
                p=self.root/'native/terrain/capture.json';original=p.read_bytes();self.mutate(mutation)
                with self.assertRaises(ValueError):self.run_validation()
                p.write_bytes(original)

    def test_temporal_drift_and_pixel_difference(self):
        p=self.root/'native/terrain/f4.ppm';p.write_bytes(p.read_bytes()[:-1]+b'\x7f')
        with self.assertRaisesRegex(ValueError,'temporal drift'):self.run_validation()
        for n in range(4):(self.root/f'native/terrain/f{n}.ppm').write_bytes(p.read_bytes())
        r=self.run_validation();self.assertEqual(r['scenes'][0]['comparison']['different_pixels'],1)

    def test_duplicate_physical_readback(self):
        self.mutate(lambda m:m.update(readbacks=['f0.ppm','./f0.ppm','f1.ppm','f2.ppm','f3.ppm']))
        with self.assertRaisesRegex(ValueError,'alias'):self.run_validation()

    def test_no_scope_downgrade(self):
        self.contract['scope']='full-c6'
        with self.assertRaisesRegex(ValueError,'omits categories'):self.run_validation()

    def test_bad_timing_and_path(self):
        (self.root/'native/terrain/timing.tsv').write_text('frame\tcpu_draw_present_ms\n0\tnan\n')
        with self.assertRaises(ValueError):self.run_validation()
        self.mutate(lambda m:m.update(runtime_log='../../outside.log'))
        with self.assertRaises(ValueError):self.run_validation()

    def test_incomplete_or_degenerate_parameters(self):
        for key in ('gameplay_map_id','camera','view_matrix','random_seed','lighting','render_flags'):
            with self.subTest(key=key):
                value=self.scene['parameters'].pop(key)
                with self.assertRaises(ValueError):self.run_validation()
                self.scene['parameters'][key]=value
        self.scene['parameters']['fog']={'unresolved':'needs capture'}
        with self.assertRaisesRegex(ValueError,'unresolved'):self.run_validation()
        self.scene['parameters']['fog']={'enabled':False}
        self.scene['parameters']['camera']['target']=self.scene['parameters']['camera']['eye']
        with self.assertRaisesRegex(ValueError,'degenerate'):self.run_validation()

    def test_nonfinite_timing_and_blank_image(self):
        p=self.root/'native/terrain/timing.tsv';original=p.read_bytes()
        p.write_text('frame\tcpu_draw_present_ms\n'+''.join(f'{i}\t'+('nan' if i==0 else '1')+'\n' for i in range(119)))
        with self.assertRaisesRegex(ValueError,'timing values'):self.run_validation()
        p.write_bytes(original)
        for i in range(5):(self.root/f'native/terrain/f{i}.ppm').write_bytes(b'P6\n2 2\n255\n'+bytes(12))
        with self.assertRaisesRegex(ValueError,'blank'):self.run_validation()

    def test_geometry_audit_required(self):
        self.scene['required_categories']=['skinned_characters']
        self.scene['parameters']['render_flags']['characters']=True
        self.mutate(lambda m:m.update(scene_sha256=val.scene_hash(self.scene),actual_parameters=self.scene['parameters'],draws={'skinned_characters':1}))
        with self.assertRaisesRegex(ValueError,'finite mesh audit'):self.run_validation()

    def test_draft_rejected(self):
        self.contract['status']='DRAFT'
        with self.assertRaisesRegex(ValueError,'approval'):self.run_validation()


if __name__=='__main__':unittest.main()
