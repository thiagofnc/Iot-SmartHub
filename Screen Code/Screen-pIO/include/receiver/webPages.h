#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// ── Login page ────────────────────────────────────────────────────────────────
// Error shown via URL param ?error=1 to avoid snprintf % escaping.
const char loginHtml[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SmartHub - Login</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap" rel="stylesheet">
    <style>
        body { margin:0; font-family:'Inter',sans-serif; background:linear-gradient(135deg,#1e3c72,#2a5298); color:white; min-height:100vh; display:flex; justify-content:center; align-items:center; }
        .container { background:rgba(255,255,255,0.1); backdrop-filter:blur(15px); -webkit-backdrop-filter:blur(15px); padding:40px 30px; border-radius:20px; box-shadow:0 8px 32px 0 rgba(0,0,0,0.37); border:1px solid rgba(255,255,255,0.18); width:90%; max-width:360px; }
        h1 { text-align:center; font-weight:600; margin:0 0 28px; font-size:24px; letter-spacing:1px; }
        .ig { margin-bottom:16px; }
        label { display:block; font-size:11px; color:#b0c4de; margin-bottom:6px; text-transform:uppercase; letter-spacing:1px; }
        input[type=text], input[type=password] { width:100%; padding:12px; border-radius:10px; background:rgba(255,255,255,0.15); border:1px solid rgba(255,255,255,0.2); color:white; font-size:15px; font-family:'Inter',sans-serif; outline:none; box-sizing:border-box; }
        .btn { width:100%; padding:13px; border-radius:10px; background:#4da8da; border:none; color:white; font-size:15px; font-family:'Inter',sans-serif; font-weight:600; cursor:pointer; margin-top:8px; transition:background 0.3s; }
        .btn:hover { background:#3a8fbf; }
        .err { color:#ff5252; font-size:13px; text-align:center; margin-bottom:12px; display:none; }
        .back { text-align:center; margin-top:16px; font-size:14px; color:#b0c4de; }
        .back a { color:#4da8da; text-decoration:none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Admin Login</h1>
        <p id="err" class="err">Incorrect username or password.</p>
        <form method="POST" action="/login">
            <div class="ig">
                <label>Username</label>
                <input type="text" name="USERNAME" autocomplete="off">
            </div>
            <div class="ig">
                <label>Password</label>
                <input type="password" name="PASSWORD">
            </div>
            <button type="submit" class="btn">Sign In</button>
        </form>
        <div class="back"><a href="/">&#8592; Back to Dashboard</a></div>
    </div>
    <script>
        if (location.search.indexOf('error=1') !== -1)
            document.getElementById('err').style.display = 'block';
    </script>
</body>
</html>
)=====";

// ── Unauthenticated main page (no servo control) ──────────────────────────────
const char indexUnauthHtml[] PROGMEM = R"=====(
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
        .card:hover { transform: translateY(-2px); }
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
        .weather-info {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-top: 15px;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            padding-bottom: 15px;
        }
        .weather-info:last-child { border-bottom: none; padding-bottom: 0; }
        .weather-value { font-size: 32px; font-weight: 600; }
        .weather-desc { font-size: 14px; color: #b0c4de; text-transform: capitalize; }
        .login-btn {
            display: inline-block;
            padding: 10px 24px;
            border-radius: 10px;
            background: #4da8da;
            color: white;
            text-decoration: none;
            font-weight: 600;
            font-size: 14px;
            transition: background 0.3s;
        }
        .login-btn:hover { background: #3a8fbf; }
    </style>
</head>
<body>
    <div class="container">
        <h1>SmartHub Control</h1>

        <div class="card">
            <h2>Environment Data</h2>
            <div style="font-size: 14px; color: #b0c4de; margin-bottom: 5px;">Outdoor (OpenWeatherMap)</div>
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
            <div style="font-size: 14px; color: #b0c4de; margin-top: 15px; margin-bottom: 5px;">Indoor (LoRa Sensor)</div>
            <div class="weather-info">
                <div>
                    <div class="weather-value" id="remoteTemp">--&deg;F</div>
                    <div class="weather-desc">Temperature</div>
                </div>
                <div style="text-align: right;">
                    <div class="weather-value" id="remoteHum">--%</div>
                    <div class="weather-desc">Humidity</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Screen Brightness</h2>
            <input type="range" min="0" max="255" value="128" class="slider" id="brightnessSlider" onchange="updateBrightness(this.value)">
            <div style="text-align: right; font-size: 14px; margin-top: 5px; color: #b0c4de;" id="brightnessValue">128</div>
        </div>

        <div class="card" style="text-align: center;">
            <h2>Servo Control</h2>
            <div style="font-size: 40px; margin: 8px 0;">&#128274;</div>
            <p style="color: #b0c4de; font-size: 14px; margin: 8px 0 16px;">Admin access required to control the servo.</p>
            <a href="/login" class="login-btn">Login as Admin</a>
        </div>
    </div>

    <script>
        document.getElementById('brightnessSlider').addEventListener('input', function() {
            document.getElementById('brightnessValue').innerText = this.value;
        });

        function updateBrightness(val) {
            fetch(`/api/brightness?value=${val}`).catch(err => console.error(err));
        }

        function updateWeather() {
            fetch('/api/weather')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('temp').innerHTML = data.temp + '&deg;F';
                    document.getElementById('hum').innerHTML = data.humidity + '%';
                    document.getElementById('desc').innerText = data.description;
                    document.getElementById('remoteTemp').innerHTML = data.remoteTemp + '&deg;F';
                    document.getElementById('remoteHum').innerHTML = data.remoteHum + '%';
                })
                .catch(err => console.error(err));
        }

        setInterval(updateWeather, 60000);
        updateWeather();
    </script>
</body>
</html>
)=====";

// ── Authenticated main page (servo control unlocked) ─────────────────────────
const char indexAuthHtml[] PROGMEM = R"=====(
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
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
        }
        h1 {
            font-weight: 600;
            margin: 0;
            font-size: 22px;
            letter-spacing: 1px;
        }
        .logout-btn {
            font-size: 13px;
            color: #b0c4de;
            text-decoration: none;
            border: 1px solid rgba(255,255,255,0.2);
            padding: 6px 12px;
            border-radius: 8px;
            transition: background 0.3s;
        }
        .logout-btn:hover { background: rgba(255,255,255,0.1); }
        .card {
            background: rgba(0, 0, 0, 0.2);
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 20px;
            transition: transform 0.2s ease;
        }
        .card:hover { transform: translateY(-2px); }
        .card h2 {
            margin-top: 0;
            font-size: 16px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #b0c4de;
        }
        .admin-badge {
            font-size: 10px;
            background: rgba(77,168,218,0.25);
            color: #4da8da;
            padding: 2px 7px;
            border-radius: 4px;
            vertical-align: middle;
            font-weight: 600;
            letter-spacing: 0.5px;
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
        .select-box:hover { background: rgba(255, 255, 255, 0.25); }
        .select-box option { background: #2a5298; color: white; }
        .weather-info {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-top: 15px;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            padding-bottom: 15px;
        }
        .weather-info:last-child { border-bottom: none; padding-bottom: 0; }
        .weather-value { font-size: 32px; font-weight: 600; }
        .weather-desc { font-size: 14px; color: #b0c4de; text-transform: capitalize; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>SmartHub Control</h1>
            <a href="/logout" class="logout-btn">Logout</a>
        </div>

        <div class="card">
            <h2>Environment Data</h2>
            <div style="font-size: 14px; color: #b0c4de; margin-bottom: 5px;">Outdoor (OpenWeatherMap)</div>
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
            <div style="font-size: 14px; color: #b0c4de; margin-top: 15px; margin-bottom: 5px;">Indoor (LoRa Sensor)</div>
            <div class="weather-info">
                <div>
                    <div class="weather-value" id="remoteTemp">--&deg;F</div>
                    <div class="weather-desc">Temperature</div>
                </div>
                <div style="text-align: right;">
                    <div class="weather-value" id="remoteHum">--%</div>
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
            <h2>Servo Control <span class="admin-badge">ADMIN</span></h2>
            <select class="select-box" id="rotationSelect" onchange="updateRotation(this.value)">
                <option value="0">0&deg; (Landscape)</option>
                <option value="90">90&deg; (Portrait)</option>
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
                    document.getElementById('remoteTemp').innerHTML = data.remoteTemp + '&deg;F';
                    document.getElementById('remoteHum').innerHTML = data.remoteHum + '%';
                })
                .catch(err => console.error(err));
        }

        setInterval(updateWeather, 60000);
        updateWeather();
    </script>
</body>
</html>
)=====";

#endif // WEBPAGES_H
