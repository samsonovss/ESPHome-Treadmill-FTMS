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
