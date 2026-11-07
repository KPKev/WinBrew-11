"""Reviewed recipe delivery and rejection tests; no console, compiler or git."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


stage = load("vapor_recipe_stage", "tools/stage-steam-runtime-assets.py")
review = load("vapor_recipe_review", "tools/assemble-steam-review.py")


class Recipes(unittest.TestCase):
    def test_delivery_uses_recipe_list_and_current_asset_verifier(self):
        # Export the small pinned asset set into a fixture with no .git folder.
        # Stage and verify real recipes through the production functions.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "source"
            for name in stage.read_runtime_asset_lock():
                path = repo / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((stage.REPO / name).read_bytes())
            ca = b"x" * 188900
            bundle = root / "cacert.pem"
            bundle.write_bytes(ca)
            dest = root / "staged"
            with mock.patch.object(stage, "REPO", repo), mock.patch.object(
                    stage, "CA_SHA256", hashlib.sha256(ca).hexdigest()), mock.patch.object(
                    stage, "git", side_effect=AssertionError("test must not invoke git")), mock.patch.object(
                    review, "ASSETS", dest):
                stage.stage(dest, bundle)
                receipt = review.verify_assets(stage)
                recipe_list = (dest / stage.RECIPE_LIST).read_text()
                self.assertTrue(recipe_list.startswith((repo / stage.RECIPE_LIST).read_text()))
                apps = set()
                for entry in json.loads((stage.VAPOR_RECIPES / "manifest.json").read_text())["recipes"]:
                    self.assertEqual(recipe_list.count(entry["file"]), 1)
                    name = "runtime/recipes/" + entry["file"]
                    data = (dest / name).read_bytes()
                    self.assertEqual(receipt["files"][name], hashlib.sha256(data).hexdigest())
                    recipe = json.loads(data)
                    self.assertEqual(recipe["locked"], [])
                    self.assertEqual(recipe["defaults"]["steam_features"], "emulate")
                    apps.add(recipe["app_id"])
                self.assertEqual(apps, {240, 105600, 489830, 1942280})
                # A changed payload and forged receipt cannot authorize it.
                name = "runtime/recipes/" + entry["file"]
                (dest / name).write_bytes(b"{}")
                receipt["files"][name] = hashlib.sha256(b"{}").hexdigest()
                (dest / "runtime-assets.json").write_text(json.dumps(receipt))
                with self.assertRaisesRegex(ValueError, "pinned Playden source"):
                    review.verify_assets(stage)

    def test_tampered_recipe_and_manifest_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for path in stage.VAPOR_RECIPES.iterdir():
                (root / path.name).write_bytes(path.read_bytes())
            with mock.patch.object(stage, "VAPOR_RECIPES", root):
                manifest_path = root / "manifest.json"
                manifest = json.loads(manifest_path.read_text())
                recipe = root / manifest["recipes"][0]["file"]
                original = recipe.read_bytes()
                recipe.write_bytes(b"{}")
                with self.assertRaisesRegex(ValueError, "reviewed digest"):
                    stage.vapor_recipe_overlay(b"upstream.json\n")
                recipe.write_bytes(original)
                manifest_path.write_bytes(b"{}")
                with self.assertRaisesRegex(ValueError, "manifest differs"):
                    stage.vapor_recipe_overlay(b"upstream.json\n")

    def test_existing_recipe_collision_refused(self):
        name = json.loads((stage.VAPOR_RECIPES / "manifest.json").read_text())["recipes"][0]["file"]
        with self.assertRaisesRegex(ValueError, "duplicate"):
            stage.vapor_recipe_overlay((name + "\n").encode())


if __name__ == "__main__":
    unittest.main()
