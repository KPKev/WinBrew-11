"""Admit Vapor's owner-requested 0777 title folders during settings publication."""
from pathlib import Path
import sys


def transform(source: str) -> str:
    old = '((title_info.st_mode & 07777) != 0700 && (title_info.st_mode & 07777) != 0755)'
    new = ('((title_info.st_mode & 07777) != 0700 && (title_info.st_mode & 07777) != 0755 && '
           '(title_info.st_mode & 07777) != 0777)')
    if source.count(old) != 2:
        raise ValueError('Unexpected staged-title permission checks')
    return source.replace(old, new)


def transform_coordinator(source: str) -> str:
    old = '((info.st_mode & 07777) == 0700 || (info.st_mode & 07777) == 0755)'
    new = '((info.st_mode & 07777) == 0700 || (info.st_mode & 07777) == 0755 || (info.st_mode & 07777) == 0777)'
    if source.count(old) != 1:
        raise ValueError('Unexpected native title-directory ownership check')
    source = source.replace('// Playden owns a title directory it created: owned by this user and not\n'
                            '// writable by others. ShadowMount must read it, so 0755 as well as 0700.',
                            '// Vapor also admits the owner-requested editable 0777 title directories.')
    return source.replace(old, new)


if __name__ == '__main__':
    if len(sys.argv) != 3: raise SystemExit('usage: prepare_vapor_publication_permissions.py INPUT OUTPUT')
    source, dest = map(Path, sys.argv[1:])
    dest.write_text(transform(source.read_text()), encoding='utf8', newline='\n')
