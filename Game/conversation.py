"""Durable, character-local memory and incremental structured-dialogue decoding."""
import json
import re


def reply_prefix(raw):
    """Expose only decoded dialogue, never the JSON control fields or partial escapes."""
    match = re.search(r'(?:^\s*\{|,)\s*"reply"\s*:\s*"', raw)
    if not match:
        return ''
    start = match.end()
    escaped = False
    end = start
    for i in range(start, len(raw)):
        char = raw[i]
        if char == '"' and not escaped:
            end = i
            break
        escaped = not escaped if char == '\\' else False
        end = i + 1
    fragment = raw[start:end]
    # A chunk can end inside an escape or UTF-16 surrogate pair.
    for trim in range(min(12, len(fragment)) + 1):
        candidate = fragment[:len(fragment)-trim] if trim else fragment
        try:
            value = json.loads('"' + candidate + '"')
            if value and 0xD800 <= ord(value[-1]) <= 0xDBFF:
                continue
            return value
        except (ValueError, UnicodeError):
            continue
    return ''


def fresh_memory():
    return dict(feeling='neutral', unresolved=[], facts=[], events=[])


def offers_repair(text):
    # A greeting, topic change, compliment or model-selected label cannot erase an incident.
    return bool(re.search(r"\b(sorry|apolog\w*|regret\w*|forgive\w*|make it right|take (?:that|it) back|"
        r"should(?:n't| not) have|was (?:wrong|cruel|unfair|hurtful)|my fault|i hurt you|"
        r"perd[oó]n\w*|lo siento|me disculpo)\b", text.casefold()))


def relevant_history(journal, utterance, limit=6):
    """Retrieve older witnessed exchanges in addition to the recent conversation."""
    words = set(re.findall(r'\w{4,}', utterance.casefold()))
    ranked = sorted(enumerate(journal[:-16]),
                    key=lambda item: (len(words & set(re.findall(r'\w{4,}', item[1].casefold()))), item[0]),
                    reverse=True)
    return [text for _, text in ranked[:limit] if words & set(re.findall(r'\w{4,}', text.casefold()))]


def remember(memory, response, player, node):
    evidence = response.get('evidence', '')
    supported = isinstance(evidence, str) and bool(evidence.strip()) and evidence.strip().casefold() in player.casefold()
    reaction = response.get('reaction', 'hold')
    # Firm dialogue is already a boundary signal in older saved requests.
    upset = (response.get('boundary') == 'new_violation' and reaction == 'upset') if 'boundary' in response else (reaction == 'upset' or response.get('mood') == 'firm' or response.get('outcome') == 'hurt')
    if upset and supported and (not memory['unresolved'] or response.get('outcome') == 'hurt' or response.get('boundary') == 'new_violation'):
        incident = dict(node=node, player=player, response=response['reply'], resolved=False)
        memory['unresolved'].append(incident)
        memory['events'].append(dict(kind='hurt', **incident))
        memory['feeling'] = 'upset'
    elif reaction == 'reconciled' and supported and offers_repair(player) and memory['unresolved']:
        memory['events'].append(dict(kind='reconciliation', node=node, player=player, response=response['reply']))
        memory['unresolved'] = []
        memory['feeling'] = 'cautious'
    elif not memory['unresolved']:
        memory['feeling'] = response.get('mood', 'thoughtful')
    for fact in response.get('memories', []) if isinstance(response.get('memories', []), list) else []:
        if not isinstance(fact, dict):
            continue
        quote, note = fact.get('quote', ''), fact.get('note', '')
        if isinstance(quote, str) and quote.strip() and quote.casefold() in player.casefold() and isinstance(note, str) and note.strip():
            item = dict(node=node, quote=quote[:800], note=note[:500])
            if item not in memory['facts']:
                memory['facts'].append(item)
