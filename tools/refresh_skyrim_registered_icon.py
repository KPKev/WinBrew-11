"""Refresh Skyrim's two registered-title icon copies after the managed repair."""
from pathlib import Path
import refresh_skyrim_home_icon as repair

repair.TARGETS = (
    ('/user/app/FAKE78005', '0666', True),
    ('/user/app/FAKE78005/sce_sys', '0666', True),
)
repair.STATE = Path(__file__).resolve().parents[1] / 'build/skyrim-crash-investigation/registered-icon-refresh.json'
repair.TAG = 'wine11-skyrim-icon'

if __name__ == '__main__': repair.main()
