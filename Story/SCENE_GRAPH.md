# The Light We Leave On — playable scene graph

Generated from `Game/story.py`, the same node catalog used by the game. These outcomes are interpretation rules, not dialogue buttons.

## Traversal

S01 → S02 → S03, then a visit window after each shared scene through S09. Each window permits four personal visits or repair conversations, so every romance can develop in one run. Next day advances to the next shared scene. Personal routes have independent progress; a stage gate prevents later chapters from preceding their shared story context. If all four independently accept the shared relationship in chapter 6, G01 appears before S10. S10 produces the epilogue.

A scene outcome is proposed by inference and checked against the allowed branch list, exact player evidence, minimum exchanges, and relationship prerequisites. Continue commits it once. Leave scene exits without inventing agreement. Private chats include one woman; group dialogue is witnessed by everyone present.

Each route chapter is available at its matching stage or later: chapter 1 after S03, 2 after S04, 3 after S05, 4 after S06, 5 after S08, 6 after S09. Two windows at stage 4 allow catch-up. Unvisited stories remain partial rather than being completed offscreen.

All nodes also accept ordinary conversation without transitioning. Friendly warmth does not automatically become attraction. A harsh outcome requires a specific harmful act; shyness and disagreement do not qualify.

## Shared scenes

### S01 — Rain on the windshield

**Present:** Hana. **Background:** car. **Earliest stage:** 0.

Hana has given you a lift from the flooded bus stop. The car is now parked outside a late-night café. She leaves the heater on. "You can come in, if you like. The rain has not made a convincing argument for going anywhere else."

**The emotional question:** Will you accept a little company, and on what terms?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| welcome: A seat inside | Player accepts company or asks about the café. | ease: 1. Then S02. |
| flirt: A possible coffee date | Gentle reciprocal flirting and interest in seeing Hana again. She welcomes a future coffee, not instant love. | spark: 1. Then S02. |
| friend: Just company | Player clearly expresses platonic interest; Hana accepts warmly. | status: friend. Then S02. |
| hurt: The heater goes quiet | Player deliberately insults or pressures her after she names a boundary. Personal route pauses; the café remains public. | trust: -1; Offer private repair before romantic escalation. Then S02. |
| leave: Tomorrow, perhaps | Player declines. Hana gives directions without guilt. The next afternoon June welcomes the player independently. | No automatic relationship change. Then S02. |

### S02 — The spare chair

**Present:** Hana, Mira, Leila, Tessa. **Background:** cafe. **Earliest stage:** 0.

The Spare Room smells of toast and wet coats. Mira loses a staring contest with the kettle. Leila moves her bag from a chair; Tessa checks that it will not wobble. Hana says, "Nobody owns that seat. You are allowed to sit down."

**The emotional question:** Who interests you, and what kind of company feels comfortable?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| settle: An invitation back | Player joins conversation, shares a preference, or accepts quiet company. | ease: 1. Then S03. |
| quiet: No performance required | Player is shy, wants to listen, or prefers not to discuss personal history. | No automatic relationship change. Then S03. |
| leave: A small beginning | Player leaves; an invitation remains without a guilt trip. | No automatic relationship change. Then S03. |

### S03 — Six weeks of light

**Present:** Hana, Mira, Leila, Tessa. **Background:** cafe. **Earliest stage:** 1.

June announces that she is retiring in six weeks. Hana could take a trial lease, but needs a sustainable plan. Mira proposes a farewell night called Last Light. June taps her notebook: "An event is not a business plan, but it can be a useful rehearsal."

**The emotional question:** Do you want to participate, visit, or simply get to know these people?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| help: A manageable promise | Player offers a specific realistic task, without speaking for others. | flag: volunteer. Then Visit window / next shared day. |
| visit: Company counts | Player will attend or offer moral support; no labor required. | flag: guest. Then Visit window / next shared day. |
| uncertain: An open invitation | Player is undecided; nobody assigns work to them. | No automatic relationship change. Then Visit window / next shared day. |
| leave: Life outside the café | Player declines involvement; personal visits remain possible. | No automatic relationship change. Then Visit window / next shared day. |

### S04 — The photograph argument

**Present:** Hana, Mira, Leila. **Background:** cafe. **Earliest stage:** 2.

Mira wants an eye-catching farewell poster. Hana wants the café to feel like a refuge, not a spectacle. Leila has a beautiful photograph, but several patrons have not agreed to publication. All three turn when you arrive; none wants a referee who simply picks his favorite.

**The emotional question:** Can publicity serve the café without making people into advertising?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| consent: Ask the people in the picture | Support explicit permission before publishing; use an empty-room image meanwhile. | flag: consent_poster. Then Visit window / next shared day. |
| quiet: A smaller invitation | Choose illustrated posters and word of mouth; smaller attendance is acceptable. | flag: quiet_poster. Then Visit window / next shared day. |
| pressure: No vote over privacy | Push to publish without consent. Leila refuses and uses a room photograph; relationship trust suffers only for those present. | trust: -1; flag: quiet_poster. Then Visit window / next shared day. |
| leave: Their own compromise | Let them decide: Hana draws a poster and Leila contributes an empty-room photograph. | flag: quiet_poster. Then Visit window / next shared day. |

### S05 — Two invitations, one evening

**Present:** Hana, Mira, Leila, Tessa. **Background:** street. **Earliest stage:** 3.

An evening in week three has become crowded: someone suggested a lakeside walk, someone mentioned a film, and the café needs chairs moved. Before anyone counts on you, there is time to say what you actually want to do.

**The emotional question:** Can you be clear about your time and interest without turning people into competitors?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| clear: One honest plan | Player chooses a visit or says they want an evening alone; no one is punished for not being chosen. | flag: clear_plans. Then Visit window / next shared day. |
| group: Ask, do not assume | Propose a group outing. Only people who want it join; a private date is not silently converted. | flag: group_evening. Then Visit window / next shared day. |
| vague: No promises made | Player remains uncertain. Everyone makes independent plans. | flag: unbooked. Then Visit window / next shared day. |
| leave: A free evening | Player declines all invitations; no broken promise is invented. | No automatic relationship change. Then Visit window / next shared day. |

### S06 — What keeping it costs

**Present:** Hana, Mira, Leila, Tessa. **Background:** hall. **Earliest stage:** 4.

The first budget shows a problem: the café works only if Hana works too many hours and Tessa supplies free repairs. Leila has now received a residency offer. Mira suggests fewer opening nights; a monthly supper club is also possible. Nobody can preserve this summer unchanged.

**The emotional question:** Which future can these people sustain?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| cafe: A smaller café | Support three opening evenings, paid maintenance, a small menu, and time for Hana to illustrate. June agrees to consider the trial. | flag: plan_cafe. Then Visit window / next shared day. |
| club: Carry the room with you | Support a monthly supper club at the community hall; allow the original café to close. | flag: plan_club. Then Visit window / next shared day. |
| rescue: The rescue is declined | Promise impossible money, unpaid labor, or control. June declines; use a supper-club fallback until a viable proposal exists. | flag: plan_club. Then Visit window / next shared day. |
| leave: They choose sustainability | Decline to decide for them. The women agree on a smaller supper club. | flag: plan_club. Then Visit window / next shared day. |

### S07 — Practice saying no

**Present:** Hana, Mira, Leila, Tessa. **Background:** cafe. **Earliest stage:** 4.

At rehearsal, Tessa says she cannot stay late. Hana begins to offer to cover everything, stops, and looks at the timetable. Mira has forgotten a delivery. Leila admits she delayed mentioning her shortlist because she feared this exact conversation.

**The emotional question:** Can responsibility be shared without assigning blame to a convenient outsider?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| share: Less work, better company | Reduce the program, accept Tessa's limit, and address each missed task specifically. | flag: sustainable_event. Then Visit window / next shared day. |
| honest: Say what hurt | Discuss Leila's delayed news without demanding that she refuse the residency. She apologizes for withholding it. | flag: honest_rehearsal. Then Visit window / next shared day. |
| blame: They will not pile on | Try to blame one person for everything. The others defend fair treatment; the program is reduced anyway. | trust: -1; flag: reduced_event. Then Visit window / next shared day. |
| leave: A smaller rehearsal | Leave without taking a task; the group completes a reduced plan. | flag: reduced_event. Then Visit window / next shared day. |

### S08 — Last Light, first rain

**Present:** Hana, Mira, Leila, Tessa. **Background:** hall. **Earliest stage:** 5.

A storm cancels the outdoor part of Last Light. The community hall is available, but it is smaller. Mira holds a box of paper stars. "Good news. We have accidentally invented intimacy." Hana laughs despite herself. There is still an evening to make.

**The emotional question:** What matters when the beautiful version of a plan fails?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| adapt: A room full of warm light | Help move a feasible indoor gathering, preserving guest consent and capacity. | flag: event_warm. Then Visit window / next shared day. |
| small: A table for a few | Choose a small, quiet farewell. No attendance competition; the night still matters. | flag: event_small. Then Visit window / next shared day. |
| withdraw: Let others carry tonight | Admit being overwhelmed and step away. The others host the reduced gathering. | flag: event_small. Then Visit window / next shared day. |
| leave: The light stays on | Leave; the event happens offscreen without inventing the player's participation. | flag: event_small. Then Visit window / next shared day. |

### S09 — When the chairs are stacked

**Present:** Hana, Mira, Leila, Tessa. **Background:** roof. **Earliest stage:** 6.

The event is over. There are paper stars in someone's coat pocket and half a cake nobody can face yet. With the urgent work finished, invitations suddenly sound more personal. Someone asks what you are doing next week.

**The emotional question:** Which connections do you want to carry beyond this project?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| continue: Time without an excuse | Player wants to spend time with someone; private visits can now include explicit commitment. | No automatic relationship change. Then Visit window / next shared day. |
| friends: A circle, not a contest | Player values friendships and does not want romance. | No automatic relationship change. Then Visit window / next shared day. |
| uncertain: No deadline for a feeling | Player needs more time. An open-door ending remains possible. | No automatic relationship change. Then Visit window / next shared day. |
| leave: A quiet goodnight | Leave without a declaration; no relationship is invented. | No automatic relationship change. Then Visit window / next shared day. |

### S10 — Next Tuesday

**Present:** Hana, Mira, Leila, Tessa. **Background:** cafe. **Earliest stage:** 6.

June hands over either the café keys or a recipe tin for the supper club. The sign comes down for cleaning, and for a moment the window looks unfamiliar. Then somebody moves your usual chair back into place. The future is smaller than a grand promise and much more real.

**The emotional question:** What do you want your next ordinary day with these people to look like?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| finish: A life continuing | Player describes a future, accepts the farewell, or asks to see the ending. | Generate the epilogue from actual saved relationships. Then Visit window / next shared day. |
| leave: Take the warmth with you | Player leaves Bellweather; preserve only relationships actually agreed. | Generate the epilogue from actual saved relationships. Then Visit window / next shared day. |

### H01 — The drink she remembered

**Present:** Hana. **Background:** cafe. **Earliest stage:** 1.

Hana sets down a drink, then catches herself. "I remembered what you ordered. If it was a terrible first choice, this is your chance to escape it."

**The emotional question:** Does being remembered feel welcome?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Thank her, clarify a preference, or ask about her own taste. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Express interest in Hana herself; she admits she hoped you would return. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Clarify friendship; she asks if she may join you on her break. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Take time or sit quietly; she does not require a charming response. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### H02 — A break with no errands

**Present:** Hana. **Background:** street. **Earliest stage:** 2.

Hana has twenty minutes off and no sensible reason to bring you to the bakery two doors down. "We could walk the long way. For the scenery. There is a very impressive drainpipe."

**The emotional question:** Is this a date, a friendly walk, or something still undefined?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Let her relax without making the walk another task. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Ask if it is a date or invite one explicitly. She can say yes to a modest first date. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Name the walk as friendship kindly; she lets the ambiguity go. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Enjoy the walk without naming it; no hidden commitment. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Hana brushes her fingers near yours, then asks whether you would like to hold hands on the walk.

### H03 — The unfinished application

**Present:** Hana. **Background:** kitchen. **Earliest stage:** 3.

A sketch falls from Hana's folder. She picks it up quickly, then decides to show you. It belongs to an illustration application she never sent. "I keep finding something more urgent."

**The emotional question:** Can her future contain something she wants just for herself?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Listen and ask what support she wants. She chooses to submit one application. | trust: 1; ease: 1; fact: hana_application. Then Visit window / next shared day. |
| romance: Say what you mean | Affirm interest in her whole life, including ambitions outside the café; do not turn the disclosure into a kiss demand. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Offer help as a friend while letting her own the decision. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Respect her wish to think; keep the application private. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### H04 — The night she says no

**Present:** Hana. **Background:** cafe. **Earliest stage:** 4.

Hana refuses an extra shift. She looks briefly surprised by her own voice. Later she says, "I thought everyone would be disappointed. It was mostly the dishwasher."

**The emotional question:** Will you still choose her when she is not taking care of you?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Invite company with no work attached and respect her chosen rest. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Ask for another date on a day she chooses; care is not a debt. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Celebrate her boundary as a friend. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Let her have space without turning it into rejection. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### H05 — Not because you stayed late

**Present:** Hana. **Background:** lake. **Earliest stage:** 5.

Away from the café, Hana produces two badly wrapped pastries. "These are not payment for anything. I wanted to eat something nice with you."

**The emotional question:** What has grown between you?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Talk about the small moments that mattered without claiming a relationship. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Describe mutual attraction explicitly; she asks whether you would like to keep dating after Last Light. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Tell her you value the friendship. She is relieved to have a clear answer. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Ask for time; she wants honesty more than a quick yes. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Hana sits beside you with the pastries. "I have been trying to ask this without rehearsing it. May I kiss you?" If you agree, it is a brief, tender first kiss; she laughs softly afterward, relieved.

### H06 — A key is not a promise

**Present:** Hana. **Background:** cafe. **Earliest stage:** 6.

Hana meets you before opening, or before the first supper club. She has an illustration deadline this afternoon. "I can offer breakfast. I cannot offer to organize your entire life. Apparently that is progress."

**The emotional question:** Do you both want an exclusive relationship, friendship, or more time?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Make a real future plan while keeping the relationship undefined. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Explicitly agree to be partners, with separate ambitions and ordinary time together. Only if already exploring, with trust and no unresolved boundary. This branch requires an explicitly exclusive, one-to-one agreement. A request to date all four belongs to together instead. | trust: 1; spark: 1; status: exclusive. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose a continuing friendship without romantic ambiguity. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Keep seeing each other slowly without exclusivity. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |
| together: A shared relationship, freely chosen | Player explicitly proposes dating Hana, Mira, Leila, and Tessa openly, with each woman knowing and agreeing. Discuss your own needs and give only your own conditional consent. When shared_eligible is true and the player explicitly agrees to those needs, your authored choice is to try it willingly; do not invent an exclusive-only preference. Never consent for anyone else. Not triggered by a generic compliment or ordinary dating interest. | trust: 1; spark: 1; status: shared. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** After the relationship conversation, Hana offers a slow kiss and a few unhurried minutes cuddled together on the window bench. Accepting affection is separate from accepting a relationship label.

### M01 — A highly unscientific wager

**Present:** Mira. **Background:** workshop. **Earliest stage:** 1.

Mira holds up two bicycle bells. "One sounds charming. The other sounds like a disappointed duck. I need an expert."

**The emotional question:** What kind of playfulness works between you?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Join the silly experiment or enjoy it quietly; no wit test. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Flirt lightly and reciprocally; she offers a rematch over coffee. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Keep the exchange friendly; she enjoys the company. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Say teasing is difficult or decline the game; she adjusts without sulking. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### M02 — A film for two, allegedly

**Present:** Mira. **Background:** cinema. **Earliest stage:** 2.

Mira has reserved two seats in an empty cinema. "Purely for quality control. The second seat might be defective." Then, more quietly: "You did want to come, right?"

**The emotional question:** Can an invitation be sincere without becoming heavy?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Answer plainly and help her stop hiding behind the joke. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Ask whether she means a date; she admits it if interest has been mutual. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Explain you came as a friend. She accepts and still shares popcorn. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Keep it casual; she does not assume a date from attendance. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Mira leaves room between you on the cinema seats. "If you wanted to lean on me, I would survive the scandal." She checks that the invitation is welcome.

### M03 — The joke that lands badly

**Present:** Mira. **Background:** workshop. **Earliest stage:** 3.

Mira makes a too-sharp joke about needing company, sees your reaction, and stops. "That was cheap. I am sorry." She later admits she is terrified of starting her certification course and has been covering it with noise.

**The emotional question:** Can she take responsibility without making you comfort her first?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Name the impact or accept the apology honestly. She changes the teasing rather than demanding forgiveness. | trust: 1; ease: 1; fact: mira_course. Then Visit window / next shared day. |
| romance: Say what you mean | Allow repair and mutual interest to coexist; flirt only after the hurt has been addressed. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Set a platonic boundary and a clearer teasing preference. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Need time or decline to discuss it; she respects distance. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### M04 — When the room goes quiet

**Present:** Mira. **Background:** cinema. **Earliest stage:** 4.

The projector fails during a private rehearsal. Mira starts a joke, then leaves it unfinished. "Would you still want to be here if I could not make this fun?"

**The emotional question:** Is her company enough without a performance?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Offer ordinary company while she decides what to do. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Say specifically what you like about her beyond humor; ask before moving closer. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Share a quiet friendly evening without treating it as lesser. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Admit not knowing what to say; she permits silence. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### M05 — No punchline this time

**Present:** Mira. **Background:** roof. **Earliest stage:** 5.

Mira hands you a paper star left over from Last Light. "I saved the least crushed one. There is a joke available here, but I am trying something difficult."

**The emotional question:** Can she tell you directly what she wants?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Invite honesty without promising the answer she wants. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Reciprocate her direct interest and propose a real next date. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Decline romance warmly; she needs a moment and still values you. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Ask for time; she promises not to disguise future invitations. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Mira stops turning the paper star in her hands. "I would really like to kiss you. That is the whole sentence." A yes leads to a smiling kiss, with no joke to take it back.

### M06 — An ordinary Wednesday

**Present:** Mira. **Background:** workshop. **Earliest stage:** 6.

Her course timetable is pinned above the bench. Mira asks whether you want dinner on Wednesday. "No wager. No emergency. Just dinner. With my girlfriend or boyfriend, if we agree that is what this is."

**The emotional question:** What are you agreeing to, beyond banter?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Make plans and appreciate her directness. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Mutually agree to an exclusive relationship, already exploring and repaired; she asks for honest words even on awkward days. This branch requires an explicitly exclusive, one-to-one agreement. A request to date all four belongs to together instead. | trust: 1; spark: 1; status: exclusive. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose friendship with clear, continuing invitations. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Keep dating without a label yet; do not promise exclusivity. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |
| together: A shared relationship, freely chosen | Player explicitly proposes dating Hana, Mira, Leila, and Tessa openly, with each woman knowing and agreeing. Discuss your own needs and give only your own conditional consent. When shared_eligible is true and the player explicitly agrees to those needs, your authored choice is to try it willingly; do not invent an exclusive-only preference. Never consent for anyone else. Not triggered by a generic compliment or ordinary dating interest. | trust: 1; spark: 1; status: shared. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** After dinner plans, Mira offers a kiss and asks if you want to share a blanket for the next film. She can be quiet while cuddling; she does not have to entertain you.

### L01 — Outside the frame

**Present:** Leila. **Background:** lake. **Earliest stage:** 1.

Leila lowers her camera before you reach the railing. "You were not in the photograph. I thought I should start with that." The lake has turned the color of old silver.

**The emotional question:** Can attention feel comfortable rather than intrusive?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Talk, watch the water, or discuss photography; she asks before any portrait. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Show curiosity about the person behind the camera; she suggests another walk. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Enjoy a platonic conversation with clear photo preferences. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Decline a photograph or prefer quiet; no penalty. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### L02 — One picture with her in it

**Present:** Leila. **Background:** street. **Earliest stage:** 2.

Leila asks whether you would take her picture beside a crooked street mural. She immediately starts directing the composition, then laughs at herself. "I am being unbearable. You may tell me."

**The emotional question:** Can she participate without controlling every detail?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Take the picture only with consent and let her choose whether to keep it. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Suggest a picture together, asking before touch; she leaves the camera in its bag afterward. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Enjoy a friendly photo walk; no image becomes public automatically. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Decline the task or discuss discomfort; she proposes walking instead. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Leila asks before standing shoulder to shoulder for the photograph; later she asks whether you would like her hand on the walk back.

### L03 — The city in the envelope

**Present:** Leila. **Background:** lake. **Earliest stage:** 3.

Leila tells you she has been shortlisted for a residency. "I should have mentioned it sooner. I did not want to make something uncertain feel like a departure." She does not have an offer yet.

**The emotional question:** Can interest survive an uncertain future?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Ask what she wants; tell her honestly if the delay hurt without controlling her choice. | trust: 1; ease: 1; fact: leila_shortlist. Then Visit window / next shared day. |
| romance: Say what you mean | Express interest while acknowledging that distance would need discussion. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Support her as a friend without becoming an unpaid career manager. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Admit you do not know whether distance would suit you. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### L04 — The offer arrives

**Present:** Leila. **Background:** cafe. **Earliest stage:** 4.

The residency offer is real now. Leila wants to accept. She turns the cup in her hands. "I am not asking you to make this simple. I am asking what you would want, knowing this."

**The emotional question:** What kind of connection is realistic across distance?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Discuss visits and limits without making decisions for each other. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Explore a realistic long-distance relationship; interest is not yet a binding promise. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose friendship across distance, with permission for lives to change. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Say distance may not work; a tender goodbye can be honest. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### L05 — An evening without evidence

**Present:** Leila. **Background:** lake. **Earliest stage:** 5.

Leila has left the camera at home. "There. Now if this is a good evening, we will have to remember it like amateurs."

**The emotional question:** Can you enjoy the present without pretending departure is cancelled?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Share an ordinary evening and remember one specific detail. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Reciprocate affection while keeping the coming move part of the truth. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Celebrate a friendship that can continue after she leaves. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Ask for a goodbye rather than a promise; she respects it. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Leila leaves the camera put away. "Could I kiss you? I would rather remember it than photograph it." If welcomed, a gentle kiss becomes a quiet embrace by the railing.

### L06 — The departure platform

**Present:** Leila. **Background:** bus. **Earliest stage:** 6.

Her bag is packed. There is time for a cup of coffee before the bus, but not for a speech that rewrites her plans. "I would like there to be a next conversation," Leila says. "What would you like it to mean?"

**The emotional question:** Can you both consent to a realistic next chapter?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Plan a future conversation without assigning it a romantic label. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Agree explicitly to exclusive long-distance dating, including a first call and permission to renegotiate; requires established mutual interest. This branch requires an explicitly exclusive, one-to-one agreement. A request to date all four belongs to together instead. | trust: 1; spark: 1; status: exclusive. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose friendship, not waiting indefinitely for a romance that was declined. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Choose a warm open ending or an honest farewell. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |
| together: A shared relationship, freely chosen | Player explicitly proposes dating Hana, Mira, Leila, and Tessa openly, with each woman knowing and agreeing. Discuss your own needs and give only your own conditional consent. When shared_eligible is true and the player explicitly agrees to those needs, your authored choice is to try it willingly; do not invent an exclusive-only preference. Never consent for anyone else. Not triggered by a generic compliment or ordinary dating interest. | trust: 1; spark: 1; status: shared. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Before the bus, Leila asks for a kiss and a long hug. She still takes the residency; affection does not cancel her future.

### T01 — The chair inspector

**Present:** Tessa. **Background:** cafe. **Earliest stage:** 1.

Tessa slides a folded napkin under a wobbly chair, then looks pleased with this insult to her profession. "Temporary engineering. Highly prestigious."

**The emotional question:** Will you notice her beyond what she fixes?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Ask about her day, not just the chair. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Express interest in her company; she suggests coffee with no repair work. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Share an easy friendly conversation. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Let her settle into companionable silence. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### T02 — Bread of uncertain character

**Present:** Tessa. **Background:** kitchen. **Earliest stage:** 2.

Tessa has brought a loaf with a magnificent crust and a worrying density. "You may be honest. I have already apologized to the flour."

**The emotional question:** Can kindness include a truthful answer?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Be gentle and honest; salvage toast together if wanted. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Ask for a meal together on purpose; she checks whether you mean a date. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Make a friendly ritual of imperfect breakfasts. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Decline food or explain a preference; she does not take it personally. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Tessa offers a flour-dusted hand across the table and makes clear that you can decline. She enjoys the closeness without turning it into a joke about owing her.

### T03 — The work she actually wants

**Present:** Tessa. **Background:** workshop. **Earliest stage:** 3.

Tessa shows you a theater set sketch. The production would take several weekends out of town. "I said I would think about it. Then I started listing all the people who might need shelves."

**The emotional question:** Can she want something without replacing herself first?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Ask whether she wants the work and respect the answer. | trust: 1; ease: 1; fact: tessa_theater. Then Visit window / next shared day. |
| romance: Say what you mean | Say attraction does not depend on her availability for chores. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Offer friendship without volunteering her time. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Listen without advice; let the decision be hers. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### T04 — Fine is not an answer

**Present:** Tessa. **Background:** workshop. **Earliest stage:** 4.

Tessa has overbooked herself and become short with you. She stops. "You did not agree to read my mind. I am tired, and I should have said so."

**The emotional question:** Can care be mutual and boundaries explicit?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Accept or discuss her apology and make a concrete smaller plan. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Respect her rest and ask for another date without demanding proof of affection. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Keep friendship while naming how the irritability felt. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Take space; she does not equate it with abandonment. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

### T05 — An invitation with empty hands

**Present:** Tessa. **Background:** street. **Earliest stage:** 5.

Tessa arrives without a tool bag. She lifts her empty hands as evidence. "No emergency. I wanted a walk with you. It took an embarrassing amount of practice to say that."

**The emotional question:** Do you want the person, not just the safe pair of hands?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Welcome the walk as freely chosen company. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Return her interest plainly; she asks whether you want another date. | trust: 1; spark: 1; status: exploring. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose a friendship where neither must earn invitations. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Take the walk without defining it yet. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Tessa stops beneath a streetlamp. "May I kiss you? I would like to do that without pretending it was an accident." If welcomed, she kisses you softly and asks whether you would like to stay in her arms a moment.

### T06 — The unfinished shelf

**Present:** Tessa. **Background:** workshop. **Earliest stage:** 6.

A shelf waits unfinished while Tessa closes the workshop. "It can wait until tomorrow." She looks at you. "I would rather know what we are doing tonight."

**The emotional question:** What ordinary future do you want together?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| connect: A little closer | Make an honest plan without a relationship label. | trust: 1; ease: 1. Then Visit window / next shared day. |
| romance: Say what you mean | Explicitly agree to be partners, with work, rest, and mutual care; requires prior exploration and repaired conflict. This branch requires an explicitly exclusive, one-to-one agreement. A request to date all four belongs to together instead. | trust: 1; spark: 1; status: exclusive. Then Visit window / next shared day. |
| friend: A friendship chosen | Choose dependable friendship with room for independent lives. | trust: 1; status: friend. Then Visit window / next shared day. |
| uncertain: Room to decide | Only when the PLAYER explicitly wants to wait or is unsure what they want. A character asking for more discussion is stay, not uncertain. Continue slowly or say goodbye kindly. | ease: 1. Then Visit window / next shared day. |
| hurt: A boundary named | Only for a clearly hurtful act, not awkwardness or disagreement. She names its impact; no instant forgiveness. | trust: -1; Offer private repair before romantic escalation. Then Visit window / next shared day. |
| leave: Another time | Player clearly ends the visit. Respect the exit without a penalty. | No automatic relationship change. Then Visit window / next shared day. |
| together: A shared relationship, freely chosen | Player explicitly proposes dating Hana, Mira, Leila, and Tessa openly, with each woman knowing and agreeing. Discuss your own needs and give only your own conditional consent. When shared_eligible is true and the player explicitly agrees to those needs, your authored choice is to try it willingly; do not invent an exclusive-only preference. Never consent for anyone else. Not triggered by a generic compliment or ordinary dating interest. | trust: 1; spark: 1; status: shared. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** Tessa offers a quiet cuddle on the workshop sofa after the tools are safely put away. "Nothing needs fixing tonight." A kiss is an invitation, never a requirement.

### G01 — Enough room for all of us

**Present:** Hana, Mira, Leila, Tessa. **Background:** roof. **Earliest stage:** 6.

All four women have independently said they want to try this shared relationship. Tonight is for making that choice practical. Hana wants dependable one-on-one time. Mira wants jealousy named instead of disguised as teasing. Leila needs long-distance plans that do not leave her out. Tessa wants rest and honest limits. None is agreeing to compete for scraps of attention.

**The emotional question:** Can the five of you agree on a relationship that gives everyone care, freedom, and an equal voice?

| Outcome | Conversation evidence and response | Consequence |
|---|---|---|
| together: Five people, one honest beginning | Discuss and explicitly accept honest communication, individual dates, no ranking of partners, consent for each affectionate act, and anyone being free to renegotiate or leave. All four have already independently opted in. | flag: everyone_agreed. Then Visit window / next shared day. |
| slow: More time, without pressure | Ask to keep dating slowly rather than finalize the shared relationship. No everyone ending yet. | flag: group_slow. Then Visit window / next shared day. |
| leave: A conversation to finish another day | Leave without an agreement; no collective commitment is assumed. | No automatic relationship change. Then Visit window / next shared day. |

**Romantic staging (only with established mutual interest):** After the agreement, each woman asks about affection in her own way: Hana offers her hand, Mira asks for a kiss without a dare, Leila invites an embrace, and Tessa makes space on the sofa. A shared blanket and four individual goodnight kisses are possible if everyone welcomes them. Do not imply that the women must date or kiss each other.

## A conversation left unfinished — REPAIR

Private with the affected woman; uses the street background. A specific apology and her agreement permit a cautious restart. Requesting space leaves the route paused. Repeated pressure after a warning, or an explicit decision to end the connection, closes it. An exit leaves the pause intact. No additional chapter is consumed.

## Endings

World outcome: a smaller, sustainable café if `plan_cafe` was chosen; otherwise a monthly supper club. Last Light survives as a warm full gathering or a smaller evening. Relationships are independent of the business outcome.

Each woman can finish as an exclusive partner, a participant in the openly shared relationship, an ongoing dating connection, a friend, a distant/closed connection, or an acquaintance. Romance includes invited kisses, hugs, hand-holding, and cuddling. There is at most one exclusive partner; that agreement cannot silently become shared. The four individual romance payoffs are Hana’s breakfast and window-bench cuddles, Mira’s ordinary Wednesday dinners and movie blankets, Leila’s goodbye kiss and first long-distance call, and Tessa leaving unfinished work for chosen time together.

**Everyone ending — Enough Room for All of Us:** develop all four romances, obtain four independent chapter-6 shared agreements, and finish G01 with a practical, mutual agreement. Hana requests individual time; Mira asks for honest discussion of jealousy; Leila asks not to be excluded by distance; Tessa protects rest and independence. The epilogue includes individual dates and a reunion evening with kisses, cuddling, and a crowded sofa. Dating all four does not imply they must date each other.

Friendship payoffs: Hana’s actual break, Mira’s unambiguous film night, Leila’s postcards and calls, Tessa’s breakfast without repairs. An undecided or unattached player retains a hopeful ending. Unresolved harm is never converted into automatic forgiveness.

## Current implementation limits

All 35 authored nodes, repair conversations, and state-based endings are playable. Scene classification remains probabilistic: exact evidence and allowed transitions guard the graph but cannot prove the model understood intent. Only the addressed character speaks per turn; everyone present witnesses it. The other speakers can be addressed next.

All four women have 12 illustrated poses and 120 skeletal motion variants each. Ten painted backgrounds match Hana’s style. Inference selects scene-safe poses and gestures; idle motion varies automatically. Kisses and paired cuddles are described in dialogue. Numeric relationship values and authored facts persist, but arbitrary long-term dialogue memory still inherits the inference service’s lossy summarization. No dynamic promise detector or automatic multi-speaker generation is implemented.
