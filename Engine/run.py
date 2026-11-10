"""Launch the player or editor; no project-specific content is imported."""
import argparse
import json
import os
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('project', nargs='?')
    parser.add_argument('--editor', action='store_true')
    parser.add_argument('--validate', action='store_true')
    parser.add_argument('--smoke-seconds', type=float, default=0)
    parser.add_argument('--capture')
    parser.add_argument('--scene', help='Start at a named scene instead of the project intro')
    args = parser.parse_args()
    if args.validate:
        from comic.core import Project
        p = Project(args.project)
        print(json.dumps(p.capabilities(),indent=2)); return
    # Mandatory CPU raster backend, including when the parent environment requests a GPU.
    os.environ['QT_QUICK_BACKEND'] = 'software'
    os.environ.pop('QSG_RHI_BACKEND', None)
    os.environ['QT_QUICK_CONTROLS_STYLE'] = 'Basic'
    from PySide6.QtWidgets import QApplication, QFileDialog, QMessageBox
    from PySide6.QtCore import QTimer, QUrl
    app = QApplication(sys.argv)
    if args.editor:
        from comic.editor import Editor
        window = Editor(args.project)
        window.show()
    else:
        from PySide6.QtQuick import QQuickWindow, QSGRendererInterface
        from PySide6.QtQml import QQmlApplicationEngine
        from comic.runtime import Runtime
        QQuickWindow.setGraphicsApi(QSGRendererInterface.Software)
        QQuickWindow.setSceneGraphBackend('software')
        path = args.project or QFileDialog.getOpenFileName(None,'Open project','','Panel project (*.panel)')[0]
        if not path:
            return
        try:
            runtime = Runtime(path)
            if args.scene:
                runtime.scene(args.scene)
        except (ValueError,KeyError,OSError) as exc:
            QMessageBox.critical(None,'Invalid project',str(exc)); return 1
        qml = QQmlApplicationEngine()
        qml.rootContext().setContextProperty('engine',runtime)
        qml.load(QUrl.fromLocalFile(str(Path(__file__).parent/'comic'/'Player.qml')))
        if not qml.rootObjects():
            return 1
        window = qml.rootObjects()[0]
        # QML FrameAnimation drives updates in sync with presentation, not a second timer.
    if args.smoke_seconds:
        def finish():
            if args.capture:
                if args.editor:
                    window.grab().save(args.capture)
                else:
                    window.grabWindow().save(args.capture)
            if not args.editor:
                intervals = sorted(runtime.frame_intervals[10:])
                print(json.dumps({'renderer':str(window.rendererInterface().graphicsApi()),
                                  'frames':runtime.frame_count,
                                  'p95_ms':intervals[int(len(intervals)*.95)]*1000 if intervals else None}))
            app.quit()
        QTimer.singleShot(int(args.smoke_seconds*1000),finish)
    return app.exec()


if __name__ == '__main__':
    raise SystemExit(main())
