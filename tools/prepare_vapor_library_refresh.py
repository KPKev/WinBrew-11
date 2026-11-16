"""Keep transient Steam app-info misses retryable without modifying vendored Playden."""
from pathlib import Path
import sys


def prepare(source: str) -> str:
    replacements = {
        "    const bool borrowed = std::any_of(apps.begin(), apps.end(), [&](auto app) { return shared.count(app); });\n    if (!borrowed && std::any_of(apps.begin(), apps.end(), [&](auto app) { return listed.count(app); })) continue;":
        "    // A package may contain multiple games, even when one is already listed.",
        'if ((type == "game" || type == "demo") && info->name':
        'if ((type == "game" || type == "demo" || type == "application") && info->name',
        ': type != "game" && type != "demo"':
        ': type != "game" && type != "demo" && type != "application"',
        "      looked_up_[app] = name;": """      // Cache named games and confirmed non-game types, not incomplete replies.
      // A just-added license can precede its metadata becoming available.
      if (info && !type.empty() && (!name.empty() || (type != "game" && type != "demo" && type != "application")))
        looked_up_[app] = name;""",
        "      if (found == looked_up_.end()) {":
        "      if (found == looked_up_.end() && !family_left_out_.count(app)) {",
    }
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError("Steam library source changed: expected one " + repr(old))
        source = source.replace(old, new)
    return source


def prepare_owned(source: str) -> str:
    old = '"&include_appinfo=true&include_played_free_games=true&format=json";'
    new = ('"&include_appinfo=true&include_played_free_games=true"\n'
           '      "&include_free_sub=true&skip_unvetted_apps=false&format=json";')
    if source.count(old) != 1:
        raise ValueError("Steam owned library request changed")
    return source.replace(old, new)


if __name__ == "__main__":
    owned = sys.argv[1:2] == ["--owned"]
    source, output = map(Path, sys.argv[2:] if owned else sys.argv[1:])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text((prepare_owned if owned else prepare)(source.read_text(encoding="utf-8")), encoding="utf-8")
