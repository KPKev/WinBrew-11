import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('registration', ROOT / 'tools/prepare_steam_registration_reconcile.py')
registration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registration)


class TransformTests(unittest.TestCase):
    def test_startup_reconciles_before_installs_and_saved_settings(self):
        source = (ROOT / 'vendor/playden-source/src/homebrew/runtime_host.cpp').read_text()
        result = registration.transform(source)
        start = result.index('startup registration app=')
        self.assertLess(start, result.index('Log("startup installs published")'))
        self.assertLess(start, result.index('Log("startup settings app='))
        self.assertEqual(result.replace(registration.RECONCILE, '', 1), source)

    def test_source_drift_and_duplicate_patch_refused(self):
        for source in ('', registration.ANCHOR * 2, registration.ANCHOR + registration.RECONCILE):
            with self.assertRaises(ValueError):
                registration.transform(source)


if __name__ == '__main__':
    unittest.main()
