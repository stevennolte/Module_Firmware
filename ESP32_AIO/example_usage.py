#!/usr/bin/env python3
"""
Simple example script for parsing GPS UDP data from pcapng files
This script demonstrates basic usage of the GPS UDP parser
"""

import os
import sys
from gps_pcap_parser import GPSUDPParser

def example_usage():
    """Example of how to use the GPS UDP Parser"""
    
    # Example pcap file (you'll need to provide your own)
    pcap_file = "aio rtk test 20251013.pcapng"
    
    if not os.path.exists(pcap_file):
        print(f"Example pcap file '{pcap_file}' not found.")
        print("Please capture some UDP GPS data with Wireshark and save as pcapng format.")
        print("\nTo capture GPS data:")
        print("1. Start Wireshark")
        print("2. Capture on your network interface")
        print("3. Use filter: udp and (port 2233 or port 2017 or port 10110)")
        print("4. Let your ESP32-AIO system run and send GPS/PANDA data")
        print("5. Save capture as .pcapng file")
        return
    
    print("=== GPS UDP Parser Example ===")
    print(f"Parsing file: {pcap_file}")
    
    # Initialize parser
    # You can filter by specific port or IP if needed
    parser = GPSUDPParser(pcap_file, target_port=2233)  # Common GPS port
    
    # Parse the file
    parser.parse_with_pyshark()
    
    # Generate and print report
    report = parser.generate_report()
    print(report)
    
    # Save detailed analysis
    parser.save_data("gps_analysis.json")
    
    # Print some specific findings
    print("\n=== DETAILED FINDINGS ===")
    
    if parser.nmea_sentences:
        print(f"\nFound {len(parser.nmea_sentences)} NMEA sentences:")
        gga_sentences = [s for s in parser.nmea_sentences if s.get('type') == 'GGA' and s.get('valid')]
        if gga_sentences:
            print(f"  - {len(gga_sentences)} GGA (position) sentences")
            latest_gga = gga_sentences[-1]
            if 'latitude' in latest_gga and 'longitude' in latest_gga:
                print(f"  - Latest position: {latest_gga['latitude']} {latest_gga['lat_direction']}, "
                      f"{latest_gga['longitude']} {latest_gga['lon_direction']}")
        
        vtg_sentences = [s for s in parser.nmea_sentences if s.get('type') == 'VTG' and s.get('valid')]
        if vtg_sentences:
            print(f"  - {len(vtg_sentences)} VTG (velocity) sentences")
    
    if parser.panda_messages:
        print(f"\nFound {len(parser.panda_messages)} PANDA messages:")
        valid_pandas = [p for p in parser.panda_messages if p.get('valid')]
        if valid_pandas:
            latest_panda = valid_pandas[-1]
            print(f"  - Latest PANDA: Lat={latest_panda['latitude']:.6f}, "
                  f"Lon={latest_panda['longitude']:.6f}, Heading={latest_panda['heading']:.2f}°")
    
    # Show network traffic summary
    if parser.packets:
        print(f"\nNetwork Traffic Summary:")
        print(f"  - Total GPS UDP packets: {len(parser.packets)}")
        
        # Show unique IP addresses
        src_ips = set(p['src_ip'] for p in parser.packets)
        dst_ips = set(p['dst_ip'] for p in parser.packets)
        print(f"  - Source IPs: {', '.join(src_ips)}")
        print(f"  - Destination IPs: {', '.join(dst_ips)}")
        
        # Show port usage
        ports = set(f"{p['src_port']}->{p['dst_port']}" for p in parser.packets)
        print(f"  - Port pairs: {', '.join(list(ports)[:5])}")

def create_test_data():
    """Create a simple test data file for demonstration"""
    test_data = """#!/usr/bin/env python3
# Test data generator for GPS UDP parser
# This creates sample NMEA and PANDA data for testing

sample_nmea_sentences = [
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",
    "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48",
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
]

sample_panda_messages = [
    "48.07038,11.31000,84.4,0.1,-0.2,0.0",
    "48.07039,11.31001,84.5,0.2,-0.1,0.1",
    "48.07040,11.31002,84.6,0.0,-0.3,0.0"
]

print("Sample NMEA sentences:")
for sentence in sample_nmea_sentences:
    print(f"  {sentence}")

print("\\nSample PANDA messages:")
for message in sample_panda_messages:
    print(f"  {message}")
"""
    
    with open("test_gps_data.py", "w") as f:
        f.write(test_data)
    
    print("Created test_gps_data.py with sample GPS data formats")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--create-test":
        create_test_data()
    else:
        example_usage()