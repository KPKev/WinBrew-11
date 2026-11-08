"""Apply the shared Vapor preparation feedback to an explicitly selected source.

The transform refuses unexpected source and does not touch files on import.
"""
from pathlib import Path


def transform(source: str) -> str:
    old = '            round(play, 4, selected.activity == SteamGameActivity::registration_required ? 0xffb5791a : 0xff2b9c66);'
    new = '''            const auto feedback = vapor_play_feedback(selected.activity, now_ms);
            if (feedback.busy) {
              round({play.x - 5, play.y - 5, play.w + 10, play.h + 10}, 9, feedback.halo);
              round({play.x - 2, play.y - 2, play.w + 4, play.h + 4}, 6, feedback.rim);
            }
            round(play, 4, feedback.fill);'''
    replacements = [(old, new)]
    for old_line in (
        'text_fit(list.x + 20, list.y + 106, 160, state.message, 0, dim)',
        'text_fit(dx + 30, y + 348, dw - 60, state.message, 0, white)',
        'text_fit(dx + 32, y + 454, dw - 64, state.message[0] ? state.message : "Finish Setup completes PS5 registration. Then reopen Vapor and press Play.", 0, dim)',
        'text_fit(dx + 32, y + 454, dw - 64, state.message, 0, dim)',
    ):
        replacements.append((old_line, old_line.rsplit(', ', 1)[0] + ', kVaporStatusGold)'))
    for old, new in replacements:
        if source.count(old) == 1:
            source = source.replace(old, new, 1)
        elif source.count(new) != 1:
            raise ValueError('Unexpected or ambiguous Vapor feedback source: ' + old[:100])
    return source


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(transform(args.source.read_text()), encoding='utf-8')
