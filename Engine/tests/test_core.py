import copy
import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from comic.core import *


def rig():
    return {'joints':[{'id':'body','x':10,'y':20},{'id':'arm','parent':'body','x':5,'y':0}],
            'layers':[{'id':'head','joint':'body','image':'head.png'}, {'id':'arm','joint':'arm','image':'arm.png'}]}


def test_layer_isolation_and_parent_transform():
    r=rig();a={'duration':2,'loop':False,'tracks':[{'target':'joint:arm','channel':'rotation','keys':[{'time':0,'value':0},{'time':2,'value':90}]}]}
    before,_=evaluate(r,a,0);after,_=evaluate(r,a,2)
    assert before[0]['matrix']==after[0]['matrix']
    assert after[1]['matrix']==pytest.approx((0,1,-1,0,15,20))


def test_uneven_times_hold_smooth_clamp_and_loop():
    keys=[{'time':0,'value':0,'ease':'hold'},{'time':.2,'value':5},{'time':1.7,'value':20,'ease':'smooth'},{'time':3,'value':0}]
    assert sample(keys,.1,0)==0
    assert sample(keys,.95,0)==pytest.approx(12.5)
    assert sample(keys,2.35,0)==pytest.approx(10)
    r=rig();a={'duration':3,'tracks':[{'target':'layer:arm','channel':'x','keys':keys}]}
    assert evaluate(r,a,3)[0][1]['matrix'][4]==15
    assert evaluate(r,a,99,False)[0][1]['matrix'][4]==15


def test_deep_skeleton_has_no_fixed_joint_limit():
    r={'joints':[{'id':str(i),'parent':str(i-1) if i else None,'x':1} for i in range(1500)],'layers':[]}
    validate_rig(r)
    assert evaluate(r)[1]['1499'][4]==1500


@pytest.mark.parametrize('failure',['cycle','parent','duplicate','nan','layer'])
def test_reject_invalid_rigs(failure):
    r=rig()
    if failure=='cycle':r['joints'][0]['parent']='arm'
    if failure=='parent':r['joints'][0]['parent']='missing'
    if failure=='duplicate':r['joints'].append(r['joints'][0])
    if failure=='nan':r['joints'][0]['x']=float('nan')
    if failure=='layer':r['layers'][0]['joint']='unknown'
    with pytest.raises(ValueError):validate_rig(r)


@pytest.mark.parametrize('keys',[[{'time':1,'value':0},{'time':.2,'value':1}],[{'time':0,'value':0},{'time':0,'value':1}],[{'time':4,'value':0}]])
def test_invalid_key_times(keys):
    with pytest.raises(ValueError):validate_animation({'duration':3,'tracks':[{'target':'joint:body','channel':'x','keys':keys}]},rig())


def test_example_loads_without_music_or_inference():
    p=Project(Path(__file__).resolve().parents[1]/'examples/project.panel')
    assert p.capabilities()['stage']['actor']=={'standing':['sway']}
    assert not p.data['inference']
