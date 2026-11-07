import hashlib,importlib.util,subprocess,tempfile,unittest,zipfile
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
archive=load('archive','tools/archive_source.py');fetch=load('fetch','research/game_return/fetch_source.py')
class ArchiveTests(unittest.TestCase):
 def test_launcher_sources_included_and_unrelated_research_excluded(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);folder=root/'research/game_return';folder.mkdir(parents=True)
   for n in archive.REQUIRED_LAUNCHER:(folder/n).write_text(n)
   (root/'research/skyrim').mkdir();(root/'research/skyrim/private.txt').write_text('not packaged')
   (root/'src').mkdir();(root/'src/example.cpp').write_text('source')
   out=root/'source.zip';archive.create_archive(root,out)
   with zipfile.ZipFile(out) as z:
    names=set(z.namelist());self.assertTrue(all('WinBrew11/research/game_return/'+n in names for n in archive.REQUIRED_LAUNCHER));self.assertFalse(any('research/skyrim' in n for n in names))
 def test_missing_launcher_source_refuses_before_output(self):
  with tempfile.TemporaryDirectory() as d:
   out=Path(d)/'source.zip'
   with self.assertRaisesRegex(ValueError,'Launcher research source is missing'):archive.create_archive(Path(d),out)
   self.assertFalse(out.exists())
class SourceTests(unittest.TestCase):
 def test_new_exact_checkout_and_existing_dirty_preserved(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);origin=root/'origin';origin.mkdir();(origin/'native').mkdir();main=origin/'native/wine64_main.c';main.write_bytes(b'pinned launcher\n')
   def git(*args):return subprocess.check_output(['git','-C',str(origin),*args],stderr=subprocess.DEVNULL,text=True).strip()
   git('init');git('config','core.autocrlf','false');git('config','user.email','test@example.invalid');git('config','user.name','Fixture');git('add','native');git('commit','-m','fixture');rev=git('rev-parse','HEAD')
   with mock.patch.object(fetch,'REVISION',rev),mock.patch.object(fetch,'MAIN_SHA256',hashlib.sha256(main.read_bytes()).hexdigest()),mock.patch.object(fetch,'URL',str(origin)):
    dest=root/'target/source'
    global_config=root/'gitconfig';global_config.write_text('[core]\n\tautocrlf = true\n')
    with mock.patch.dict('os.environ', {'GIT_CONFIG_GLOBAL':str(global_config)}):
     fetch.prepare(dest);self.assertEqual(fetch.verify(dest),dest.resolve())
    self.assertEqual((dest/'native/wine64_main.c').read_bytes(),b'pinned launcher\n')
    self.assertEqual(subprocess.check_output(['git','-C',str(dest),'config','--local','core.autocrlf'],text=True).strip(),'false')
    changed=dest/'native/wine64_main.c';changed.write_text('keep local edits\n')
    with self.assertRaisesRegex(ValueError,'differs from the exact clean revision'):fetch.prepare(dest)
    self.assertEqual(changed.read_text(),'keep local edits\n')
 def test_nested_directory_in_outer_repo_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);subprocess.run(['git','-C',str(root),'init'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);nested=root/'nested';nested.mkdir()
   with self.assertRaisesRegex(ValueError,'own Git repository'):fetch.verify(nested)
if __name__=='__main__':unittest.main()
