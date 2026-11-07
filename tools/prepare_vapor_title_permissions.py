"""Apply the owner's browser-manageable title-directory policy to an isolated host."""
from pathlib import Path
import sys

def transform(source: str) -> str:
    start = source.index('bool TitleDirectoriesPublished(const fs::path& title) {')
    end = source.index('\n}  // namespace', start)
    block = source[start:end]
    old = '(info.st_mode & 07777) != 0755'
    if block.count(old) != 2 or block.count('::chmod(directory.c_str(), 0755)') != 1:
        raise ValueError('Title directory implementation changed; refusing ambiguous edit')
    block = block.replace(old, '(info.st_mode & 07777) != 0777')
    block = block.replace('::chmod(directory.c_str(), 0755)', '::chmod(directory.c_str(), 0777)')
    return source[:start] + block + source[end:]

def main():
    path = Path(sys.argv[1])
    original = path.read_text(encoding='utf8')
    path.write_text(transform(original), encoding='utf8', newline='\n')

if __name__ == '__main__': main()
