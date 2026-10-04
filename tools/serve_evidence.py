#!/usr/bin/env python3
"""Serve local evidence with byte ranges so browsers can seek lossless WAV."""
import argparse
import functools
import http.server
import pathlib
import re
import shutil
import threading

class EvidenceHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*args):pass
    def send_head(self):
        path=pathlib.Path(self.translate_path(self.path))
        header=self.headers.get('Range')
        if not header or not path.is_file():return super().send_head()
        match=re.fullmatch(r'bytes=(\d*)-(\d*)',header)
        size=path.stat().st_size
        if not match or not size or not any(match.groups()):
            self.send_error(416);return None
        first,last=match.groups()
        start=int(first) if first else max(0,size-int(last))
        end=min(size-1,int(last)) if first and last else size-1
        if start>end or start>=size:
            self.send_error(416);return None
        source=path.open('rb');source.seek(start);self.remaining=end-start+1
        self.send_response(206);self.send_header('Content-Type',self.guess_type(str(path)))
        self.send_header('Accept-Ranges','bytes');self.send_header('Content-Range',f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length',str(self.remaining));self.end_headers();return source
    def end_headers(self):
        self.send_header('Accept-Ranges','bytes');super().end_headers()
    def copyfile(self,source,outputfile):
        try:
            if hasattr(self,'remaining'):
                remaining=self.remaining;del self.remaining
                while remaining:
                    block=source.read(min(65536,remaining))
                    if not block:break
                    outputfile.write(block);remaining-=len(block)
            else:shutil.copyfileobj(source,outputfile)
        except (BrokenPipeError,ConnectionResetError):pass

def server(directory,port=8080):
    return http.server.ThreadingHTTPServer(('127.0.0.1',port),functools.partial(EvidenceHandler,directory=str(directory)))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--directory',type=pathlib.Path,required=True);p.add_argument('--port',type=int,default=8080);a=p.parse_args()
    httpd=server(a.directory.resolve(),a.port);print(f'http://127.0.0.1:{httpd.server_port}/index.html',flush=True)
    try:httpd.serve_forever()
    except KeyboardInterrupt:pass
    finally:httpd.server_close()
