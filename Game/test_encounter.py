"""Conversation authority, escalation and saved/retried decisions."""
import copy
import json
from pathlib import Path
import sys
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parent))
from runtime import Game,GameError

class Model:
    def __init__(self):
        self.response={};self.calls=[];self.fail=False
    def create(self,system):return 'session-'+str(len(self.calls))
    def chat(self,sid,payload):
        self.calls.append(copy.deepcopy(payload))
        if self.fail:raise GameError('Offline')
        text=json.loads(payload['message'])['player_utterance']
        return json.dumps(dict(reply='I would like some space.',conversation='open',boundary='none',
            withdraw_outcome=False,reaction='hold',memories=[],outcome='stay',evidence=text,
            mood='thoughtful',pose='neutral',gesture='breathe') | self.response)

@pytest.fixture
def game(tmp_path):
    g=Game(tmp_path/'save.json',Model());g.new('Alex');g.enter('H01');return g

def insult(g,text='Your art is worthless'):
    g.inference.response=dict(conversation='refuse',boundary='new_violation',reaction='upset')
    g.chat('hana',text)

def test_first_refusal_then_repeated_violation_survives_restart(game):
    game.state['relationships']['hana']['trust']=4
    insult(game)
    assert game.view()['choice'] and game.view()['can_keep_talking']
    assert game.view()['paths'][0]['title']=='Give her space'
    assert game.state['relationships']['hana']['trust']==3
    with pytest.raises(GameError):game.chat('hana','Skip the controls')
    game.keep_talking()
    resumed=Game(game.path,game.inference)
    assert resumed.public_context('hana')['conversation_state']['refusal_count']==1
    insult(resumed,'Your drawings are trash too')
    assert not resumed.view()['can_keep_talking']
    assert resumed.state['relationships']['hana']['trust']==2
    with pytest.raises(GameError):resumed.keep_talking()
    with pytest.raises(GameError):resumed.chat('hana','One more')
    resumed.advance()
    assert resumed.state['hub'] and resumed.state['relationships']['hana']['progress']==0
    assert len(resumed.state['memories']['hana']['unresolved'])==2
    assert 'H01' not in resumed.state['completed']

def test_refusal_can_remain_firm_without_penalizing_apology(game):
    insult(game);trust=game.state['relationships']['hana']['trust'];game.keep_talking()
    game.inference.response=dict(conversation='ended',reply='I hear your apology. I still need space.')
    game.chat('hana','Sorry I insulted your art')
    assert not game.view()['can_keep_talking']
    assert game.state['relationships']['hana']['trust']==trust
    assert len(game.state['memories']['hana']['unresolved'])==1

def test_accepted_repair_reopens_without_erasing_prior_refusal(game):
    insult(game);game.keep_talking()
    game.inference.response=dict(conversation='open',reaction='reconciled',reply='Thank you. We can talk carefully.')
    game.chat('hana','Sorry I insulted your drawings')
    assert not game.state['choice'] and not game.view()['conversation_blocked']
    assert not game.state['relationships']['hana']['repair']
    assert game.public_context('hana')['conversation_state']['refusal_count']==1

def test_completed_romance_is_remembered_and_can_be_revoked(game):
    game.inference.response=dict(conversation='ready',outcome='romance')
    game.chat('hana','I would like a date with you')
    assert game.state['choice']['key']=='romance'
    game.keep_talking()
    context=game.public_context('hana')['conversation_state']
    assert context['completed_outcome']['key']=='romance'
    assert game.public_context('mira')['conversation_state']['completed_outcome'] is None
    insult(game)
    assert game.state['choice']['refusal']
    game.advance()
    assert game.state['relationships']['hana']['status']=='acquaintance'

def test_explicit_change_of_mind_withdraws_uncommitted_outcome(game):
    game.inference.response=dict(conversation='ready',outcome='romance');game.chat('hana','A date sounds good')
    game.keep_talking();game.inference.response=dict(withdraw_outcome=True)
    game.chat('hana','Actually I am not ready to date')
    assert game.state['choice'] is None
    with pytest.raises(GameError):game.advance()

def test_retry_commits_penalty_once_and_keeps_exact_request(game):
    game.inference.response=dict(conversation='refuse',boundary='new_violation',reaction='upset')
    game.state['relationships']['hana']['trust']=4;game.inference.fail=True
    with pytest.raises(GameError):game.chat('hana','Your art is worthless')
    original=copy.deepcopy(game.state['pending']['payload'])
    resumed=Game(game.path,game.inference);resumed.inference.fail=False;resumed.retry()
    assert resumed.inference.calls[-1]==original
    assert resumed.state['relationships']['hana']['trust']==3
    with pytest.raises(GameError):resumed.retry()
    assert resumed.state['relationships']['hana']['trust']==3

def test_unsupported_violation_and_firm_mood_do_not_damage_trust(game):
    game.state['relationships']['hana']['trust']=4
    game.inference.response=dict(boundary='new_violation',reaction='upset',evidence='not in the message',mood='firm')
    game.chat('hana','Hello')
    assert game.state['relationships']['hana']['trust']==4
    assert not game.state['memories']['hana']['unresolved']

def test_terminal_boundary_cannot_be_bypassed_by_other_speaker(game):
    game.enter('S04');insult(game);game.keep_talking();insult(game,'Your work is trash')
    with pytest.raises(GameError):game.chat('mira','Let us ignore her')
    with pytest.raises(GameError):game.leave_scene()
    assert game.state['encounter']['terminal']

def test_malformed_control_does_not_mutate_relationship_or_choice(game):
    game.inference.response=dict(conversation='invented')
    before=copy.deepcopy(game.state['relationships'])
    with pytest.raises(GameError):game.chat('hana','Hello')
    assert game.state['relationships']==before and game.state['turns']==0
    assert game.state['choice'] is None and game.state['pending'] is None

def test_legacy_pending_is_replayed_before_seed_upgrade(game):
    game.inference.fail=True
    with pytest.raises(GameError):game.chat('hana','Hello')
    game.state.pop('dialogue_protocol');game.state.pop('encounter');game.state.pop('encounter_protocol')
    game.state['pending'].pop('protocol');game.save()
    sessions=game.state['sessions'].copy()
    resumed=Game(game.path,game.inference)
    assert resumed.state['sessions']==sessions
    resumed.inference.fail=False;resumed.retry()
    resumed.chat('hana','Another thought')
    assert resumed.state['legacy_sessions']==sessions
    assert resumed.state['dialogue_protocol']==2


def test_completed_outcome_wins_over_willingness_to_keep_talking(game):
    game.inference.response=dict(conversation='open',outcome='romance')
    game.chat('hana','I want to go on a date with you')
    assert game.state['choice']['key']=='romance'
    assert game.view()['can_keep_talking']
    schema=game.inference.calls[-1]['response_schema']
    assert schema['properties']['evidence']['enum']==['','I want to go on a date with you']
