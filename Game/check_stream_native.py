"""Test the actual native streaming UI without relying on model timing."""
import json
import secrets
import subprocess
import threading
import time
import uuid
from http.server import ThreadingHTTPServer
from runtime import Game, ROOT
from server import handler_for

class SlowVoice:
    def create(self, system): return str(uuid.uuid4())
    def speak(self, sid, payload, emit):
        line='I remember our conversation. I am still hurt, but I am listening.'
        for end in range(1,len(line)+1):
            emit(line[:end])
            time.sleep(.035)
        # The native client must already have rendered a partial reply before completion.
        assert (ROOT/'data'/'stream-partial.ppm').exists()
        return line
    def chat(self,sid,payload):
        # Simulate the slow director while checking even the LAST speech chunk is visible.
        time.sleep(1)
        assert (ROOT/'data'/'stream-visible.txt').read_text()=='I remember our conversation. I am still hurt, but I am listening.'
        return json.dumps(dict(reaction='hold',mood='thoughtful',outcome='stay',evidence='',memories=[]))

game=Game(ROOT/'data'/('stream-test-'+str(uuid.uuid4())+'.json'),SlowVoice())
game.enter('H02')
partial=ROOT/'data'/'stream-partial.ppm'
if partial.exists(): partial.unlink()
token=secrets.token_urlsafe(32)
server=ThreadingHTTPServer(('127.0.0.1',0),handler_for(game,token))
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    capture=ROOT/'data'/'stream-complete.ppm'
    subprocess.run([str(ROOT.parent/'ComicEngine/bin/StoryGame.exe'),str(server.server_port),token,
                    '--stream-smoke',str(capture)],check=True,timeout=40)
    assert int(capture.with_suffix('.ppm.txt').read_text())>2
    assert game.state['pending'] is None
    assert game.state['last_reply'].endswith('listening.')
    assert partial.read_bytes()!=capture.read_bytes()
    print('PASS: native client painted incremental speech before generation completed; final state committed.')
finally:
    server.shutdown();server.server_close()
