"""Conversation lifecycle, separate from authored route effects.

A refusal is not a route completion. One retry can be offered; a repeated
violation ends this encounter. Durable relationship memories live elsewhere.
"""
from copy import deepcopy

CONTRACT = """Choose conversation=open, ready, refuse, or ended together with your spoken reply.
You know the allowed node outcomes and their completion rules. Recognize a fulfilled
objective when the player and you have actually agreed; return its outcome and exact
player evidence with conversation=ready. A valid non-stay outcome offers Continue even
if conversation=open (you remain willing to chat). Do not ask them to earn the same agreement
again. completed_outcome remains achieved while lingering, unless new conduct or an
explicit change of mind invalidates it (withdraw_outcome=true). Never claim a kiss,
romance or commitment is complete when it is not supported by the conversation.
Use refuse when you say you do not want to talk: Continue will be offered, with one
Keep talking opportunity. If refusal_count is already positive and the player commits
another new violation, say firmly that this conversation is over and use ended.
You may also use ended after a prior refusal when you still need space. A sincere
apology may be accepted or declined; it never obliges you to continue. On a first
refusal there is one chance to address it. Use boundary=new_violation only for a NEW
insult, pressure or boundary violation supported by exact current player evidence,
with reaction=upset. Remembering old hurt, greetings, shyness and respectful disagreement
are not new violations. Do not reward a refusal with romantic progress. Changing the
addressed character does not erase the current encounter's boundary. No UI instructions
or internal field names in dialogue. The reply and decision must agree."""


def fresh():
    return dict(blocked=False, terminal=False, refusals={}, reason='')


def context(state, who):
    e=state['encounter'];choice=state.get('choice')
    return dict(refusal_count=e['refusals'].get(who,0),
                completed_outcome=deepcopy(choice) if choice and choice['who']==who and not choice.get('refusal') else None,
                contract=CONTRACT)


def validate(response):
    if response.get('conversation') not in ('open','ready','refuse','ended'):
        raise ValueError('Missing conversation decision')
    if response.get('boundary') not in ('none','new_violation'):
        raise ValueError('Missing boundary classification')
    if not isinstance(response.get('withdraw_outcome'),bool):
        raise ValueError('Missing outcome continuity decision')


def refusal_branch(node):
    # End the encounter without awarding an objective, romantic consent or chapter.
    branch=dict(label='Give her space',rule='The character has ended this conversation.',interrupted=True)
    if node['id'] in ('S01','S02'):
        branch['next']='S02' if node['id']=='S01' else 'S03'
    if any(b.get('ending') for b in node['outcomes'].values()):
        branch['ending']=True
    return branch
