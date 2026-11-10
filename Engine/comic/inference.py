"""Background NDJSON client. Completion is distinct from worker teardown."""
import json
import os
import threading
import urllib.request
import uuid
from pathlib import Path
from PySide6.QtCore import QObject, Signal


class InferenceClient(QObject):
    token = Signal(str)
    completed = Signal(str)
    failed = Signal(str)
    command = Signal('QVariantMap')

    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.config = config
        self.sessions = {}
        self.store_path = Path(config['session_store']) if config.get('session_store') else None
        if self.store_path and self.store_path.exists():
            data=json.loads(self.store_path.read_text(encoding='utf-8-sig'))
            if data.get('format')!='panel.sessions' or data.get('version')!=1:
                raise ValueError('Unsupported session store format')
            self.sessions=data['sessions']
        self._guard = threading.Lock()
        self.pending = None

    def send(self, key, system, message):
        key=json.dumps(key)
        # UI also guards admission; no dependency on thread.is_alive().
        with self._guard:
            if self.pending is not None:
                return False
            self.pending = str(uuid.uuid4())
        threading.Thread(target=self._run,args=(key,system,message,self.pending),daemon=True).start()
        return True

    def _run(self, key, system, message, request_id):
        headers = {'Content-Type':'application/json'}
        secret = os.environ.get(self.config.get('key_env','INFERENCE_API_KEY'),'')
        if secret:
            headers['Authorization'] = 'Bearer '+secret
        base = self.config['url'].rstrip('/')
        def request(path, body):
            return urllib.request.urlopen(urllib.request.Request(base+path,
                data=json.dumps(body).encode('utf-8'),headers=headers),timeout=self.config.get('timeout',660))
        try:
            if key not in self.sessions:
                with request('/v1/sessions', {'system':system}) as response:
                    self.sessions[key] = json.load(response)['session_id']
                if self.store_path:
                    from .core import save
                    self.store_path.parent.mkdir(parents=True,exist_ok=True)
                    save(self.store_path,{'format':'panel.sessions','version':1,'sessions':self.sessions})
            payload = {'request_id':request_id,'message':message,'stream':True,
                       'max_tokens':self.config.get('max_tokens',512),
                       'temperature':self.config.get('temperature',0.7)}
            with request('/v1/sessions/'+self.sessions[key]+'/chat',payload) as response:
                for line in response:
                    if not line.strip():
                        continue
                    event = json.loads(line)
                    if event['type'] == 'token':
                        self.token.emit(event['content'])
                    elif event['type'] == 'animation':
                        self.command.emit({'type':'animation','actor':event['actor'],
                                           'pose':event['pose'],'animation':event['animation']})
                    elif event['type'] == 'error':
                        raise ValueError(event.get('detail','Generation failed'))
                    elif event['type'] == 'done':
                        with self._guard:
                            self.pending = None
                        self.completed.emit(event['content'])
                        return
            raise ValueError('Stream ended without a completion event')
        except Exception as exc:
            with self._guard:
                self.pending = None
            self.failed.emit(str(exc))
