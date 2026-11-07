import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('guard', Path(__file__).resolve().parents[1] / 'tools/prepare_steam_uid_guard.py')
guard = importlib.util.module_from_spec(spec); spec.loader.exec_module(guard)
class GuardTransformTest(unittest.TestCase):
    def test_exact_source_and_ambiguous_rejected(self):
        source = (Path(__file__).resolve().parents[1] / 'vendor/playden-source/src/platform/effective_uid.cpp').read_text()
        result = guard.transform(source)
        self.assertIn('Unable to restore the original effective UID', result)
        with self.assertRaises(ValueError): guard.transform(source.replace(guard.OLD, 'changed'))
        with self.assertRaises(ValueError): guard.transform(source + guard.OLD)
if __name__ == '__main__': unittest.main()
