FROM python:3.12-slim

WORKDIR /app

# Install system dependencies (if needed)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Install SegoeUI font
RUN mkdir -p /usr/share/fonts/Microsoft/TrueType/SegoeUI && \
    wget -q https://github.com/mrbvrz/segoe-ui/raw/master/font/segoeui.ttf?raw=true -O /usr/share/fonts/Microsoft/TrueType/SegoeUI/segoeui.ttf && \
    wget -q https://github.com/mrbvrz/segoe-ui/raw/master/font/segoeuib.ttf?raw=true -O /usr/share/fonts/Microsoft/TrueType/SegoeUI/seguisb.ttf && \
    fc-cache -f /usr/share/fonts/Microsoft/TrueType/SegoeUI

# Copy requirements and install Python dependencies
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code
COPY . .

# Expose the Flask default port
EXPOSE 5000

# Set environment variables for Flask
ENV FLASK_APP=app.py
ENV FLASK_RUN_HOST=0.0.0.0
ENV FLASK_ENV=production

# Run the Flask app
CMD ["flask", "run"]