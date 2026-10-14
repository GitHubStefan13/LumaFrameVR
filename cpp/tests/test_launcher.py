from pathlib import Path
import os
import shlex
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
fixture = Path(sys.argv[2]) / "launcher path with spaces"
fixture.mkdir(parents=True, exist_ok=True)
shutil.copy2(root / "launch.sh", fixture / "launch.sh")
player = Path(sys.argv[1]).resolve()
if len(sys.argv) > 3:
    bridge = fixture / "lumaframevr"
    bridge.write_text("#!/bin/sh\nexec " + shlex.quote(sys.argv[3]) + " " + shlex.quote(str(player)) + ' "$@"\n')
    bridge.chmod(0o755)
else:
    shutil.copy2(player, fixture / "lumaframevr")
for directory in ("runtime", "mesa", "shaders", "branding"):
    (fixture / directory).mkdir(exist_ok=True)
env = os.environ.copy()
for key in ("SteamAppId", "SteamGameId"):
    env.pop(key, None)
env["LUMAFRAMEVR_LOG_FILE"] = str(fixture / "startup.log")
for argument, expected in [("--help", "lumaframevr [FILE|DIRECTORY]"), ("--version", "C++ Vulkan/OpenXR")]:
    result = subprocess.run(["sh", str(fixture / "launch.sh"), argument], env=env, cwd=fixture.parent,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
    combined = result.stdout + ((fixture / "startup.log").read_text() if (fixture / "startup.log").exists() else "")
    assert result.returncode == 0 and expected in combined, f"SSH/Steam-ID-independent real player launch failed: {result.stdout}"
assert "vrcmd" not in (fixture / "launch.sh").read_text(), "no fragile mandatory prelaunch helper"
print("PASS actual C++ player reached through launcher without Steam IDs and from a path containing spaces")
