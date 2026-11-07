"""Fetch the LLVM apt.llvm.org 18.1.8 packages into an isolated build cache on Linux/WSL."""
from pathlib import Path
import hashlib, os, subprocess, urllib.request
root = Path(os.environ.get('WINE11_LLVM_CACHE', str(Path.home() / '.cache/wine11-llvm-18.1.8')))
root.mkdir(parents=True, exist_ok=True)
packages = {
 'lld-18': '2970b2cfc3abaf9f25ab07225b559b222e3d14e3508d0121f92c3beae2a2d820',
 'clang-18': '6b23c30bd68a86e9485cafd0806d3aa46a812089fba74d8283befa611825b42b',
 'libclang-common-18-dev': 'c8775e56339bcbf050994ed97ab4cc442350d3583a86cb29de4a06aec7576dde',
 'libclang-cpp18': '6abcc0f657b09434acfa561afb73791323f326782a0ea513f9747ef49440941d',
 'libclang1-18': '208abb2defadc4bf204ffc4cd74da0419aa66ae00763ba6e4a7aae48a5bbfb46',
 'libllvm18': '4c2ccb3bdca15e5741f57bb025a05d79cdd57becfd5e23633ce82f120007cece',
 'llvm-18': '294aaf79a3425cbce05d11ccd68875497ee01d2f6e4aee24ed6c952938737036',
 'llvm-18-dev': 'ea97bf6a8ac1463f2a97c7d09411ba07ccb97bae40760ae735ce7d8083795c8c',
}
suffix = '_18.1.8~++20240731025043+3b5b5c1ec4a3-1~exp1~20240731145144.92_amd64.deb'
for name, expected in packages.items():
    deb = root / (name + suffix)
    if not deb.exists():
        print('Downloading', name, flush=True)
        url = 'https://apt.llvm.org/noble/pool/main/l/llvm-toolchain-18/' + deb.name
        with urllib.request.urlopen(url, timeout=120) as response, (root / (deb.name + '.part')).open('wb') as out:
            while chunk := response.read(1024 * 1024): out.write(chunk)
        (root / (deb.name + '.part')).replace(deb)
    if hashlib.sha256(deb.read_bytes()).hexdigest() != expected:
        raise SystemExit('LLVM package hash mismatch: ' + name)
    subprocess.run(['dpkg-deb', '-x', str(deb), str(root / 'root')], check=True)
print('LLVM cache ready:', root / 'root/usr/lib/llvm-18/bin', flush=True)
