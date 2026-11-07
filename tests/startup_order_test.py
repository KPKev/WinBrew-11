"""Enforce the helper's single-thread startup boundary in the native entry point.

The cooperative helper rejects targets once service-created threads exist.
This catches a return of the update5 ordering even though host service mocks do
not create the native threads that make it fail on hardware.
"""
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[1] / "src/ps5.cpp").read_text()
main = source[source.index("int main() {"):]
main = re.sub(r"//[^\n]*|/\*.*?\*/", "", main, flags=re.S)
request = "elevation::request(elevation::Capability::filesystem)"
assert main.count(request) == 1, "One startup elevation transaction is required"
boundary = main.index(request)
assert main.index('open("/download0"') < boundary
assert main.index('shell.init("/app0/assets"') < boundary
for module in ("native_input::preload_modules(", "native_browser::preload_modules("):
    assert main.index(module) < boundary, module + " must retain sandbox lookup"
for service in (
    "sceUserServiceInitialize(", "native_input::open(",
    "native_browser::initialize_dialog(", "scePadInit(",
    "sceSysmoduleLoadModuleInternal(", "sceNetCtlInit(", "open_audio(",
    "ps5::demo::run_frames(",
):
    assert service in main and main.index(service) > boundary, (
        service + " must follow cooperative elevation"
    )
print("PASS: sandbox module loading precedes root; root precedes service initialization")
