import http.server
from pathlib import Path
import subprocess
import threading
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  assert self.path.endswith('?action=servers&format=text')
  body=b'LC-DIRECTORY-1\nmain\tTest\t127.0.0.1\t31457\t2\t8\n'
  self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
server=http.server.HTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:subprocess.run([str(Path(__file__).with_name('test_discovery').resolve()),f'http://127.0.0.1:{server.server_port}/api/'],check=True)
finally:server.shutdown();server.server_close();thread.join()
print('PASS: native asynchronous HTTP discovery')
