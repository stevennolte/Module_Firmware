#!/usr/bin/env python3
"""
GPS Data Plotter for ESP32_GPS Module
Reads CSV log file and creates interactive plots of GPS track, speed, and satellite info.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import sys
import folium
from folium import plugins

def haversine_distance(lat1, lon1, lat2, lon2):
    """Calculate distance between two GPS points in meters using Haversine formula."""
    R = 6371000  # Earth radius in meters
    phi1, phi2 = np.radians(lat1), np.radians(lat2)
    dphi = np.radians(lat2 - lat1)
    dlambda = np.radians(lon2 - lon1)
    
    a = np.sin(dphi/2)**2 + np.cos(phi1) * np.cos(phi2) * np.sin(dlambda/2)**2
    c = 2 * np.arctan2(np.sqrt(a), np.sqrt(1-a))
    
    return R * c

def create_interactive_map(df, output_file):
    """Create an interactive HTML map with GPS track and data overlays."""
    
    print("\nCreating interactive map...")
    
    # Calculate map center
    center_lat = df['latitude'].mean()
    center_lon = df['longitude'].mean()
    
    # Create folium map
    m = folium.Map(
        location=[center_lat, center_lon],
        zoom_start=17,
        tiles='OpenStreetMap',
        control_scale=True
    )
    
    # Add additional tile layers
    folium.TileLayer(
        tiles='https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png',
        attr='Map data: &copy; OpenStreetMap contributors, SRTM | Map style: &copy; OpenTopoMap',
        name='Topo',
        overlay=False,
        control=True
    ).add_to(m)
    folium.TileLayer('CartoDB positron', name='Light').add_to(m)
    folium.TileLayer('CartoDB dark_matter', name='Dark').add_to(m)
    
    # Downsample data for markers if too many points (show every Nth point)
    marker_step = max(1, len(df) // 100)  # Max 100 markers
    
    # Create feature groups for different layers
    track_group = folium.FeatureGroup(name='GPS Track', show=True)
    marker_group = folium.FeatureGroup(name='Data Points', show=True)
    heatmap_group = folium.FeatureGroup(name='Heatmap', show=False)
    
    # Color mapping for speed
    max_speed = df['speed_kn'].max()
    
    def get_color(speed):
        """Get color based on speed."""
        if max_speed <= 0:
            return 'blue'
        ratio = speed / max_speed
        if ratio < 0.2:
            return 'green'
        elif ratio < 0.5:
            return 'lightgreen'
        elif ratio < 0.7:
            return 'orange'
        else:
            return 'red'
    
    # Create GPS track polyline with color segments
    coordinates = [[row['latitude'], row['longitude']] for _, row in df.iterrows()]
    
    # Main track line
    folium.PolyLine(
        coordinates,
        color='blue',
        weight=3,
        opacity=0.7,
        popup='GPS Track'
    ).add_to(track_group)
    
    # Add markers at downsampled points
    for i in range(0, len(df), marker_step):
        row = df.iloc[i]
        
        # Create popup content with GPS data
        popup_html = f"""
        <div style="font-family: monospace; font-size: 11px;">
            <b>Point #{i+1}</b><br>
            <hr style="margin: 3px 0;">
            <b>Position:</b><br>
            Lat: {row['latitude']:.6f}°<br>
            Lon: {row['longitude']:.6f}°<br>
            Alt: {row['altitude_m']:.1f} m<br>
            <hr style="margin: 3px 0;">
            <b>Motion:</b><br>
            Speed: {row['speed_kn']:.1f} kn ({row['speed_kn']*0.5144:.2f} m/s)<br>
            Heading: {row['heading_deg']:.1f}°<br>
            <hr style="margin: 3px 0;">
            <b>Quality:</b><br>
            Fix: {row['fix_quality']:.0f}<br>
            Sats: {row['satellites']:.0f}<br>
            HDOP: {row['hdop']:.2f}<br>
            <hr style="margin: 3px 0;">
            <b>Time:</b><br>
            {row['time_sec']:.1f} seconds<br>
            Distance: {row['cumulative_m']:.1f} m
        </div>
        """
        
        # Choose icon color based on speed
        icon_color = get_color(row['speed_kn'])
        
        folium.CircleMarker(
            location=[row['latitude'], row['longitude']],
            radius=4,
            popup=folium.Popup(popup_html, max_width=250),
            color=icon_color,
            fill=True,
            fillColor=icon_color,
            fillOpacity=0.7,
            weight=1
        ).add_to(marker_group)
    
    # Add start marker (green)
    start_row = df.iloc[0]
    folium.Marker(
        location=[start_row['latitude'], start_row['longitude']],
        popup=f"<b>START</b><br>Lat: {start_row['latitude']:.6f}°<br>Lon: {start_row['longitude']:.6f}°",
        icon=folium.Icon(color='green', icon='play', prefix='fa'),
        tooltip='Start Point'
    ).add_to(track_group)
    
    # Add end marker (red)
    end_row = df.iloc[-1]
    folium.Marker(
        location=[end_row['latitude'], end_row['longitude']],
        popup=f"<b>END</b><br>Lat: {end_row['latitude']:.6f}°<br>Lon: {end_row['longitude']:.6f}°",
        icon=folium.Icon(color='red', icon='stop', prefix='fa'),
        tooltip='End Point'
    ).add_to(track_group)
    
    # Create heatmap layer (speed intensity)
    if max_speed > 0:
        heat_data = [[row['latitude'], row['longitude'], row['speed_kn']] 
                     for _, row in df.iterrows()]
    else:
        heat_data = [[row['latitude'], row['longitude'], 1] 
                     for _, row in df.iterrows()]
    
    plugins.HeatMap(
        heat_data,
        name='Speed Heatmap',
        min_opacity=0.3,
        max_zoom=18,
        radius=15,
        blur=15,
        gradient={0.4: 'blue', 0.6: 'lime', 0.8: 'yellow', 1.0: 'red'}
    ).add_to(heatmap_group)
    
    # Add feature groups to map
    track_group.add_to(m)
    marker_group.add_to(m)
    heatmap_group.add_to(m)
    
    # Add drawing tools
    plugins.Draw(
        export=True,
        position='topleft',
        draw_options={
            'polyline': True,
            'polygon': True,
            'circle': False,
            'rectangle': True,
            'marker': True,
            'circlemarker': False
        }
    ).add_to(m)
    
    # Add measurement tool
    plugins.MeasureControl(
        position='topleft',
        primary_length_unit='meters',
        secondary_length_unit='kilometers',
        primary_area_unit='sqmeters',
        secondary_area_unit='hectares'
    ).add_to(m)
    
    # Add fullscreen button
    plugins.Fullscreen(
        position='topleft',
        title='Fullscreen',
        title_cancel='Exit Fullscreen',
        force_separate_button=True
    ).add_to(m)
    
    # Add mouse position display
    plugins.MousePosition(
        position='bottomright',
        separator=' | ',
        empty_string='NaN',
        lng_first=False,
        num_digits=6,
        prefix='Position:',
    ).add_to(m)
    
    # Add minimap
    minimap = plugins.MiniMap(toggle_display=True)
    m.add_child(minimap)
    
    # Add layer control
    folium.LayerControl(position='topright', collapsed=False).add_to(m)
    
    # Add legend for speed colors
    legend_html = f'''
    <div style="position: fixed; 
                bottom: 50px; right: 20px; width: 180px; height: auto;
                background-color: white; border:2px solid grey; z-index:9999; 
                font-size:12px; padding: 10px; border-radius: 5px;
                box-shadow: 2px 2px 6px rgba(0,0,0,0.3);">
        <p style="margin: 0 0 5px 0; font-weight: bold;">Speed Legend</p>
        <p style="margin: 3px 0;"><span style="color: green;">●</span> 0 - {max_speed*0.2:.1f} kn</p>
        <p style="margin: 3px 0;"><span style="color: lightgreen;">●</span> {max_speed*0.2:.1f} - {max_speed*0.5:.1f} kn</p>
        <p style="margin: 3px 0;"><span style="color: orange;">●</span> {max_speed*0.5:.1f} - {max_speed*0.7:.1f} kn</p>
        <p style="margin: 3px 0;"><span style="color: red;">●</span> {max_speed*0.7:.1f} - {max_speed:.1f} kn</p>
        <hr style="margin: 5px 0;">
        <p style="margin: 3px 0; font-size: 10px;">Total Points: {len(df)}</p>
        <p style="margin: 3px 0; font-size: 10px;">Distance: {df['cumulative_m'].iloc[-1]:.1f} m</p>
        <p style="margin: 3px 0; font-size: 10px;">Satellites: {df['satellites'].mean():.1f} avg</p>
    </div>
    '''
    m.get_root().html.add_child(folium.Element(legend_html))
    
    # Save map
    m.save(output_file)
    print(f"Interactive map saved to: {output_file}")
    print(f"Open {output_file} in a web browser to view the interactive map")

def plot_gps_data(csv_file):
    """Load and plot GPS data from CSV file."""
    
    # Check if file exists
    if not Path(csv_file).exists():
        print(f"Error: File '{csv_file}' not found!")
        print("Download the GPS log from http://esp32_gps.local using the 'Download CSV' button")
        sys.exit(1)
    
    # Load CSV data
    print(f"Loading GPS data from {csv_file}...")
    df = pd.read_csv(csv_file)
    
    if df.empty:
        print("Error: CSV file is empty!")
        sys.exit(1)
    
    # Convert numeric columns (handle any non-numeric values)
    numeric_cols = ['timestamp_ms', 'latitude', 'longitude', 'altitude_m', 
                   'fix_quality', 'satellites', 'hdop', 'speed_kn', 'heading_deg']
    
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')
    
    # Remove rows with invalid data (NaN in critical columns)
    initial_count = len(df)
    df = df.dropna(subset=['latitude', 'longitude'])
    removed_count = initial_count - len(df)
    
    if removed_count > 0:
        print(f"Warning: Removed {removed_count} rows with invalid data")
    
    if df.empty:
        print("Error: No valid GPS data after filtering!")
        sys.exit(1)
    
    print(f"Loaded {len(df)} valid GPS points")
    print(f"Fix quality range: {df['fix_quality'].min():.0f} to {df['fix_quality'].max():.0f}")
    print(f"Satellite count: {df['satellites'].min():.1f} to {df['satellites'].max():.1f}")
    
    # Fill NaN values with defaults for plotting
    df = df.fillna({
        'altitude_m': 0,
        'fix_quality': 0,
        'satellites': 0,
        'hdop': 99.9,
        'speed_kn': 0,
        'heading_deg': 0
    })
    
    # Calculate elapsed time in seconds
    if 'timestamp_ms' in df.columns:
        df['time_sec'] = (df['timestamp_ms'] - df['timestamp_ms'].iloc[0]) / 1000.0
    else:
        # If no timestamp, create synthetic time based on row index (assume 10Hz)
        df['time_sec'] = np.arange(len(df)) * 0.1
        print("Warning: No timestamp column found, using synthetic timing at 10Hz")
    
    # Calculate distance traveled
    distances = [0]
    for i in range(1, len(df)):
        d = haversine_distance(
            df['latitude'].iloc[i-1], df['longitude'].iloc[i-1],
            df['latitude'].iloc[i], df['longitude'].iloc[i]
        )
        distances.append(d)
    df['distance_m'] = distances
    df['cumulative_m'] = df['distance_m'].cumsum()
    
    # Filter out stationary points (< 0.1m movement)
    moving_mask = df['distance_m'] > 0.1
    
    # Create figure with subplots
    fig = plt.figure(figsize=(16, 10))
    fig.suptitle('GPS Module Data Analysis', fontsize=16, fontweight='bold')
    
    # 1. GPS Track Plot
    ax1 = plt.subplot(2, 3, (1, 4))
    
    # Color by speed if available
    if df['speed_kn'].max() > 0:
        scatter = ax1.scatter(df['longitude'], df['latitude'], 
                            c=df['speed_kn'], cmap='viridis', 
                            s=20, alpha=0.6, edgecolors='none')
        plt.colorbar(scatter, ax=ax1, label='Speed (knots)')
    else:
        ax1.plot(df['longitude'], df['latitude'], 
                'b-', linewidth=1, alpha=0.6)
        ax1.scatter(df['longitude'], df['latitude'], 
                   c='blue', s=10, alpha=0.5)
    
    # Mark start and end points
    ax1.plot(df['longitude'].iloc[0], df['latitude'].iloc[0], 
            'go', markersize=12, label='Start', markeredgecolor='black', markeredgewidth=1.5)
    ax1.plot(df['longitude'].iloc[-1], df['latitude'].iloc[-1], 
            'r^', markersize=12, label='End', markeredgecolor='black', markeredgewidth=1.5)
    
    ax1.set_xlabel('Longitude', fontweight='bold')
    ax1.set_ylabel('Latitude', fontweight='bold')
    ax1.set_title('GPS Track', fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_aspect('equal', adjustable='datalim')
    
    # Calculate and display statistics
    center_lat = df['latitude'].mean()
    center_lon = df['longitude'].mean()
    lat_range = df['latitude'].max() - df['latitude'].min()
    lon_range = df['longitude'].max() - df['longitude'].min()
    
    stats_text = f"Center: {center_lat:.6f}°, {center_lon:.6f}°\n"
    stats_text += f"Range: {lat_range*111000:.1f}m × {lon_range*111000*np.cos(np.radians(center_lat)):.1f}m\n"
    stats_text += f"Total distance: {df['cumulative_m'].iloc[-1]:.1f}m"
    
    ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes,
            verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8),
            fontsize=9, family='monospace')
    
    # 2. Satellite Count Over Time
    ax2 = plt.subplot(2, 3, 2)
    ax2.plot(df['time_sec'], df['satellites'], 'g-', linewidth=1.5)
    ax2.fill_between(df['time_sec'], df['satellites'], alpha=0.3, color='green')
    ax2.set_xlabel('Time (seconds)', fontweight='bold')
    ax2.set_ylabel('Satellites', fontweight='bold')
    ax2.set_title('Satellite Count', fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(bottom=0)
    
    # Add average line
    avg_sats = df['satellites'].mean()
    ax2.axhline(y=avg_sats, color='r', linestyle='--', linewidth=1, 
               label=f'Avg: {avg_sats:.1f}')
    ax2.legend()
    
    # 3. HDOP Over Time
    ax3 = plt.subplot(2, 3, 3)
    ax3.plot(df['time_sec'], df['hdop'], 'orange', linewidth=1.5)
    ax3.fill_between(df['time_sec'], df['hdop'], alpha=0.3, color='orange')
    ax3.set_xlabel('Time (seconds)', fontweight='bold')
    ax3.set_ylabel('HDOP', fontweight='bold')
    ax3.set_title('Horizontal Dilution of Precision', fontweight='bold')
    ax3.grid(True, alpha=0.3)
    ax3.set_ylim(bottom=0)
    
    # Add quality indicators
    ax3.axhline(y=1, color='green', linestyle='--', linewidth=0.8, alpha=0.5, label='Excellent')
    ax3.axhline(y=2, color='yellow', linestyle='--', linewidth=0.8, alpha=0.5, label='Good')
    ax3.axhline(y=5, color='red', linestyle='--', linewidth=0.8, alpha=0.5, label='Moderate')
    ax3.legend(fontsize=8)
    
    # 4. Speed Over Time
    ax4 = plt.subplot(2, 3, 5)
    speed_ms = df['speed_kn'] * 0.5144  # Convert knots to m/s
    ax4.plot(df['time_sec'], speed_ms, 'purple', linewidth=1.5)
    ax4.fill_between(df['time_sec'], speed_ms, alpha=0.3, color='purple')
    ax4.set_xlabel('Time (seconds)', fontweight='bold')
    ax4.set_ylabel('Speed (m/s)', fontweight='bold')
    ax4.set_title('Speed Over Time', fontweight='bold')
    ax4.grid(True, alpha=0.3)
    ax4.set_ylim(bottom=0)
    
    # Add average speed (only for moving points)
    if moving_mask.sum() > 0:
        avg_speed = speed_ms[moving_mask].mean()
        ax4.axhline(y=avg_speed, color='r', linestyle='--', linewidth=1,
                   label=f'Avg (moving): {avg_speed:.2f} m/s')
        ax4.legend()
    
    # 5. Altitude Over Time
    ax5 = plt.subplot(2, 3, 6)
    ax5.plot(df['time_sec'], df['altitude_m'], 'brown', linewidth=1.5)
    ax5.fill_between(df['time_sec'], df['altitude_m'], alpha=0.3, color='brown')
    ax5.set_xlabel('Time (seconds)', fontweight='bold')
    ax5.set_ylabel('Altitude (m)', fontweight='bold')
    ax5.set_title('Altitude Profile', fontweight='bold')
    ax5.grid(True, alpha=0.3)
    
    # Add reference line
    avg_alt = df['altitude_m'].mean()
    ax5.axhline(y=avg_alt, color='r', linestyle='--', linewidth=1,
               label=f'Avg: {avg_alt:.1f}m')
    ax5.legend()
    
    plt.tight_layout()
    
    # Save plot
    output_file = csv_file.replace('.csv', '_plot.png')
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nPlot saved to: {output_file}")
    
    # Create interactive map
    map_output = csv_file.replace('.csv', '_map.html')
    create_interactive_map(df, map_output)
    
    # Display statistics
    print("\n" + "="*60)
    print("GPS DATA SUMMARY")
    print("="*60)
    duration = df['time_sec'].iloc[-1] if len(df) > 0 else 0
    print(f"Duration:          {duration:.1f} seconds ({duration/60:.1f} minutes)")
    print(f"Total points:      {len(df)}")
    if duration > 0:
        print(f"Update rate:       {len(df)/duration:.1f} Hz")
    print(f"\nPosition:")
    print(f"  Latitude:        {df['latitude'].min():.6f}° to {df['latitude'].max():.6f}°")
    print(f"  Longitude:       {df['longitude'].min():.6f}° to {df['longitude'].max():.6f}°")
    print(f"  Altitude:        {df['altitude_m'].min():.1f}m to {df['altitude_m'].max():.1f}m (avg: {avg_alt:.1f}m)")
    print(f"\nDistance:")
    print(f"  Total traveled:  {df['cumulative_m'].iloc[-1]:.1f} meters")
    straight_dist = haversine_distance(df['latitude'].iloc[0], df['longitude'].iloc[0], 
                                      df['latitude'].iloc[-1], df['longitude'].iloc[-1])
    print(f"  Straight line:   {straight_dist:.1f} meters")
    print(f"\nSpeed:")
    max_speed = df['speed_kn'].max()
    print(f"  Max:             {max_speed:.2f} knots ({max_speed*0.5144:.2f} m/s)")
    if moving_mask.sum() > 0:
        avg_speed_moving = df['speed_kn'][moving_mask].mean()
        print(f"  Avg (moving):    {avg_speed_moving:.2f} knots ({avg_speed_moving*0.5144:.2f} m/s)")
    print(f"\nFix Quality:")
    fix_mode = df['fix_quality'].mode()
    if len(fix_mode) > 0:
        print(f"  Quality:         {fix_mode[0]:.0f} (mode)")
    print(f"  Satellites:      {df['satellites'].min():.0f} to {df['satellites'].max():.0f} (avg: {avg_sats:.1f})")
    print(f"  HDOP:            {df['hdop'].min():.2f} to {df['hdop'].max():.2f} (avg: {df['hdop'].mean():.2f})")
    print("="*60)
    
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        csv_file = sys.argv[1]
    else:
        csv_file = "gpslog.csv"
    
    print("="*60)
    print("ESP32 GPS Data Plotter")
    print("="*60)
    
    plot_gps_data(csv_file)
