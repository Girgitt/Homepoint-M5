Import("env")

import subprocess
import sys


def _run_platformio_target(target_name):
    project_dir = env.subst("$PROJECT_DIR")
    environment = env.get("PIOENV")

    command = [
        sys.executable,
        "-m",
        "platformio",
        "run",
        "--project-dir",
        project_dir,
        "--environment",
        environment,
        "--target",
        target_name,
    ]

    print("[uploadfresh] running: {}".format(" ".join(command)))
    subprocess.run(command, check=True)


def upload_fresh(*_args, **_kwargs):
    print("[uploadfresh] WARNING: this replaces the complete LittleFS partition")
    print("[uploadfresh] Filesystem contents will be rebuilt from data/")
    print("[uploadfresh] Step 1/2: upload firmware")
    _run_platformio_target("upload")
    print("[uploadfresh] Step 2/2: upload fresh LittleFS image")
    _run_platformio_target("uploadfs")
    print("[uploadfresh] completed")


env.AddCustomTarget(
    name="uploadfresh",
    dependencies=None,
    actions=upload_fresh,
    title="Upload Fresh",
    description="Upload firmware, then replace LittleFS with a fresh image built from data/",
    always_build=True,
)
