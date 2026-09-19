Import("env")

import atexit
import shutil
import time
from pathlib import Path
from SCons.Script import GetBuildFailures

build_started = time.time()

build_dir = Path(env.subst("$BUILD_DIR"))
project_dir = Path(env.subst("$PROJECT_DIR"))
output_dir = project_dir / "build-output"


def copy_successful_build():
    if GetBuildFailures():
        return

    output_dir.mkdir(parents=True, exist_ok=True)

    artifacts = []

    for pattern in ("firmware*.bin", "firmware*.elf", "firmware*.hex"):
        for path in build_dir.glob(pattern):
            if path.stat().st_mtime >= build_started:
                artifacts.append(path)

    for src in sorted(artifacts):
        dst = output_dir / src.name
        shutil.copy2(src, dst)
        print(f"Copied build output: {src} -> {dst}")


atexit.register(copy_successful_build)
