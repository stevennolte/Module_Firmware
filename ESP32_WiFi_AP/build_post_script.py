import shutil
import os
Import("env", "projenv")

version_path = r"./include/Version.h"
bin_path = r"./.pio/build/esp32doit-devkit-v1/firmware.bin"
program = ""
newVersion = ""

# Detect CI environment (GitHub Actions, etc.)
IS_CI = bool(os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS"))


def getInfo():
    """Read program name and current version from Version.h."""
    global program, newVersion
    with open(version_path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    for line in lines:
        if line.startswith("#define NAME "):
            program = line.replace("#define NAME ", "").replace('"', "").strip()
        if "#define VERSION" in line:
            newVersion = line.replace("#define VERSION ", "").replace('"', "").strip()
    print(f"Program: {program}  Version: {newVersion}")


def increment_version():
    """Increment the patch component of the version in Version.h (local builds only)."""
    global newVersion
    lines = []
    with open(version_path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    for i, line in enumerate(lines):
        if "#define VERSION" in line:
            old = line.replace("#define VERSION ", "").replace('"', "").strip()
            parts = old.split(".")
            if len(parts) != 3:
                print(f"WARNING: Unexpected version format '{old}' – skipping increment")
                break
            try:
                patch = int(parts[-1]) + 1
            except ValueError:
                print(f"WARNING: Cannot parse patch '{parts[-1]}' – skipping increment")
                break
            if patch >= 255:
                patch = 0
                parts[-2] = str(int(parts[-2]) + 1)
            parts[-1] = f"{patch:04d}"
            newVersion = f"{parts[0].strip()}.{parts[1].strip()}.{parts[2]}"
            lines[i] = f'#define VERSION "{newVersion}"\n'
            print(f"Version updated: {old} -> {newVersion}")
            break
    with open(version_path, "w", encoding="utf-8") as f:
        f.writelines(lines)


def copy_firmware(source, target, env):
    """Copy the built firmware binary to the project root as {NAME}_{VERSION}.bin."""
    dest = f"./{program}_{newVersion}.bin"
    print(f"Copying firmware to {dest}")
    shutil.copy(bin_path, dest)


def upload_filesystem(source, target, env):
    """Build and upload filesystem before firmware upload."""
    print("")
    print("=" * 60)
    print("Building and uploading filesystem (HTML files)...")
    print("=" * 60)
    env.Execute("pio run --target buildfs")
    env.Execute("pio run --target uploadfs")
    print("=" * 60)
    print("Filesystem upload complete. Proceeding with firmware upload...")
    print("=" * 60)
    print("")


# ── Execution ──────────────────────────────────────────────────────────────
getInfo()
if not IS_CI:
    increment_version()

env.AddPostAction("buildprog", copy_firmware)
env.AddPreAction("upload", upload_filesystem)
env.AddPostAction("upload", copy_firmware)

print("")
print("#########################################################")
