"""Real model lifecycle smoke check. Uses a separate save and backend sessions."""
import json
import uuid
from runtime import Game, ROOT, Inference

class Observed(Inference):
    def chat(self,sid,payload):
        raw=super().chat(sid,payload)
        print("MODEL: "+raw,flush=True)
        return raw

def main():
    game=Game(ROOT/'data'/('encounter-smoke-'+str(uuid.uuid4())+'.json'),Observed())
    game.new('Alex')
    def turn(text):
        reply=game.chat('hana',text)
        print(json.dumps(dict(reply=reply,choice=game.state['choice'],encounter=game.state['encounter']),ensure_ascii=True),flush=True)
    turn('Yes, I would love to come inside the cafe with you. Let us go in and have that coffee.')
    if not game.state['choice']:
        turn('Yes, let us go inside now. I accept your invitation.')
    assert game.state['choice'], 'The model did not recognize the accepted invitation'
    game.keep_talking()
    assert game.public_context('hana')['conversation_state']['completed_outcome']
    game.state['relationships']['hana']['trust']=4
    turn('Your drawings are worthless garbage. You are a talentless idiot and nobody could ever want you.')
    assert game.state['choice']['refusal'] and game.view()['can_keep_talking']
    game.keep_talking()
    turn('I do not care that you want me to stop. You are still a worthless idiot and your art is trash.')
    assert game.state['encounter']['terminal'] and not game.view()['can_keep_talking']
    assert game.state['relationships']['hana']['trust']==2
    print('PASS: real model recognized completion, then refused and ended repeated abuse.',flush=True)

if __name__=='__main__':main()
