"""
This script renumbers vehicle IDs in a SUMO route file sequentially from 0.
It reads 'input.rou.xml' and writes the output to 'output.rou.xml'.
Additionally, it can set all vehicles to depart at time 0 (commented out).
"""

import xml.etree.ElementTree as ET
import sys

input_file = "mmArea1net.rou.xml"       
output_file = "mmArea1net_ordered.rou.xml" 

try:
    print(f"Reading {input_file}...")
    tree = ET.parse(input_file)
    root = tree.getroot()

    count = 0
  
    for vehicle in root.findall('vehicle'):
        vehicle.set('id', str(count)) 
        vehicle.set('depart', '0') # Optional: set departure time to 0 for all
        count += 1

    print(f"Renumbered {count} vehicles.")
    
    # Save the new file
    tree.write(output_file, encoding="UTF-8", xml_declaration=True)
    print(f"Done! New file saved as: {output_file}")

except FileNotFoundError:
    print("Error: File not found. Check the input file name.")
except Exception as e:
    print(f"Error: {e}")