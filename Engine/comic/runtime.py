from pathlib import Path
import json
import time
from PySide6.QtCore import QObject, Property, Signal, Slot, QUrl, QTimer
from PySide6.QtGui import QMatrix4x4, QFontDatabase
from PySide6.QtMultimedia import QAudioOutput, QMediaPlayer
from .core import Project, evaluate, matrix, mul, asset
from .inference import InferenceClient


class Layer(QObject):
    changed = Signal()
    def __init__(self, source, parent=None):
        super().__init__(parent)
        self._source = source
        self._matrix = QMatrix4x4()
        self._opacity = 1.
        self._z = 0.
        self._last = None
    source = Property(str, lambda s:s._source, constant=True)
    transform = Property(QMatrix4x4, lambda s:s._matrix, notify=changed)
    opacity = Property(float,lambda s:s._opacity, notify=changed)
    z = Property(float,lambda s:s._z,notify=changed)
    def update(self, m, opacity, z):
        values = (m, opacity, z)
        if values == self._last:
            return
        self._last = values
        a,b,c,d,x,y = m
        self._matrix = QMatrix4x4(a,c,0,x,b,d,0,y,0,0,1,0,0,0,0,1)
        self._opacity = max(0,min(1,opacity))
        self._z = z
        self.changed.emit()


class Runtime(QObject):
    changed = Signal()
    layersChanged = Signal()
    dialogueChanged = Signal()
    busyChanged = Signal()
    animationStateChanged = Signal()
    def __init__(self, path):
        super().__init__()
        self.project = Project(path)
        self._layers = []
        self.actors = []
        self._scene = {}
        self._theme = {}
        self._background = ''
        self._dialogue = ''
        self._busy = False
        self._token_buffer = ''
        self.text_timer = QTimer(self)
        self.text_timer.setInterval(33)
        self.text_timer.timeout.connect(self.flush_tokens)
        self.current_actor = ''
        self.frame_count = 0
        self.frame_intervals = []
        self.last_frame = None
        self.player = QMediaPlayer(self)
        self.audio = QAudioOutput(self)
        self.player.setAudioOutput(self.audio)
        self.player.errorOccurred.connect(lambda error,text: self.message('Music: '+text))
        config = dict(self.project.data.get('inference',{}))
        if config.get('session_store'):
            config['session_store']=str(asset(self.project.path,config['session_store']))
        self.client = InferenceClient(config,self) if config.get('url') else None
        if self.client:
            self.client.token.connect(self.token)
            self.client.completed.connect(self.complete)
            self.client.failed.connect(self.failure)
            self.client.command.connect(self.animation_command)
        self.scene(self.project.data['intro'])

    layers = Property('QVariantList',lambda s:s._layers,notify=layersChanged)
    sceneData = Property('QVariantMap',lambda s:s._scene,notify=changed)
    theme = Property('QVariantMap',lambda s:s._theme,notify=changed)
    background = Property(str,lambda s:s._background,notify=changed)
    dialogue = Property(str,lambda s:s._dialogue,notify=dialogueChanged)
    busy = Property(bool,lambda s:s._busy,notify=busyChanged)
    chatEnabled = Property(bool,lambda s:s.client is not None and bool(s.actors),notify=changed)
    animated = Property(bool,lambda s:any(a.get('animation') and not a.get('finished',False) for a in s.actors),notify=animationStateChanged)
    actorNames = Property('QVariantList',lambda s:[{'id':a['data']['id'],'name':a['data'].get('name',a['data']['id'])} for a in s.actors],notify=changed)
    canvasWidth = Property(int,lambda s:s.project.data.get('canvas',[1280,720])[0],constant=True)
    canvasHeight = Property(int,lambda s:s.project.data.get('canvas',[1280,720])[1],constant=True)
    title = Property(str,lambda s:s.project.data.get('title','Panel'),constant=True)

    def font(self, theme, path):
        result = dict(theme)
        if result.get('font'):
            index = QFontDatabase.addApplicationFont(str(asset(path,result['font'])))
            if index < 0:
                raise ValueError('Unable to load font: '+result['font'])
            result['font_family'] = QFontDatabase.applicationFontFamilies(index)[0]
        return result

    @Slot(str)
    def scene(self, sid):
        if self._busy:
            return
        self.sid = sid
        self.scene_path,self._scene = self.project.scenes[sid]
        self._theme = self.font(self.project.data.get('theme',{}),self.project.path)
        self._theme.update(self.font(self._scene.get('theme',{}),self.scene_path))
        self._background = QUrl.fromLocalFile(str(asset(self.scene_path,self._scene['background']))).toString() if self._scene.get('background') else ''
        old_layers = self._layers
        self._layers = []
        self.actors = []
        for data in self._scene.get('actors',[]):
            state = {'data':data,'pose':data['pose'],'started':time.perf_counter(),'nodes':[]}
            self.actors.append(state)
            self.build_actor(state)
        self.current_actor = self.actors[0]['data']['id'] if self.actors else ''
        self.selectActor(self.current_actor)
        self.message(self._scene.get('text',''))
        self.player.stop()
        music = self._scene.get('music')
        if music and music.get('path'):
            self.audio.setVolume(music.get('volume',0.5))
            self.player.setLoops(QMediaPlayer.Infinite if music.get('loop',True) else 1)
            self.player.setSource(QUrl.fromLocalFile(str(asset(self.scene_path,music['path']))))
            self.player.play()
        self.layersChanged.emit()
        self.animationStateChanged.emit()
        self.changed.emit()
        self.tick()
        for node in old_layers:
            node.deleteLater()

    def build_actor(self, state):
        data = state['data']
        rp,rig,animations = self.project.poses[(self.sid,data['id'],state['pose'])]
        state.update(rig=rig,animations=animations)
        pose = data['poses'][state['pose']]
        state['animation'] = pose.get('animation')
        for layer in rig['layers']:
            node = Layer(QUrl.fromLocalFile(str(asset(rp,layer['image']))).toString(),self)
            state['nodes'].append(node)
            self._layers.append(node)

    @Slot(str)
    def selectActor(self, id):
        self.current_actor = id
        actor = next((a for a in self.actors if a['data']['id']==id),None)
        self._theme = self.font(self.project.data.get('theme',{}),self.project.path)
        self._theme.update(self.font(self._scene.get('theme',{}),self.scene_path))
        if actor:
            self._theme.update(self.font(actor['data'].get('bubble',{}),self.scene_path))
        self.changed.emit()

    @Slot('QVariantMap')
    def action(self, action):
        if self._busy:
            return
        try:
            if action['type']=='scene':
                self.scene(action['scene'])
            elif action['type']=='animation':
                self.play(action['actor'],action['pose'],action['animation'])
            elif action['type']=='text':
                self.message(action['text'])
        except (KeyError,ValueError) as exc:
            self.message('Invalid action: '+str(exc))

    @Slot(str,str,str)
    def play(self, actor, pose, animation):
        state = next((a for a in self.actors if a['data']['id']==actor),None)
        if state is None:
            raise ValueError('Unknown actor')
        if (self.sid,actor,pose) not in self.project.poses:
            raise ValueError('Unknown pose')
        if animation not in self.project.poses[(self.sid,actor,pose)][2]:
            raise ValueError('Unknown animation')
        if pose != state['pose']:
            old = state['nodes']
            self._layers = [n for n in self._layers if n not in old]
            state.update(pose=pose,nodes=[])
            self.build_actor(state)
            self.layersChanged.emit()
            for node in old:
                node.deleteLater()
        state.update(animation=animation,started=time.perf_counter(),finished=False)
        self.animationStateChanged.emit()
        self.tick()

    @Slot('QVariantMap')
    def animation_command(self, command):
        try:
            self.play(command['actor'],command['pose'],command['animation'])
        except (KeyError,ValueError):
            # Model commands are data; only declared capabilities can execute.
            pass

    @Slot()
    def tick(self):
        now = time.perf_counter()
        if self.last_frame is not None and len(self.frame_intervals)<10000:
            self.frame_intervals.append(now-self.last_frame)
        self.last_frame = now
        self.frame_count += 1
        for state in self.actors:
            if state.get('finished',False):
                continue
            animation = state['animations'].get(state['animation'])
            layers,_ = evaluate(state['rig'],animation,now-state['started'])
            outer = matrix(state['data'])
            for node,layer in zip(state['nodes'],layers):
                node.update(mul(outer,layer['matrix']),layer.get('opacity',1),state['data'].get('z',0)+layer.get('z',0))
            if animation and not animation.get('loop',True) and now-state['started']>=animation['duration']:
                state['finished']=True
                self.animationStateChanged.emit()

    def message(self,text):
        self._dialogue = text
        self.dialogueChanged.emit()

    @Slot(str)
    def send(self, text):
        if self._busy or not self.client or not text.strip() or not self.current_actor:
            return
        actor = next(a['data'] for a in self.actors if a['data']['id']==self.current_actor)
        system = actor.get('startup_prompt','')
        history = actor.get('history',[])
        system += '\nScene history:\n'+json.dumps(history,ensure_ascii=False)
        system += '\nAvailable animation capabilities:\n'+json.dumps(self.project.capabilities()[self.sid][actor['id']])
        if self.client.send((self.sid,actor['id']),system,text):
            self._busy = True
            self.busyChanged.emit()
            self.message('')

    @Slot(str)
    def token(self,text):
        self._token_buffer += text
        if not self.text_timer.isActive():
            self.text_timer.start()

    def flush_tokens(self):
        self.text_timer.stop()
        self._dialogue += self._token_buffer
        self._token_buffer = ''
        self.dialogueChanged.emit()

    @Slot(str)
    def complete(self,text):
        self._busy = False
        self.busyChanged.emit()
        self.text_timer.stop()
        self._token_buffer = ''
        self.message(text)

    @Slot(str)
    def failure(self,text):
        self._busy = False
        self.busyChanged.emit()
        self.text_timer.stop()
        self._token_buffer = ''
        self.message('Inference: '+text)
