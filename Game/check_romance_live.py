"""Live prompt checks for affection and an individual shared-relationship agreement."""
import json
import uuid
from runtime import Game,ROOT

def main():
    game=Game(ROOT/'data'/('romance-check-'+str(uuid.uuid4())+'.json'))
    for r in game.state['relationships'].values():
        r.update(status='exploring',trust=4,ease=4,spark=3,visits=5)
    game.enter('H05')
    invitation='I have really enjoyed our dates. If you would like to kiss me, my answer is yes. I would like that very much.'
    kiss=game.chat('hana',invitation)
    print('KISS: '+kiss,flush=True)
    assert kiss.strip()!=invitation and any(word in kiss.casefold() for word in ('kiss','lips','lean'))
    game.enter('M06')
    for text in [
        'I want to discuss an open, shared relationship with you, Hana, Leila, and Tessa. Nobody would be exclusive or agree for someone else. Would you personally want to explore that?',
        'I want you as a person, not as a number. I agree to real individual dates with you, no ranking anyone, honest talk about jealousy, and everyone being free to change her mind. I would like to date all four of you openly only if each independently wants it. Those are my commitments to you.']:
        print('SHARED: '+game.chat('mira',text),flush=True)
    print('PROPOSAL: '+json.dumps(game.state['choice'],ensure_ascii=True),flush=True)
    assert game.state['choice'] and game.state['choice']['key']=='together'
    print('PASS: real model supports affection and an independent shared agreement.',flush=True)

if __name__=='__main__':main()
