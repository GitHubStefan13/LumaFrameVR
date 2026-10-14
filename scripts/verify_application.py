import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--video", required=True, type=Path)
parser.add_argument("--runner", type=Path)
arguments = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = arguments.build.resolve()
video = arguments.video.resolve()
stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
output = build / ("verification-" + stamp)
output.mkdir()
records = []


def command(label, args, expected=(0,), timeout=180):
    if arguments.runner and Path(args[0]).is_file() and Path(args[0]).read_bytes()[:4] == b"\x7fELF":
        args = [str(arguments.runner), *args]
    result = subprocess.run(args, cwd=root, env=os.environ.copy(), text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (output / (label + ".log")).write_text(result.stdout)
    records.append({"label": label, "command": list(map(str, args)), "exit_code": result.returncode})
    print(label, "exit", result.returncode, flush=True)
    if result.returncode not in expected:
        raise RuntimeError(label + " failed; inspect " + str(output / (label + ".log")))
    return result.stdout


def png(ppm):
    data = ppm.read_bytes()
    header, geometry, maximum, pixels = data.split(b"\n", 3)
    if header != b"P6" or maximum != b"255":
        raise RuntimeError("Unsupported verification PPM")
    width, height = map(int, geometry.split())
    if len(pixels) != width * height * 3:
        raise RuntimeError("Truncated verification raster")

    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    image = b"\x89PNG\r\n\x1a\n"
    image += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    image += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    (output / (ppm.stem + ".png")).write_bytes(image)


command("build", ["cmake", "--build", str(build), "-j1"], timeout=360)
command("ctest", ["ctest", "--test-dir", str(build), "--output-on-failure", "-V"], timeout=180)
executable = build / "lumaframevr"
for mode in ("help", "version"):
    command(mode, [str(executable), "--" + mode])
command("probe", [str(executable), str(video), "--probe"])
command("startup", [str(executable), str(video), "--seconds", "1"], expected=(0, 1), timeout=30)
command("render-probe", [str(executable), str(video), "--render-probe", "--seconds", "1"], expected=(0, 1), timeout=30)
install = output / "installed"
command("install", ["cmake", "--install", str(build), "--prefix", str(install)])
for mode in ("help", "version"):
    command("installed-" + mode, [str(install / "bin/lumaframevr"), "--" + mode])
command("installed-probe", [str(install / "bin/lumaframevr"), str(video), "--probe"])
shutil.copy2(build / "application_overlay_acceptance", install / "bin/application_overlay_acceptance")
command("relocated-overlays", [str(install / "bin/application_overlay_acceptance")])
env_only = output / "env-only/bin"
env_only.mkdir(parents=True)
shutil.copy2(build / "application_overlay_acceptance", env_only / "overlay")
previous_shader_dir = os.environ.get("LUMAFRAMEVR_SHADER_DIR")
os.environ["LUMAFRAMEVR_SHADER_DIR"] = str(install / "share/lumaframevr/shaders")
command("shader-env-overlays", [str(env_only / "overlay")])
if previous_shader_dir is None:
    os.environ.pop("LUMAFRAMEVR_SHADER_DIR")
else:
    os.environ["LUMAFRAMEVR_SHADER_DIR"] = previous_shader_dir
for ppm in sorted((build / "panel-rasters").glob("*.ppm")):
    png(ppm)
files = [root / "CMakeLists.txt"]
files.extend(path for path in (root / "cpp/application").glob("*") if path.suffix in (".cpp", ".hpp"))
files.extend(root / "cmake" / (name + ".cmake") for name in ("ApplicationAssembly", "ApplicationRuntime"))
files.extend(root / "cpp/tests" / (name + "_acceptance.cpp") for name in ("routing", "pointer", "media_loop", "scene", "overlay", "panel_drawing", "scene_playback", "browser_controls", "controller_visual", "controller_layer"))
files.extend(root / name for name in ("scripts/verify_application.py", "tests/test_project_hygiene.py"))
inventory = []
for path in sorted(files):
    data = path.read_bytes()
    lines = len(data.splitlines())
    if lines >= 200:
        raise RuntimeError("Source exceeds strict <200 LOC bound: " + str(path))
    if str(root).encode() in data:
        raise RuntimeError("Non-relocatable absolute source path: " + str(path))
    inventory.append({"path": str(path.relative_to(root)), "bytes": len(data), "lines": lines,
                      "sha256": hashlib.sha256(data).hexdigest()})
(output / "source-inventory.json").write_text(json.dumps(inventory, indent=2) + "\n")
(output / "commands.json").write_text(json.dumps(records, indent=2) + "\n")
print(json.dumps({"evidence": str(output), "source_files": len(inventory),
                  "maximum_lines": max(item["lines"] for item in inventory)}, indent=2))
