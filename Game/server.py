"""Loopback-only native game bridge; launches the existing-engine game frontend."""
import argparse
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import subprocess
import threading

from runtime import Game, GameError, ROOT

def wire(game, error=''):
    v=game.view()
    transcript='\r\n\r\n'.join(f"{t['speaker']}\r\n{t['text']}" for t in v['transcript'])
    fields={**v,'cast':','.join(v['cast']),'available':','.join(v['available']),
            'transcript':transcript,'error':error}
    return '\n'.join(k+'='+str(value).encode('utf-8').hex() for k,value in fields.items()).encode('ascii')

def handler_for(game, token):
    lock=threading.Lock()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):
            pass  # Never log player messages or bridge credentials.

        def handle_request(self):
            if not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+token):
                self.send_error(401);return
            if self.headers.get('Origin'):
                self.send_error(403);return
            if not lock.acquire(blocking=False):
                self.send_error(409,'A conversation is already running');return
            error=''
            streaming=False
            disconnected=False
            def send_event(kind,who,value):
                nonlocal disconnected
                fields={'stream':kind,'speaker':who}
                if kind=='start':
                    fields.update({'pose_'+who:value['pose'],'gesture_'+who:value['gesture']})
                else:
                    fields['reply']=value
                data='\n'.join(k+'='+str(v).encode('utf-8').hex() for k,v in fields.items()).encode('ascii')+b'\n\n'
                if not disconnected:
                    try:self.wfile.write(data);self.wfile.flush()
                    except (BrokenPipeError,ConnectionResetError,OSError):disconnected=True
            try:
                if self.command=='POST':
                    length=int(self.headers.get('Content-Length','0'))
                    if not 0<=length<=32000:
                        self.send_error(413);return
                    body=self.rfile.read(length).decode('ascii')
                    args={}
                    for line in body.splitlines():
                        key,sep,value=line.partition('=')
                        if not sep: raise GameError('Invalid request')
                        args[key]=bytes.fromhex(value).decode('utf-8')
                    if self.path in ('/chat-stream','/retry-stream'):
                        streaming=True
                        self.send_response(200)
                        self.send_header('Content-Type','text/plain; charset=ascii')
                        self.send_header('Cache-Control','no-store')
                        self.send_header('Connection','close')
                        self.end_headers()
                        if self.path=='/chat-stream':game.chat(args.get('who',''),args.get('text',''),send_event)
                        else:game.retry(send_event)
                    elif self.path=='/chat': game.chat(args.get('who',''),args.get('text',''))
                    elif self.path=='/retry': game.retry()
                    elif self.path=='/advance': game.advance()
                    elif self.path=='/leave': game.leave_scene()
                    elif self.path=='/keep-talking': game.keep_talking()
                    elif self.path=='/visit': game.visit(args.get('who',''))
                    elif self.path=='/new': game.new(game.valid_name(args.get('name','')))
                    elif self.path=='/name': game.set_name(args.get('name',''))
                    else: self.send_error(404);return
                elif self.path!='/state':
                    self.send_error(404);return
            except (GameError,ValueError,KeyError) as exc:
                error=str(exc)
            except Exception:
                # Preserve the pending request and show a safe retryable error, without private tracebacks.
                error='Could not complete this action. Your last saved progress is retained. Retry the response or restart the game.'
            finally:
                result=wire(game,error)
                lock.release()
            if streaming:
                if not disconnected:
                    try:
                        self.wfile.write(b'stream=646f6e65\n'+result+b'\n\n');self.wfile.flush()
                    except (BrokenPipeError,ConnectionResetError,OSError):pass
                self.close_connection=True
                return
            self.send_response(200)
            self.send_header('Content-Type','text/plain; charset=ascii')
            self.send_header('Content-Length',str(len(result)))
            self.send_header('Cache-Control','no-store')
            self.end_headers()
            try: self.wfile.write(result)
            except (BrokenPipeError,ConnectionResetError): pass
        do_GET=handle_request
        do_POST=handle_request
    return Handler

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--save',type=Path)
    parser.add_argument('--serve-only',action='store_true',help='Print bridge details for integration tests; no game window.')
    args=parser.parse_args()
    game=Game(args.save)
    token=secrets.token_urlsafe(32)
    server=ThreadingHTTPServer(('127.0.0.1',0),handler_for(game,token))
    server.daemon_threads=True
    port=server.server_address[1]
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    try:
        if args.serve_only:
            print(json.dumps(dict(port=port,token=token)),flush=True)
            thread.join()
        else:
            exe=ROOT.parent/'ComicEngine'/'bin'/'StoryGame.exe'
            if not exe.exists():
                raise GameError('Build ComicEngine first: StoryGame.exe is missing.')
            # One native game window; the Python bridge has no additional UI.
            subprocess.run([str(exe),str(port),token],cwd=ROOT.parent,check=True)
    finally:
        server.shutdown()
        server.server_close()

if __name__=='__main__':
    main()
