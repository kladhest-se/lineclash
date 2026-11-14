#!/usr/bin/env python3
"""PIN URLs must not leak an Apache/reverse-proxy backend port."""
from pathlib import Path
import os
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,request,fp,code,message,headers,newurl):return None
opener=urllib.request.build_opener(NoRedirect)
def port():
    with socket.socket() as listener:listener.bind(('127.0.0.1',0));return listener.getsockname()[1]
def request(address,path,data=None):
    body=urllib.parse.urlencode(data).encode() if data is not None else None
    req=urllib.request.Request(address+path,body,{'Host':'public.example:6669','X-Forwarded-Proto':'https'})
    try:response=opener.open(req,timeout=3)
    except urllib.error.HTTPError as error:response=error
    with response:return response.code,response.headers,response.read().decode()
def ready(address,process,log):
    for _ in range(100):
        if process.poll() is not None:
            log.seek(0);raise AssertionError(log.read().decode())
        try:request(address,'/');return
        except OSError:time.sleep(.03)
    raise AssertionError('Test server did not start')

with tempfile.TemporaryDirectory(prefix='lineclash-pin-routes-') as folder,tempfile.TemporaryFile() as log:
    base=Path(folder).resolve();address=f'http://127.0.0.1:{port()}'
    env=dict(os.environ,LINECLASH_WEB_CONFIG=str(base/'missing-config.php'))
    app=subprocess.Popen(['php','-S',address.removeprefix('http://'),'-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=env,stdout=log,stderr=log)
    try:
        ready(address,app,log)
        for path in ['/pin','/pin/','/pin/index.php','/account/pin','/account/pin/index.php']:
            code,headers,body=request(address,path+'?code=12345678')
            assert code==303 and headers['Location']=='/account/pin/?code=12345678',(path,code,dict(headers))
            assert 'no-store' in headers['Cache-Control']
        code,headers,body=request(address,'/pin/?code=https://evil.invalid&next=https://evil.invalid')
        assert code==303 and headers['Location']=='/account/pin/'
        code,headers,body=request(address,'/account/?code=a1b2c3d4')
        assert code==303 and headers['Location']=='/account/pin/?code=A1B2C3D4'
        code,headers,body=request(address,'/account/pin/')
        assert code==200 and 'Enter the PIN shown' in body and 'action="/account/pin/"' in body
        assert 'href="../../assets/site.css"' in body and 'src="../../assets/blocks.js"' in body
        code,headers,body=request(address,'/pin/',dict(action='lookup',pin='12345678',password='fixture-not-a-real-password'))
        assert code==200 and not headers.get('Location') and 'This form expired' in body
        assert 'action="/account/pin/"' in body
    finally:app.terminate();app.wait(timeout=5)

    # Exercise the actual .htaccess with mod_dir, using harmless static .php
    # fixture files. PHP redirects and forms are covered above and by PIN tests.
    httpd=Path('/usr/sbin/httpd');modules=Path('/usr/libexec/apache2')
    if httpd.is_file() and (modules/'mod_rewrite.so').is_file():
        web=base/'apache-web';(web/'pin').mkdir(parents=True);(web/'account/pin').mkdir(parents=True)
        (web/'pin/index.php').write_text('legacy-pin-fixture')
        (web/'account/pin/index.php').write_text('canonical-pin-fixture')
        htaccess=(ROOT/'public-web/.htaccess').read_text()
        (web/'.htaccess').write_text('\n'.join(line for line in htaccess.splitlines() if not line.startswith(('RewriteRule ^pin$','RewriteRule ^account/pin$'))))
        loaded=subprocess.check_output([str(httpd),'-l'],text=True)
        names=['authz_core','dir','rewrite']
        if 'mod_unixd.c' not in loaded and (modules/'mod_unixd.so').is_file():names.append('unixd')
        loads=f'LoadModule mpm_prefork_module "{modules}/mod_mpm_prefork.so"\n'+'\n'.join(f'LoadModule {name}_module "{modules}/mod_{name}.so"' for name in names)
        listen=port();address=f'http://127.0.0.1:{listen}';config=base/'httpd.conf'
        config.write_text(f'''ServerRoot "{base}"
PidFile "{base}/httpd.pid"
DefaultRuntimeDir "{base}"
Listen 127.0.0.1:{listen}
ServerName public.example:6669
UseCanonicalName On
{loads}
DocumentRoot "{web}"
ErrorLog "{base}/httpd-error.log"
LogLevel warn
DirectoryIndex index.php
<Directory "{web}">
Require all granted
AllowOverride All
Options FollowSymLinks
</Directory>
''')
        app=subprocess.Popen([str(httpd),'-f',str(config),'-DFOREGROUND'],stdout=log,stderr=log,start_new_session=True)
        try:
            ready(address,app,log)
            code,headers,_=request(address,'/pin')
            assert code==301 and headers['Location']=='http://public.example:6669/pin/'
            (web/'.htaccess').write_text(htaccess)
            for route in ['pin','account/pin']:(web/route/'.htaccess').write_text((ROOT/'public-web'/route/'.htaccess').read_text())
            for path,token in [('/pin','legacy-pin-fixture'),('/account/pin','canonical-pin-fixture')]:
                code,headers,body=request(address,path)
                assert code==200 and not headers.get('Location') and body==token,(path,code,dict(headers),body,(base/"httpd-error.log").read_text())
            print('PASS: actual Apache mod_dir backend-port leak reproduced; PIN rewrites avoid it')
        finally:app.terminate();app.wait(timeout=5)
    else:print('SKIP: local Apache/mod_rewrite unavailable; PHP route checks passed')
print('PASS: canonical Account PIN page, safe legacy redirects, prefilled codes, no credential redirects and correct asset/form paths')
