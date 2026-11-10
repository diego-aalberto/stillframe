"""Raster editor sharing the player's rig evaluator and file formats."""
from copy import deepcopy
import json
import os
from pathlib import Path
import time
from PySide6.QtCore import Qt, QTimer, QPointF, QRectF
from PySide6.QtGui import QColor, QPainter, QPen, QPixmap, QTransform, QAction, QKeySequence
from PySide6.QtWidgets import (QMainWindow,QWidget,QHBoxLayout,QVBoxLayout,QSplitter,
    QGraphicsView,QGraphicsScene,QGraphicsEllipseItem,QGraphicsPixmapItem,QGraphicsItem,
    QTreeWidget,QTreeWidgetItem,QDoubleSpinBox,QComboBox,QCheckBox,QPushButton,QLabel,
    QFileDialog,QMessageBox,QInputDialog,QFormLayout,QScrollArea,QPlainTextEdit,QTabWidget)
from .core import load,save,validate_rig,validate_animation,evaluate,CHANNELS,asset,Project


def transform(m):
    a,b,c,d,x,y = m
    return QTransform(a,b,c,d,x,y)


class JointItem(QGraphicsEllipseItem):
    def __init__(self, editor, id):
        super().__init__(-7,-7,14,14)
        self.editor,self.id = editor,id
        self.setBrush(QColor('#ffce69')); self.setPen(QPen(QColor('#1c2230'),2))
        self.setFlags(QGraphicsItem.ItemIsMovable | QGraphicsItem.ItemIsSelectable | QGraphicsItem.ItemIgnoresTransformations)
        self.setZValue(100000)
    def mousePressEvent(self,event):
        self.editor.choose('joint:'+self.id)
        self.editor.checkpoint()
        super().mousePressEvent(event)
    def mouseReleaseEvent(self,event):
        super().mouseReleaseEvent(event)
        self.editor.move_joint(self.id,self.pos())


class LayerItem(QGraphicsPixmapItem):
    def __init__(self, editor, id, pixmap):
        super().__init__(pixmap)
        self.editor,self.id = editor,id
        self.setFlags(QGraphicsItem.ItemIsSelectable | QGraphicsItem.ItemIsMovable)
        self.setTransformationMode(Qt.SmoothTransformation)
    def mousePressEvent(self,event):
        self.editor.choose('layer:'+self.id)
        self.editor.checkpoint()
        super().mousePressEvent(event)
    def mouseReleaseEvent(self,event):
        super().mouseReleaseEvent(event)
        delta = self.pos()
        self.setPos(0,0)
        self.editor.move_layer(self.id,delta)


class Canvas(QGraphicsView):
    def __init__(self, editor, scene):
        super().__init__(scene)
        self.editor = editor
        self.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
        self.setBackgroundBrush(QColor('#252b37'))
        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setViewportUpdateMode(QGraphicsView.MinimalViewportUpdate)
    def mousePressEvent(self,event):
        if self.editor.add_mode and event.button()==Qt.LeftButton:
            self.editor.add_joint(self.mapToScene(event.position().toPoint()))
            return
        super().mousePressEvent(event)
    def wheelEvent(self,event):
        factor = 1.15 if event.angleDelta().y()>0 else 1/1.15
        if .03 < self.transform().m11()*factor < 20:
            self.scale(factor,factor)
        event.accept()


class Timeline(QWidget):
    """Each row is a track. Drag a diamond to any timestamp; click to scrub."""
    def __init__(self, editor):
        super().__init__(); self.editor=editor; self.drag=None
        self.setMinimumHeight(150)
    def tracks(self):
        return self.editor.animation.get('tracks',[]) if self.editor.animation else []
    def xpos(self,t):
        duration = self.editor.duration.value()
        return 190 + t/max(duration,.001)*max(1,self.width()-205)
    def timeat(self,x):
        return round(max(0,min(self.editor.duration.value(),(x-190)/max(1,self.width()-205)*self.editor.duration.value())),3)
    def paintEvent(self,event):
        p=QPainter(self); p.fillRect(self.rect(),QColor('#181e29'))
        tracks=self.tracks(); self.setMinimumHeight(max(150,35+len(tracks)*26))
        p.setPen(QColor('#aab6cb'))
        for i in range(11):
            t=self.editor.duration.value()*i/10
            x=self.xpos(t)
            p.drawText(QPointF(x,16),f'{t:.2f}')
            p.setPen(QColor('#303a4b'));p.drawLine(QPointF(x,24),QPointF(x,self.height()));p.setPen(QColor('#aab6cb'))
        for row,tr in enumerate(tracks):
            y=40+row*26
            p.drawText(QPointF(6,y+4),tr['target']+' / '+tr['channel'])
            for key in tr['keys']:
                x=self.xpos(key['time'])
                p.setBrush(QColor('#ffce69'));p.drawEllipse(QPointF(x,y),5,5)
        p.setPen(QPen(QColor('#73e3dc'),2))
        x=self.xpos(self.editor.time.value());p.drawLine(QPointF(x,20),QPointF(x,self.height()))
        if not tracks:
            p.drawText(QPointF(10,65),'Choose a part, set a time, change a value, then add a key.')
    def mousePressEvent(self,event):
        x,y=event.position().x(),event.position().y()
        row=round((y-40)/26)
        if 0<=row<len(self.tracks()):
            tr=self.tracks()[row]
            for key in tr['keys']:
                if abs(self.xpos(key['time'])-x)<9:
                    self.editor.checkpoint();self.drag=(tr,key)
                    self.editor.choose(tr['target']);self.editor.channel.setCurrentText(tr['channel'])
                    self.editor.time.setValue(key['time']);return
        self.editor.time.setValue(self.timeat(x))
    def mouseMoveEvent(self,event):
        t=self.timeat(event.position().x())
        if self.drag:
            tr,key=self.drag
            if all(k is key or abs(k['time']-t)>.0001 for k in tr['keys']):
                key['time']=t;tr['keys'].sort(key=lambda k:k['time']);self.editor.dirty=True
        self.editor.time.setValue(t);self.update()
    def mouseReleaseEvent(self,event):
        self.drag=None;self.editor.refresh()


class Editor(QMainWindow):
    def __init__(self, path=None):
        super().__init__()
        self.setWindowTitle('Panel — Layer & animation editor'); self.resize(1400,900)
        self.rig={'format':'panel.rig','version':1,'joints':[],'layers':[]}
        self.rig_path=Path.cwd()/'untitled.rig'
        self.animation=None;self.animation_path=None;self.selected=None
        self.saved_rig=deepcopy(self.rig);self.saved_animation=None
        self.undo_stack=[];self.redo_stack=[];self.dirty=False;self.add_mode=False;self.playing=False;self.updating=False
        self.project_path=None;self.document_paths=[]
        self.scene=QGraphicsScene(self);self.scene.setSceneRect(-500,-500,2500,2500)
        self.canvas=Canvas(self,self.scene);self.joint_items={};self.layer_items={};self.bone_lines=[]
        self.tree=QTreeWidget();self.tree.setHeaderLabels(['Parts']);self.tree.currentItemChanged.connect(self.tree_selection)
        left=QWidget();left_layout=QVBoxLayout(left);left_layout.addWidget(self.tree)
        for label,callback in [('Import image layer',self.import_layer),('Add joint: click canvas',self.start_joint),('Rename selected part',self.rename_part),('Delete selected part',self.delete_part),('Fit canvas',self.fit)]:
            button=QPushButton(label);button.clicked.connect(callback);left_layout.addWidget(button)
        self.mode=QComboBox();self.mode.addItems(['Rest / skeleton setup','Animation keys']);self.mode.currentIndexChanged.connect(self.refresh)
        self.parent_box=QComboBox();self.parent_box.currentIndexChanged.connect(self.change_parent)
        self.fields={};form=QFormLayout();form.addRow('Editing',self.mode);form.addRow('Parent / bound joint',self.parent_box)
        for channel in CHANNELS:
            spin=QDoubleSpinBox();spin.setRange(-100000,100000);spin.setDecimals(3);spin.setSingleStep(1 if channel in ('x','y','rotation') else .05)
            spin.editingFinished.connect(lambda c=channel:self.edit_value(c))
            self.fields[channel]=spin;form.addRow(channel,spin)
        self.pivot_x=QDoubleSpinBox();self.pivot_y=QDoubleSpinBox()
        for spin in (self.pivot_x,self.pivot_y):
            spin.setRange(-100000,100000);spin.editingFinished.connect(self.edit_pivot)
        form.addRow('Layer pivot X',self.pivot_x);form.addRow('Layer pivot Y',self.pivot_y)
        self.z=QDoubleSpinBox();self.z.setRange(-99999,99999);self.z.editingFinished.connect(self.edit_z);form.addRow('Layer order',self.z)
        self.channel=QComboBox();self.channel.addItems(CHANNELS)
        self.ease=QComboBox();self.ease.addItems(['linear','smooth','hold'])
        form.addRow('Key channel',self.channel);form.addRow('Interpolation after key',self.ease)
        self.ease.currentTextChanged.connect(self.change_ease)
        for label,callback in [('Add / replace channel key',self.add_key),('Key all transform channels',self.key_all),('Delete channel key at cursor',self.delete_key)]:
            b=QPushButton(label);b.clicked.connect(callback);form.addRow(b)
        right=QWidget();right.setLayout(form)
        self.tabs=QTabWidget();self.tabs.addTab(self.canvas,'Sprite canvas')
        docs=QWidget();docs_layout=QVBoxLayout(docs)
        self.documents=QComboBox();self.documents.currentIndexChanged.connect(self.read_document)
        self.document_text=QPlainTextEdit();self.document_text.setPlaceholderText('Open a project to edit scene prompts, history, poses, fonts, buttons and music here.')
        docs_layout.addWidget(QLabel('Project / scene files — paths are relative to the file containing them.'))
        docs_layout.addWidget(self.documents);docs_layout.addWidget(self.document_text)
        b=QPushButton('Validate and save document');b.clicked.connect(self.save_document);docs_layout.addWidget(b)
        self.tabs.addTab(docs,'Project & scenes')
        split=QSplitter();split.addWidget(left);split.addWidget(self.tabs);split.addWidget(right);split.setSizes([220,850,280])
        center=QWidget();layout=QVBoxLayout(center);layout.addWidget(split,1)
        controls=QHBoxLayout()
        self.time=QDoubleSpinBox();self.time.setDecimals(3);self.time.setRange(0,3600);self.time.setSuffix(' s');self.time.valueChanged.connect(self.refresh)
        self.duration=QDoubleSpinBox();self.duration.setDecimals(3);self.duration.setRange(.001,86400);self.duration.setValue(3);self.duration.setSuffix(' s');self.duration.editingFinished.connect(self.change_duration)
        self.loop=QCheckBox('Loop');self.loop.setChecked(True);self.loop.toggled.connect(self.change_loop)
        self.play_button=QPushButton('Play');self.play_button.clicked.connect(self.toggle_play)
        controls.addWidget(self.play_button);controls.addWidget(QLabel('Cursor'));controls.addWidget(self.time);controls.addWidget(QLabel('Clip duration'));controls.addWidget(self.duration);controls.addWidget(self.loop);controls.addStretch()
        layout.addLayout(controls)
        self.timeline=Timeline(self);scroll=QScrollArea();scroll.setWidgetResizable(True);scroll.setWidget(self.timeline);scroll.setMinimumHeight(180);scroll.setMaximumHeight(300);layout.addWidget(scroll)
        self.setCentralWidget(center)
        toolbar=self.addToolBar('Files')
        commands=[('New rig',self.new_rig,None),('Open rig',self.open_rig,None),('Save rig',self.save_rig,'Ctrl+S'),('New clip',self.new_clip,None),('Open clip',self.open_clip,None),('Save clip',self.save_clip,'Ctrl+Shift+S'),('Open project',self.open_project,None),('New project',self.new_project,None),('Undo',self.undo,'Ctrl+Z'),('Redo',self.redo,'Ctrl+Y')]
        for title,callback,shortcut in commands:
            a=QAction(title,self);a.triggered.connect(callback)
            if shortcut:a.setShortcut(QKeySequence(shortcut))
            toolbar.addAction(a)
        self.timer=QTimer(self);self.timer.setTimerType(Qt.PreciseTimer);self.timer.setInterval(16);self.timer.timeout.connect(self.tick)
        self.statusBar().showMessage('Import separate transparent layers. Select a parent joint, then click Add joint and the canvas. Wheel zooms; drag empty space to pan.')
        self.rebuild()
        if path:
            if str(path).endswith('.panel'):self.load_project(path)
            else:self.load_rig(path)
        QTimer.singleShot(0,self.fit)

    def error(self,exc):
        QMessageBox.warning(self,'Cannot complete operation',str(exc))

    def checkpoint(self):
        self.undo_stack.append((deepcopy(self.rig),deepcopy(self.animation),self.selected))
        self.redo_stack.clear();self.dirty=True
    def undo(self):
        if not self.undo_stack:return
        self.dirty=True
        self.redo_stack.append((deepcopy(self.rig),deepcopy(self.animation),self.selected))
        self.rig,self.animation,self.selected=self.undo_stack.pop();self.rebuild()
    def redo(self):
        if not self.redo_stack:return
        self.dirty=True
        self.undo_stack.append((deepcopy(self.rig),deepcopy(self.animation),self.selected))
        self.rig,self.animation,self.selected=self.redo_stack.pop();self.rebuild()

    def obj(self):
        if not self.selected:return None
        kind,id=self.selected.split(':',1)
        return next((o for o in self.rig['joints' if kind=='joint' else 'layers'] if o['id']==id),None)
    def tree_selection(self,item,previous):
        if item:self.selected=item.data(0,Qt.UserRole);self.refresh()
    def choose(self,target):
        self.selected=target
        for i in range(self.tree.topLevelItemCount()):
            item=self.tree.topLevelItem(i)
            if item.data(0,Qt.UserRole)==target:self.tree.setCurrentItem(item);break
        self.refresh()

    def rebuild(self):
        self.updating=True;self.tree.clear();self.scene.clear();self.joint_items={};self.layer_items={};self.bone_lines=[]
        for kind,group in [('joint','joints'),('layer','layers')]:
            for obj in self.rig[group]:
                item=QTreeWidgetItem([kind+' · '+obj['id']]);item.setData(0,Qt.UserRole,kind+':'+obj['id']);self.tree.addTopLevelItem(item)
                if kind=='joint':
                    node=JointItem(self,obj['id']);self.joint_items[obj['id']]=node
                else:
                    pix=QPixmap(str(asset(self.rig_path,obj['image'])))
                    node=LayerItem(self,obj['id'],pix);self.layer_items[obj['id']]=node
                self.scene.addItem(node)
        self.updating=False
        if self.selected:self.choose(self.selected)
        self.refresh()

    def refresh(self,*args):
        if self.updating:return
        self.updating=True
        if self.animation:
            self.duration.setValue(self.animation['duration'])
            self.loop.blockSignals(True);self.loop.setChecked(self.animation.get('loop',True));self.loop.blockSignals(False)
        anim=self.animation if self.mode.currentIndex()==1 else None
        layers,world=evaluate(self.rig,anim,self.time.value(),loop=False)
        for layer in layers:
            node=self.layer_items[layer['id']];node.setTransform(transform(layer['matrix']));node.setOpacity(max(0,min(1,layer.get('opacity',1))));node.setZValue(layer.get('z',0))
        for id,node in self.joint_items.items():
            m=world[id];node.setPos(m[4],m[5])
        for line in self.bone_lines:self.scene.removeItem(line)
        self.bone_lines=[]
        for joint in self.rig['joints']:
            if joint.get('parent'):
                a,b=world[joint['id']],world[joint['parent']]
                line=self.scene.addLine(a[4],a[5],b[4],b[5],QPen(QColor('#ffce69'),2));line.setZValue(99999);self.bone_lines.append(line)
        obj=self.obj()
        self.parent_box.clear();self.parent_box.addItem('(world)',None)
        for joint in self.rig['joints']:
            if self.selected!='joint:'+joint['id']:self.parent_box.addItem(joint['id'],joint['id'])
        if obj:
            from .core import sample
            values=dict(obj)
            if anim:
                for tr in anim['tracks']:
                    if tr['target']==self.selected:values[tr['channel']]=sample(tr['keys'],self.time.value(),CHANNELS[tr['channel']])
            for key,spin in self.fields.items():spin.setValue(values.get(key,CHANNELS[key]))
            parent=obj.get('parent' if self.selected.startswith('joint:') else 'joint')
            self.parent_box.setCurrentIndex(max(0,self.parent_box.findData(parent)))
            px,py=obj.get('pivot',[0,0]);self.pivot_x.setValue(px);self.pivot_y.setValue(py);self.z.setValue(obj.get('z',0))
        self.updating=False;self.timeline.update()

    def fit(self):
        bounds=self.scene.itemsBoundingRect()
        if not bounds.isEmpty():self.canvas.fitInView(bounds.adjusted(-40,-40,40,40),Qt.KeepAspectRatio)
    def start_joint(self):
        self.add_mode=True;self.statusBar().showMessage('Click the joint position. The selected joint becomes its parent.')
    def unique_id(self,prefix,group):
        used={o['id'] for o in self.rig[group]};i=1
        while f'{prefix}{i}' in used:i+=1
        return f'{prefix}{i}'
    def add_joint(self,point):
        self.add_mode=False;self.checkpoint()
        parent=self.selected.split(':',1)[1] if self.selected and self.selected.startswith('joint:') else None
        if parent:
            _,world=evaluate(self.rig);inverse,ok=transform(world[parent]).inverted()
            if not ok:self.error('Cannot add below a zero-scale joint');return
            point=inverse.map(point)
        id=self.unique_id('joint','joints')
        self.rig['joints'].append({'id':id,'parent':parent,'x':point.x(),'y':point.y()})
        self.selected='joint:'+id;self.rebuild()
    def move_joint(self,id,point):
        obj=next(j for j in self.rig['joints'] if j['id']==id)
        parent=obj.get('parent')
        if parent:
            _,world=evaluate(self.rig,self.animation if self.mode.currentIndex()==1 else None,self.time.value(),False)
            inverse,ok=transform(world[parent]).inverted()
            if not ok:self.error('Parent has zero scale');self.refresh();return
            point=inverse.map(point)
        self.selected='joint:'+id
        for channel,value in [('x',point.x()),('y',point.y())]:self.set_value(channel,value)
        self.refresh()
    def move_layer(self,id,delta):
        self.selected='layer:'+id
        obj=self.obj()
        if obj.get('joint'):
            _,world=evaluate(self.rig,self.animation if self.mode.currentIndex()==1 else None,self.time.value(),False)
            inverse,ok=transform(world[obj['joint']]).inverted()
            if not ok:self.error('Bound joint has zero scale');self.refresh();return
            delta=inverse.map(delta)-inverse.map(QPointF(0,0))
        self.set_value('x',self.fields['x'].value()+delta.x())
        self.set_value('y',self.fields['y'].value()+delta.y())
        self.refresh()
    def import_layer(self):
        path=QFileDialog.getOpenFileName(self,'Import image layer','','Images (*.png *.webp *.jpg *.bmp)')[0]
        if not path:return
        pix=QPixmap(path)
        if pix.isNull():self.error('Cannot read image');return
        self.checkpoint();id=self.unique_id('layer','layers')
        self.rig['layers'].append({'id':id,'image':os.path.relpath(path,self.rig_path.parent).replace('\\','/'),'joint':None,'x':0,'y':0,'pivot':[0,0],'z':len(self.rig['layers'])})
        self.selected='layer:'+id;self.rebuild();self.fit()
    def delete_part(self):
        obj=self.obj()
        if not obj:return
        if self.selected.startswith('joint:') and (any(j.get('parent')==obj['id'] for j in self.rig['joints']) or any(l.get('joint')==obj['id'] for l in self.rig['layers'])):
            self.error('Reparent children and unbind layers before deleting this joint.');return
        self.checkpoint();group='joints' if self.selected.startswith('joint:') else 'layers';self.rig[group].remove(obj)
        if self.animation:self.animation['tracks']=[tr for tr in self.animation['tracks'] if tr['target']!=self.selected]
        self.selected=None;self.rebuild()
    def rename_part(self):
        obj=self.obj()
        if not obj:return
        name,ok=QInputDialog.getText(self,'Rename part','New ID (other clips must use the same ID):',text=obj['id'])
        if not ok or not name.strip() or name==obj['id']:return
        name=name.strip();kind,old=self.selected.split(':',1)
        group='joints' if kind=='joint' else 'layers'
        if any(o['id']==name for o in self.rig[group]):self.error('That ID already exists.');return
        self.checkpoint();obj['id']=name
        if kind=='joint':
            for j in self.rig['joints']:
                if j.get('parent')==old:j['parent']=name
            for layer in self.rig['layers']:
                if layer.get('joint')==old:layer['joint']=name
        if self.animation:
            for track in self.animation['tracks']:
                if track['target']==self.selected:track['target']=kind+':'+name
        self.selected=kind+':'+name;self.rebuild()
    def change_parent(self,*args):
        if self.updating or not self.obj():return
        if self.mode.currentIndex()!=0:self.error('Change parent in rest / skeleton setup mode.');self.refresh();return
        obj=self.obj();field='parent' if self.selected.startswith('joint:') else 'joint';old=obj.get(field)
        self.checkpoint();obj[field]=self.parent_box.currentData()
        try:validate_rig(self.rig)
        except ValueError as exc:obj[field]=old;self.undo_stack.pop();self.error(exc)
        self.refresh()
    def set_value(self,channel,value):
        obj=self.obj()
        if not obj:return
        if self.mode.currentIndex()==0:obj[channel]=value
        elif self.animation:
            self.put_key(channel,value)
        else:self.error('Create or open an animation clip first.')
    def edit_value(self,channel):
        if self.updating or not self.obj():return
        self.checkpoint();self.set_value(channel,self.fields[channel].value());self.refresh()
    def edit_pivot(self):
        if self.updating or not self.obj():return
        if self.selected.startswith('layer:'):
            self.checkpoint();self.obj()['pivot']=[self.pivot_x.value(),self.pivot_y.value()];self.refresh()
    def edit_z(self):
        if self.updating or not self.obj():return
        self.checkpoint();self.obj()['z']=self.z.value();self.refresh()
    def put_key(self,channel,value):
        t=self.time.value()
        if t>self.animation['duration']:
            self.error('Cursor is beyond clip duration. Increase duration first.');return
        tracks=self.animation['tracks'];tr=next((tr for tr in tracks if tr['target']==self.selected and tr['channel']==channel),None)
        if tr is None:tr={'target':self.selected,'channel':channel,'keys':[]};tracks.append(tr)
        key=next((k for k in tr['keys'] if abs(k['time']-t)<.0001),None)
        if key is None:key={'time':t};tr['keys'].append(key)
        key.update(value=value,ease=self.ease.currentText());tr['keys'].sort(key=lambda k:k['time'])
    def add_key(self):
        if not self.animation or not self.obj():self.error('Select a part and create/open a clip.');return
        self.checkpoint();c=self.channel.currentText();self.put_key(c,self.fields[c].value());self.mode.setCurrentIndex(1);self.refresh()
    def key_all(self):
        if not self.animation or not self.obj():return
        self.checkpoint()
        for c,spin in self.fields.items():self.put_key(c,spin.value())
        self.mode.setCurrentIndex(1);self.refresh()
    def delete_key(self):
        if not self.animation:return
        self.checkpoint()
        for tr in self.animation['tracks']:
            if tr['target']==self.selected and tr['channel']==self.channel.currentText():tr['keys']=[k for k in tr['keys'] if abs(k['time']-self.time.value())>.0001]
        self.animation['tracks']=[tr for tr in self.animation['tracks'] if tr['keys']];self.refresh()
    def change_ease(self,*args):
        if self.updating or not self.animation:return
        for tr in self.animation['tracks']:
            if tr['target']==self.selected and tr['channel']==self.channel.currentText():
                for key in tr['keys']:
                    if abs(key['time']-self.time.value())<.0001:self.checkpoint();key['ease']=self.ease.currentText()
        self.refresh()
    def change_duration(self):
        if not self.animation:return
        maximum=max((k['time'] for tr in self.animation['tracks'] for k in tr['keys']),default=0)
        if self.duration.value()<maximum:self.error('Move or delete keys beyond the new duration first.');self.duration.setValue(self.animation['duration']);return
        self.checkpoint();self.animation['duration']=self.duration.value();self.timeline.update()
    def change_loop(self,value):
        if self.animation:self.checkpoint();self.animation['loop']=value
    def toggle_play(self):
        if not self.animation:return
        self.playing=not self.playing;self.play_button.setText('Pause' if self.playing else 'Play')
        if self.playing:self.mode.setCurrentIndex(1);self.started=time.perf_counter()-self.time.value();self.timer.start()
        else:self.timer.stop()
    def tick(self):
        t=time.perf_counter()-self.started;d=self.animation['duration']
        if not self.animation.get('loop',True) and t>=d:self.time.setValue(d);self.toggle_play();return
        self.time.setValue(t%d)

    def confirm_discard(self):
        changed=self.dirty and (self.rig!=self.saved_rig or self.animation!=self.saved_animation)
        return not changed or QMessageBox.question(self,'Unsaved changes','Discard unsaved rig/clip changes?',QMessageBox.Yes|QMessageBox.No)==QMessageBox.Yes
    def new_rig(self):
        if not self.confirm_discard():return
        self.rig={'format':'panel.rig','version':1,'joints':[],'layers':[]};self.rig_path=Path.cwd()/'untitled.rig';self.animation=None;self.selected=None;self.dirty=False;self.undo_stack.clear();self.rebuild()
        self.saved_rig=deepcopy(self.rig);self.saved_animation=None
        self.timer.stop();self.playing=False;self.play_button.setText('Play')
    def open_rig(self):
        if not self.confirm_discard():return
        path=QFileDialog.getOpenFileName(self,'Open rig','','Panel rig (*.rig)')[0]
        if path:self.load_rig(path)
    def load_rig(self,path):
        try:
            rig=load(path,'rig');validate_rig(rig)
            self.rig=rig;self.rig_path=Path(path).resolve();self.animation=None;self.animation_path=None;self.selected=None;self.dirty=False;self.undo_stack.clear();self.redo_stack.clear();self.rebuild();self.fit()
            self.saved_rig=deepcopy(self.rig);self.saved_animation=None
            self.timer.stop();self.playing=False;self.play_button.setText('Play')
        except (ValueError,KeyError,OSError) as exc:self.error(exc)
    def save_rig(self):
        path=QFileDialog.getSaveFileName(self,'Save rig',str(self.rig_path),'Panel rig (*.rig)')[0]
        if not path:return
        try:
            validate_rig(self.rig);copy=deepcopy(self.rig)
            for layer in copy['layers']:layer['image']=os.path.relpath(asset(self.rig_path,layer['image']),Path(path).resolve().parent).replace('\\','/')
            save(path,copy);self.rig=copy;self.rig_path=Path(path).resolve();self.dirty=bool(self.animation);self.statusBar().showMessage('Rig saved. Save the clip separately if edited.')
            self.saved_rig=deepcopy(self.rig)
        except (ValueError,OSError) as exc:self.error(exc)
    def new_clip(self):
        if self.animation and not self.confirm_discard():return
        self.checkpoint();self.animation={'format':'panel.animation','version':1,'duration':self.duration.value(),'loop':self.loop.isChecked(),'tracks':[]};self.animation_path=None;self.mode.setCurrentIndex(1);self.time.setValue(0);self.refresh()
    def open_clip(self):
        if self.animation!=self.saved_animation and not self.confirm_discard():return
        path=QFileDialog.getOpenFileName(self,'Open animation','','Panel animation (*.anim)')[0]
        if path:self.load_clip(path)
    def load_clip(self,path):
        try:
            clip=load(path,'animation');validate_animation(clip,self.rig);self.animation=clip;self.animation_path=Path(path).resolve();self.duration.setValue(clip['duration']);self.loop.blockSignals(True);self.loop.setChecked(clip.get('loop',True));self.loop.blockSignals(False);self.mode.setCurrentIndex(1);self.time.setValue(0);self.refresh()
            self.saved_animation=deepcopy(self.animation)
        except (ValueError,KeyError,OSError) as exc:self.error(exc)
    def save_clip(self):
        if not self.animation:return
        path=QFileDialog.getSaveFileName(self,'Save animation',str(self.animation_path or self.rig_path.with_suffix('.anim')),'Panel animation (*.anim)')[0]
        if path:
            try:
                validate_animation(self.animation,self.rig);save(path,self.animation);self.animation_path=Path(path)
                self.saved_animation=deepcopy(self.animation)
                self.dirty=self.rig!=self.saved_rig
                self.statusBar().showMessage('Clip saved. Save the rig separately if edited.')
            except (ValueError,OSError) as exc:self.error(exc)
    def open_project(self):
        if not self.confirm_discard():return
        path=QFileDialog.getOpenFileName(self,'Open project','','Panel project (*.panel)')[0]
        if path:self.load_project(path)
    def load_project(self,path):
        try:
            project=Project(path);self.project_path=Path(path).resolve();self.document_paths=[self.project_path]+[p for p,s in project.scenes.values()]
            self.documents.clear();self.documents.addItems([str(p) for p in self.document_paths]);self.read_document()
            options=list(project.poses)
            if options:
                names=[' / '.join(k) for k in options]
                name,ok=QInputDialog.getItem(self,'Choose pose','Scene / actor / pose',names,0,False) if len(names)>1 else (names[0],True)
                if ok:
                    key=options[names.index(name)];rp,rig,animations=project.poses[key];self.load_rig(rp)
                    sp,scene=project.scenes[key[0]];actor=next(a for a in scene['actors'] if a['id']==key[1]);pose=actor['poses'][key[2]]
                    if pose.get('animation'):self.load_clip(asset(sp,pose['animations'][pose['animation']]))
            self.statusBar().showMessage('Project loaded. Open another clip to edit any animation for this pose.')
        except (ValueError,KeyError,OSError) as exc:self.error(exc)
    def read_document(self,*args):
        i=self.documents.currentIndex()
        if 0<=i<len(self.document_paths):self.document_text.setPlainText(self.document_paths[i].read_text(encoding='utf-8-sig'))
    def save_document(self):
        i=self.documents.currentIndex()
        if i<0:return
        path=self.document_paths[i]
        try:
            doc=json.loads(self.document_text.toPlainText());old=path.read_bytes();save(path,doc)
            try:Project(self.project_path)
            except Exception:path.write_bytes(old);raise
            self.statusBar().showMessage('Document saved and project validated.')
        except (ValueError,KeyError,OSError) as exc:self.error(exc)
    def new_project(self):
        directory=QFileDialog.getExistingDirectory(self,'Choose an empty project folder')
        if not directory:return
        root=Path(directory)
        if any(root.iterdir()):self.error('Choose an empty folder to avoid overwriting assets.');return
        save(root/'intro.scene',{'format':'panel.scene','version':1,'text':'','actors':[],'buttons':[]})
        save(root/'project.panel',{'format':'panel.project','version':1,'title':root.name,'canvas':[1280,720],'intro':'intro','scenes':{'intro':'intro.scene'},'theme':{},'inference':{}})
        self.load_project(root/'project.panel');self.tabs.setCurrentIndex(1)
    def closeEvent(self,event):
        if self.confirm_discard():event.accept()
        else:event.ignore()
