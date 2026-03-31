import shutil
import os
Import("env", "projenv")

version_path = r"./include/Version.h"
bin_path = r"./.pio/build/seeed_xiao_esp32s3/firmware.bin"
fs_bin_path = r"./.pio/build/seeed_xiao_esp32s3/littlefs.bin"
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
        if line.startswith('#define VERSION "'):
            newVersion = line.replace("#define VERSION ", "").replace('"', "").strip()
    print(f"Program: {program}  Version: {newVersion}")


def increment_version():
    """Increment the patch component of the version in Version.h (local builds only)."""
    global newVersion
    lines = []
    with open(version_path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    for i, line in enumerate(lines):
        if line.startswith('#define VERSION "'):
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


def copy_filesystem(source, target, env):
    """Copy the built filesystem binary to the project root as {NAME}_FS_{VERSION}.bin."""
    if not os.path.exists(fs_bin_path):
        print(f"Filesystem binary not found at {fs_bin_path}, skipping copy")
        return
    dest = f"./{program}_FS_{newVersion}.bin"
    print(f"Copying filesystem to {dest}")
    shutil.copy(fs_bin_path, dest)


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
# For local builds, increment version immediately (before any build actions)
if not IS_CI:
    print("Local build detected - incrementing version...")
    increment_version()
    getInfo()  # Re-read to show updated version
else:
    print("CI build detected - version will not be auto-incremented")
    getInfo()

env.AddPostAction("buildprog", copy_firmware)
env.AddPostAction("buildfs", copy_filesystem)
env.AddPreAction("upload", upload_filesystem)
env.AddPostAction("upload", copy_firmware)

print("")
print("#########################################################")
