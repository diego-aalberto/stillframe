"""Exercise the authenticated bridge and native renderer in hidden test windows."""
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import secrets
import subprocess
import threading
import urllib.error
import urllib.request
import uuid
from runtime import Game, ROOT
from server import handler_for
from story import NODES

def main():
    path=ROOT/'data'/('native-smoke-'+str(uuid.uuid4())+'.json')
    game=Game(path)
    token=secrets.token_urlsafe(32)
    server=ThreadingHTTPServer(('127.0.0.1',0),handler_for(game,token))
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        url=f'http://127.0.0.1:{server.server_address[1]}/state'
        try:
            urllib.request.urlopen(url)
            raise AssertionError('Unauthenticated state was accepted')
        except urllib.error.HTTPError as exc:assert exc.code==401
        nodes=['S01','S02','M01','L01','T01','G01']
        seen={NODES[n]['background'] for n in nodes}
        for n in NODES.values():
            if n['background'] not in seen:
                nodes.append(n['id']);seen.add(n['background'])
        for node in nodes:
            game.enter(node)
            image=ROOT/'data'/f'native-{node}.ppm'
            result=subprocess.run([str(ROOT.parent/'ComicEngine'/'bin'/'StoryGame.exe'),
                str(server.server_address[1]),token,'--smoke',str(image)],timeout=45)
            assert result.returncode==0 and image.stat().st_size>10000
            assert image.with_suffix('.ppm.txt').read_text().splitlines()[0]==node
            print(f'PASS {node}: authenticated bridge -> native comic scene -> screenshot',flush=True)
    finally:
        server.shutdown();server.server_close()

if __name__=='__main__':main()
