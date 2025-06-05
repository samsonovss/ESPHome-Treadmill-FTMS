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
  ```
- The script processes elevation data and generates `incline_data.h`, containing treadmill incline values from **0 to 150** (`0 = 0%`, `150 = 15%`). These values are prepared for **UART control**.

- Since the treadmill incline system **maps a real-world incline of 5% to a treadmill incline setting of 15%**, the script **multiplies the real incline by 30** to scale it properly. This ensures that elevation changes are converted accurately for the treadmill’s incline mechanism.

- **Important:** You should **measure the actual incline of your treadmill** and adjust the coefficient if it differs from this scaling. The default conversion assumes **5% real incline = 15% treadmill incline**, but your treadmill may use a different ratio.


### **4. Integrate with ESPHome**
- Add the incline_data.h array to the global section in ESPHome.
```yaml
  - id: incline_profile_map_your_name
    type: float[58]
    restore_value: no
    initial_value: "{30, 0, 0, 60, 0, 0, 0, 70, 150, 150, 150, 80, 70, 10, 70, 20, 50, 10, 0, 20, 50, 80, 30, 0, 0, 0, 0, 0, 0, 20, 10, 0, 50, 0, 0, 30, 60, 0, 0, 0, 0, 30, 100, 0, 0, 0, 0, 0, 0, 50, 60, 0, 0, 0, 0, 100, 30, 0}"
```
- Use it in the ESPHome script:
```- id: update_incline_by_map```
- Include it in the select component to choose a map profile
```- id: map_select```

Notes
- Ensure GPX files include waypoints with both distance and elevation data for accurate calculations.
- The incline values are adjusted to match treadmill behavior and ESPHome communication.
