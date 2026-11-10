"""Regenerate the optional, per-scene music catalog without touching any audio."""
import csv
import json
from pathlib import Path
from story import NODES

folder=Path(__file__).resolve().parent/'music'
folder.mkdir(exist_ok=True)
tracks=[dict(scene=n['id'],title=n['title'],filename=n['id']+'.wav') for n in NODES.values()]
tracks.append(dict(scene='REPAIR',title='A private conversation to repair trust',filename='REPAIR.wav'))
(folder/'tracks.json').write_text(json.dumps(tracks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
with (folder/'tracks.csv').open('w',encoding='utf-8-sig',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=['scene','title','filename'])
    writer.writeheader();writer.writerows(tracks)
lines=['# Scene music','',
       'Put your audio files directly in this folder using the filenames below.',
       'The game loops the current scene’s track automatically and switches when the scene changes.',
       'Missing files are silent; no placeholder audio is supplied. Restart or leave and return to a scene after adding its file.',
       'Use uncompressed PCM WAV (16-bit stereo, 44.1 or 48 kHz). The file should have a seamless loop if you want a seamless repeat.',
       'The private repair scene uses REPAIR.wav for every character. Closing the game stops playback.',
       '', '| Scene | Expected file | Scene title |','|---|---|---|']
lines += [f"| {t['scene']} | {t['filename']} | {t['title']} |" for t in tracks]
(folder/'README.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'{len(tracks)} scene music filenames written to {folder}')
