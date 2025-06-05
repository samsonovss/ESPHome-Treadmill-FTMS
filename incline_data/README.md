# Incline Data  
This directory contains elevation-related data and processing scripts for treadmill incline mapping.  

## Structure  
- **`gpx_files/`** – Stores GPX files with elevation profiles.  
- **`scripts/`** – Includes Python scripts for converting elevation data into incline percentages.  
- **`README.md`** – Provides an overview of the directory contents and instructions.  

## How to Prepare Elevation Data for Treadmill Incline  

### **1. Create a Route**  
- Draw a route in **Google Maps**.  
- Save the file as **KML** (`your_file.kml`).  

### **2. Convert KML to GPX with Elevation Data**  
- Go to: [GPS Visualizer](https://www.gpsvisualizer.com/convert_input).  
- Set the following options:  
  - **Output format:** `GPX`  
  - **Add DEM elevation data:** `Best available source`  
  - **Add equal-interval tickmarks as waypoints:** `30 meters`  
- Convert the file. The result will be a **GPX file** with elevation data sampled every **30 meters**.  

### **3. Process the GPX File with a Script**  
- Place the GPX file inside `incline_data/gpx_files/`.  
- Run the script from `incline_data/scripts/`:  
  ```bash
  python gpx_to_treadmill_incline.py your_file.gpx
- The script processes elevation data and generates incline_data.h, containing treadmill incline values from 0 to 150 (0 = 0%, 150 = 150%). These values are prepared for UART control.

4. Integrate with ESPHome
- Add the incline_data.h array to the global section in ESPHome.
- Use it in the ESPHome script:
```- id: update_incline_by_map```
- Include it in the select component to choose a map profile
```- id: map_select```

Notes
- Ensure GPX files include waypoints with both distance and elevation data for accurate calculations.
- The incline values are adjusted to match treadmill behavior and ESPHome communication.
