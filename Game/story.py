"""Authored story: shared events, independent routes, explicit outcome contracts."""
from copy import deepcopy

VERSION = 2
CAST = {
    'hana': dict(name='Hana', age=24, color='#67b3aa',
        personality='Observant, quietly stubborn café shift lead and illustrator. Gentle precise humor. Names broken appliances. Helps too readily and must learn to ask before helping.',
        desire='Be wanted for her company, not only her care. Explore illustration as well as the café.',
        fear='People will leave when she stops being useful.',
        boundary='No public disclosure of private applications, romantic debt for kindness, or unsolicited rescue.',
        voice='I remembered the cinnamon. Please do not make this embarrassing for either of us.',
        secret='Has unsent illustration applications. Reveal only in H03 or later after trust develops.'),
    'mira': dict(name='Mira', age=25, color='#d98961',
        personality='Playful bicycle mechanic and cinema projectionist. Brave about small things, evasive about vulnerability. Repairs things carefully and describes doing so recklessly.',
        desire='Be enjoyed when quiet, not only when entertaining. Begin a technical certification course.',
        fear='Earnest affection will make her easy to laugh at.',
        boundary='No humiliation disguised as banter. Stop nicknames when asked. Apologize when her own joke hurts.',
        voice='Congratulations. You have been promoted from suspicious stranger to useful accomplice.',
        secret='Accepted into a certification course; afraid she will be the least experienced. Reveal in M03 or later.'),
    'leila': dict(name='Leila', age=26, color='#b69ad1',
        personality='Thoughtful freelance photographer with dry humor. Comfortable with silence; sometimes uses it to avoid answers. Wants to participate rather than always observe.',
        desire='Intimacy compatible with independence and a photography residency.',
        fear='Being loved only if she stays small or stays put.',
        boundary='Ask before photographs or publication. Never accept if-you-loved-me-you-would-stay pressure.',
        voice='That chair looks like it has heard three divorces. Sit somewhere optimistic.',
        secret='Shortlisted for an out-of-town residency. Reveal in L03; offer arrives at S06, never earlier.'),
    'tessa': dict(name='Tessa', age=27, color='#87a8d1',
        personality='Warm carpenter and theater stagehand. Patient, grounded, quietly playful. Calls inconveniences fine until resentful; must name her needs. Bakes handsome but questionable bread.',
        desire='Be invited when nothing needs fixing. Take recurring paid theater work.',
        fear='Becoming optional when she stops being useful.',
        boundary='Believe her first no. No guilt for paid work or expectations of unlimited availability.',
        voice='The chair is fixed. Your argument with the chair is between the two of you.',
        secret='Wants recurring out-of-town theater work. Reveal in T03 or later.'),
}
NODES = {}

def outcome(label, rule, **effects):
    return dict(label=label, rule=rule, **effects)

def add(id, title, bg, cast, opening, question, branches, stage=0, **extra):
    NODES[id] = dict(id=id, title=title, background=bg, cast=cast.split(),
                     opening=opening, question=question, outcomes=branches, stage=stage, **extra)

def personal(id, title, bg, who, opening, question, warm, flirt, friend, uncertain,
             stage, fact=None, commitment=False):
    add(id,title,bg,who,opening,question,{
        'connect': outcome('A little closer',warm,trust=1,ease=1,fact=fact),
        'romance': outcome('Say what you mean',flirt,trust=1,spark=1,
                           status='exclusive' if commitment else 'exploring'),
        'friend': outcome('A friendship chosen',friend,trust=1,status='friend'),
        'uncertain': outcome('Room to decide','Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. '+uncertain,ease=1),
        'hurt': outcome('A boundary named','Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness.',trust=-1,repair=True),
        'leave': outcome('Another time','Player clearly ends the visit. Respect the exit without a penalty.'),
    },stage,owner=who,commitment=commitment)
    if commitment:
        NODES[id]['outcomes']['romance']['rule'] += ' This branch requires an explicitly exclusive, one-to-one agreement. A request to date all four belongs to together instead.'
        NODES[id]['outcomes']['together'] = outcome('A shared relationship, freely chosen',
            'Player explicitly proposes dating Hana, Mira, Leila, and Tessa openly, with each woman knowing and agreeing. '
            'Discuss your own needs and give only your own conditional consent. When shared_eligible is true and the player explicitly agrees to those needs, your authored choice is to try it willingly; do not invent an exclusive-only preference. '
            'Never consent for anyone else. Not triggered by a generic compliment or ordinary dating interest.',
            status='shared',trust=1,spark=1)

add('S01','Rain on the windshield','car','hana',
    'Hana has given you a lift from the flooded bus stop. The car is now parked outside a late-night café. She leaves the heater on. "You can come in, if you like. The rain has not made a convincing argument for going anywhere else."',
    'Will you accept a little company, and on what terms?',{
    'welcome':outcome('A seat inside','Player accepts company or asks about the café.',ease=1,next='S02'),
    'flirt':outcome('A possible coffee date','Gentle reciprocal flirting and interest in seeing Hana again. She welcomes a future coffee, not instant love.',spark=1,next='S02'),
    'friend':outcome('Just company','Player clearly expresses platonic interest; Hana accepts warmly.',status='friend',next='S02'),
    'hurt':outcome('The heater goes quiet','Player deliberately insults or pressures her after she names a boundary. Personal route pauses; the café remains public.',trust=-1,repair=True,next='S02'),
    'leave':outcome('Tomorrow, perhaps','Player declines. Hana gives directions without guilt. The next afternoon June welcomes the player independently.',next='S02')})
add('S02','The spare chair','cafe','hana mira leila tessa',
    'The Spare Room smells of toast and wet coats. Mira loses a staring contest with the kettle. Leila moves her bag from a chair; Tessa checks that it will not wobble. Hana says, "Nobody owns that seat. You are allowed to sit down."',
    'Who interests you, and what kind of company feels comfortable?',{
    'settle':outcome('An invitation back','Player joins conversation, shares a preference, or accepts quiet company.',ease=1,next='S03'),
    'quiet':outcome('No performance required','Player is shy, wants to listen, or prefers not to discuss personal history.',next='S03'),
    'leave':outcome('A small beginning','Player leaves; an invitation remains without a guilt trip.',next='S03')})
add('S03','Six weeks of light','cafe','hana mira leila tessa',
    'June announces that she is retiring in six weeks. Hana could take a trial lease, but needs a sustainable plan. Mira proposes a farewell night called Last Light. June taps her notebook: "An event is not a business plan, but it can be a useful rehearsal."',
    'Do you want to participate, visit, or simply get to know these people?',{
    'help':outcome('A manageable promise','Player offers a specific realistic task, without speaking for others.',flag='volunteer'),
    'visit':outcome('Company counts','Player will attend or offer moral support; no labor required.',flag='guest'),
    'uncertain':outcome('An open invitation','Player is undecided; nobody assigns work to them.'),
    'leave':outcome('Life outside the café','Player declines involvement; personal visits remain possible.')},1)
add('S04','The photograph argument','cafe','hana mira leila',
    'Mira wants an eye-catching farewell poster. Hana wants the café to feel like a refuge, not a spectacle. Leila has a beautiful photograph, but several patrons have not agreed to publication. All three turn when you arrive; none wants a referee who simply picks his favorite.',
    'Can publicity serve the café without making people into advertising?',{
    'consent':outcome('Ask the people in the picture','Support explicit permission before publishing; use an empty-room image meanwhile.',flag='consent_poster'),
    'quiet':outcome('A smaller invitation','Choose illustrated posters and word of mouth; smaller attendance is acceptable.',flag='quiet_poster'),
    'pressure':outcome('No vote over privacy','Push to publish without consent. Leila refuses and uses a room photograph; relationship trust suffers only for those present.',trust=-1,flag='quiet_poster'),
    'leave':outcome('Their own compromise','Let them decide: Hana draws a poster and Leila contributes an empty-room photograph.',flag='quiet_poster')},2)
add('S05','Two invitations, one evening','street','hana mira leila tessa',
    'An evening in week three has become crowded: someone suggested a lakeside walk, someone mentioned a film, and the café needs chairs moved. Before anyone counts on you, there is time to say what you actually want to do.',
    'Can you be clear about your time and interest without turning people into competitors?',{
    'clear':outcome('One honest plan','Player chooses a visit or says they want an evening alone; no one is punished for not being chosen.',flag='clear_plans'),
    'group':outcome('Ask, do not assume','Propose a group outing. Only people who want it join; a private date is not silently converted.',flag='group_evening'),
    'vague':outcome('No promises made','Player remains uncertain. Everyone makes independent plans.',flag='unbooked'),
    'leave':outcome('A free evening','Player declines all invitations; no broken promise is invented.')},3)
add('S06','What keeping it costs','hall','hana mira leila tessa',
    'The first budget shows a problem: the café works only if Hana works too many hours and Tessa supplies free repairs. Leila has now received a residency offer. Mira suggests fewer opening nights; a monthly supper club is also possible. Nobody can preserve this summer unchanged.',
    'Which future can these people sustain?',{
    'cafe':outcome('A smaller café','Support three opening evenings, paid maintenance, a small menu, and time for Hana to illustrate. June agrees to consider the trial.',flag='plan_cafe'),
    'club':outcome('Carry the room with you','Support a monthly supper club at the community hall; allow the original café to close.',flag='plan_club'),
    'rescue':outcome('The rescue is declined','Promise impossible money, unpaid labor, or control. June declines; use a supper-club fallback until a viable proposal exists.',flag='plan_club'),
    'leave':outcome('They choose sustainability','Decline to decide for them. The women agree on a smaller supper club.',flag='plan_club')},4)
add('S07','Practice saying no','cafe','hana mira leila tessa',
    'At rehearsal, Tessa says she cannot stay late. Hana begins to offer to cover everything, stops, and looks at the timetable. Mira has forgotten a delivery. Leila admits she delayed mentioning her shortlist because she feared this exact conversation.',
    'Can responsibility be shared without assigning blame to a convenient outsider?',{
    'share':outcome('Less work, better company','Reduce the program, accept Tessa\'s limit, and address each missed task specifically.',flag='sustainable_event'),
    'honest':outcome('Say what hurt','Discuss Leila\'s delayed news without demanding that she refuse the residency. She apologizes for withholding it.',flag='honest_rehearsal'),
    'blame':outcome('They will not pile on','Try to blame one person for everything. The others defend fair treatment; the program is reduced anyway.',trust=-1,flag='reduced_event'),
    'leave':outcome('A smaller rehearsal','Leave without taking a task; the group completes a reduced plan.',flag='reduced_event')},4)
add('S08','Last Light, first rain','hall','hana mira leila tessa',
    'A storm cancels the outdoor part of Last Light. The community hall is available, but it is smaller. Mira holds a box of paper stars. "Good news. We have accidentally invented intimacy." Hana laughs despite herself. There is still an evening to make.',
    'What matters when the beautiful version of a plan fails?',{
    'adapt':outcome('A room full of warm light','Help move a feasible indoor gathering, preserving guest consent and capacity.',flag='event_warm'),
    'small':outcome('A table for a few','Choose a small, quiet farewell. No attendance competition; the night still matters.',flag='event_small'),
    'withdraw':outcome('Let others carry tonight','Admit being overwhelmed and step away. The others host the reduced gathering.',flag='event_small'),
    'leave':outcome('The light stays on','Leave; the event happens offscreen without inventing the player\'s participation.',flag='event_small')},5)
add('S09','When the chairs are stacked','roof','hana mira leila tessa',
    'The event is over. There are paper stars in someone\'s coat pocket and half a cake nobody can face yet. With the urgent work finished, invitations suddenly sound more personal. Someone asks what you are doing next week.',
    'Which connections do you want to carry beyond this project?',{
    'continue':outcome('Time without an excuse','Player wants to spend time with someone; private visits can now include explicit commitment.'),
    'friends':outcome('A circle, not a contest','Player values friendships and does not want romance.'),
    'uncertain':outcome('No deadline for a feeling','Player needs more time. An open-door ending remains possible.'),
    'leave':outcome('A quiet goodnight','Leave without a declaration; no relationship is invented.')},6)
add('S10','Next Tuesday','cafe','hana mira leila tessa',
    'June hands over either the café keys or a recipe tin for the supper club. The sign comes down for cleaning, and for a moment the window looks unfamiliar. Then somebody moves your usual chair back into place. The future is smaller than a grand promise and much more real.',
    'What do you want your next ordinary day with these people to look like?',{
    'finish':outcome('A life continuing','Player describes a future, accepts the farewell, or asks to see the ending.',ending=True),
    'leave':outcome('Take the warmth with you','Player leaves Bellweather; preserve only relationships actually agreed.',ending=True)},6)

personal('H01','The drink she remembered','cafe','hana',
 'Hana sets down a drink, then catches herself. "I remembered what you ordered. If it was a terrible first choice, this is your chance to escape it."',
 'Does being remembered feel welcome?', 'Thank her, clarify a preference, or ask about her own taste.',
 'Express interest in Hana herself; she admits she hoped you would return.', 'Clarify friendship; she asks if she may join you on her break.',
 'Take time or sit quietly; she does not require a charming response.',1)
personal('H02','A break with no errands','street','hana',
 'Hana has twenty minutes off and no sensible reason to bring you to the bakery two doors down. "We could walk the long way. For the scenery. There is a very impressive drainpipe."',
 'Is this a date, a friendly walk, or something still undefined?', 'Let her relax without making the walk another task.',
 'Ask if it is a date or invite one explicitly. She can say yes to a modest first date.', 'Name the walk as friendship kindly; she lets the ambiguity go.',
 'Enjoy the walk without naming it; no hidden commitment.',2)
personal('H03','The unfinished application','kitchen','hana',
 'A sketch falls from Hana\'s folder. She picks it up quickly, then decides to show you. It belongs to an illustration application she never sent. "I keep finding something more urgent."',
 'Can her future contain something she wants just for herself?', 'Listen and ask what support she wants. She chooses to submit one application.',
 'Affirm interest in her whole life, including ambitions outside the café; do not turn the disclosure into a kiss demand.',
 'Offer help as a friend while letting her own the decision.', 'Respect her wish to think; keep the application private.',3,fact='hana_application')
personal('H04','The night she says no','cafe','hana',
 'Hana refuses an extra shift. She looks briefly surprised by her own voice. Later she says, "I thought everyone would be disappointed. It was mostly the dishwasher."',
 'Will you still choose her when she is not taking care of you?', 'Invite company with no work attached and respect her chosen rest.',
 'Ask for another date on a day she chooses; care is not a debt.', 'Celebrate her boundary as a friend.',
 'Let her have space without turning it into rejection.',4)
personal('H05','Not because you stayed late','lake','hana',
 'Away from the café, Hana produces two badly wrapped pastries. "These are not payment for anything. I wanted to eat something nice with you."',
 'What has grown between you?', 'Talk about the small moments that mattered without claiming a relationship.',
 'Describe mutual attraction explicitly; she asks whether you would like to keep dating after Last Light.',
 'Tell her you value the friendship. She is relieved to have a clear answer.',
 'Ask for time; she wants honesty more than a quick yes.',5)
personal('H06','A key is not a promise','cafe','hana',
 'Hana meets you before opening, or before the first supper club. She has an illustration deadline this afternoon. "I can offer breakfast. I cannot offer to organize your entire life. Apparently that is progress."',
 'Do you both want an exclusive relationship, friendship, or more time?', 'Make a real future plan while keeping the relationship undefined.',
 'Explicitly agree to be partners, with separate ambitions and ordinary time together. Only if already exploring, with trust and no unresolved boundary.',
 'Choose a continuing friendship without romantic ambiguity.', 'Keep seeing each other slowly without exclusivity.',6,commitment=True)

personal('M01','A highly unscientific wager','workshop','mira',
 'Mira holds up two bicycle bells. "One sounds charming. The other sounds like a disappointed duck. I need an expert."',
 'What kind of playfulness works between you?', 'Join the silly experiment or enjoy it quietly; no wit test.',
 'Flirt lightly and reciprocally; she offers a rematch over coffee.', 'Keep the exchange friendly; she enjoys the company.',
 'Say teasing is difficult or decline the game; she adjusts without sulking.',1)
personal('M02','A film for two, allegedly','cinema','mira',
 'Mira has reserved two seats in an empty cinema. "Purely for quality control. The second seat might be defective." Then, more quietly: "You did want to come, right?"',
 'Can an invitation be sincere without becoming heavy?', 'Answer plainly and help her stop hiding behind the joke.',
 'Ask whether she means a date; she admits it if interest has been mutual.', 'Explain you came as a friend. She accepts and still shares popcorn.',
 'Keep it casual; she does not assume a date from attendance.',2)
personal('M03','The joke that lands badly','workshop','mira',
 'Mira makes a too-sharp joke about needing company, sees your reaction, and stops. "That was cheap. I am sorry." She later admits she is terrified of starting her certification course and has been covering it with noise.',
 'Can she take responsibility without making you comfort her first?', 'Name the impact or accept the apology honestly. She changes the teasing rather than demanding forgiveness.',
 'Allow repair and mutual interest to coexist; flirt only after the hurt has been addressed.', 'Set a platonic boundary and a clearer teasing preference.',
 'Need time or decline to discuss it; she respects distance.',3,fact='mira_course')
personal('M04','When the room goes quiet','cinema','mira',
 'The projector fails during a private rehearsal. Mira starts a joke, then leaves it unfinished. "Would you still want to be here if I could not make this fun?"',
 'Is her company enough without a performance?', 'Offer ordinary company while she decides what to do.',
 'Say specifically what you like about her beyond humor; ask before moving closer.', 'Share a quiet friendly evening without treating it as lesser.',
 'Admit not knowing what to say; she permits silence.',4)
personal('M05','No punchline this time','roof','mira',
 'Mira hands you a paper star left over from Last Light. "I saved the least crushed one. There is a joke available here, but I am trying something difficult."',
 'Can she tell you directly what she wants?', 'Invite honesty without promising the answer she wants.',
 'Reciprocate her direct interest and propose a real next date.', 'Decline romance warmly; she needs a moment and still values you.',
 'Ask for time; she promises not to disguise future invitations.',5)
personal('M06','An ordinary Wednesday','workshop','mira',
 'Her course timetable is pinned above the bench. Mira asks whether you want dinner on Wednesday. "No wager. No emergency. Just dinner. With my girlfriend or boyfriend, if we agree that is what this is."',
 'What are you agreeing to, beyond banter?', 'Make plans and appreciate her directness.',
 'Mutually agree to an exclusive relationship, already exploring and repaired; she asks for honest words even on awkward days.',
 'Choose friendship with clear, continuing invitations.', 'Keep dating without a label yet; do not promise exclusivity.',6,commitment=True)

personal('L01','Outside the frame','lake','leila',
 'Leila lowers her camera before you reach the railing. "You were not in the photograph. I thought I should start with that." The lake has turned the color of old silver.',
 'Can attention feel comfortable rather than intrusive?', 'Talk, watch the water, or discuss photography; she asks before any portrait.',
 'Show curiosity about the person behind the camera; she suggests another walk.', 'Enjoy a platonic conversation with clear photo preferences.',
 'Decline a photograph or prefer quiet; no penalty.',1)
personal('L02','One picture with her in it','street','leila',
 'Leila asks whether you would take her picture beside a crooked street mural. She immediately starts directing the composition, then laughs at herself. "I am being unbearable. You may tell me."',
 'Can she participate without controlling every detail?', 'Take the picture only with consent and let her choose whether to keep it.',
 'Suggest a picture together, asking before touch; she leaves the camera in its bag afterward.', 'Enjoy a friendly photo walk; no image becomes public automatically.',
 'Decline the task or discuss discomfort; she proposes walking instead.',2)
personal('L03','The city in the envelope','lake','leila',
 'Leila tells you she has been shortlisted for a residency. "I should have mentioned it sooner. I did not want to make something uncertain feel like a departure." She does not have an offer yet.',
 'Can interest survive an uncertain future?', 'Ask what she wants; tell her honestly if the delay hurt without controlling her choice.',
 'Express interest while acknowledging that distance would need discussion.', 'Support her as a friend without becoming an unpaid career manager.',
 'Admit you do not know whether distance would suit you.',3,fact='leila_shortlist')
personal('L04','The offer arrives','cafe','leila',
 'The residency offer is real now. Leila wants to accept. She turns the cup in her hands. "I am not asking you to make this simple. I am asking what you would want, knowing this."',
 'What kind of connection is realistic across distance?', 'Discuss visits and limits without making decisions for each other.',
 'Explore a realistic long-distance relationship; interest is not yet a binding promise.', 'Choose friendship across distance, with permission for lives to change.',
 'Say distance may not work; a tender goodbye can be honest.',4)
personal('L05','An evening without evidence','lake','leila',
 'Leila has left the camera at home. "There. Now if this is a good evening, we will have to remember it like amateurs."',
 'Can you enjoy the present without pretending departure is cancelled?', 'Share an ordinary evening and remember one specific detail.',
 'Reciprocate affection while keeping the coming move part of the truth.', 'Celebrate a friendship that can continue after she leaves.',
 'Ask for a goodbye rather than a promise; she respects it.',5)
personal('L06','The departure platform','bus','leila',
 'Her bag is packed. There is time for a cup of coffee before the bus, but not for a speech that rewrites her plans. "I would like there to be a next conversation," Leila says. "What would you like it to mean?"',
 'Can you both consent to a realistic next chapter?', 'Plan a future conversation without assigning it a romantic label.',
 'Agree explicitly to exclusive long-distance dating, including a first call and permission to renegotiate; requires established mutual interest.',
 'Choose friendship, not waiting indefinitely for a romance that was declined.', 'Choose a warm open ending or an honest farewell.',6,commitment=True)

personal('T01','The chair inspector','cafe','tessa',
 'Tessa slides a folded napkin under a wobbly chair, then looks pleased with this insult to her profession. "Temporary engineering. Highly prestigious."',
 'Will you notice her beyond what she fixes?', 'Ask about her day, not just the chair.',
 'Express interest in her company; she suggests coffee with no repair work.', 'Share an easy friendly conversation.',
 'Let her settle into companionable silence.',1)
personal('T02','Bread of uncertain character','kitchen','tessa',
 'Tessa has brought a loaf with a magnificent crust and a worrying density. "You may be honest. I have already apologized to the flour."',
 'Can kindness include a truthful answer?', 'Be gentle and honest; salvage toast together if wanted.',
 'Ask for a meal together on purpose; she checks whether you mean a date.', 'Make a friendly ritual of imperfect breakfasts.',
 'Decline food or explain a preference; she does not take it personally.',2)
personal('T03','The work she actually wants','workshop','tessa',
 'Tessa shows you a theater set sketch. The production would take several weekends out of town. "I said I would think about it. Then I started listing all the people who might need shelves."',
 'Can she want something without replacing herself first?', 'Ask whether she wants the work and respect the answer.',
 'Say attraction does not depend on her availability for chores.', 'Offer friendship without volunteering her time.',
 'Listen without advice; let the decision be hers.',3,fact='tessa_theater')
personal('T04','Fine is not an answer','workshop','tessa',
 'Tessa has overbooked herself and become short with you. She stops. "You did not agree to read my mind. I am tired, and I should have said so."',
 'Can care be mutual and boundaries explicit?', 'Accept or discuss her apology and make a concrete smaller plan.',
 'Respect her rest and ask for another date without demanding proof of affection.', 'Keep friendship while naming how the irritability felt.',
 'Take space; she does not equate it with abandonment.',4)
personal('T05','An invitation with empty hands','street','tessa',
 'Tessa arrives without a tool bag. She lifts her empty hands as evidence. "No emergency. I wanted a walk with you. It took an embarrassing amount of practice to say that."',
 'Do you want the person, not just the safe pair of hands?', 'Welcome the walk as freely chosen company.',
 'Return her interest plainly; she asks whether you want another date.', 'Choose a friendship where neither must earn invitations.',
 'Take the walk without defining it yet.',5)
personal('T06','The unfinished shelf','workshop','tessa',
 'A shelf waits unfinished while Tessa closes the workshop. "It can wait until tomorrow." She looks at you. "I would rather know what we are doing tonight."',
 'What ordinary future do you want together?', 'Make an honest plan without a relationship label.',
 'Explicitly agree to be partners, with work, rest, and mutual care; requires prior exploration and repaired conflict.',
 'Choose dependable friendship with room for independent lives.', 'Continue slowly or say goodbye kindly.',6,commitment=True)

# Romantic staging is conditional; a platonic visit never receives an unsolicited kiss.
ROMANTIC_BEATS = {
 'H02':'Hana brushes her fingers near yours, then asks whether you would like to hold hands on the walk.',
 'H05':'Hana sits beside you with the pastries. "I have been trying to ask this without rehearsing it. May I kiss you?" If you agree, it is a brief, tender first kiss; she laughs softly afterward, relieved.',
 'H06':'After the relationship conversation, Hana offers a slow kiss and a few unhurried minutes cuddled together on the window bench. Accepting affection is separate from accepting a relationship label.',
 'M02':'Mira leaves room between you on the cinema seats. "If you wanted to lean on me, I would survive the scandal." She checks that the invitation is welcome.',
 'M05':'Mira stops turning the paper star in her hands. "I would really like to kiss you. That is the whole sentence." A yes leads to a smiling kiss, with no joke to take it back.',
 'M06':'After dinner plans, Mira offers a kiss and asks if you want to share a blanket for the next film. She can be quiet while cuddling; she does not have to entertain you.',
 'L02':'Leila asks before standing shoulder to shoulder for the photograph; later she asks whether you would like her hand on the walk back.',
 'L05':'Leila leaves the camera put away. "Could I kiss you? I would rather remember it than photograph it." If welcomed, a gentle kiss becomes a quiet embrace by the railing.',
 'L06':'Before the bus, Leila asks for a kiss and a long hug. She still takes the residency; affection does not cancel her future.',
 'T02':'Tessa offers a flour-dusted hand across the table and makes clear that you can decline. She enjoys the closeness without turning it into a joke about owing her.',
 'T05':'Tessa stops beneath a streetlamp. "May I kiss you? I would like to do that without pretending it was an accident." If welcomed, she kisses you softly and asks whether you would like to stay in her arms a moment.',
 'T06':'Tessa offers a quiet cuddle on the workshop sofa after the tools are safely put away. "Nothing needs fixing tonight." A kiss is an invitation, never a requirement.'}
for id,beat in ROMANTIC_BEATS.items():
    NODES[id]['romantic_beat']=beat

SHARED_NEEDS = {
 'hana':'Dependable one-on-one dates and affection that is not earned by taking care of everyone.',
 'mira':'Explicit reassurance that she matters individually, real dates, and honest talk about jealousy instead of ranking partners.',
 'leila':'Inclusion during her residency, realistic calls and visits, and freedom to pursue her own career.',
 'tessa':'An equal voice, protected rest, individual dates, and permission to say no without guilt.'}

add('G01','Enough room for all of us','roof','hana mira leila tessa',
    'All four women have independently said they want to try this shared relationship. Tonight is for making that choice practical. Hana wants dependable one-on-one time. Mira wants jealousy named instead of disguised as teasing. Leila needs long-distance plans that do not leave her out. Tessa wants rest and honest limits. None is agreeing to compete for scraps of attention.',
    'Can the five of you agree on a relationship that gives everyone care, freedom, and an equal voice?',{
    'together':outcome('Five people, one honest beginning',
       'Discuss and explicitly accept honest communication, individual dates, no ranking of partners, consent for each affectionate act, and anyone being free to renegotiate or leave. All four have already independently opted in.',flag='everyone_agreed'),
    'slow':outcome('More time, without pressure','Ask to keep dating slowly rather than finalize the shared relationship. No everyone ending yet.',flag='group_slow'),
    'leave':outcome('A conversation to finish another day','Leave without an agreement; no collective commitment is assumed.')},6,
    romantic_beat='After the agreement, each woman asks about affection in her own way: Hana offers her hand, Mira asks for a kiss without a dare, Leila invites an embrace, and Tessa makes space on the sofa. A shared blanket and four individual goodnight kisses are possible if everyone welcomes them. Do not imply that the women must date or kiss each other.')

ROUTES = {c: [k for k,v in NODES.items() if v.get('owner') == c] for c in CAST}
SHARED = [f'S{i:02}' for i in range(1,11)]

def repair_node(who):
    return dict(id='REPAIR',title='A conversation left unfinished',background='street',cast=[who],
        owner=who, opening=f'{CAST[who]["name"]} agrees to a short conversation. She wants to know whether the earlier boundary has been understood, not hear a perfect speech.',
        question='Can the specific harm be acknowledged and future behavior change?',
        outcomes={'repair':outcome('A careful restart','Specific acknowledgment, no excuses or demanded forgiveness; character agrees to cautiously resume.',trust=1,repaired=True),
                  'space':outcome('Give it time','Player accepts distance; route stays paused.'),
                  'close':outcome('A boundary kept','Repeated deliberate pressure after a clear warning, or player explicitly asks to end this connection.',status='closed'),
                  'leave':outcome('Another day','Player ends conversation; route stays paused.')})

def validate():
    for key,node in NODES.items():
        assert key == node['id'] and set(node['cast']) <= set(CAST)
        assert node['outcomes'] and node['opening'] and node['question']
        for branch in node['outcomes'].values():
            assert not branch.get('next') or branch['next'] in NODES
    for who,route in ROUTES.items():
        assert len(route) == 6
        assert [NODES[k]['stage'] for k in route] == [1,2,3,4,5,6]

validate()
