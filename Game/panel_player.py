"""Story-owned adapter for the generic Panel CPU engine. No engine modifications."""
import argparse
import html
import os
from pathlib import Path
import re
import sys
import threading

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'Engine'))
os.environ['QT_QUICK_BACKEND']='software'
os.environ.pop('QSG_RHI_BACKEND',None)
os.environ['QT_QUICK_CONTROLS_STYLE']='Basic'
from PySide6.QtCore import Property,Signal,Slot,QUrl,QTimer
from PySide6.QtWidgets import QApplication
from PySide6.QtQuick import QQuickWindow,QSGRendererInterface
from PySide6.QtQml import QQmlApplicationEngine
from comic.runtime import Runtime
from runtime import Game
from story import CAST

class StoryPlayer(Runtime):
    storyChanged=Signal()
    speakerChanged=Signal()
    formattedChanged=Signal()
    progress=Signal(str,str,object)
    finished=Signal(str)

    def __init__(self,save=None,inference=None):
        self.game=Game(save,inference)
        self._view={};self._menu=True;self._intro=True;self._error='';self._latest=None
        super().__init__(ROOT/'panel/story.panel')
        self.changed.connect(self.speakerChanged.emit)
        self.dialogueChanged.connect(self.formattedChanged.emit)
        self.progress.connect(self.on_progress)
        self.finished.connect(self.on_finished)
        self.sync(initial=True)

    story = Property('QVariantMap',lambda s:s._view,notify=storyChanged)
    menu = Property(bool,lambda s:s._menu,notify=storyChanged)
    intro = Property(bool,lambda s:s._intro,notify=storyChanged)
    error = Property(str,lambda s:s._error,notify=storyChanged)
    speaker = Property(str,lambda s:CAST.get(s.current_actor,{}).get('name','A moment here'),notify=speakerChanged)
    visits = Property('QVariantList',lambda s:[dict(id=c,name=CAST[c]['name']) for c in s._view.get('available',[])],notify=storyChanged)
    canChat = Property(bool,lambda s:not(s._menu or s._intro or s._view.get('hub') or s._view.get('ended') or s._view.get('conversation_blocked') or s._view.get('pending') or s._view.get('needs_name')),notify=storyChanged)
    richDialogue = Property(str,lambda s:re.sub(r'(&quot;.*?(?:&quot;|$)|“.*?(?:”|$))',r'<b>\1</b>',html.escape(s._dialogue)).replace('\n','<br>'),notify=formattedChanged)
    history = Property(str,lambda s:'\n\n'.join(t['speaker']+': '+t['text'] for t in s._view.get('transcript',[])),notify=storyChanged)

    def sync(self,initial=False):
        previous=self._view.get('node')
        self._view=self.game.view()
        sid=self._view['node']
        if sid=='REPAIR':sid+='_'+self.game.node['cast'][0]
        if self.sid!=sid or initial:
            super().scene(sid)
            self._intro=not self._view['turns'] and not self._view['hub'] and not self._view['ended']
            track=ROOT/'music'/(self._view['node']+'.wav')
            if track.is_file():
                self.player.setSource(QUrl.fromLocalFile(str(track)));self.player.setLoops(-1);self.player.play()
        if initial or self.current_actor not in self._view['cast']:self.selectActor(self._view['speaker'])
        for who in self._view['cast']:
            visual=self.game.state['visuals'][who]
            state=next(a for a in self.actors if a['data']['id']==who)
            if state['pose']!=visual['pose'] or state['animation']!=visual['gesture']:
                self.play(who,visual['pose'],visual['gesture'])
        text=self._view['ending'] if self._view['ended'] else self._view['opening'] if self._intro else self._view['reply'] or self._view['opening']
        self.message(text);self.storyChanged.emit()

    @Slot(str,str,object)
    def on_progress(self,kind,who,value):
        if self.current_actor!=who:self.selectActor(who)
        if kind=='start':self.play(who,value['pose'],value['gesture'])
        else:
            self._latest=value
            if not self.text_timer.isActive():self.text_timer.start()

    def flush_tokens(self):
        self.text_timer.stop()
        if self._latest is not None:self.message(self._latest);self._latest=None

    @Slot(str)
    def on_finished(self,error):
        self._busy=False;self.busyChanged.emit();self.text_timer.stop();self._latest=None
        self._error=error;self.sync()

    @Slot(str)
    def send(self,text):
        if self._busy or not self.canChat or not text.strip():return
        self.chat(text)

    def chat(self,text=None):
        if self._busy:return
        self._busy=True;self.busyChanged.emit();self._error='';self.message('');self.storyChanged.emit()
        who=self.current_actor
        def worker():
            try:
                callback=lambda *args:self.progress.emit(*args)
                if text is None:self.game.retry(callback)
                else:self.game.chat(who,text,callback)
                self.finished.emit('')
            except Exception as exc:self.finished.emit(str(exc))
        threading.Thread(target=worker,daemon=True).start()

    @Slot(str,str)
    def command(self,action,value=''):
        if self._busy and action not in ('menu','resume'):return
        try:
            self._error=''
            if action=='menu':self._menu=True
            elif action=='resume':self._menu=False
            elif action=='begin':self._intro=False
            elif action=='new':self.game.new(value);self._menu=False;self._intro=True
            elif action=='name':self.game.set_name(value);self._menu=False;self._intro=True
            elif action=='advance':self.game.advance()
            elif action=='leave':self.game.leave_scene()
            elif action=='keep':self.game.keep_talking()
            elif action=='visit':self.game.visit(value)
            elif action=='retry':self.chat();return
            if not self._busy:self.sync()
            else:self.storyChanged.emit()
        except Exception as exc:self._error=str(exc);self.storyChanged.emit()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--save',type=Path);parser.add_argument('--smoke-seconds',type=float,default=0);parser.add_argument('--capture');args=parser.parse_args()
    app=QApplication(sys.argv)
    QQuickWindow.setGraphicsApi(QSGRendererInterface.Software);QQuickWindow.setSceneGraphBackend('software')
    runtime=StoryPlayer(args.save)
    qml=QQmlApplicationEngine();qml.rootContext().setContextProperty('engine',runtime)
    qml.load(QUrl.fromLocalFile(str(ROOT/'StoryPlayer.qml')))
    if not qml.rootObjects():return 1
    if args.smoke_seconds:
        def finish():
            window=qml.rootObjects()[0]
            if args.capture:window.grabWindow().save(args.capture)
            print('Renderer:',window.rendererInterface().graphicsApi(),flush=True);app.quit()
        QTimer.singleShot(int(args.smoke_seconds*1000),finish)
    result=app.exec();runtime.player.stop();qml.deleteLater();return result

if __name__=='__main__':raise SystemExit(main())
