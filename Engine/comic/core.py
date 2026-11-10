"""Panel engine: data and animation evaluation, independent of the GUI."""
from bisect import bisect_right
from copy import deepcopy
import json
import math
from pathlib import Path

VERSION = 1
CHANNELS = {'x': 0., 'y': 0., 'rotation': 0., 'sx': 1., 'sy': 1., 'opacity': 1.}


def load(path, kind):
    path = Path(path).resolve()
    doc = json.loads(path.read_text(encoding='utf-8-sig'))
    if doc.get('format') != 'panel.' + kind or doc.get('version') != VERSION:
        raise ValueError(f'{path}: expected panel.{kind} version {VERSION}')
    return doc


def save(path, doc):
    path = Path(path)
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(doc, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    tmp.replace(path)


def asset(base, path):
    return (Path(base).parent / path).resolve()


def number(value):
    if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value):
        raise ValueError('Transform values and timestamps must be finite numbers')
    return value


def validate_rig(rig):
    joints = rig.get('joints', [])
    layers = rig.get('layers', [])
    ids = [j['id'] for j in joints]
    if len(ids) != len(set(ids)) or len({l['id'] for l in layers}) != len(layers):
        raise ValueError('Joint and layer IDs must be unique within their groups')
    parents = {j['id']: j.get('parent') for j in joints}
    for joint in joints:
        seen = set()
        current = joint['id']
        while current is not None:
            if current in seen or current not in parents:
                raise ValueError('Skeleton has a cycle or unknown parent')
            seen.add(current)
            current = parents[current]
    for obj in joints + layers:
        for key in CHANNELS:
            number(obj.get(key, CHANNELS[key]))
        pivot=obj.get('pivot',[0,0])
        if not isinstance(pivot,list) or len(pivot)!=2:
            raise ValueError('Pivot must be [x, y]')
        for value in pivot:number(value)
        number(obj.get('z',0))
    for layer in layers:
        if layer.get('joint') is not None and layer['joint'] not in parents:
            raise ValueError('Layer references an unknown joint')
        if not layer.get('image'):
            raise ValueError('Each layer needs an image path')


def validate_animation(anim, rig):
    duration = number(anim['duration'])
    if duration <= 0:
        raise ValueError('Animation duration must be positive')
    targets = {f'joint:{j["id"]}' for j in rig['joints']} | {f'layer:{l["id"]}' for l in rig['layers']}
    found = set()
    for track in anim.get('tracks', []):
        identity = (track['target'], track['channel'])
        if identity in found or identity[0] not in targets or identity[1] not in CHANNELS:
            raise ValueError('Duplicate or invalid animation track')
        found.add(identity)
        last = -1
        for key in track['keys']:
            t = number(key['time'])
            number(key['value'])
            if not 0 <= t <= duration or t <= last:
                raise ValueError('Keys must have unique, increasing times inside duration')
            if key.get('ease', 'linear') not in ('linear', 'smooth', 'hold'):
                raise ValueError('Unknown interpolation')
            last = t


def sample(keys, time, default):
    if not keys:
        return default
    i = bisect_right(keys, time, key=lambda k:k['time']) - 1
    if i < 0:
        return keys[0]['value']
    a = keys[i]
    if i == len(keys) - 1 or a.get('ease') == 'hold':
        return a['value']
    b = keys[i + 1]
    u = (time - a['time']) / (b['time'] - a['time'])
    if a.get('ease') == 'smooth':
        u = u*u*(3-2*u)
    return a['value'] + (b['value'] - a['value']) * u


def mul(a, b):
    x,y,z,w,tx,ty = a
    X,Y,Z,W,TX,TY = b
    return (x*X+z*Y, y*X+w*Y, x*Z+z*W, y*Z+w*W, x*TX+z*TY+tx, y*TX+w*TY+ty)


def matrix(obj):
    a = math.radians(obj.get('rotation', 0))
    c,s = math.cos(a), math.sin(a)
    sx,sy = obj.get('sx',1),obj.get('sy',1)
    px,py = obj.get('pivot', [0,0])
    return (c*sx,s*sx,-s*sy,c*sy,obj.get('x',0)-c*sx*px+s*sy*py,obj.get('y',0)-s*sx*px-c*sy*py)


IDENTITY = (1,0,0,1,0,0)


def evaluate(rig, anim=None, time=0, loop=True):
    objects = {f'{kind}:{o["id"]}': dict(o) for kind,group in [('joint','joints'),('layer','layers')] for o in rig[group]}
    if anim:
        t = time % anim['duration'] if loop and anim.get('loop', True) else min(max(time,0),anim['duration'])
        for track in anim.get('tracks', []):
            obj = objects[track['target']]
            channel = track['channel']
            obj[channel] = sample(track['keys'], t, obj.get(channel, CHANNELS[channel]))
    world = {}
    alpha = {}
    def joint(id):
        if id is None:
            return IDENTITY
        if id not in world:
            obj = objects['joint:'+id]
            world[id] = mul(joint(obj.get('parent')), matrix(obj))
        return world[id]
    # Iterative topological evaluation avoids an artificial Python recursion joint limit.
    remaining = list(rig['joints'])
    while remaining:
        next_pass = []
        for j in remaining:
            obj = objects['joint:'+j['id']]
            parent = obj.get('parent')
            if parent is not None and parent not in world:
                next_pass.append(j)
            else:
                world[j['id']] = mul(world.get(parent, IDENTITY), matrix(obj))
                alpha[j['id']] = alpha.get(parent,1) * max(0,min(1,obj.get('opacity',1)))
        if len(next_pass) == len(remaining):
            raise ValueError('Invalid joint hierarchy')
        remaining = next_pass
    layers = []
    for original in rig['layers']:
        obj = objects['layer:'+original['id']]
        layers.append({**obj, 'opacity':obj.get('opacity',1)*alpha.get(obj.get('joint'),1),
                       'matrix': mul(world.get(obj.get('joint'), IDENTITY),matrix(obj))})
    return layers, world


class Project:
    def __init__(self, path):
        self.path = Path(path).resolve()
        self.data = load(self.path, 'project')
        canvas=self.data.get('canvas',[1280,720])
        if len(canvas)!=2 or any(not isinstance(v,int) or isinstance(v,bool) or v<=0 for v in canvas):
            raise ValueError('Canvas must contain two positive integer dimensions')
        self.scenes = {}
        self.poses = {}
        for sid, relative in self.data['scenes'].items():
            scene_path = asset(self.path, relative)
            scene = load(scene_path, 'scene')
            if scene.get('background') and not asset(scene_path,scene['background']).is_file():
                raise ValueError('Missing background: '+scene['background'])
            self.scenes[sid] = (scene_path,scene)
            for actor in scene.get('actors', []):
                if actor['pose'] not in actor['poses']:
                    raise ValueError('Initial pose must exist')
                for pid, pose in actor['poses'].items():
                    rp = asset(scene_path, pose['rig'])
                    rig = load(rp, 'rig'); validate_rig(rig)
                    for layer in rig['layers']:
                        image = asset(rp,layer['image'])
                        if not image.is_file():
                            raise ValueError(f'Missing layer image: {image}')
                    animations = {}
                    for aid, relative_anim in pose.get('animations', {}).items():
                        animation = load(asset(scene_path, relative_anim), 'animation')
                        validate_animation(animation,rig)
                        animations[aid] = animation
                    if pose.get('animation') and pose['animation'] not in animations:
                        raise ValueError('Unknown default animation')
                    self.poses[(sid,actor['id'],pid)] = (rp,rig,animations)
            if len({a['id'] for a in scene.get('actors',[])}) != len(scene.get('actors',[])):
                raise ValueError('Duplicate actor ID')
        if self.data['intro'] not in self.scenes:
            raise ValueError('Unknown intro scene')
        for _,scene in self.scenes.values():
            for button in scene.get('buttons',[]):
                action=button['action']
                if action['type'] not in ('scene','animation','text'):
                    raise ValueError('Unknown button action: '+action['type'])
                if action['type']=='scene' and action['scene'] not in self.scenes:
                    raise ValueError('Button references unknown scene')
                if action['type']=='animation':
                    key=(next(k for k,v in self.scenes.items() if v[1] is scene),action['actor'],action['pose'])
                    if key not in self.poses or action['animation'] not in self.poses[key][2]:
                        raise ValueError('Button references unknown actor, pose or animation')

    def capabilities(self):
        return {sid: {a['id']: {p: list(self.poses[(sid,a['id'],p)][2]) for p in a['poses']}
                      for a in scene.get('actors',[])} for sid,(_,scene) in self.scenes.items()}
