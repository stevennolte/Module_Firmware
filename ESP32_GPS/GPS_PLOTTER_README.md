# GPS Data Plotter

Python script to visualize and analyze GPS data from the ESP32_GPS module.

## Features

- **Interactive HTML Map**: Zoomable, pannable map with GPS track overlay
  - Multiple tile layers (OpenStreetMap, Terrain, Light, Dark themes)
  - Click markers to see detailed GPS data at each point
  - Speed heatmap visualization
  - Measurement tools for distance and area
  - Drawing tools to annotate the map
  - Mini-map and full-screen mode
- **GPS Track Visualization**: Static plots showing your GPS path with color-coded speed
- **Satellite Count**: Monitor satellite availability over time
- **HDOP Analysis**: Track positioning accuracy (Horizontal Dilution of Precision)
- **Speed Profile**: View speed changes throughout the recording
- **Altitude Profile**: Analyze elevation changes
- **Statistics**: Comprehensive summary of distance, speed, satellites, and accuracy
- **Robust Data Handling**: Automatically handles corrupted/invalid GPS data

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

Or install manually:
```bash
pip install pandas matplotlib numpy folium
```

## Usage

1. **Download GPS log from ESP32**:
   - Open http://esp32_gps.local in your browser
   - Enable GPS logging and let it record for a while
   - Click "Download CSV" to save `gpslog.csv`

2. **Run the plotter**:
```bash
python plot_gps.py gpslog.csv
```

Or if the file is named `gpslog.csv` in the same directory:
```bash
python plot_gps.py
```

3. **View results**:
   - An interactive plot window will open
   - A PNG image will be saved as `gpslog_plot.png`
   - An interactive HTML map will be saved as `gpslog_map.html`
   - Statistics will be printed to the console

## Output Files

The script generates two files:

1. **`<filename>_plot.png`**: Static matplotlib plots with 6 panels showing comprehensive GPS analysis
2. **`<filename>_map.html`**: Interactive HTML map - **double-click to open in your web browser**

## Interactive Map Features

The HTML map (`<filename>_map.html`) includes:

- **GPS Track**: Blue polyline showing your complete path
- **Start/End Markers**: Green play icon (start) and red stop icon (end)
- **Data Points**: Colored circles showing speed (green=slow, orange=medium, red=fast)
  - Click any point to see: position, speed, heading, satellites, HDOP, fix quality
- **Tile Layers**: Switch between map styles using the layers control (top-right)
  - OpenStreetMap (default)
  - Terrain
  - Light (CartoDB Positron)
  **Static plots (PNG)**: 6-panel visualization showing track, satellites, HDOP, speed, and altitude
- **Interactive map (HTML)**: Zoomable map with clickable markers and multiple overlay layers
- **Console statistics**: Duration, distance traveled, average speed, satellite count, etc.
- Saved files for documentation and analysisruler icon to measure distances and areas
- **Drawing Tools**: Add your own markers, lines, and polygons
- **Full Screen**: Expand to full screen for better viewing
- **Mini Map**: Small overview map in bottom-left corner
- **Mouse Position**: Shows coordinates as you move the mouse
- **Legend**: Summary box showing point count, total distance, and satellite average

## CSV Format

The GPS log uses the following format:
```
timestamp_ms,latitude,longitude,altitude_m,fix_quality,satellites,hdop,speed_kn,heading_deg
```

- **timestamp_ms**: Milliseconds since ESP32 boot
- **latitude**: Decimal degrees (positive = North, negative = South)
- **longitude**: Decimal degrees (positive = East, negative = West)
- **altitude_m**: Altitude in meters above mean sea level
- **fix_quality**: 0=invalid, 1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float
- **satellites**: Number of satellites in use
- **hdop**: Horizontal Dilution of Precision (lower is better, <2 is good)
- **speed_kn**: Ground speed in knots
- **heading_deg**: IMU heading in degrees (0-360)

## Example Output

The script generates:
- 6-panel visualization showing track, satellites, HDOP, speed, and altitude
- Console statistics including duration, distance traveled, average speed
- Saved PNG file for documentation

## Tips

- **HDOP < 1**: Excellent accuracy
- **HDOP 1-2**: Good accuracy
- **HDOP 2-5**: Moderate accuracy
- **HDOP > 5**: Poor accuracy

- For best results, enable GPS logging when you have a good satellite fix (8+ satellites)
- RTK Fixed (quality=4) provides centimeter-level accuracy
- The plotter filters out stationary points (<0.1m) for speed calculations
