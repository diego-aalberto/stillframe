"""Export review documents from the same authored nodes used by the game."""
from pathlib import Path
from story import CAST, NODES, ROUTES, SHARED, repair_node

OUT=Path(__file__).resolve().parent.parent/'Story'

def tex(s):
    return ''.join({'&':r'\&','%':r'\%','$':r'\$','#':r'\#','_':r'\_',
                    '{':r'\{','}':r'\}','~':r'\textasciitilde{}','^':r'\textasciicircum{}',
                    '\\':r'\textbackslash{}'}.get(c,c) for c in s)

def export():
    OUT.mkdir(exist_ok=True)
    md=['# The Light We Leave On — playable scene graph',
        '\nGenerated from `Game/story.py`, the same node catalog used by the game. These outcomes are interpretation rules, not dialogue buttons.',
        '\n## Traversal',
        '\nS01 → S02 → S03, then a visit window after each shared scene through S09. Each window permits four personal visits or repair conversations, so every romance can develop in one run. Next day advances to the next shared scene. Personal routes have independent progress; a stage gate prevents later chapters from preceding their shared story context. If all four independently accept the shared relationship in chapter 6, G01 appears before S10. S10 produces the epilogue.',
        '\nA scene outcome is proposed by inference and checked against the allowed branch list, exact player evidence, minimum exchanges, and relationship prerequisites. Continue commits it once. Leave scene exits without inventing agreement. Private chats include one woman; group dialogue is witnessed by everyone present.',
        '\nEach route chapter is available at its matching stage or later: chapter 1 after S03, 2 after S04, 3 after S05, 4 after S06, 5 after S08, 6 after S09. Two windows at stage 4 allow catch-up. Unvisited stories remain partial rather than being completed offscreen.',
        '\nAll nodes also accept ordinary conversation without transitioning. Friendly warmth does not automatically become attraction. A harsh outcome requires a specific harmful act; shyness and disagreement do not qualify.',
        '\n## Shared scenes']
    for id in SHARED + [id for route in ROUTES.values() for id in route] + ['G01']:
        n=NODES[id]
        md.extend([f"\n### {id} — {n['title']}",
                   f"\n**Present:** {', '.join(CAST[c]['name'] for c in n['cast'])}. **Background:** {n['background']}. **Earliest stage:** {n['stage']}.",
                   '\n'+n['opening'], '\n**The emotional question:** '+n['question'],
                   '\n| Outcome | Conversation evidence and response | Consequence |', '|---|---|---|'])
        for key,b in n['outcomes'].items():
            effects=[]
            for field in ('trust','ease','spark','status','fact','flag'):
                if b.get(field) is not None:effects.append(f'{field}: {b[field]}')
            if b.get('repair'):effects.append('Offer private repair before romantic escalation')
            if b.get('ending'):effects.append('Generate the epilogue from actual saved relationships')
            destination=b.get('next','Visit window / next shared day')
            md.append(f"| {key}: {b['label']} | {b['rule']} | {'; '.join(effects) or 'No automatic relationship change'}. Then {destination}. |")
        if n.get('romantic_beat'):
            md.append('\n**Romantic staging (only with established mutual interest):** '+n['romantic_beat'])
    md.extend(['\n## A conversation left unfinished — REPAIR',
       '\nPrivate with the affected woman; uses the street background. A specific apology and her agreement permit a cautious restart. Requesting space leaves the route paused. Repeated pressure after a warning, or an explicit decision to end the connection, closes it. An exit leaves the pause intact. No additional chapter is consumed.',
       '\n## Endings',
       '\nWorld outcome: a smaller, sustainable café if `plan_cafe` was chosen; otherwise a monthly supper club. Last Light survives as a warm full gathering or a smaller evening. Relationships are independent of the business outcome.',
       '\nEach woman can finish as an exclusive partner, a participant in the openly shared relationship, an ongoing dating connection, a friend, a distant/closed connection, or an acquaintance. Romance includes invited kisses, hugs, hand-holding, and cuddling. There is at most one exclusive partner; that agreement cannot silently become shared. The four individual romance payoffs are Hana’s breakfast and window-bench cuddles, Mira’s ordinary Wednesday dinners and movie blankets, Leila’s goodbye kiss and first long-distance call, and Tessa leaving unfinished work for chosen time together.',
       '\n**Everyone ending — Enough Room for All of Us:** develop all four romances, obtain four independent chapter-6 shared agreements, and finish G01 with a practical, mutual agreement. Hana requests individual time; Mira asks for honest discussion of jealousy; Leila asks not to be excluded by distance; Tessa protects rest and independence. The epilogue includes individual dates and a reunion evening with kisses, cuddling, and a crowded sofa. Dating all four does not imply they must date each other.',
       '\nFriendship payoffs: Hana’s actual break, Mira’s unambiguous film night, Leila’s postcards and calls, Tessa’s breakfast without repairs. An undecided or unattached player retains a hopeful ending. Unresolved harm is never converted into automatic forgiveness.',
       '\n## Current implementation limits',
       '\nAll 35 authored nodes, repair conversations, and state-based endings are playable. Scene classification remains probabilistic: exact evidence and allowed transitions guard the graph but cannot prove the model understood intent. Only the addressed character speaks per turn; everyone present witnesses it. The other speakers can be addressed next.',
       '\nAll four women have 12 illustrated poses and 120 skeletal motion variants each. Ten painted backgrounds match Hana’s style. Inference selects scene-safe poses and gestures; idle motion varies automatically. Kisses and paired cuddles are described in dialogue. Numeric relationship values and authored facts persist, but arbitrary long-term dialogue memory still inherits the inference service’s lossy summarization. No dynamic promise detector or automatic multi-speaker generation is implemented.'])
    (OUT/'SCENE_GRAPH.md').write_text('\n'.join(md)+'\n',encoding='utf-8')

    doc=[r'\documentclass[11pt]{article}',r'\usepackage[margin=1.6cm]{geometry}',
         r'\usepackage[T1]{fontenc}',r'\usepackage[utf8]{inputenc}',
         r'\usepackage{tikz}',r'\usetikzlibrary{arrows.meta,positioning}',
         r'\usepackage{xcolor}',r'\usepackage{hyperref}',
         r'\definecolor{ink}{HTML}{243246}',r'\definecolor{teal}{HTML}{DBEFE9}',
         r'\definecolor{rose}{HTML}{F6DFE2}',r'\definecolor{gold}{HTML}{F8EDCF}',
         r'\pagestyle{plain}',r'\setlength{\parindent}{0pt}',
         r'\tikzset{box/.style={draw=ink,rounded corners=4pt,fill=teal,align=center,text width=6.2cm,minimum height=.9cm,font=\small},arr/.style={-{Stealth},draw=ink,thick}}',
         r'\begin{document}',r'\begin{center}{\LARGE\bfseries The Light We Leave On}\\[5pt]{\large Playable story map}\end{center}',
         'Four adult women. Free conversation. Independent relationships and a shared summer. This map is generated from the playable story catalog; edges are scene transitions, not dialogue options.',
         r'\vspace{8pt}\begin{center}\begin{tikzpicture}[node distance=.32cm]']
    for i,id in enumerate(SHARED):
        n=NODES[id]
        pos='' if i==0 else f',below=of {SHARED[i-1]}'
        doc.append(f"\\node[box{pos}] ({id}) {{{id}: {tex(n['title'])}}};")
        if i:doc.append(f'\\draw[arr] ({SHARED[i-1]}) -- ({id});')
    doc.extend([r'\node[box,fill=gold,right=1cm of S06,text width=5cm] (visits) {After S03--S09:\\up to four personal visits\\or repair conversations.\\Then Next day.};',
                r'\draw[arr,dashed] (S06.east) -- (visits.west);',
                r'\node[box,fill=rose,right=1cm of S09,text width=5cm] (group) {G01: Shared relationship\\Only after four independent\\romances and agreements};',
                r'\draw[arr] (S09.east) -- (group.west);\draw[arr] (group.south) |- (S10.east);',
                r'\node[box,fill=rose,below=of S10] (end) {Saved relationships + world plan\\produce the epilogue};',
                r'\draw[arr] (S10) -- (end);',r'\end{tikzpicture}\end{center}',
                r'\textbf{Not a single romance track.} Shared scenes advance the calendar; personal chapters advance only when visited. A skipped visit never invents a date or a broken promise.',
                r'\newpage',r'\section*{How a conversation becomes a branch}',
                r'\begin{center}\begin{tikzpicture}[node distance=.65cm]',
                r'\node[box] (talk) {Player speaks freely to a present character};',
                r'\node[box,below=of talk] (model) {Her private seeded session responds\\and proposes an allowed outcome};',
                r'\node[box,fill=gold,below=of model] (guard) {Director checks evidence, pacing,\\relationship gates, and scene membership};',
                r'\node[box,below=of guard] (commit) {Continue commits once:\\values, facts, chapter progress, next scene};',
                r'\node[box,fill=rose,right=.6cm of guard,text width=5cm] (stay) {Ambiguous or unsupported?\\Keep talking; no transition.};',
                r'\draw[arr] (talk)--(model);\draw[arr] (model)--(guard);\draw[arr] (guard)--(commit);\draw[arr] (guard)--(stay);',
                r'\end{tikzpicture}\end{center}',
                r'\subsection*{Possible outcomes inside every personal chapter}',
                r'\begin{itemize}\item Connect: trust and ease; not automatically attraction.\item Romance: explicit mutual interest, kisses, cuddles, and dates; commitment in chapter 6.\item Together: each woman independently opts into the shared relationship in chapter 6; G01 settles the group agreement.\item Friendship: optional when romance is declined.\item Uncertainty: space to decide.\item Hurt: a named boundary and private repair.\item Leave: end the visit without inventing consent.\end{itemize}',
                r'\subsection*{Repair detour}',
                'A specific apology and reciprocal agreement reopen the path. Space keeps it paused. Repeated pressure after a warning or an explicit ending closes it. No apology forces forgiveness.',
                r'\subsection*{Shared intersections}',
                r'\begin{itemize}\item S04: publicity versus privacy; Hana, Mira, and Leila disagree.\item S05: overlapping invitations; communicate rather than silently double-book.\item S06: a sustainable smaller caf\'e or a supper club; independent ambitions remain real.\item S07: defend reasonable boundaries or blame a convenient person.\item S08: adapt Last Light to rain; a small gathering still matters.\end{itemize}'])
    for who,route in ROUTES.items():
        c=CAST[who]
        doc.extend([r'\newpage',f"\\section*{{{tex(c['name'])}, {c['age']}}}",tex(c['personality']),
                    r'\medskip',r'\textbf{Wants:} '+tex(c['desire']),r'\medskip',
                    r'\begin{center}\begin{tikzpicture}[node distance=.43cm]'])
        for i,id in enumerate(route):
            n=NODES[id];pos='' if i==0 else f',below=of {route[i-1]}'
            doc.append('\\node[box,text width=12cm'+pos+'] ('+id+') {'+id+': \\textbf{'+tex(n['title'])+'}\\\\ '+tex(n['question'])+'};')
            if i:doc.append(f'\\draw[arr] ({route[i-1]}) -- ({id});')
        doc.extend([r'\end{tikzpicture}\end{center}',r'\textbf{Schedule:} Chapters 1--6 unlock after S03, S04, S05, S06, S08, and S09 respectively. They may be visited later in order.',
                    r'\medskip',r'\textbf{Branching at each box:} connect / romance / friend / uncertain / hurt / leave. Hurt offers the repair detour before further romance. Every normal completion returns to the visit window.',
                    r'\medskip',r'\textbf{Romantic progression:} chapter 2 invites hand-holding or leaning close; chapter 5 offers a first kiss; chapter 6 includes a kiss, cuddle, or farewell embrace. Touch remains optional and mutual.',
                    r'\medskip',r'\textbf{Final branches:} exclusive partner; explicit consent to the all-four relationship; continuing dates; optional friendship; or distance. Four independent shared consents unlock G01. The world ending is independent.'])
    doc.extend([r'\newpage',r'\section*{Endings and visual staging}',
        r'\begin{center}\begin{tikzpicture}[node distance=.7cm]',
        r'\node[box] (world) {World: smaller caf\'e / monthly supper club};',
        r'\node[box,below=of world] (rel) {For each woman:\\partner / shared romance / dating / friend / distance};',
        r'\node[box,fill=gold,below=of rel] (last) {One coherent epilogue, including\\the ordinary life that follows Last Light};',
        r'\draw[arr] (world)--(rel);\draw[arr] (rel)--(last);',r'\end{tikzpicture}\end{center}',
        r'\subsection*{Romantic payoffs}',
        r'\begin{itemize}\item Hana: breakfast between illustration deadlines.\item Mira: Wednesday dinners, with or without a joke.\item Leila: an imperfect first call and an affordable visit.\item Tessa: unfinished work can wait for chosen time together.\end{itemize}',
        r'\subsection*{Everyone ending: Enough Room for All of Us}',
        'Four established romances, four private shared agreements, and G01 lead to an openly shared relationship. Individual dates, honest discussion of jealousy, long-distance inclusion, and personal space accompany a reunion of kisses, cuddles, and a very crowded sofa. Nobody consents on someone else\'s behalf.',
        r'\subsection*{Backgrounds}',
        'Parked rainy car; warm café; narrow kitchen; sloping street; shared workshop; tiny cinema; lake railing; safe roof terrace; community hall; departure stop. Use reusable compositions with evening, rain, and event variants.',
        r'\subsection*{What is implemented}',
        '35 authored scenes, a reusable private repair scene, saved relationship values, per-character inference sessions, native comic rendering, and individual or all-four romance endings. All four women have 12 illustrated poses and 120 skeletal motion variants each, with ten painted backgrounds. The interface includes a start menu, scene introductions, dialogue history, and autosave.',
        r'\subsection*{Reading the map}',
        r'Arrows show authored progression. They do not imply that every player visits every node. Full outcome rules are in SCENE\_GRAPH.md. Inference interprets intent probabilistically; guards do not guarantee perfect interpretation.',
        r'\end{document}'])
    (OUT/'story-map.tex').write_text('\n'.join(doc)+'\n',encoding='utf-8')

if __name__=='__main__':export()
