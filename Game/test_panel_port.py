"""Port contract: original director state, all scenes, streaming, and Panel assets."""
import json
from pathlib import Path
import sys
import time
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parent))
from panel_player import StoryPlayer,ROOT
from story import NODES,CAST
from visuals import POSES,GESTURES
from PySide6.QtWidgets import QApplication
from comic.core import Project,evaluate
from PySide6.QtCore import QUrl, QObject, QMetaObject
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickWindow,QSGRendererInterface

@pytest.fixture(scope='module')
def app():return QApplication.instance() or QApplication([])

class Fake:
    def create(self,system):return 'test-session'
    def stream_chat(self,sid,payload,emit):
        emit('Hello');emit('Hello, Alex.')
        return json.dumps(dict(conversation='open',boundary='none',withdraw_outcome=False,reply='Hello, Alex.',reaction='hold',memories=[],outcome='stay',evidence='',mood='warm',pose='neutral'))

def test_every_scene_name_start_resume_and_repair(app,tmp_path):
    player=StoryPlayer(tmp_path/'save.json',Fake())
    player.command('name','Alex');player.command('begin','')
    assert player.canChat and player.game.state['player']=='Alex'
    for sid in NODES:
        player.game.enter(sid);player.sync();player.command('begin','')
        assert player.canChat,sid
        assert len(player.actors)==len(NODES[sid]['cast'])
        player.command('menu','');assert not player.canChat
        player.command('resume','');assert player.canChat
    for who in CAST:
        player.game.state['repair_who']=who;player.game.enter('REPAIR');player.sync();player.command('begin','')
        assert player.sid=='REPAIR_'+who and player.canChat
    player.player.stop()

def test_stream_commit_and_existing_save_continuity(app,tmp_path):
    save=tmp_path/'save.json';player=StoryPlayer(save,Fake())
    player.command('name','Alex');player.command('begin','');player.send('Hello')
    deadline=time.monotonic()+5
    while player.busy and time.monotonic()<deadline:app.processEvents();time.sleep(.005)
    assert not player.busy and player.dialogue=='Hello, Alex.'
    assert player.game.state['pending'] is None
    restored=StoryPlayer(save,Fake())
    assert restored.game.state['transcript']==player.game.state['transcript']
    assert restored.game.state['player']=='Alex'
    player.message('She says "hello".');assert '<b>&quot;hello&quot;</b>' in player.richDialogue

def test_all_pose_and_animation_capabilities():
    project=Project(ROOT/'panel/story.panel')
    assert len(project.scenes)==39
    for sid,actors in project.capabilities().items():
        for who,poses in actors.items():
            assert set(poses)==set(POSES)
            for pose,gestures in poses.items():assert set(gestures)==set(GESTURES)
    for who in CAST:
        for pose in POSES:
            _,rig,clips=project.poses[('S02',who,pose)]
            for clip in clips.values():
                first,_=evaluate(rig,clip,0);middle,_=evaluate(rig,clip,clip['duration']/4)
                assert first[0]['matrix']!=middle[0]['matrix']

def test_qml_menu_scene_and_group_render(app,tmp_path):
    QQuickWindow.setGraphicsApi(QSGRendererInterface.Software)
    QQuickWindow.setSceneGraphBackend('software')
    player=StoryPlayer(tmp_path/'save.json',Fake())
    qml=QQmlApplicationEngine();warnings=[]
    qml.warnings.connect(lambda errors:warnings.extend(str(e) for e in errors))
    qml.rootContext().setContextProperty('engine',player)
    qml.load(QUrl.fromLocalFile(str(ROOT/'StoryPlayer.qml')))
    assert qml.rootObjects()
    window=qml.rootObjects()[0]
    popup=window.findChild(QObject,'pathsPanel')
    assert popup is not None
    QMetaObject.invokeMethod(popup,'open')
    for sid in ('S01','S02'):
        player.command('name','Alex');player.game.enter(sid);player.sync();player.command('begin','')
        for _ in range(30):app.processEvents();time.sleep(.01)
        assert not window.grabWindow().isNull()
        assert window.rendererInterface().graphicsApi()==QSGRendererInterface.Software
    QMetaObject.invokeMethod(popup,'close')
    player.game.enter('H01');player.sync();player.command('begin','')
    player.game.select_outcome('hana','romance');player.sync();app.processEvents()
    assert window.findChild(QObject,'continueAction').property('visible')
    assert window.findChild(QObject,'keepTalkingAction').property('visible')
    player.command('keep','');app.processEvents()
    assert window.findChild(QObject,'conversationInput').property('visible')
    refusal=dict(conversation='refuse',boundary='new_violation',reaction='upset',withdraw_outcome=False,reply='Give me space.')
    player.game.resolve_conversation('hana',refusal,True,'Your art is worthless');player.sync();app.processEvents()
    assert window.findChild(QObject,'keepTalkingAction').property('visible')
    player.command('keep','')
    player.game.resolve_conversation('hana',refusal,True,'Still worthless');player.sync();app.processEvents()
    assert window.findChild(QObject,'continueAction').property('visible')
    assert not window.findChild(QObject,'keepTalkingAction').property('visible')
    assert not window.findChild(QObject,'conversationInput').property('visible')
    window.close();qml.deleteLater();app.processEvents()
    assert not warnings,warnings


def test_onward_paths_group_destinations_and_only_available_visits(app,tmp_path):
    player=StoryPlayer(tmp_path/'save.json',Fake())
    assert len(player.game.view()['paths'])==1
    assert player.game.view()['paths'][0]['destination']==NODES['S02']['title']
    player.game.state.update(hub=True,stage=1,shared_index=2)
    paths=player.game.view()['paths']
    assert len(paths)==len(player.game.available())+1
    player.game.state['visits']=4
    assert len(player.game.view()['paths'])==1
    player.game.state['ended']=True
    assert player.game.view()['paths']==[]
