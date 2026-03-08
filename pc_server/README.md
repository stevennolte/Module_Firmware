# PC Module Management Server

A lightweight Python/Flask web application that runs on a PC and provides:

- **mDNS discovery** – finds `ESP32_AIO.local` and `ESP32_Row_Controller.local` on the LAN
- **Live diagnostics** – polls each module's `/getDebugVars` endpoint and displays the results
- **Firmware management** – checks this GitHub repository for the latest firmware release, compares it with the version running on each module, and can push an OTA update with one click

---

## Requirements

- Python 3.10+
- The ESP32 modules and the PC must be on the **same local network**
- mDNS must be working on the PC's OS (Bonjour on macOS/Windows, Avahi on Linux)

---

## Setup

```bash
cd pc_server
python -m venv .venv

# Windows
.venv\Scripts\activate
# macOS / Linux
source .venv/bin/activate

pip install -r requirements.txt
```

---

## Running

### Easy method (Windows) – Double-click launcher

Simply double-click **`start_server.bat`** in the `pc_server` folder.

**To create a desktop shortcut:**
1. Right-click `start_server.bat` → **Create shortcut**
2. Drag the shortcut to your Desktop
3. (Optional) Right-click the shortcut → **Properties** → **Change Icon** to customize

### Command line method

```bash
# Windows (if PowerShell script execution is enabled)
.venv\Scripts\activate
python server.py

# macOS / Linux
source .venv/bin/activate
python server.py
```

Then open a browser to **http://localhost:5000**

**Note for Windows users:** If you get a PowerShell execution policy error, use the `start_server.bat` launcher instead, or run:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Environment variables (all optional)

| Variable        | Default           | Description |
|-----------------|-------------------|-------------|
| `REPO_OWNER`    | `stevennolte`     | GitHub repo owner |
| `REPO_NAME`     | `Module_Firmware` | GitHub repo name |
| `GITHUB_TOKEN`  | *(empty)*         | Personal access token – avoids GitHub API rate limits |
| `HOST`          | `0.0.0.0`         | Bind address |
| `PORT`          | `5000`            | Bind port |
| `FLASK_DEBUG`   | `0`               | Set to `1` for debug/auto-reload |

Example with a token:

```bash
GITHUB_TOKEN=ghp_xxxx python server.py
```

---

## How it works

### Module discovery

The server resolves `<MODULE_NAME>.local` using the OS mDNS resolver (no extra daemon required).  Each ESP32 module registers its mDNS hostname when it connects to WiFi (`MDNS.begin(NAME)` in the firmware).

### Version endpoint

Each module exposes `GET /version` which returns:

```json
{"name": "ESP32_AIO", "version": "1.1.005"}
```

The server compares this version against the latest GitHub release for that module.

### Firmware release convention

GitHub Actions (`.github/workflows/build_firmware.yml`) builds firmware and creates releases tagged:

```
ESP32_AIO_1.1.005
ESP32_Row_Controller_1.0.002
```

Each release contains a `.bin` asset named `{MODULE}_{VERSION}.bin`.  The PC server fetches the GitHub Releases API, finds the latest matching release, and downloads the binary on demand.

### OTA push

Clicking **Push OTA Update** in the dashboard causes the server to:

1. Download the `.bin` from GitHub
2. HTTP POST it to `http://<module-ip>/update` (multipart form, field name `firmware`)
3. The module validates that the filename starts with its own `NAME`, writes it to flash, and reboots

---

## API endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET  | `/` | Dashboard UI |
| GET  | `/api/modules` | JSON list of all module statuses |
| GET  | `/api/module/<name>/diagnostics` | Live debug vars from module |
| GET  | `/api/module/<name>/latest-firmware` | Latest GitHub release info |
| POST | `/api/module/<name>/update` | Download + push OTA firmware |
| POST | `/api/module/<name>/reboot` | Send reboot command to module |
