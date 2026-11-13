#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
with tempfile.TemporaryDirectory(prefix='lineclash-profiles-') as d:
    root=Path(d);(root/'LineClash').mkdir()
    path=root/'LineClash/accounts' if os.name=='nt' else root/'.lineclash_accounts'
    binary=Path(__file__).with_name('test_account_profiles'+('.exe' if os.name=='nt' else ''))
    subprocess.run([str(binary),str(path)],check=True,env=dict(os.environ,HOME=d,APPDATA=d))
