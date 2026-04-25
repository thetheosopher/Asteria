"""Download and install Qt 6.8.2 using aqtinstall with SSL workarounds for corporate networks."""
import ssl
import sys
import urllib3

# Disable SSL verification (corporate CRL block workaround)
ssl._create_default_https_context = ssl._create_unverified_context
urllib3.disable_warnings()

import requests
_orig_init = requests.Session.__init__
def _patched_init(self, *a, **kw):
    _orig_init(self, *a, **kw)
    self.verify = False
requests.Session.__init__ = _patched_init

from aqt import Cli
sys.exit(Cli().run([
    '-c', r'C:\Projects\other\Asteria\tools\aqt-settings.ini',
    'install-qt', 'windows', 'desktop', '6.8.2', 'win64_msvc2022_64',
    '-O', r'C:\Qt',
    '-b', 'https://download.qt.io'
]))
