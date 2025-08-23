from flask import Flask, send_file
import logging
logger = logging.getLogger()
logger.setLevel(logging.INFO)

from PIL import Image, ImageDraw, ImageFont
from datetime import datetime, timedelta, date, timezone
from io import BytesIO
import os
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

# Load config from config.yaml
with open('config/config.yaml', 'r') as file:
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

# last generated image
last_image = None
last_image_time = None
last_image_successfull = True
last_request_time = None

def fetch_weather_data():
    logger.info("Fetching weather data...")
    # Fetch weather data from OpenMeto API
    lat = config['open_meteo']['lat']
    lon = config['open_meteo']['lon']
    today = datetime.now().strftime("%Y-%m-%d")
    url = f"https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&daily=temperature_2m_max,temperature_2m_min,weather_code&hourly=weather_code,temperature_2m,rain&timezone=Europe%2FBerlin&start_date={today}&end_date={today}"
    response = http_session.get(url)
    return response.json()

def fetch_upcoming_events(max_lines=10):
    """
    Fetch upcoming events (next 7 days) from CalDav calendar.
    Returns a dict calendar name -> lists of (start_datetime, summary) tuples.
    All events, including the calendar name, are limited to a maximum of `max_lines` lines.
    """
    logger.info("Fetching upcoming events...")
    events = []
    url = config['caldav']['url']
    username = config['caldav']['username']
    password = config['caldav']['password']
    client = caldav.DAVClient(url, username=username, password=password)
    principal = client.principal()
    calendars = principal.calendars()
    if not calendars:
        return {}
    now = datetime.now(timezone.utc) # time must be in UTC otherwise full-day events from yesterday are found (see https://github.com/python-caldav/caldav/issues/351)
    future = now + timedelta(days=7)
    events = defaultdict(list)
    max_lines -= len(config['caldav']['calendars']) # reserve one line for each calendar name
    if max_lines <= 0:
        raise ValueError("max_lines must be greater than the number of calendars")
    for calendar in calendars:
        if not calendar.name in config['caldav']['calendars']:
            continue
        results = calendar.search(start=now, end=future, event=True, expand=True, sort_keys="dtstart")
        for event in results:
            events[calendar.name].append(event.instance.vevent)
            if len(events) >= max_lines: #each calendar could be the only one with events, so fetch the maximum here
                break
    # now limit the number of events per calendar
    # we remove from the calendar with the most events until it fits
    # there might be a smarter algorithm
    while sum(len(evs) for evs in events.values()) > max_lines:
        # find the calendar with the most events
        max_cal = max(events, key=lambda k: len(events[k]))
        del events[max_cal][-1]
    return events

def format_event_start_time(start_time: datetime|date) -> str:
    # full-day event?
    if not isinstance(start_time, datetime):
        return " ——"
    else:
        return start_time.strftime("%H:%M")

@app.route('/update', methods=['POST'])
def update_image():
    """ Update the image and store it for later retrieval. """
    global last_image, last_image_successfull

    # Is there a special image for today?
    today = datetime.now().strftime("%Y-%m-%d")
    special_image_path = f'special_images/{today}.png'
    if os.path.exists(special_image_path):
        logger.info(f"Using special image for today: {special_image_path}")
        with open(special_image_path, 'rb') as img:
            last_image = img.read()
        last_image_successfull = True
    else:
        # Create a blank image
        img = Image.new("1", (800, 480), color=1) # 1bpp mode, white background
        draw = ImageDraw.Draw(img)
        try:
            # Draw separators
            #draw.line((400, 0, 400, 480))
            #draw.line((0, 280, 800, 280))

            # Draw date
            now = datetime.now()
            draw.text((200, 20), now.strftime("%d. %b %Y"), font=font_large, anchor="ma")
            
            # Current day weather
            weather_data = fetch_weather_data()
            draw.text((170, 70), f"{weather_data['daily']['temperature_2m_max'][0]:.1f} °C", font=font_xlarge) # max
            draw.text((170, 130), f"{weather_data['daily']['temperature_2m_min'][0]:.1f} °C", font=font_large) # min
            icon_name = wmo_to_icon_name.get(weather_data['daily']['weather_code'][0], 'na')
            draw.bitmap((30, 70), Image.open(f"weather-icons/wi-{icon_name}_128.png")) # weather icon

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
            events = fetch_upcoming_events(max_lines=10)
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
                    print(event.summary.value, event.dtstart.value)
                    draw.text((agenda_pos[0] + 0, agenda_pos[1] + ai * agenda_spacing), event.dtstart.value.strftime("%a"), font=font_small, anchor="la")
                    draw.text((agenda_pos[0] + 33, agenda_pos[1] + ai * agenda_spacing), event.dtstart.value.strftime("%d.%m"), font=font_small, anchor="la")
                    draw.text((agenda_pos[0] + 82, agenda_pos[1] + ai * agenda_spacing), format_event_start_time(event.dtstart.value), font=font_small, anchor="la")
                    draw.text((agenda_pos[0] + 135, agenda_pos[1] + ai * agenda_spacing), summary_str, font=font_small, anchor="la")
                    ai += 1

            # Timestamp
            draw.text((800-2, 480-2), f"Stand: {now.isoformat(timespec='seconds')}", font=font_xsmall, anchor="rb")
            last_image_successfull = True
        except Exception as e:
            logger.exception("Error while generating image")
            last_image_successfull = False
            # Create an error image
            draw.rectangle((0, 0, 800, 480), fill=0)
            draw.text((400, 50), "Error", font=font_xlarge, anchor="ma", fill=1)
            draw.text((400, 130), str(e), font=font_small, anchor="ma", fill=1)

        # Output
        img_io = BytesIO()
        img.save(img_io, 'PNG', bits=1, optimize=True)
        img_io.seek(0)
        last_image = img_io.getvalue()
    
    global last_image_time
    last_image_time = datetime.now()
    return ('', 204) # No content response

@app.route('/frame.png')
def frame_image():
    """ Serve the current frame image. """
    global last_image, last_request_time
    last_request_time = datetime.now()
    if last_image is None:
        return "No image available", 404
    return send_file(BytesIO(last_image), mimetype='image/png')

@app.route('/status')
def status():
    """ Serve the current status information. """
    global last_image_time, last_request_time
    return {
        "last_image_time": last_image_time,
        "last_request_time": last_request_time,
        "last_image_successful": last_image_successfull,
    }

if __name__ == '__main__':
    app.run(debug=True)