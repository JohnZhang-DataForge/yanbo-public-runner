# SPDX-License-Identifier: GPL-2.0-or-later
import struct,tempfile,unittest,zipfile
from pathlib import Path
from build_romfs import build
class RomfsTests(unittest.TestCase):
 def test_lookup(self):
  with tempfile.TemporaryDirectory() as t:
   a,o=Path(t)/'a.zip',Path(t)/'a.romfs';expected={'root':b'A','asset/alpha':b'BC'*100,'asset/\u732b':b'hello','empty':b''}
   with zipfile.ZipFile(a,'w') as z:
    for n,b in expected.items():z.writestr('romfs/'+n,b)
    z.writestr('dlcfs/extra',b'NOT IN BASE')
   self.assertEqual(build(a,o)['files'],4);image=o.read_bytes();h=struct.unpack_from('<10Q',image);self.assertEqual(h[0],80)
   def lookup(parent,name,file):
    value=parent^123456789
    for byte in name:value=(((value>>5)|(value<<27))^byte)&0xffffffff
    ht,hs,tab=(h[5],h[6],h[7]) if file else (h[1],h[2],h[3])
    off=struct.unpack_from('<I',image,ht+4*(value%(hs//4)))[0]
    while off!=0xffffffff:
     fields=struct.unpack_from('<IIQQII' if file else '<6I',image,tab+off);start=tab+off+(32 if file else 24)
     if fields[0]==parent and image[start:start+fields[-1]]==name:return off,fields
     off=fields[-2]
    raise KeyError(name)
   for path,data in expected.items():
    parent=0;parts=path.split('/')
    for name in parts[:-1]:parent,_=lookup(parent,name.encode(),False)
    _,f=lookup(parent,parts[-1].encode(),True);self.assertEqual(image[h[9]+f[2]:h[9]+f[2]+f[3]],data)
   self.assertNotIn(b'NOT IN BASE',image)
   with self.assertRaises(ValueError):build(a,o)
 def test_bad_paths(self):
  for names in [('romfs/../x',),('romfs/a','romfs/a/b'),('romfs/a','romfs/a'),('romfs/a\\b',)]:
   with self.subTest(names=names),tempfile.TemporaryDirectory() as t:
    a,o=Path(t)/'a.zip',Path(t)/'out'
    with zipfile.ZipFile(a,'w') as z:
     for n in names:
      info=zipfile.ZipInfo();info.filename=n;z.writestr(info,b'x')
    with self.assertRaises(ValueError):build(a,o)
    self.assertFalse(o.exists())
 def test_existing_part_preserved(self):
  with tempfile.TemporaryDirectory() as t:
   a,o=Path(t)/'a.zip',Path(t)/'out'
   with zipfile.ZipFile(a,'w') as z:z.writestr('romfs/a',b'x')
   part=Path(str(o)+'.part');part.write_bytes(b'keep')
   with self.assertRaises(FileExistsError):build(a,o)
   self.assertEqual(part.read_bytes(),b'keep')
