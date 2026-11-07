#!/usr/bin/env python3
"""Apply Wine 11's exception cleanup to an isolated copy of Playden's UID guard.

A failed restore still leaves credentials unverified. This transformation does
not qualify the target runtime or modify the upstream checkout.
"""
from pathlib import Path

OLD = '''  const bool ok = work();
  return ::seteuid(user) == 0 && ok;'''
NEW = '''  bool ok;
  try {
    ok = work();
  } catch (...) {
    // Do not continue with the original exception as if restoration succeeded.
    if (::seteuid(user) != 0)
      throw std::runtime_error("Unable to restore the original effective UID");
    throw;
  }
  return ::seteuid(user) == 0 && ok;'''


def transform(source: str) -> str:
    if source.count(OLD) != 1 or source.count('#include <mutex>') != 1:
        raise ValueError('Unexpected UID guard source; refusing ambiguous transformation')
    return source.replace('#include <mutex>', '#include <mutex>\n#include <stdexcept>', 1).replace(OLD, NEW, 1)


def apply(path: Path) -> None:
    path.write_text(transform(path.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error('Source and output must be different files')
    args.output.write_text(transform(args.source.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
