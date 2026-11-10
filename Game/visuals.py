"""Scene-safe visual vocabulary shared with the native character library."""
POSES = ('neutral','wave','explain','folded','shy','laugh','worried','firm',
         'seated','seated_relaxed','affectionate','cuddle')
GESTURES = ('breathe','nod','shake_head','tilt','emphasize','chuckle',
            'bashful','reassure','sigh','glance')
SEATED_ROOMS = {'car','cafe','kitchen','cinema','roof','hall','bus','workshop'}
ROMANTIC_POSES = {'affectionate','cuddle'}
APPEARANCE = {
    'hana': 'Brown bob, amber eyes, gold hair clip; sleeveless ivory blouse with teal ribbon, navy pleated summer skirt, bare legs, brown ankle boots.',
    'mira': 'Short dark curls with a copper streak, warm tan skin, green eyes; rust open shirt, black cropped camisole, olive shorts, black high-top sneakers.',
    'leila': 'Long dark wavy hair loosely tied back, green-gray eyes; cream camisole, plum cardigan, charcoal tailored shorts, brown loafers.',
    'tessa': 'Brown side braid, blue eyes; sleeveless tied denim shirt over cream camisole, dark denim shorts, brown ankle boots.',
}

def allowed_poses(background, relationship):
    poses=list(POSES)
    if background not in SEATED_ROOMS:
        poses=[p for p in poses if p not in ('seated','seated_relaxed','cuddle')]
    if background=='car':
        poses=[p for p in poses if p in ('seated','seated_relaxed','cuddle')]
    if relationship['status'] not in ('exploring','exclusive','shared') or relationship['repair']:
        poses=[p for p in poses if p not in ROMANTIC_POSES]
    return poses

def default_pose(background):
    return 'seated' if background in ('car','cinema') else 'neutral'

def select_visual(response, background, relationship):
    permitted=allowed_poses(background,relationship)
    mood=response.get('mood','thoughtful')
    fallback={'warm':'neutral','playful':'laugh','thoughtful':'explain','firm':'firm'}.get(mood,'neutral')
    if fallback not in permitted:fallback=default_pose(background)
    pose=response.get('pose',fallback)
    if pose not in permitted:pose=fallback
    gesture=response.get('gesture',{'warm':'breathe','playful':'chuckle','thoughtful':'tilt','firm':'shake_head'}.get(mood,'breathe'))
    if gesture not in GESTURES:gesture='breathe'
    return {'pose':pose,'gesture':gesture}
