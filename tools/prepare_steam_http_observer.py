"""Generate the scoped HTTP receive observer without changing Playden's source."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root / 'vendor/playden-source/src/steam/backend/http_client.cpp').read_text()
def replace(old, new):
    global source
    if source.count(old) != 1: raise SystemExit('Unexpected HTTP client source; refusing ambiguous patch')
    source = source.replace(old, new, 1)
replace('#include "steam/backend/http_client.h"', '#include "steam/backend/http_client.h"\n#include "steam_integration/network_traffic.hpp"')
replace('size_t WriteBody(char* bytes, size_t size, size_t count, void* context) {\n  auto& transfer = *static_cast<TransferBuffer*>(context);\n  std::size_t length = 0;', '''size_t WriteBody(char* bytes, size_t size, size_t count, void* context) {
  auto& transfer = *static_cast<TransferBuffer*>(context);
  // Count payload delivered by curl even if this attempt is later refused,
  // truncated or retried. Scopes restrict it to this install's chunk requests.
  if (size == 0 || count <= std::numeric_limits<std::uint64_t>::max() / size)
    ww::steam_integration::traffic::received(static_cast<std::uint64_t>(size) * count);
  std::size_t length = 0;''')
out = Path(sys.argv[1]) if len(sys.argv) == 2 else root / 'build/steam-native-experiment/src/steam_integration/observed_http_client.cpp'
out.write_text(source, encoding='utf-8', newline='\n')
