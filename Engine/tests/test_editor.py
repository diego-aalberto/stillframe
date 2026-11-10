import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QPointF
from comic.editor import Editor
from comic.core import evaluate,validate_animation,save,load


def test_editor_joint_drag_uneven_keys_save_reload(tmp_path):
    app=QApplication.instance() or QApplication([])
    editor=Editor()
    editor.add_joint(QPointF(30,40))
    editor.add_joint(QPointF(60,80))
    assert len(editor.rig['joints'])==2
    assert editor.rig['joints'][1]['parent']=='joint1'
    editor.new_clip()
    editor.time.setValue(.25);editor.move_joint('joint2',QPointF(70,90))
    editor.time.setValue(1.73);editor.move_joint('joint2',QPointF(100,110))
    validate_animation(editor.animation,editor.rig)
    assert [k['time'] for k in editor.animation['tracks'][0]['keys']]==[.25,1.73]
    _,world=evaluate(editor.rig,editor.animation,1.73,False)
    assert world['joint2'][4:]==(100,110)
    save(tmp_path/'clip.anim',editor.animation)
    assert load(tmp_path/'clip.anim','animation')==editor.animation
    editor.dirty=False;editor.close()


def test_runtime_completion_unlocks_before_display():
    from comic.runtime import Runtime
    app=QApplication.instance() or QApplication([])
    runtime=Runtime(Path(__file__).resolve().parents[1]/'examples/project.panel')
    runtime._busy=True
    events=[]
    runtime.busyChanged.connect(lambda:events.append(('busy',runtime.busy)))
    runtime.dialogueChanged.connect(lambda:events.append(('text',runtime.busy)))
    runtime.complete('done')
    assert events==[('busy',False),('text',False)]


def test_client_releases_admission_before_response_context_closes(monkeypatch):
    import json
    import threading
    import time
    from comic.inference import InferenceClient
    app=QApplication.instance() or QApplication([])
    closing=threading.Event();release=threading.Event();closed=threading.Event()
    count=[0]
    class Response:
        def __init__(self,first):self.first=first
        def __enter__(self):return self
        def __iter__(self):return iter([json.dumps({'type':'done','content':'answer'}).encode()])
        def __exit__(self,*args):
            if self.first:
                closing.set();release.wait(2);closed.set()
    def open_request(*args,**kwargs):
        count[0]+=1
        return Response(count[0]==1)
    monkeypatch.setattr('urllib.request.urlopen',open_request)
    client=InferenceClient({'url':'http://example.invalid'})
    client.sessions[json.dumps(('scene','actor'))]='session'
    received=[];client.completed.connect(received.append)
    assert client.send(('scene','actor'),'prompt','first')
    try:
        assert closing.wait(1)
        app.processEvents()
        assert received==['answer']
        assert client.pending is None
        assert client.send(('scene','actor'),'prompt','second')
        deadline=time.monotonic()+1
        while client.pending is not None and time.monotonic()<deadline:time.sleep(.001)
        assert client.pending is None
    finally:
        release.set();assert closed.wait(1)
