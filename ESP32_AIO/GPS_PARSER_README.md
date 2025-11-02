# GPS UDP Data Parser for Wireshark PCAPNG Files

This Python program parses Wireshark pcapng files to extract and analyze GPS UDP data, including NMEA sentences and PANDA messages from your ESP32-AIO agricultural controller.

## Features

- **NMEA Sentence Parsing**: Extracts and analyzes standard GPS NMEA sentences (GGA, VTG, RMC, etc.)
- **PANDA Message Analysis**: Parses AgOpenGPS PANDA format messages with position and orientation data
- **Network Traffic Analysis**: Analyzes UDP packet flows, IP addresses, and port usage
- **Comprehensive Reporting**: Generates detailed analysis reports with statistics
- **Flexible Filtering**: Filter by specific IP addresses or UDP ports
- **Multiple Output Formats**: Text reports and JSON data export

## Installation

1. **Install Python Requirements**:
   ```bash
   pip install -r requirements.txt
   ```

2. **Required Python Packages**:
   - `pyshark`: For robust pcapng file parsing
   - `scapy`: Fallback parser and packet manipulation

## Usage

### Basic Usage

```bash
python gps_pcap_parser.py capture.pcapng
```

### Advanced Usage with Filters

```bash
# Filter by specific UDP port (common GPS ports: 2017, 2233, 10110)
python gps_pcap_parser.py capture.pcapng --port 2233

# Filter by specific IP address
python gps_pcap_parser.py capture.pcapng --ip 192.168.1.100

# Generate detailed output files
python gps_pcap_parser.py capture.pcapng --report analysis_report.txt --output detailed_data.json

# Combine filters
python gps_pcap_parser.py capture.pcapng --port 2233 --ip 192.168.1.100 --report esp32_gps_analysis.txt
```

### Example Usage Script

```bash
python example_usage.py
```

## Capturing GPS Data with Wireshark

### Step-by-Step Capture Process

1. **Start Wireshark**
2. **Select Network Interface**: Choose the interface connected to your GPS network
3. **Apply Filter**: Use one of these filters:
   ```
   udp and (port 2233 or port 2017 or port 10110)
   udp and host 192.168.1.100
   ```
4. **Start Capture**: Begin capturing while your ESP32-AIO system is running
5. **Save as PCAPNG**: Save the capture in pcapng format for analysis

### Common GPS UDP Ports

- **2017**: NTRIP/RTK corrections
- **2233**: GPS position data
- **10110**: AgOpenGPS standard port
- **Custom ports**: Your ESP32-AIO may use different ports (check your configuration)

## Output Examples

### Console Report Output

```
================================================================================
GPS UDP DATA ANALYSIS REPORT
================================================================================
File: esp32_gps_capture.pcapng
Generated: 2025-10-19T10:30:00

STATISTICS:
----------------------------------------
Total Packets: 1250
Udp Packets: 890
Gps Packets: 456
Nmea Sentences: 234
Panda Messages: 89
Parse Errors: 2

NMEA SENTENCES ANALYSIS:
----------------------------------------
GGA: 78 sentences
VTG: 78 sentences
RMC: 78 sentences

Sample NMEA Sentences:
  1. GGA: $GPGGA,103000.00,4807.03823,N,01131.00028,E,1,08,0.9,545.4,M,46.9,M,,*47
  2. VTG: $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A*48
  3. RMC: $GPRMC,103000.00,A,4807.03823,N,01131.00028,E,022.4,084.4,191025,003.1,W*6A

PANDA MESSAGES ANALYSIS:
----------------------------------------
Latitude range: 48.070382 to 48.070389
Longitude range: 11.310002 to 11.310008
Heading range: 84.2° to 84.8°

Sample PANDA Messages:
  1. Lat: 48.070382, Lon: 11.310002, Heading: 84.20°
  2. Lat: 48.070385, Lon: 11.310005, Heading: 84.50°
  3. Lat: 48.070389, Lon: 11.310008, Heading: 84.80°

NETWORK TRAFFIC ANALYSIS:
----------------------------------------
Source IPs:
  192.168.1.100: 456 packets

Destination IPs:
  192.168.1.255: 456 packets

Port pairs (src->dst):
  2233->2233: 456 packets
```

### JSON Output Structure

```json
{
  "statistics": {
    "total_packets": 1250,
    "udp_packets": 890,
    "gps_packets": 456,
    "nmea_sentences": 234,
    "panda_messages": 89,
    "parse_errors": 2
  },
  "nmea_sentences": [
    {
      "type": "GGA",
      "raw": "$GPGGA,103000.00,4807.03823,N,01131.00028,E,1,08,0.9,545.4,M,46.9,M,,*47",
      "timestamp": "2025-10-19T10:30:00",
      "valid": true,
      "time": "103000.00",
      "latitude": "4807.03823",
      "lat_direction": "N",
      "longitude": "01131.00028",
      "lon_direction": "E",
      "quality": "1",
      "satellites": "08",
      "hdop": "0.9",
      "altitude": "545.4",
      "altitude_unit": "M"
    }
  ],
  "panda_messages": [
    {
      "type": "PANDA",
      "latitude": 48.070382,
      "longitude": 11.310002,
      "heading": 84.2,
      "roll": 0.1,
      "pitch": -0.2,
      "yaw": 0.0,
      "raw": "48.070382,11.310002,84.2,0.1,-0.2,0.0",
      "timestamp": "2025-10-19T10:30:00",
      "valid": true
    }
  ]
}
```

## Supported Data Formats

### NMEA Sentences

- **GGA**: Global Positioning System Fix Data
- **VTG**: Track Made Good and Ground Speed
- **RMC**: Recommended Minimum Navigation Information
- **GSA**: GPS DOP and active satellites
- **GSV**: GPS Satellites in view
- **Custom**: Any valid NMEA sentence format

### PANDA Messages

Format: `latitude,longitude,heading,roll,pitch,yaw`

Example: `48.070382,11.310002,84.2,0.1,-0.2,0.0`

## Troubleshooting

### Common Issues

1. **"pyshark module not found"**
   ```bash
   pip install pyshark
   ```

2. **"scapy module not found"**
   ```bash
   pip install scapy
   ```

3. **No GPS packets found**
   - Check if the correct ports are being used
   - Verify IP addresses in the capture
   - Try without filters first: `python gps_pcap_parser.py capture.pcapng`

4. **Permission errors on Windows**
   - Run command prompt as Administrator
   - Install WinPcap or Npcap for packet capture support

### Debug Tips

- Use Wireshark's built-in filters to verify your capture contains UDP data
- Check that your ESP32-AIO is actually sending UDP packets during capture
- Verify network connectivity between capture device and ESP32-AIO
- Use `--port` and `--ip` filters to narrow down the analysis

## Integration with ESP32-AIO Project

This parser is specifically designed to work with the ESP32-AIO agricultural controller:

- **NMEA Parsing**: Matches the GPS.cpp NMEA parsing implementation
- **PANDA Format**: Compatible with the buildPandaSentence() output format
- **UDP Ports**: Supports the standard ports used by ESP32-AIO (2017, 2233, 10110)
- **Network Analysis**: Helps debug UDP communication issues

### Typical ESP32-AIO UDP Flow

1. **GPS NMEA Input**: ESP32 receives NMEA from GPS module
2. **PANDA Generation**: ESP32 converts to PANDA format for AgOpenGPS
3. **UDP Broadcast**: PANDA messages sent via UDP to network
4. **Capture & Analysis**: Use this parser to analyze the UDP traffic

## Contributing

Feel free to extend this parser for additional GPS formats or analysis features. The modular design makes it easy to add new parsing capabilities.

## License

This tool is part of the ESP32-AIO agricultural controller project.