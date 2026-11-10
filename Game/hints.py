"""Small authored nudges, not model control rules or dialogue choices."""
from story import CAST

ROMANCE={
 'hana':['Notice her drawings, and her company.','Ask for more time together.','Support her ambitions without taking over.','Make her feel wanted, not needed.','Tell her you want the kiss.','Offer ordinary time together and a clear commitment.'],
 'mira':['Meet her teasing with sincere curiosity.','Enjoy the quiet beneath the banter.','Take her plans seriously.','Be honest instead of trying to win.','Say what you feel without hiding behind a joke.','Offer real dates and honest words.'],
 'leila':['Notice the person behind the camera.','Ask before getting closer.','Support her own future.','Give her attention without making demands.','Be present, and welcome a gentle kiss.','Choose a future that leaves room for her ambitions.'],
 'tessa':['Appreciate her company, not just her help.','Invite her to share something simple.','Respect the work she wants to do.','Make room for her needs too.','Say you want to be close.','Invite her when nothing needs fixing.']}
NUDGES=dict(welcome='Accept a little company.',flirt='Show a gentle interest in seeing her again.',
 friend='Be clear that you want friendship.',hurt='Cruelty or pressure can damage trust.',leave='Say goodbye when you are ready.',
 connect='Listen, share something, and get to know her.',uncertain='Ask for time if you need it.',
 together='Discuss dating openly, with individual care and everyone agreeing.',slow='Ask for a gentler pace.',
 settle='Join in, or share a little about yourself.',quiet='It is okay to listen or ask for quiet.',
 help='Offer help you can actually give.',visit='Make time to know someone personally.',
 consent='Ask before sharing someone’s photograph.',pressure='Ignoring a boundary can strain trust.',
 clear='Be honest about your plans and who you want to see.',group='Suggest company together, without avoiding the conversation.',
 vague='Avoiding a clear answer can leave people unsure.',cafe='Support a sustainable smaller café.',
 club='Consider a supper club and a different future.',rescue='Taking everything on yourself can backfire.',
 share='Share responsibility and listen to what people need.',honest='Say what you can realistically offer.',
 blame='Blame can pull the group apart.',adapt='Make a plan that respects everyone’s changing lives.',
 small='A smaller promise can still matter.',withdraw='Be honest if you need to step back.',
 continue_='Make time together beyond this summer.',friends='Name the friendships you want to keep.',
 finish='Talk about what comes next, then close this chapter.',repaired='Acknowledge the specific hurt and offer a real change.',
 distance='Respect her need for space.')
NUDGES['continue']=NUDGES.pop('continue_')
NUDGES.update(repair='Acknowledge the specific hurt and offer a real change.',space='Respect her need for time and distance.',close='Ending the connection or repeated pressure closes this path.')

def scene_hints(node,state):
    lines=[]
    who=node.get('owner',node['cast'][0])
    for key,branch in node['outcomes'].items():
        label=branch['label']
        hint=NUDGES.get(key,'Listen carefully and speak honestly about what you want.')
        if key=='romance':
            label='Romance '+CAST[who]['name']
            hint=ROMANCE[who][min(5,max(0,node.get('stage',1)-1))]
            if state['relationships'][who]['repair']:hint='First address what hurt her; rebuilding trust takes honesty.'
        lines.append(label+'\n'+hint)
    return '\n\n'.join(lines)


def onward_paths(node, state, available):
    """Actual next stops, grouped by destination rather than reaction variants."""
    from story import NODES, ROUTES, SHARED
    if state['ended']:
        return []
    who=node.get('owner',node['cast'][0])
    color=CAST[who]['color']
    def card(title,hint,destination,color=color):
        return dict(title=title,hint=hint,destination=destination,color=color)
    if state['hub']:
        cards=[]
        for person in available:
            rel=state['relationships'][person]
            destination='A careful restart' if rel['repair'] else NODES[ROUTES[person][rel['progress']]]['title']
            cards.append(card('Spend time with '+CAST[person]['name'],
                'Address what hurt her, and listen.' if rel['repair'] else 'Choose her visit to continue your story together.',
                destination,CAST[person]['color']))
        together=(state['node']!='G01' and state['shared_index']==8
                  and all(r['status']=='shared' and not r['repair'] for r in state['relationships'].values())
                  and 'G01' not in state['completed'])
        sid='G01' if together else SHARED[min(state['shared_index']+1,len(SHARED)-1)]
        cards.append(card('Continue together','Choose Next day when you are ready.',NODES[sid]['title']))
        return cards
    if state.get('choice') and state['choice'].get('refusal'):
        branch=state['choice']['branch']
        destination='Your epilogue' if branch.get('ending') else NODES[branch['next']]['title'] if branch.get('next') else 'Back to personal visits'
        return [card('Give her space','Continue ends this conversation. Her feelings and what happened stay with her.',destination)]
    branches=[state['choice']['branch']] if state['choice'] else list(node['outcomes'].values())
    destinations=list(dict.fromkeys('END' if b.get('ending') else b.get('next','VISITS') for b in branches))
    cards=[]
    for destination in destinations:
        if destination=='END':
            cards.append(card('Close this chapter','Talk about what comes next, then Continue.','Your epilogue'))
        elif destination=='VISITS':
            hint=ROMANCE[who][min(5,max(0,node.get('stage',1)-1))] if node.get('owner') else node['question']
            if node['id']=='REPAIR' or state['relationships'][who]['repair']:hint='Acknowledge the hurt and listen to what she needs.'
            cards.append(card('Continue your time together',hint+' Use Continue when an outcome is ready.','Personal visits · then the next shared scene'))
        else:
            cards.append(card('On to '+NODES[destination]['title'],
                'Accept her invitation or show interest in another coffee.' if node['id']=='S01' else 'Share a little about yourself, or enjoy the company.',
                NODES[destination]['title']))
    return cards
