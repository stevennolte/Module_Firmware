# PC Module Management Server

A lightweight Python/Flask web application that runs on a PC and provides:

- **mDNS discovery** – finds `ESP32_AIO.local` and `ESP32_Row_Controller.local` on the LAN
- **Live diagnostics** – polls each module's `/getDebugVars` endpoint and displays the results
- **Firmware management** – checks this GitHub repository for the latest firmware release, compares it with the version running on each module, and can push an OTA update with one click
- **Local data logging** – periodically saves each module's diagnostic data to a CSV file on disk so data is never lost when offline
- **Google Drive sync** – automatically appends locally logged rows to a Google Sheets spreadsheet whenever internet connectivity is detected, enabling remote monitoring

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

| Variable                    | Default                    | Description |
|-----------------------------|----------------------------|-------------|
| `REPO_OWNER`                | `stevennolte`              | GitHub repo owner |
| `REPO_NAME`                 | `Module_Firmware`          | GitHub repo name |
| `GITHUB_TOKEN`              | *(empty)*                  | Personal access token – avoids GitHub API rate limits |
| `HOST`                      | `0.0.0.0`                  | Bind address |
| `PORT`                      | `5000`                     | Bind port |
| `FLASK_DEBUG`               | `0`                        | Set to `1` for debug/auto-reload |
| `DATA_LOG_FILE`             | `module_data_log.csv`      | Path to the local CSV log file |
| `DATA_LOG_INTERVAL`         | `60`                       | Seconds between data-collection polls |
| `GDRIVE_CREDENTIALS_FILE`   | *(empty)*                  | Path to a Google service-account JSON key file |
| `GDRIVE_SPREADSHEET_ID`     | *(empty)*                  | Google Sheets spreadsheet ID |
| `GDRIVE_SHEET_NAME`         | `ModuleData`               | Worksheet tab name in the spreadsheet |
| `GDRIVE_SYNC_INTERVAL`      | `300`                      | Seconds between automatic Google Drive sync attempts |

Example with a token and Google Drive enabled:

```bash
GITHUB_TOKEN=ghp_xxxx \
GDRIVE_CREDENTIALS_FILE=credentials.json \
GDRIVE_SPREADSHEET_ID=1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgVE2upms \
python server.py
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

### Local data logging

A background thread polls every module's `/getDebugVars` endpoint every `DATA_LOG_INTERVAL` seconds and appends one row per online module to `module_data_log.csv`:

| timestamp | module_name | module_ip | version | data |
|-----------|-------------|-----------|---------|------|
| 2024-01-01T12:00:00+00:00 | ESP32_AIO | 192.168.1.100 | 1.1.005 | `["var1: 100", "var2: 200"]` |

The CSV file is kept locally even when the internet is unavailable, ensuring no data is lost.

### Google Drive sync

A second background thread runs every `GDRIVE_SYNC_INTERVAL` seconds.  When it detects an internet connection it appends all rows that have not yet been uploaded to a Google Sheets spreadsheet, then records the sync position in `sync_state.json`.  You can also trigger an immediate sync from the dashboard.

#### Setting up Google Drive (service account)

1. Go to [Google Cloud Console](https://console.cloud.google.com/) and create a project (or pick an existing one).
2. Enable the **Google Sheets API** for the project.
3. Create a **Service Account** (IAM & Admin → Service Accounts → Create).
4. Create a JSON key for the service account and download it (e.g. `credentials.json`).
5. Create a Google Sheets spreadsheet and **share it** with the service account's email address (Editor access).
6. Copy the spreadsheet ID from its URL:
   `https://docs.google.com/spreadsheets/d/<SPREADSHEET_ID>/edit`
7. Set environment variables before starting the server:

```bash
GDRIVE_CREDENTIALS_FILE=credentials.json
GDRIVE_SPREADSHEET_ID=<your-spreadsheet-id>
```

The server will automatically create a header row and append data rows to the `ModuleData` worksheet tab.

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
| GET  | `/api/data-log/status` | Data logging and sync status |
| POST | `/api/data-log/sync-gdrive` | Trigger an immediate Google Drive sync |
