from flask import Flask, send_file

from PIL import Image, ImageDraw, ImageFont
from datetime import datetime, timedelta
from io import BytesIO
import locale

import yaml
import requests_cache
import calendar
import caldav
from collections import defaultdict
import textwrap

locale.setlocale(locale.LC_ALL, '') # set default locale from the system settings

#http_session = requests_cache.CachedSession('.cache', expire_after=3600, backend='filesystem')
import requests
import numpy as np
from scipy.interpolate import make_interp_spline
http_session = requests
# DAVClient has not support for caching, so enabled it globally...
# TODO: Looks like it does not work...
requests_cache.install_cache('.cache', expire_after=3600, backend='filesystem')

# TODO: Set german locale (either here, or in some env file?)

# Load config from config.yaml
with open('config.yaml', 'r') as file:
    config = yaml.safe_load(file)

# Load WMO to weather icon mapping
with open('weather-icons/wmo-mapping.yaml', 'r') as file:
    wmo_to_icon_name = yaml.safe_load(file)

app = Flask(__name__)

# load fonts
font_xsmall = ImageFont.truetype("segoeui.ttf", 10)
font_small = ImageFont.truetype("segoeui.ttf", 18)
font_small_semibold = ImageFont.truetype("seguisb.ttf", 18)
font_large = ImageFont.truetype("segoeui.ttf", 33)
font_xlarge = ImageFont.truetype("segoeui.ttf", 50)

weather_data = None
def fetch_weather_data():
    print("Fetching weather data...")
    global weather_data
    # Fetch weather data from OpenMeto API
    lat = config['open_meteo']['lat']
    lon = config['open_meteo']['lon']
    today = datetime.now().strftime("%Y-%m-%d")
    url = f"https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&daily=temperature_2m_max,temperature_2m_min,weather_code&hourly=weather_code,temperature_2m,rain&timezone=Europe%2FBerlin&start_date={today}&end_date={today}"
    response = http_session.get(url)
    weather_data = response.json()

fetch_weather_data()

def fetch_upcoming_events(max_events=3):
    """
    Fetch upcoming events from CalDav calendar.
    Returns a dict calendar name -> lists of (start_datetime, summary) tuples.
    """
    # TODO: Add some logic to limits the number of events per calendar. We can show at most 11-12 lines, including the calendar name.
    print("Fetching upcoming events...")
    events = []
    url = config['caldav']['url']
    username = config['caldav']['username']
    password = config['caldav']['password']
    client = caldav.DAVClient(url, username=username, password=password)
    principal = client.principal()
    calendars = principal.calendars()
    if not calendars:
        return {}
    now = datetime.now()
    future = now + timedelta(days=7)
    events = defaultdict(list)
    for calendar in calendars:
        if not calendar.name in config['caldav']['calendars']:
            continue
        results = calendar.search(start=now, end=future, event=True, expand=True, sort_keys="dtstart")
        for event in results:
            events[calendar.name].append(event.instance.vevent)
            if len(events) >= max_events:
                break
    return events

events = fetch_upcoming_events(3)

@app.route('/frame.png')
def frame_image():
    # Create a blank image
    img = Image.new("1", (800, 480), color=1) # 1bpp mode, white background
    draw = ImageDraw.Draw(img)

    # Draw separators
    #draw.line((400, 0, 400, 480))
    #draw.line((0, 280, 800, 280))

    # Draw date/time
    now = datetime.now()
    draw.text((200, 20), now.strftime("%d. %b %Y"), font=font_large, anchor="ma")
    
    # Current day weather
    assert weather_data
    draw.text((170, 70), f"{weather_data['daily']['temperature_2m_max'][0]:.1f} °C", font=font_xlarge) # max
    draw.text((170, 130), f"{weather_data['daily']['temperature_2m_min'][0]:.1f} °C", font=font_large) # min
    icon_name = wmo_to_icon_name.get(weather_data['daily']['weather_code'][0], 'na')
    draw.bitmap((30, 50), Image.open(f"weather-icons/wi-{icon_name}_128.png")) # weather icon

    draw.text((50+0*80, 240), "08:00", font=font_small)
    icon_name = wmo_to_icon_name.get(weather_data['hourly']['weather_code'][8], 'na')
    draw.bitmap((40+0*80, 180), Image.open(f"weather-icons/wi-{icon_name}_64.png")) # weather icon
    draw.text((50+1*80, 240), "12:00", font=font_small)
    icon_name = wmo_to_icon_name.get(weather_data['hourly']['weather_code'][12], 'na')
    draw.bitmap((40+1*80, 180), Image.open(f"weather-icons/wi-{icon_name}_64.png")) # weather icon
    draw.text((50+2*80, 240), "16:00", font=font_small)
    icon_name = wmo_to_icon_name.get(weather_data['hourly']['weather_code'][16], 'na')
    draw.bitmap((40+2*80, 180), Image.open(f"weather-icons/wi-{icon_name}_64.png")) # weather icon
    draw.text((50+3*80, 240), "20:00", font=font_small)
    icon_name = wmo_to_icon_name.get(weather_data['hourly']['weather_code'][20], 'na')
    draw.bitmap((40+3*80, 180), Image.open(f"weather-icons/wi-{icon_name}_64.png")) # weather icon

    # Temperature chart
    draw.line((30, 450, 360, 450)) # x-axis
    draw.line((30, 450, 30, 300)) # y-axis
    for i in range(24): # x-axis ticks
        x = 30 + i * (360-30) / 24
        draw.line((x, 450, x, 455)) 
        if i in [0, 12, 23]:
            draw.text((x, 465), f"{i:02d}:00", font=font_xsmall, anchor="ma")
    draw.text((20, 300), "°C", font=font_xsmall, anchor="rm")
    for i in range(-10, 30, 5): # y-axis ticks
        y = 450 - (i + 10) * (450-300) / 40
        draw.line((25, y, 30, y))
        if i in [-10, 0, 20]:
            draw.text((20, y), f"{i}", font=font_xsmall, anchor="rm")
    # chart data
    # Prepare data points
    x = np.array([30 + i * (360-30) / 24 for i in range(24)])
    y = np.array([450 - (d + 10) * (450-300) / 40 for d in weather_data['hourly']['temperature_2m']])

    # Spline interpolation for smooth curve
    x_new = np.linspace(x.min(), x.max(), 200)
    spline = make_interp_spline(x, y, k=3)
    y_smooth = spline(x_new)

    # Draw smooth line
    points = list(zip(x_new, y_smooth))
    draw.line(points, fill=0, width=2)

    # Draw circles at original data points
    # for px, py in zip(x, y):
    #     draw.circle((px, py), 2, fill=0)

    # Draw checkerboard pattern under the curve
    checker_size = 1
    for i in range(len(x_new) - 1):
        x_start = int(x_new[i])
        x_end = int(x_new[i + 1])
        y_top = int(y_smooth[i])
        y_bottom = 450  # bottom of chart area
        for x_pos in range(x_start, x_end):
            for y_pos in range(y_top, y_bottom):
                if ((x_pos // checker_size) + (y_pos // checker_size)) % 2 == 0:
                    img.putpixel((x_pos, y_pos), 0)

    # Calendar
    cal_pos = (450, 300)
    cal_spacing = (30, 25)
    cal = calendar.Calendar()
    for di,day in enumerate(["Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"]):
        draw.text((cal_pos[0] + di * cal_spacing[0], cal_pos[1]), day, font=font_small_semibold, anchor="mm")
    for wi,week in enumerate(cal.monthdayscalendar(now.year, now.month)):
        for di,day in enumerate(week):
            color = 0
            if day == now.day:
                draw.circle((cal_pos[0] + di * cal_spacing[0], cal_pos[1] + (wi+1) * cal_spacing[1] + 1), 15, fill=0)
                color = 1
            draw.text((cal_pos[0] + di * cal_spacing[0], cal_pos[1] + (wi+1) * cal_spacing[1]), str(day) if day != 0 else '', font=font_small, anchor="mm", fill=color)
    draw.text((720, 350), now.strftime("%b"), font=font_xlarge, anchor="mm")
    draw.text((720, 400), now.strftime("%Y"), font=font_large, anchor="mm")

    # Agenda
    agenda_pos = (420, 20)
    agenda_spacing = 25
    ai = 0
    for cal_name in config['caldav']['calendars']: # we don't use events.values() here because we want to keep the order from the config file
        cal_events = events.get(cal_name, [])
        draw.text((agenda_pos[0], agenda_pos[1] + ai * agenda_spacing), cal_name, font=font_small_semibold, anchor="la")
        ai += 1
        for event in cal_events:
            summary_str = textwrap.shorten(event.summary.value, width=28, placeholder="...")
            # Draw the event details separately to align the columns
            draw.text((agenda_pos[0], agenda_pos[1] + ai * agenda_spacing), event.dtstart.value.strftime("%a %d.%m"), font=font_small, anchor="la")
            draw.text((agenda_pos[0] + 80, agenda_pos[1] + ai * agenda_spacing), event.dtstart.value.strftime("%H:%M"), font=font_small, anchor="la")
            draw.text((agenda_pos[0] + 130, agenda_pos[1] + ai * agenda_spacing), summary_str, font=font_small, anchor="la")
            ai += 1

    # Timestamp
    draw.text((800-2, 480-2), f"Stand: {now.isoformat(timespec='seconds')}", font=font_xsmall, anchor="rb")

    # Output
    img_io = BytesIO()
    img.save(img_io, 'PNG', bits=1)
    img_io.seek(0)
    return send_file(img_io, mimetype='image/png')

if __name__ == '__main__':
    app.run(debug=True)