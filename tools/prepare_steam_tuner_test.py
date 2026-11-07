"""Make the real generated downloader use a deterministic tuner clock for host tests."""
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text(encoding='utf-8')
old = 'static_cast<std::uint64_t>(Milliseconds(Clock::now() - download_start))'
needle = '      const bool changed_limit = auto_tuner.observe(now_ms, stats.compressed_bytes);'
if source.count(old) != 1 or source.count(needle) != 1:
    raise SystemExit('Unexpected generated downloader; refusing ambiguous test clock')
limit_update = '        active_limit = auto_tuner.slots(); stats.slots = active_limit; changed.notify_all();'
if source.count(limit_update) != 1:
    raise SystemExit('Unexpected generated worker limit update')
source = '#include <atomic>\nextern std::atomic<unsigned long long> simulated_tuner_ms, observed_tuner_ms, observed_tuner_bytes, applied_tuner_limit;\n' + source.replace(old, 'simulated_tuner_ms.load()').replace(needle, needle + '\n      observed_tuner_bytes.store(stats.compressed_bytes); observed_tuner_ms.store(now_ms + 1);').replace(limit_update, limit_update + '\n        applied_tuner_limit.store(active_limit);')
Path(sys.argv[2]).write_text(source, encoding='utf-8', newline='\n')
