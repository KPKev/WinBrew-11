"""Resume only already-armed native registration journals on Vapor startup.

The existing OWNER driver checks native identity and the frozen publication.
Its register/await-native arms prohibit another registration submission. Other
recovery journals remain explicit operations. The pinned vendor stays intact.
"""
from pathlib import Path
import sys

ANCHOR = '      Log("startup generated recipes loaded");\n'
RECONCILE = '''      // ShadowMount completes registration while Vapor is closed. Reconcile
      // the persisted registration arm before publishing installs or applying
      // saved settings, so the next start exposes Play instead of Finish Setup.
      if (c.kit)
        for (const auto& record : c.repository.Snapshot()) {
          if (!AwaitingRegistration(record) ||
              record.state != rt::PreparationState::kRegistering ||
              c.coordinator->Activity(record.title_id) != rt::NativeActivity::kStopped)
            continue;
          // Step resumes the frozen OWNER transaction; an armed registration
          // only queries the console. Never regenerate or resubmit it here.
          const auto result = c.service->Step(record.install_id);
          Log("startup registration app=" + std::to_string(record.runtime_number) +
              " ok=" + std::to_string(result.ok()) + " message=" + result.message);
        }
'''


def transform(text):
    if text.count(ANCHOR) != 1 or 'startup registration app=' in text:
        raise ValueError('RuntimeHost changed; refusing ambiguous registration reconciliation')
    return text.replace(ANCHOR, ANCHOR + RECONCILE, 1)


if __name__ == '__main__':
    source, target = map(Path, sys.argv[1:])
    target.write_text(transform(source.read_text()))
