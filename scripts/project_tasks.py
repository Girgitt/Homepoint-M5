Import("env")

import subprocess
import sys


def _run_platformio(*args):
    project_dir = env.subst("$PROJECT_DIR")
    command = [sys.executable, "-m", "platformio", *args]
    print("[task] running: {}".format(" ".join(command)))
    subprocess.run(command, cwd=project_dir, check=True)


def test_native(*_args, **_kwargs):
    _run_platformio("test", "-e", "native")


def verify_all(*_args, **_kwargs):
    _run_platformio("test", "-e", "native")
    _run_platformio("run", "-e", "m5stack-core2")


env.AddCustomTarget(
    name="testnative",
    dependencies=None,
    actions=test_native,
    title="Test Native",
    description="Run the host-side native regression suite",
    always_build=True,
)

env.AddCustomTarget(
    name="verifyall",
    dependencies=None,
    actions=verify_all,
    title="Verify All",
    description="Run native regression tests, then build the Core2 firmware",
    always_build=True,
)
