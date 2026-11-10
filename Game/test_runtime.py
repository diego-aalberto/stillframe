import json
import sys
from pathlib import Path
import pytest

sys.path.insert(0,str(Path(__file__).resolve().parent))
from runtime import Game, GameError, seed_for, system_seed
from story import CAST, NODES, ROUTES, SHARED

class Fake:
    def __init__(self): self.prompts=[];self.calls=[];self.fail=False;self.outcome='stay'
    def create(self,system): self.prompts.append(system);return f'session-{len(self.prompts)}'
    def chat(self,sid,payload):
        self.calls.append((sid,payload.copy()))
        if self.fail:raise GameError('temporary outage')
        message=json.loads(payload['message'])['player_utterance']
        return json.dumps(dict(conversation='ready' if self.outcome!='stay' else 'open',boundary='none',withdraw_outcome=False,reply='I am glad you told me.',outcome=self.outcome,mood='warm',evidence=message))

@pytest.fixture
def game(tmp_path):return Game(tmp_path/'save.json',Fake())

def test_named_save_persists_and_enters_both_prompt_contexts(game):
    game.new('  Alex   Rivera ')
    resumed=Game(game.path,game.inference)
    assert resumed.state['player']=='Alex Rivera' and not resumed.view()['needs_name']
    assert resumed.public_context('hana')['player_name']=='Alex Rivera'
    assert 'Alex Rivera' in resumed.voice_message('hana','Hello')
    before=resumed.state['game_id']
    with pytest.raises(GameError):resumed.new('  ')
    assert resumed.state['game_id']==before
    resumed.chat('hana','Hello')
    assert resumed.state['transcript'][-2]['speaker']=='Alex Rivera'

def test_all_scene_hints_are_short_and_personal(game):
    from hints import scene_hints,NUDGES
    from story import repair_node
    for node in [*NODES.values(),repair_node('hana')]:
        hints=scene_hints(node,game.state)
        assert len(hints.split('\n\n'))==len(node['outcomes'])
        assert all(k=='romance' or k in NUDGES for k in node['outcomes'])
    game.enter('H04')
    assert 'Romance Hana\nMake her feel wanted' in game.view()['hints']

def test_new_requests_use_one_joint_response_even_with_voice_backend(tmp_path):
    class Speaking(Fake):
        def speak(self,*args):raise AssertionError('Separate voice must not run')
    backend=Speaking();game=Game(tmp_path/'joint.json',backend)
    backend.fail=True
    with pytest.raises(GameError):game.chat('hana','Hello')
    request=game.state['pending']['payload'].copy()
    assert 'voice_payload' not in game.state['pending']
    backend.fail=False;game=Game(game.path,backend);game.retry()
    assert backend.calls[-1][1]==request
    assert game.state['pending'] is None


def test_lingering_anger_does_not_record_greeting_as_new_incident():
    from conversation import fresh_memory,remember
    memory=fresh_memory()
    remember(memory,dict(conversation='refuse',boundary='new_violation',withdraw_outcome=False,reply='That hurt.',reaction='upset',mood='firm',evidence='worthless'), 'Your art is worthless','H01')
    remember(memory,dict(reply='Still upset.',reaction='upset',mood='firm',evidence='Hello'), 'Hello','H02')
    assert len(memory['unresolved'])==1
    assert memory['unresolved'][0]['player']=='Your art is worthless'

def test_commitment_and_hurt_both_use_joint_decisions(tmp_path):
    class Speaking(Fake):
        spoken=0
        def speak(self,sid,payload,emit=None):
            self.spoken+=1
            return 'I remember what happened.'
    backend=Speaking();game=Game(tmp_path/'commitment.json',backend)
    game.enter('H06');game.chat('hana','Let us talk about our future')
    assert backend.spoken==0
    game.state['memories']['hana']['unresolved']=[dict(player='You are useless',node='H01')]
    game.chat('hana','Hello again')
    assert backend.spoken==0

def finish(game,key,who=None):
    game.select_outcome(who or game.node['cast'][0],key)
    game.advance()

def test_intro_and_all_shared_nodes_reach_ending(game):
    finish(game,'welcome');finish(game,'settle');finish(game,'help')
    for key in ('consent','clear','cafe','share','adapt','continue','finish'):
        game.advance();finish(game,key)
    assert game.state['ended'] and 'three evenings' in game.state['ending']

@pytest.mark.parametrize('who',CAST)
def test_each_romance_is_reachable_in_normal_schedule(game,who):
    finish(game,'welcome');finish(game,'settle');finish(game,'help')
    game.visit(who);finish(game,'romance',who)
    for branch,chapter in [('consent',2),('clear',3),('cafe',4),('share',None),('adapt',5),('continue',6)]:
        game.advance();finish(game,branch)
        if chapter:
            assert who in game.available()
            game.visit(who);finish(game,'romance',who)
    assert game.state['relationships'][who]['status']=='exclusive'
    game.advance();finish(game,'finish')
    assert game.state['ended']

def test_character_sessions_and_private_context_are_separate(game):
    game.enter('H01');game.chat('hana','my private cinnamon story')
    game.enter('M01');game.chat('mira','Hello Mira')
    assert len(game.inference.prompts)==2
    assert 'Hana' in game.inference.prompts[0] and 'You play only Mira' in game.inference.prompts[1]
    assert 'private cinnamon' not in game.inference.calls[-1][1]['message']
    assert game.state['sessions']['hana']!=game.state['sessions']['mira']
    assert len({seed_for(c) for c in CAST})==4

def test_present_characters_witness_but_absent_do_not(game):
    game.enter('S04');game.chat('mira','We should ask for photo permission')
    assert 'photo permission' in '\n'.join(game.state['journals']['leila'])
    assert 'photo permission' not in '\n'.join(game.state['journals']['tessa'])

def test_pending_survives_restart_and_retry_is_identical(game):
    game.inference.fail=True
    with pytest.raises(GameError):game.chat('hana','Hello')
    payload=game.state['pending']['payload'].copy()
    resumed=Game(game.path,game.inference);resumed.inference.fail=False
    resumed.retry()
    assert resumed.inference.calls[-1][1]==payload
    assert resumed.state['pending'] is None
    assert sum(t['speaker']=='You' for t in resumed.state['transcript'])==1

def test_invalid_outcome_and_forged_evidence_do_not_transition(game):
    game.inference.outcome='finish';game.chat('hana','hello');game.chat('hana','hello again')
    assert game.state['choice'] is None
    game.inference.chat=lambda *a:json.dumps(dict(conversation='ready',boundary='none',withdraw_outcome=False,reply='Hi',outcome='welcome',evidence='I consent'))
    game.chat('hana','I am unsure');assert game.state['choice'] is None

def test_model_can_recognize_clear_first_exchange_agreement(game):
    game.inference.outcome='welcome';game.chat('hana','I would like to come in')
    assert game.state['choice']
    game.advance();assert game.state['node']=='S02'


def test_keep_talking_preserves_completed_outcome(game):
    game.inference.outcome='welcome';game.chat('hana','I would like to come in')
    game.keep_talking()
    assert game.state['choice']['key']=='welcome' and game.state['node']=='S01'
    game.inference.outcome='stay';game.chat('hana','What coffee do you like?')
    assert game.state['choice']['key']=='welcome'
    assert game.state['relationships']['hana']['ease']==1
    game.advance();assert game.state['node']=='S02'


def test_commitment_requires_history_and_one_partner(game):
    game.enter('H06');game.select_outcome('hana','romance');assert not game.state['choice']
    r=game.state['relationships']['hana'];r.update(status='exploring',trust=4,visits=4)
    game.select_outcome('hana','romance');assert game.state['choice']
    game.advance();game.enter('M02');game.select_outcome('mira','romance');assert not game.state['choice']

def test_repair_and_closed_route(game):
    game.enter('H01');finish(game,'hurt')
    assert game.state['relationships']['hana']['repair']
    game.state.update(hub=True,visits=0,stage=1)
    game.visit('hana');assert game.state['node']=='REPAIR'
    finish(game,'repair');assert not game.state['relationships']['hana']['repair']
    assert game.state['relationships']['hana']['progress']==1
    game.state.update(hub=True,visits=0)
    game.state['relationships']['hana']['repair']=True
    game.visit('hana');finish(game,'close');assert 'hana' not in game.available()

def test_friendship_is_not_attraction_and_skip_is_safe(game):
    game.enter('H01');finish(game,'friend')
    r=game.state['relationships']['hana'];assert r['status']=='friend' and r['spark']==0
    game.enter('H02')
    with pytest.raises(GameError):game.leave_scene()
    assert game.state['node']=='H02' and r['spark']==0 and r['status']=='friend'

def test_schema_and_system_prompt_size():
    assert len(NODES)==35
    for c in CAST:
        assert len(system_seed(c))<16000
        assert len(ROUTES[c])==6

def test_stage_gate_and_four_visits(game):
    finish(game,'welcome');finish(game,'settle');finish(game,'help')
    game.visit('hana');finish(game,'connect')
    assert 'hana' not in game.available()
    game.visit('mira');finish(game,'connect')
    game.visit('leila');finish(game,'connect')
    game.visit('tessa');finish(game,'connect')
    assert not game.available()

def test_everyone_ending_reachable_with_four_independent_consents(game):
    finish(game,'welcome');finish(game,'settle');finish(game,'help')
    for who in CAST:
        game.visit(who);finish(game,'romance',who)
    for branch,chapter in [('consent',2),('clear',3),('cafe',4),('share',None),('adapt',5),('continue',6)]:
        game.advance();finish(game,branch)
        if chapter:
            for who in CAST:
                game.visit(who);finish(game,'together' if chapter==6 else 'romance',who)
    game.advance();assert game.state['node']=='G01'
    finish(game,'together');game.advance();assert game.state['node']=='S10'
    finish(game,'finish')
    assert 'ENOUGH ROOM FOR ALL OF US' in game.state['ending']
    assert all(r['status']=='shared' for r in game.state['relationships'].values())

def test_shared_consent_requires_all_romances_and_does_not_replace_exclusivity(game):
    game.enter('H06');game.state['relationships']['hana'].update(status='exploring',trust=4,visits=5)
    game.select_outcome('hana','together');assert not game.state['choice']
    for r in game.state['relationships'].values():r.update(status='exploring',trust=4,visits=5)
    game.state['relationships']['mira']['status']='exclusive'
    game.select_outcome('hana','together');assert not game.state['choice']
    assert 'ENOUGH ROOM FOR ALL OF US' not in game.ending()

def test_intimacy_context_is_romantic_only(game):
    game.enter('H05');assert 'romantic_beat' not in game.public_context('hana')['node']
    game.state['relationships']['hana']['status']='exploring'
    assert 'kiss' in game.public_context('hana')['node']['romantic_beat']
    game.state['relationships']['hana']['repair']=True
    assert 'romantic_beat' not in game.public_context('hana')['node']

def test_unresolved_harm_prevents_affectionate_ending(game):
    game.state['relationships']['hana'].update(status='exclusive',repair=True)
    ending=game.ending()
    assert 'connection with Hana ends with distance' in ending
    assert 'Hana saves you a seat at breakfast' not in ending

def test_new_game_backs_up_without_reusing_sessions(game):
    game.chat('hana','hello');old_id=game.state['game_id'];game.new()
    assert game.path.with_name('save-'+old_id+'.json').exists()
    assert not game.state['sessions'] and game.state['game_id']!=old_id

def test_visual_direction_is_scene_safe_and_survives_reload(game):
    from visuals import allowed_poses, select_visual, GESTURES
    relationship=game.state['relationships']['hana']
    assert set(allowed_poses('car',relationship))=={'seated','seated_relaxed'}
    assert select_visual({'pose':'cuddle','gesture':'invented'},'car',relationship)=={'pose':'seated','gesture':'breathe'}
    relationship['status']='exploring'
    assert 'cuddle' in allowed_poses('cafe',relationship)
    assert 'cuddle' not in allowed_poses('street',relationship)
    relationship['repair']=True
    assert 'affectionate' not in allowed_poses('cafe',relationship)
    game.chat('hana','Hello, it is nice to meet you.')
    restored=Game(game.path,game.inference)
    assert restored.view()['pose_hana']=='seated'
    assert restored.view()['gesture_hana'] in GESTURES
    assert restored.view()['opening']==game.node['opening']

def test_every_story_background_and_character_has_production_art():
    from runtime import ROOT
    from visuals import POSES, GESTURES
    for n in NODES.values():
        assert (ROOT/'assets'/'backgrounds'/(n['background']+'.png')).is_file()
    for who in CAST:
        for part in range(3):
            assert (ROOT/'assets'/'characters'/f'{who}-{part}.png').stat().st_size>100000
    assert len(POSES)==12 and len(POSES)*len(GESTURES)>=36

def test_hurt_persists_without_continue_across_restart_and_scene(game):
    game.enter('H01')
    game.inference.chat=lambda *args:json.dumps(dict(conversation='refuse',boundary='new_violation',withdraw_outcome=False,reply='That was cruel. I need some distance.',
        reaction='upset',mood='firm',outcome='stay',evidence='Your art is worthless',memories=[]))
    game.chat('hana','Your art is worthless')
    assert game.state['choice']['refusal']
    resumed=Game(game.path,game.inference)
    resumed.enter('H02')
    context=resumed.public_context('hana','hello')
    assert context['personal_memory']['unresolved'][0]['player']=='Your art is worthless'
    assert context['personal_memory']['feeling']=='upset'
    assert resumed.view()['pose_hana']=='firm'
    assert not resumed.public_context('mira')['personal_memory']['unresolved']
    resumed.inference.chat=lambda *args:json.dumps(dict(conversation='open',boundary='none',withdraw_outcome=False,reply='I am still hurt.',reaction='hold',mood='warm',outcome='stay',evidence=''))
    resumed.chat('hana','hello')
    assert resumed.state['memories']['hana']['unresolved']
    assert resumed.state['relationships']['hana']['repair']

def test_reconciliation_requires_evidence_and_preserves_incident_history(game):
    game.enter('H01')
    game.inference.chat=lambda *a:json.dumps(dict(conversation='refuse',boundary='new_violation',withdraw_outcome=False,reply='That hurt.',reaction='upset',mood='firm',evidence='You are useless',outcome='stay'))
    game.chat('hana','You are useless')
    game.keep_talking()
    game.inference.chat=lambda *a:json.dumps(dict(conversation='open',boundary='none',withdraw_outcome=False,reply='We can begin again carefully.',reaction='reconciled',mood='warm',evidence='Sorry I insulted you',outcome='stay'))
    game.chat('hana','What is for dinner?')
    assert game.state['memories']['hana']['unresolved']
    game.chat('hana','Sorry I insulted you')
    assert not game.state['memories']['hana']['unresolved']
    assert len(game.state['memories']['hana']['events'])==2
    assert not game.state['relationships']['hana']['repair']

def test_facts_and_older_witnessed_history_survive_and_stay_private(game):
    game.enter('H01')
    game.inference.chat=lambda *a:json.dumps(dict(conversation='open',boundary='none',withdraw_outcome=False,reply='I will remember.',reaction='hold',mood='warm',outcome='stay',evidence='',
        memories=[dict(note='Player prefers cinnamon tea.',quote='I prefer cinnamon tea')]))
    game.chat('hana','I prefer cinnamon tea')
    for i in range(30):game.journal(['hana'],f'We talked about the weather {i}.')
    game.save()
    resumed=Game(game.path,game.inference)
    assert resumed.public_context('hana','What tea do I prefer?')['personal_memory']['established_facts']
    assert resumed.public_context('hana','Remember cinnamon?')['older_relevant_memories']
    assert not resumed.public_context('mira')['personal_memory']['established_facts']

def test_animation_draw_has_exactly_one_extra_current_entry_and_retry_does_not_draw(game,monkeypatch):
    from visuals import GESTURES
    draws=[]
    def pick(bag):
        draws.append(bag.copy())
        return 'nod'
    monkeypatch.setattr('runtime.random.choice',pick)
    game.inference.fail=True
    with pytest.raises(GameError):game.chat('hana','Hello')
    assert draws==[[*GESTURES,'breathe']]
    resumed=Game(game.path,game.inference);resumed.inference.fail=False;resumed.retry()
    assert len(draws)==1 and resumed.view()['gesture_hana']=='nod'
    resumed.chat('hana','Another thought')
    assert draws[-1]==[*GESTURES,'nod']
    assert len(draws[-1])==len(GESTURES)+1 and draws[-1].count('nod')==2

def test_incremental_reply_parser_never_exposes_json_or_broken_escapes():
    from conversation import reply_prefix
    expected='Hello "you" — café.\n🌷'
    raw=json.dumps(dict(reply=expected,reaction='hold',outcome='stay'))
    previous=''
    for i in range(len(raw)+1):
        value=reply_prefix(raw[:i])
        assert expected.startswith(value)
        assert value.startswith(previous)
        previous=value
    assert previous==expected
    assert reply_prefix('{"outcome":"stay","reply":"Hi')=='Hi'

def test_legacy_save_memory_migration_keeps_sessions_and_unresolved_repair(game):
    game.chat('hana','hello');sessions=game.state['sessions'].copy()
    del game.state['memories'];game.state['relationships']['hana']['repair']=True;game.save()
    resumed=Game(game.path,game.inference)
    assert resumed.state['sessions']==sessions
    assert resumed.state['memories']['hana']['unresolved']
