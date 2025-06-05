import gpxpy

# Загрузка GPX файла
with open('you_name_file', 'r', encoding='utf-8-sig') as gpx_file:
    gpx = gpxpy.parse(gpx_file)

# Извлечение точек из <wpt>
wpt_points = [wpt for wpt in gpx.waypoints if wpt.name and "distance" in wpt.name]
wpt_distances = [float(wpt.name.split()[0]) for wpt in wpt_points]
wpt_altitudes = [wpt.elevation for wpt in wpt_points]

# Расчет наклона с отладкой
inclines = []
for i in range(len(wpt_altitudes) - 1):
    height_diff = wpt_altitudes[i + 1] - wpt_altitudes[i]
    distance = wpt_distances[i + 1] - wpt_distances[i]
    incline = (height_diff / distance) * 100
    inclines.append(round(incline, 2))
    print(f"Интервал {wpt_distances[i]}-{wpt_distances[i+1]} м: перепад {height_diff} м, наклон {incline}%")
    if abs(incline) > 5:
        print(f"Аномалия на {wpt_distances[i]} м: перепад {height_diff} м, наклон {incline}%")

# Преобразование в единицы беговой дорожки
final_inclines = []
for i, incline in enumerate(inclines):
    treadmill_value = max(0, round(incline * 30))
    treadmill_value_orig = treadmill_value
    treadmill_value = (treadmill_value // 10) * 10
    if treadmill_value > 150:
        treadmill_value = 150
    final_inclines.append(treadmill_value)
    if treadmill_value_orig != treadmill_value:
        print(f"Интервал {wpt_distances[i]} м: исходное {treadmill_value_orig}, после ограничения {treadmill_value}")

# Создание файла incline_data.h
with open('incline_data.h', 'w', encoding='utf-8') as f:
    f.write('#ifndef INCLINE_DATA_H\n')
    f.write('#define INCLINE_DATA_H\n\n')
    f.write('float incline_profile_map1[{}] = {{\n'.format(len(final_inclines)))
    for i in range(0, len(final_inclines), 10):
        line = final_inclines[i:i+10]
        f.write('    ' + ', '.join(map(str, line)) + ',\n')
    f.write('};\n\n')
    f.write('const int map1_length = {};\n'.format(len(final_inclines)))
    f.write('#endif')

print(f"Generated incline_data.h with {len(final_inclines)} points for {len(final_inclines)} intervals")
