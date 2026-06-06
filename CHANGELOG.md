- January 21, 2026:  
  - Compatibility with **ESPHome 2026.1.0**
  
- December 17, 2025:
  - **Refactored FTMS implementation** to ensure full compatibility with **ESPHome 2025.12.0**
  - **Updated Adaptive Speed Correction algorithm** it can be enabled or disabled, and it takes into account user weight, belt friction, and dynamic load to maintain the target speed more accurately.
    - **Added three speed‑correction modes** for different running styles and treadmill behavior:
      - Soft – minimal intervention, smooth adjustments.
      - Precise – balanced correction for most users.
      - Aggressive – fast corrections (possibly better suited for interval training)
  - Added new acceleration profiles and improved speed‑control loop:
    - Smooth alignment of current speed (target_speed) to the goal.
    - Added detailed acceleration profiles:
      - Soft — ~0.5 km/h per second, very smooth, ideal for walking.
      - Normal — ~1.0 km/h per second, balanced universal mode.
      - Fast — ~1.5 km/h per second, suitable for interval training.
      - Technogym — ~0.8–1.0 km/h per second, emulates Technogym MyRun behavior.
  - Updated interval training algorithm:
    - Added the ability to configure:
      - Number of cycles.
      - Work phase duration (acceleration).
      - Recovery phase duration (deceleration).
    - Interval logic now works with heart‑rate zones for more personalized training intensity.
  - Added adjustable HR Control interval time — the user can now set how often the heart‑rate‑based speed‑control algorithm updates treadmill speed in HR‑based running mode. (5 seconds, 10 seconds, 30 seconds with a step of 1)

  - Reworked Pause/Resume logic: added dedicated buttons and improved state handling.
  - Added new sensors for Home Assistant, enabling detailed export of workout metrics and integration with dashboards.
  - Updated Nextion and ESPHome UI components: replaced switch elements with dual button widgets for more reliable event handling and error‑free operation.
  - Added average running pace output to logs and sensors for more complete workout analytics.
  - Expanded code comments to improve readability, maintainability, and developer understanding of internal logic.

- October 20, 2025:
  - Added automatic treadmill shutdown on motor overheating (>85°C), enabled via Nextion or ESPHome switch: triggers an alert page with overheat notification, stops the treadmill, and updates the status to "Overheat Emergency Stop" for improved safety.
  - Redesigned Nextion display interface and tabs for improved navigation and user convenience.
  - Added new Zwift running routes with auto-incline profiles: "Zwift 5K loop (5.0km)", "Zwift Mountain Mash (5.9km)", "Zwift Chili Pepper (8.0km)", "Zwift Hilly Route Reverse Run (9.2km)", "Zwift Ocean Blvd (11.2km)", "Zwift Beach Island Loop (12.8km)", and "Zwift Jon’s Route (12.6km)", which can be run autonomously without the Zwift app for enhanced virtual training options.

- August 29, 2025:  
  - Integrated a distance sensor (TOF400C‑VL53L1X) to expand treadmill control capabilities:  
    - **Free Run speed control** – adjust treadmill speed dynamically using proximity detection, without pressing buttons.  
    - **Safe zone operation** – automatically stops the treadmill if the user steps off, eliminating the need for a safety clip.  
    - **Automatic interactive calibration** – measures treadmill belt length and defines acceleration/deceleration zones for precise speed control in distance sensor mode.  
    - **Display power management** – automatically turns the display off after 1 minute of inactivity (no user detected on the treadmill) and powers it back on when the user returns.  
- June 11, 2025:
  - Added swipe gestures to the Nextion display for seamless navigation between tabs and screens, enhancing user interaction and interface fluidity.

- June 05, 2025:
  - Added elevation profiles for three running routes with auto-incline control, allowing users to select a route with or without a heart rate monitor for more personalized training sessions.
  - Integrated motor temperature sensor ds18b20 to monitor and prevent overheating, enhancing treadmill safety and longevity.
  - Implemented a 3-2-1 countdown display on the Nextion screen to improve the pre-workout user experience.
  - Added a new tab on the Nextion display for post-run summary information, providing a comprehensive overview of workout results alongside existing detailed logs.
  - Processed elevation data from GPS Visualizer ([https://www.gpsvisualizer.com/elevation](https://www.gpsvisualizer.com/convert_input)) with a 30-meter resolution, converting it into an array for ESPHome to enable accurate auto-incline adjustments based on route profiles.

- May 29, 2025:
  - Added calculation of burned fat (in grams) to Workout Summary log.

- May 27, 2025:
  - Added MET and VO2 calculations with support for interval training, enabling more accurate analysis of energy expenditure and aerobic capacity.
  - Implemented workout results display on the Nextion screen after completion, showing duration, distance, calories, average speed, incline, MET, VO2, heart rate, and time in heart rate zones.
  - Added detailed workout results logging in "Workout Summary" format, including user data (gender, age, weight), workout metrics (duration, distance, calories, speed, incline, MET, VO2), and heart rate zones.
  - Updated Nextion display interface with an improved design and added user age input field for more accurate kilocalorie calculations.
  - Reworked Nextion keyboard to prevent incorrect input, enhancing user interaction reliability.
    
- May 26, 2025:
  - Fixed automatic treadmill restart by QZ Fitness when incline control for Zwift is enabled, by implementing `manual_stop` flag in `stop_program` and blocking "Start" (0x07) commands in `ftms_control_point_char` when `manual_stop=true`. No issues in standard modes.
  - Added `reset_manual_stop` script to reset `manual_stop` flag after 5 seconds, allowing other FTMS apps (e.g., Zwift, Qdomyos-Zwift) to start the treadmill after a manual stop.
  - Prevented unwanted incline commands from QZ Fitness after stop in Zwift incline control mode by rejecting "Set Incline" (0x03) commands when `motor_running=false` in `ftms_control_point_char`.
  - Optimized `stop_program` sequence by sending FTMS notifications ("Stopped or Paused" and "Idle") and UART stop commands (`[SETSPD:000]`) before resetting `motor_running` and setting `manual_stop`, improving synchronization with QZ Fitness in Zwift incline control mode.
    
- April 12, 2025:
  - Added full FTMS support for Kinomap on iOS using shortened UUIDs.
  - Added support for FTMS statuses (Training Status / Fitness Machine Status).
    
- April 9, 2025: Initial FTMS support added for Kinomap (Android), FitShow, and Kinni.