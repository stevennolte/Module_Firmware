#!/usr/bin/env python3
"""
GPS UDP Data Parser for Wireshark PCAPNG Files

This script parses Wireshark pcapng files to extract and analyze GPS UDP data,
including NMEA sentences and PANDA messages from ESP32-AIO agricultural controller.

Requirements:
    pip install pyshark scapy

Usage:
    python gps_pcap_parser.py capture.pcapng [--port PORT] [--ip IP] [--output output.txt]

Author: ESP32-AIO Project
Date: October 2025
"""

import argparse
import sys
import re
from datetime import datetime
from typing import List, Dict, Optional, Tuple
import json

try:
    import pyshark
except ImportError:
    print("Error: pyshark module not found. Install with: pip install pyshark")
    sys.exit(1)

try:
    from scapy.all import rdpcap, UDP, IP
    from scapy.layers.inet import UDP, IP
except ImportError:
    print("Error: scapy module not found. Install with: pip install scapy")
    sys.exit(1)


class GPSUDPParser:
    """Parser for GPS UDP data from pcapng files"""
    
    def __init__(self, pcap_file: str, target_port: int = None, target_ip: str = None):
        """
        Initialize the GPS UDP parser
        
        Args:
            pcap_file: Path to the pcapng file
            target_port: Specific UDP port to filter (optional)
            target_ip: Specific IP address to filter (optional)
        """
        self.pcap_file = pcap_file
        self.target_port = target_port
        self.target_ip = target_ip
        self.packets = []
        self.nmea_sentences = []
        self.panda_messages = []
        self.statistics = {
            'total_packets': 0,
            'udp_packets': 0,
            'gps_packets': 0,
            'nmea_sentences': 0,
            'panda_messages': 0,
            'parse_errors': 0
        }
    
    def is_nmea_sentence(self, data: str) -> bool:
        """Check if data contains a valid NMEA sentence"""
        # NMEA sentences start with $ and end with *checksum
        nmea_pattern = r'\$[A-Z]{2}[A-Z0-9]{3},[^*]*\*[0-9A-F]{2}'
        return bool(re.search(nmea_pattern, data))
    
    def is_panda_message(self, data: str) -> bool:
        """Check if data contains a PANDA message for AgOpenGPS"""
        # PANDA messages typically start with specific format
        # Based on your implementation: latitude,longitude,heading,roll,pitch,yaw
        panda_pattern = r'^-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+'
        return bool(re.match(panda_pattern, data.strip()))
    
    def parse_nmea_sentence(self, sentence: str) -> Dict:
        """Parse an NMEA sentence and extract information"""
        try:
            parts = sentence.strip().split(',')
            if len(parts) < 2:
                return None
                
            sentence_type = parts[0][3:]  # Remove the $ and first two characters
            
            parsed = {
                'type': sentence_type,
                'raw': sentence.strip(),
                'timestamp': datetime.now().isoformat(),
                'valid': True
            }
            
            # Parse specific sentence types
            if sentence_type == 'GGA':  # Global Positioning System Fix Data
                if len(parts) >= 15:
                    parsed.update({
                        'time': parts[1],
                        'latitude': parts[2],
                        'lat_direction': parts[3],
                        'longitude': parts[4],
                        'lon_direction': parts[5],
                        'quality': parts[6],
                        'satellites': parts[7],
                        'hdop': parts[8],
                        'altitude': parts[9],
                        'altitude_unit': parts[10]
                    })
            
            elif sentence_type == 'VTG':  # Track Made Good and Ground Speed
                if len(parts) >= 10:
                    parsed.update({
                        'true_track': parts[1],
                        'magnetic_track': parts[3],
                        'speed_knots': parts[5],
                        'speed_kmh': parts[7],
                        'mode': parts[9]
                    })
            
            elif sentence_type == 'RMC':  # Recommended Minimum Navigation Information
                if len(parts) >= 13:
                    parsed.update({
                        'time': parts[1],
                        'status': parts[2],
                        'latitude': parts[3],
                        'lat_direction': parts[4],
                        'longitude': parts[5],
                        'lon_direction': parts[6],
                        'speed': parts[7],
                        'course': parts[8],
                        'date': parts[9]
                    })
            
            return parsed
            
        except Exception as e:
            self.statistics['parse_errors'] += 1
            return {'error': str(e), 'raw': sentence, 'valid': False}
    
    def parse_panda_message(self, message: str) -> Dict:
        """Parse a PANDA message for AgOpenGPS"""
        try:
            parts = message.strip().split(',')
            if len(parts) >= 6:
                return {
                    'type': 'PANDA',
                    'latitude': float(parts[0]),
                    'longitude': float(parts[1]),
                    'heading': float(parts[2]),
                    'roll': float(parts[3]),
                    'pitch': float(parts[4]),
                    'yaw': float(parts[5]),
                    'raw': message.strip(),
                    'timestamp': datetime.now().isoformat(),
                    'valid': True
                }
            else:
                return {'error': 'Invalid PANDA format', 'raw': message, 'valid': False}
                
        except Exception as e:
            self.statistics['parse_errors'] += 1
            return {'error': str(e), 'raw': message, 'valid': False}
    
    def parse_with_pyshark(self) -> None:
        """Parse pcapng file using pyshark (more robust for complex captures)"""
        try:
            print(f"Opening pcapng file: {self.pcap_file}")
            cap = pyshark.FileCapture(self.pcap_file, display_filter='udp')
            
            for packet in cap:
                self.statistics['total_packets'] += 1
                
                if hasattr(packet, 'udp'):
                    self.statistics['udp_packets'] += 1
                    
                    # Apply filters
                    src_port = int(packet.udp.srcport)
                    dst_port = int(packet.udp.dstport)
                    src_ip = packet.ip.src if hasattr(packet, 'ip') else 'unknown'
                    dst_ip = packet.ip.dst if hasattr(packet, 'ip') else 'unknown'
                    
                    # Check if this matches our filter criteria
                    port_match = (self.target_port is None or 
                                src_port == self.target_port or 
                                dst_port == self.target_port)
                    
                    ip_match = (self.target_ip is None or 
                              src_ip == self.target_ip or 
                              dst_ip == self.target_ip)
                    
                    if port_match and ip_match:
                        # Extract payload
                        try:
                            if hasattr(packet.udp, 'payload'):
                                payload = bytes.fromhex(packet.udp.payload.replace(':', '')).decode('utf-8', errors='ignore')
                            else:
                                continue
                                
                        except Exception as e:
                            print(f"Error extracting payload: {e}")
                            continue
                        
                        packet_info = {
                            'timestamp': packet.sniff_time.isoformat() if hasattr(packet, 'sniff_time') else datetime.now().isoformat(),
                            'src_ip': src_ip,
                            'dst_ip': dst_ip,
                            'src_port': src_port,
                            'dst_port': dst_port,
                            'payload': payload.strip(),
                            'length': len(payload)
                        }
                        
                        self.packets.append(packet_info)
                        self.statistics['gps_packets'] += 1
                        
                        # Parse GPS data
                        self._parse_gps_data(payload, packet_info)
            
            cap.close()
            print(f"Processed {self.statistics['total_packets']} total packets")
            
        except Exception as e:
            print(f"Error parsing with pyshark: {e}")
            print("Falling back to scapy...")
            self.parse_with_scapy()
    
    def parse_with_scapy(self) -> None:
        """Parse pcapng file using scapy (fallback method)"""
        try:
            print(f"Opening pcapng file with scapy: {self.pcap_file}")
            packets = rdpcap(self.pcap_file)
            
            for packet in packets:
                self.statistics['total_packets'] += 1
                
                if packet.haslayer(UDP):
                    self.statistics['udp_packets'] += 1
                    
                    udp_layer = packet[UDP]
                    ip_layer = packet[IP] if packet.haslayer(IP) else None
                    
                    # Apply filters
                    port_match = (self.target_port is None or 
                                udp_layer.sport == self.target_port or 
                                udp_layer.dport == self.target_port)
                    
                    ip_match = (self.target_ip is None or 
                              (ip_layer and (ip_layer.src == self.target_ip or ip_layer.dst == self.target_ip)))
                    
                    if port_match and ip_match:
                        # Extract payload
                        try:
                            payload = bytes(udp_layer.payload).decode('utf-8', errors='ignore')
                        except Exception as e:
                            continue
                        
                        packet_info = {
                            'timestamp': datetime.now().isoformat(),
                            'src_ip': ip_layer.src if ip_layer else 'unknown',
                            'dst_ip': ip_layer.dst if ip_layer else 'unknown',
                            'src_port': udp_layer.sport,
                            'dst_port': udp_layer.dport,
                            'payload': payload.strip(),
                            'length': len(payload)
                        }
                        
                        self.packets.append(packet_info)
                        self.statistics['gps_packets'] += 1
                        
                        # Parse GPS data
                        self._parse_gps_data(payload, packet_info)
            
            print(f"Processed {self.statistics['total_packets']} total packets")
            
        except Exception as e:
            print(f"Error parsing with scapy: {e}")
            return
    
    def _parse_gps_data(self, payload: str, packet_info: Dict) -> None:
        """Parse GPS data from payload"""
        lines = payload.split('\n')
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # Check for NMEA sentences
            if self.is_nmea_sentence(line):
                parsed_nmea = self.parse_nmea_sentence(line)
                if parsed_nmea:
                    parsed_nmea['packet_info'] = packet_info
                    self.nmea_sentences.append(parsed_nmea)
                    self.statistics['nmea_sentences'] += 1
            
            # Check for PANDA messages
            elif self.is_panda_message(line):
                parsed_panda = self.parse_panda_message(line)
                if parsed_panda:
                    parsed_panda['packet_info'] = packet_info
                    self.panda_messages.append(parsed_panda)
                    self.statistics['panda_messages'] += 1
    
    def generate_report(self) -> str:
        """Generate a comprehensive analysis report"""
        report = []
        report.append("=" * 80)
        report.append("GPS UDP DATA ANALYSIS REPORT")
        report.append("=" * 80)
        report.append(f"File: {self.pcap_file}")
        report.append(f"Generated: {datetime.now().isoformat()}")
        report.append("")
        
        # Statistics
        report.append("STATISTICS:")
        report.append("-" * 40)
        for key, value in self.statistics.items():
            report.append(f"{key.replace('_', ' ').title()}: {value}")
        report.append("")
        
        # NMEA Sentence Analysis
        if self.nmea_sentences:
            report.append("NMEA SENTENCES ANALYSIS:")
            report.append("-" * 40)
            
            # Count by type
            nmea_types = {}
            for sentence in self.nmea_sentences:
                if sentence.get('valid', False):
                    sentence_type = sentence.get('type', 'UNKNOWN')
                    nmea_types[sentence_type] = nmea_types.get(sentence_type, 0) + 1
            
            for sentence_type, count in sorted(nmea_types.items()):
                report.append(f"{sentence_type}: {count} sentences")
            
            report.append("")
            report.append("Sample NMEA Sentences:")
            for i, sentence in enumerate(self.nmea_sentences[:5]):
                if sentence.get('valid', False):
                    report.append(f"  {i+1}. {sentence['type']}: {sentence['raw'][:80]}...")
            report.append("")
        
        # PANDA Message Analysis
        if self.panda_messages:
            report.append("PANDA MESSAGES ANALYSIS:")
            report.append("-" * 40)
            
            valid_pandas = [p for p in self.panda_messages if p.get('valid', False)]
            if valid_pandas:
                # Calculate ranges
                lats = [p['latitude'] for p in valid_pandas]
                lons = [p['longitude'] for p in valid_pandas]
                headings = [p['heading'] for p in valid_pandas]
                
                report.append(f"Latitude range: {min(lats):.6f} to {max(lats):.6f}")
                report.append(f"Longitude range: {min(lons):.6f} to {max(lons):.6f}")
                report.append(f"Heading range: {min(headings):.2f}° to {max(headings):.2f}°")
                report.append("")
                
                report.append("Sample PANDA Messages:")
                for i, panda in enumerate(valid_pandas[:5]):
                    report.append(f"  {i+1}. Lat: {panda['latitude']:.6f}, Lon: {panda['longitude']:.6f}, "
                                f"Heading: {panda['heading']:.2f}°")
                report.append("")
        
        # Network Traffic Analysis
        if self.packets:
            report.append("NETWORK TRAFFIC ANALYSIS:")
            report.append("-" * 40)
            
            # Count by IP
            src_ips = {}
            dst_ips = {}
            ports = {}
            
            for packet in self.packets:
                src_ip = packet['src_ip']
                dst_ip = packet['dst_ip']
                src_port = packet['src_port']
                dst_port = packet['dst_port']
                
                src_ips[src_ip] = src_ips.get(src_ip, 0) + 1
                dst_ips[dst_ip] = dst_ips.get(dst_ip, 0) + 1
                ports[f"{src_port}->{dst_port}"] = ports.get(f"{src_port}->{dst_port}", 0) + 1
            
            report.append("Source IPs:")
            for ip, count in sorted(src_ips.items(), key=lambda x: x[1], reverse=True)[:10]:
                report.append(f"  {ip}: {count} packets")
            
            report.append("\nDestination IPs:")
            for ip, count in sorted(dst_ips.items(), key=lambda x: x[1], reverse=True)[:10]:
                report.append(f"  {ip}: {count} packets")
            
            report.append("\nPort pairs (src->dst):")
            for port_pair, count in sorted(ports.items(), key=lambda x: x[1], reverse=True)[:10]:
                report.append(f"  {port_pair}: {count} packets")
            report.append("")
        
        return "\n".join(report)
    
    def save_data(self, output_file: str) -> None:
        """Save parsed data to JSON file"""
        data = {
            'statistics': self.statistics,
            'nmea_sentences': self.nmea_sentences,
            'panda_messages': self.panda_messages,
            'packets': self.packets[:100]  # Limit packet data to prevent huge files
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"Data saved to {output_file}")


def main():
    """Main function to run the GPS UDP parser"""
    parser = argparse.ArgumentParser(description='Parse GPS UDP data from Wireshark pcapng files')
    parser.add_argument('pcap_file', help='Path to the pcapng file')
    parser.add_argument('--port', type=int, help='Filter by specific UDP port')
    parser.add_argument('--ip', help='Filter by specific IP address')
    parser.add_argument('--output', help='Output file for detailed data (JSON format)')
    parser.add_argument('--report', help='Output file for analysis report (text format)')
    
    args = parser.parse_args()
    
    # Check if pcap file exists
    import os
    if not os.path.exists(args.pcap_file):
        print(f"Error: File {args.pcap_file} not found")
        return 1
    
    # Initialize parser
    gps_parser = GPSUDPParser(args.pcap_file, args.port, args.ip)
    
    # Parse the file
    print("Starting GPS UDP data parsing...")
    gps_parser.parse_with_pyshark()
    
    # Generate and display report
    report = gps_parser.generate_report()
    print(report)
    
    # Save report if requested
    if args.report:
        with open(args.report, 'w') as f:
            f.write(report)
        print(f"Report saved to {args.report}")
    
    # Save detailed data if requested
    if args.output:
        gps_parser.save_data(args.output)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())