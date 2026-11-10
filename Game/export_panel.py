"""Port existing authored content and atlas cells to Panel's rigid-layer formats."""
import json
from pathlib import Path
from PySide6.QtGui import QImage
from story import CAST,NODES,repair_node
from visuals import POSES,GESTURES,default_pose

ROOT=Path(__file__).resolve().parent
DEST=ROOT/'panel'

def write(path,kind,**data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(dict(format='panel.'+kind,version=1,**data),ensure_ascii=False,indent=2),encoding='utf-8')

def export():
    dimensions={}
    for who in CAST:
        for index,pose in enumerate(POSES):
            strip=QImage(str(ROOT/'assets/characters'/f'{who}-{index//4}.png'))
            if strip.isNull():raise ValueError('Missing original character atlas')
            left=strip.width()*(index%4)//4;right=strip.width()*((index%4)+1)//4
            cell=strip.copy(left,0,right-left,strip.height()).convertToFormat(QImage.Format_RGBA8888)
            pixels=bytes(cell.constBits());stride=cell.bytesPerLine()
            points=[(x,y) for y in range(cell.height()) for x in range(cell.width()) if pixels[y*stride+x*4+3]>12]
            x0=min(x for x,y in points);x1=max(x for x,y in points)
            y0=min(y for x,y in points);y1=max(y for x,y in points)
            cell=cell.copy(max(0,x0-8),max(0,y0-8),min(cell.width()-max(0,x0-8),x1-x0+17),min(cell.height()-max(0,y0-8),y1-y0+17))
            image=DEST/'sprites'/f'{who}-{pose}.png';image.parent.mkdir(parents=True,exist_ok=True)
            if not cell.save(str(image)):raise OSError(image)
            dimensions[(who,pose)]=(cell.width(),cell.height())
            # One truthful rigid image layer; no fabricated body-part segmentation.
            write(DEST/'rigs'/f'{who}-{pose}.rig','rig',joints=[dict(id='body',parent=None)],
                  layers=[dict(id='portrait',image=f'../sprites/{who}-{pose}.png',joint='body',pivot=[cell.width()/2,cell.height()],sx=(350 if pose in ('seated','seated_relaxed','cuddle') else 460)/cell.height(),sy=(350 if pose in ('seated','seated_relaxed','cuddle') else 460)/cell.height())])
    motions={'breathe':('sy',1,1.006),'nod':('rotation',0,1.1),'shake_head':('x',0,3),
             'tilt':('rotation',0,1.8),'emphasize':('y',0,-4),'chuckle':('y',0,-5),
             'bashful':('rotation',0,-1.5),'reassure':('rotation',0,.9),'sigh':('y',0,3),'glance':('x',0,-4)}
    for gesture,(channel,rest,peak) in motions.items():
        duration=7.2 if gesture=='breathe' else 6.4 if gesture=='sigh' else 4.8
        values=[rest,peak,rest,-peak if rest==0 and gesture in ('shake_head','chuckle') else peak*.4 if rest==0 else 1.002,rest]
        write(DEST/'animations'/f'{gesture}.anim','animation',duration=duration,loop=True,
              tracks=[dict(target='joint:body',channel=channel,keys=[dict(time=duration*i/4,value=v,ease='smooth') for i,v in enumerate(values)])])
    scenes={**NODES,**{'REPAIR_'+who:repair_node(who) for who in CAST}}
    for sid,node in scenes.items():
        actors=[]
        for i,who in enumerate(node['cast']):
            zoom=1.9 if node['background']=='car' else 1.6 if node['background']=='cinema' else 1.05 if len(node['cast'])==4 else 1.16
            actors.append(dict(id=who,name=CAST[who]['name'],x=800 if node['background']=='car' else 1280*(i+.5)/len(node['cast']),y=820 if node['background']=='car' else 780 if node['background']=='cinema' else 630,sx=zoom,sy=zoom,
                startup_prompt=CAST[who]['personality'],pose=default_pose(node['background']),
                bubble=dict(bubble_fill={'hana':'#cddbf6eb','mira':'#cdffe4ce','leila':'#cdeddbff','tessa':'#cdd7eaff'}[who],text='#392e42'),
                poses={pose:dict(rig=f'../rigs/{who}-{pose}.rig',animation='breathe',animations={g:f'../animations/{g}.anim' for g in GESTURES}) for pose in POSES}))
        write(DEST/'scenes'/f'{sid}.scene','scene',background=f'../../assets/backgrounds/{node["background"]}.png',
              text=node['opening'],actors=actors,buttons=[],music=None)
    write(DEST/'story.panel','project',title='The Light We Leave On',canvas=[1280,640],intro='S01',
          scenes={sid:f'scenes/{sid}.scene' for sid in scenes},
          theme=dict(background='#f6f1fa',font_family='Segoe UI',font_size=19,text='#392e42',bubble_fill='#cdf1eaf9',bubble_radius=26),inference={})
    print(f'Ported {len(scenes)} scenes, 48 existing poses, 10 rigid clips per pose.')

if __name__=='__main__':export()
