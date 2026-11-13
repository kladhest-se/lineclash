"""Installer layout, signing requirements and archive-format regression checks."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
import platform_packages as packages

class PackagesTests(unittest.TestCase):
 def test_msi_payload_and_upgrade_identity(self):
  ns={'w':'http://wixtoolset.org/schemas/v4/wxs'}
  doc=ET.fromstring(packages.wix_document([('/tmp/game.exe','bin/lineclash.exe'),('/tmp/music.ogg','music/theme.ogg')],'1.2.3'))
  self.assertEqual(len(doc.findall('.//w:File',ns)),2)
  self.assertEqual(len(doc.findall('.//w:ComponentRef',ns)),2)
  self.assertEqual(doc.find('.//w:MediaTemplate',ns).get('EmbedCab'),'yes')
  self.assertIsNotNone(doc.find('.//w:MajorUpgrade',ns))
  self.assertEqual(doc.find('.//w:Shortcut',ns).get('WorkingDirectory'),doc.find('.//w:Directory[@Name="bin"]',ns).get('Id'))
  with self.assertRaises(ValueError):packages.wix_document([('/tmp/file','../escape')],'1.0.0')
  with self.assertRaises(ValueError):packages.wix_document([],'1.0.0-beta')
 def test_mac_never_falls_back_to_adhoc(self):
  with patch('platform_packages.platform.system',return_value='Darwin'):
   with self.assertRaises(RuntimeError):packages.preflight({'SIGN_IDENTITY':'-'})
 def test_mac_requires_unique_developer_identity(self):
  class Result:stdout='  1) '+'A'*40+' "Developer ID Application: Example (TEAM)"\n'
  with patch('platform_packages.platform.system',return_value='Darwin'),patch('platform_packages.call',return_value=Result()):
   self.assertEqual(packages.preflight({}),'A'*40)
   with self.assertRaises(RuntimeError):packages.preflight({'SIGN_IDENTITY':'Missing'})
 def test_msi_is_required_and_must_be_valid(self):
  with tempfile.TemporaryDirectory() as folder:
   folder=Path(folder);record={'platform':'Windows','installer_assets':[],'artifacts':{}}
   with self.assertRaises(RuntimeError):packages.verify_installers(record,folder,folder)
   record.update(installer_assets=['game.msi'],artifacts={'game.msi':'test'})
   (folder/'game.msi').write_bytes(b'not an installer')
   with self.assertRaises(RuntimeError):packages.verify_installers(record,folder,folder)
if __name__=='__main__':unittest.main()
