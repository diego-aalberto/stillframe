"""Small real-model integration check, isolated from the player's save."""
import json
from pathlib import Path
import time
import uuid
from runtime import Game, ROOT, Inference
from visuals import allowed_poses, GESTURES

def main():
    path=ROOT/'data'/('smoke-'+str(uuid.uuid4())+'.json')
    game=Game(path)
    report=[]
    try:
        for who,node,message in [
            ('hana','S01','Thank you for the lift. I would love to come inside and get to know you a little.'),
            ('hana','S01','Yes, let us go inside. I would enjoy having a warm drink with you.'),
            ('mira','M01','The disappointed duck sounds like my kind of expert. May I hear both bells?'),
            ('leila','L01','Thank you for checking. I would rather just enjoy the lake with you today.'),
            ('tessa','T01','Prestigious work indeed. How has your day been, besides negotiating with chairs?')]:
            if game.state['node']!=node:game.enter(node)
            start=time.monotonic();reply=game.chat(who,message)
            visual=game.state['visuals'][who]
            assert visual['pose'] in allowed_poses(game.node['background'],game.state['relationships'][who])
            assert visual['gesture'] in GESTURES
            entry=dict(character=who,node=node,seconds=round(time.monotonic()-start,2),reply=reply,
                       proposed=game.state['choice']['key'] if game.state['choice'] else None,visual=visual)
            report.append(entry);print(json.dumps(entry,ensure_ascii=True),flush=True)
        assert len(game.state['sessions'])==4 and len(set(game.state['sessions'].values()))==4
        assert report[1]['proposed'] in ('welcome','flirt')
        print('PASS: live seeded replies, opening branch, four isolated sessions.',flush=True)
    finally:
        (ROOT/'data'/'live-smoke-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':main()
