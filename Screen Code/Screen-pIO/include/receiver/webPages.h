#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

const char indexHtml[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SmartHub Control Panel</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap" rel="stylesheet">
    <style>
        body { 
            margin: 0; 
            font-family: 'Inter', sans-serif; 
            background: linear-gradient(135deg, #1e3c72, #2a5298); 
            color: white; 
            min-height: 100vh; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
        }
        .container { 
            background: rgba(255, 255, 255, 0.1); 
            backdrop-filter: blur(15px); 
            -webkit-backdrop-filter: blur(15px);
            padding: 40px 30px; 
            border-radius: 20px; 
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37); 
            border: 1px solid rgba(255, 255, 255, 0.18);
            width: 90%; 
            max-width: 400px; 
        }
        h1 { 
            text-align: center; 
            font-weight: 600; 
            margin-bottom: 30px; 
            margin-top: 0;
            font-size: 26px; 
            letter-spacing: 1px;
        }
        .card { 
            background: rgba(0, 0, 0, 0.2); 
            padding: 20px; 
            border-radius: 15px; 
            margin-bottom: 20px; 
            transition: transform 0.2s ease;
        }
        .card:hover {
            transform: translateY(-2px);
        }
        .card h2 { 
            margin-top: 0; 
            font-size: 16px; 
            font-weight: 600; 
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #b0c4de;
        }
        .slider { 
            width: 100%; 
            margin-top: 15px; 
            cursor: pointer; 
            accent-color: #4da8da;
        }
        .select-box { 
            width: 100%; 
            padding: 12px; 
            border-radius: 10px; 
            background: rgba(255, 255, 255, 0.15); 
            border: 1px solid rgba(255, 255, 255, 0.2); 
            color: white; 
            font-size: 16px; 
            font-family: 'Inter', sans-serif; 
            margin-top: 10px; 
            outline: none; 
            cursor: pointer;
            transition: background 0.3s ease;
        }
        .select-box:hover {
            background: rgba(255, 255, 255, 0.25);
        }
        .select-box option { 
            background: #2a5298; 
            color: white; 
        }
        .weather-info { 
            display: flex; 
            justify-content: space-between; 
            align-items: center; 
            margin-top: 15px; 
        }
        .weather-value { 
            font-size: 32px; 
            font-weight: 600; 
        }
        .weather-desc { 
            font-size: 14px; 
            color: #b0c4de; 
            text-transform: capitalize;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>SmartHub Control</h1>
        
        <div class="card">
            <h2>Weather</h2>
            <div class="weather-info">
                <div>
                    <div class="weather-value" id="temp">--&deg;F</div>
                    <div class="weather-desc" id="desc">Loading...</div>
                </div>
                <div style="text-align: right;">
                    <div class="weather-value" id="hum">--%</div>
                    <div class="weather-desc">Humidity</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Screen Brightness</h2>
            <input type="range" min="0" max="255" value="128" class="slider" id="brightnessSlider" onchange="updateBrightness(this.value)">
            <div style="text-align: right; font-size: 14px; margin-top: 5px; color: #b0c4de;" id="brightnessValue">128</div>
        </div>

        <div class="card">
            <h2>Screen Rotation</h2>
            <select class="select-box" id="rotationSelect" onchange="updateRotation(this.value)">
                <option value="0">0&deg; (Landscape)</option>
                <option value="90">90&deg; (Portrait)</option>
                <option value="180">180&deg; (Landscape Flipped)</option>
                <option value="270">270&deg; (Portrait Flipped)</option>
            </select>
        </div>
    </div>

    <script>
        document.getElementById('brightnessSlider').addEventListener('input', function() {
            document.getElementById('brightnessValue').innerText = this.value;
        });

        function updateBrightness(val) {
            fetch(`/api/brightness?value=${val}`).catch(err => console.error(err));
        }

        function updateRotation(val) {
            fetch(`/api/rotation?value=${val}`).catch(err => console.error(err));
        }

        function updateWeather() {
            fetch('/api/weather')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('temp').innerHTML = data.temp + '&deg;F';
                    document.getElementById('hum').innerHTML = data.humidity + '%';
                    document.getElementById('desc').innerText = data.description;
                })
                .catch(err => console.error(err));
        }

        setInterval(updateWeather, 60000); // Update weather every 1 minute
        updateWeather(); // Initial fetch
    </script>
</body>
</html>
)=====";

#endif // WEBPAGES_H
