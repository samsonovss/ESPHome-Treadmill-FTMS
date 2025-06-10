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
