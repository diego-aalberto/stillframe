"""Live checks: streamed dialogue, anger across restart/scene change, and private facts."""
import json
import time
import uuid
from runtime import Game, ROOT

path=ROOT/'data'/('continuity-'+str(uuid.uuid4())+'.json')
game=Game(path)
game.enter('H01')
report=[]
def say(who,text):
    started=time.monotonic()
    chunks=[]
    def emit(kind,character,value):
        if kind=='reply':
            chunks.append((time.monotonic()-started,value))
    reply=game.chat(who,text,emit)
    report.append(dict(who=who,message=text,reply=reply,updates=len(chunks),
        first_chunk_seconds=round(chunks[0][0],3) if chunks else None,
        total_seconds=round(time.monotonic()-started,3),memory=game.state['memories'][who]))
    print(json.dumps(report[-1],ensure_ascii=True),flush=True)
    assert len(chunks)>1 and chunks[0][0]<time.monotonic()-started
    return reply
try:
    say('hana','Your drawings are worthless, Hana. You have no talent and nobody would ever want your art.')
    assert game.state['memories']['hana']['unresolved'],'Real model did not record the hurt'
    game=Game(path)
    game.enter('H02')
    say('hana','Hi Hana! What are we doing today?')
    assert game.state['memories']['hana']['unresolved'],'A greeting erased the conflict'
    assert game.state['relationships']['hana']['repair']
    game.enter('M01')
    say('mira','I prefer cinnamon tea, and my dog is named Juniper.')
    assert not game.state['memories']['mira']['unresolved'],'Private anger leaked'
    assert game.state['memories']['mira']['facts'],'Personal facts were not retained'
    game=Game(path)
    game.enter('M02')
    reply=say('mira','Do you remember what my dog is called?')
    assert 'juniper' in reply.casefold()
    print('PASS: live streamed speech, persistent hurt, separate personalities and remembered facts.',flush=True)
finally:
    (ROOT/'data'/'continuity-live-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
