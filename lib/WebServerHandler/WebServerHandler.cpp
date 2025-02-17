#include "WebServerHandler.h"
#include <Arduino.h>
#include <WiFi.h>

#ifndef PUMP1_PIN
#define PUMP1_PIN 25
#endif
#ifndef PUMP2_PIN
#define PUMP2_PIN 27
#endif
#ifndef PUMP3_PIN
#define PUMP3_PIN 26
#endif

#ifndef TANK_HEIGHT
#define TANK_HEIGHT 18
#endif
#ifndef TANK_HEIGHT2
#define TANK_HEIGHT2 6
#endif

extern volatile float g_reservoirLevel1;
extern volatile float g_reservoirLevel2;
extern volatile float g_tdsValue;
extern bool pump1State;
extern bool pump2State;
extern bool pump3State;

WebServerHandler::WebServerHandler() : server(80) {}

void WebServerHandler::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/sensor", HTTP_GET, [this]() { handleSensor(); });
    server.on("/control", HTTP_GET, [this]() { handleControl(); });
    server.begin();
    Serial.println("Web server started on port 80.");
}

void WebServerHandler::handleClient() {
    server.handleClient();
}

String WebServerHandler::getSensorDataJSON() {
    String json = "{";
    json += "\"pump1\":" + String(pump1State ? "true" : "false") + ",";
    json += "\"pump2\":" + String(pump2State ? "true" : "false") + ",";
    json += "\"pump3\":" + String(pump3State ? "true" : "false") + ",";
    json += "\"reservoir\":" + String(g_reservoirLevel1, 1) + ",";
    json += "\"sumur\":" + String(g_reservoirLevel2, 1) + ",";
    
    // Jika salah satu pompa hidup, debit air diset ke 26.785 mL/s
    float debit = 0.0;
    if (pump1State || pump2State || pump3State) {
        debit = 26.785;  // Nilai debit dalam mL/s
    }
    json += "\"debit\":" + String(debit, 3) + ",";
    json += "\"tds\":" + String(g_tdsValue, 2);
    json += "}";
    return json;
}

String WebServerHandler::generateHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sistem Monitoring Air</title>
  <style>
    body {
      background-color: #1e1e1e;
      font-family: 'Roboto', sans-serif;
      color: #e0e0e0;
      margin: 0;
      padding: 20px;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
    }
    .card {
      background-color: #2c2c2c;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.3);
      text-align: center;
    }
    .header {
      font-size: 1.5em;
      margin-bottom: 15px;
      color: #76ff03;
    }
    .pump {
      display: inline-block;
      margin: 10px;
      padding: 10px;
      border-radius: 8px;
      background-color: #424242;
      text-align: center;
      transition: background-color 0.3s;
      font-size: 1em;
      width: 100px;
    }
    .pump.on {
      background-color: #76ff03;
      color: #1e1e1e;
      font-weight: bold;
    }
    svg {
      width: 100%;
      height: auto;
    }
    .gauge-text {
      font-size: 20px;
      fill: #fff;
      dominant-baseline: middle;
      text-anchor: middle;
    }
    .gauge-container {
      position: relative;
      margin: 15px auto;
      width: 100%;
      height: 40px;
    }
    .gauge {
      position: relative;
      height: 20px;
      background: linear-gradient(to right, #2196f3, #4caf50, #ffeb3b, #ff9800, #795548);
      border-radius: 10px;
    }
    .pointer {
      position: absolute;
      top: -8px;
      width: 4px;
      height: 36px;
      background-color: #ffffff;
      border-radius: 2px;
      transition: left 0.3s ease-out;
    }
    .data {
      font-size: 1.2em;
      margin: 15px 0;
    }
  </style>
  <script>
    var MAX_RESERVOIR = %MAX_RESERVOIR%;
    var MAX_SUMUR = %MAX_SUMUR%;
    var ARC_LENGTH = 188.5;
    
    function fetchData() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          // Update status pompa
          document.getElementById("pump1").className = "pump " + (data.pump1 ? "on" : "");
          document.getElementById("pump1").innerHTML = "Pompa 1<br>" + (data.pump1 ? "Hidup" : "Mati");
          document.getElementById("pump2").className = "pump " + (data.pump2 ? "on" : "");
          document.getElementById("pump2").innerHTML = "Pompa 2<br>" + (data.pump2 ? "Hidup" : "Mati");
          document.getElementById("pump3").className = "pump " + (data.pump3 ? "on" : "");
          document.getElementById("pump3").innerHTML = "Pompa 3<br>" + (data.pump3 ? "Hidup" : "Mati");
          
          // Update debit air (mL/s)
          document.getElementById("debit").textContent = "Debit: " + data.debit + " mL/s";

          // Gauge Reservoir
          var reservoirFraction = data.reservoir / MAX_RESERVOIR;
          reservoirFraction = Math.min(Math.max(reservoirFraction, 0), 1);
          document.getElementById("reservoirArc").setAttribute("stroke-dashoffset", 
            ARC_LENGTH * (1 - reservoirFraction));
          document.getElementById("reservoirValue").textContent = data.reservoir.toFixed(1) + " cm";
          
          // Gauge Sumur
          var sumurFraction = data.sumur / MAX_SUMUR;
          sumurFraction = Math.min(Math.max(sumurFraction, 0), 1);
          document.getElementById("sumurArc").setAttribute("stroke-dashoffset", 
            ARC_LENGTH * (1 - sumurFraction));
          document.getElementById("sumurValue").textContent = data.sumur.toFixed(1) + " cm";
          
          // Update TDS
          document.getElementById("tds").innerHTML = "TDS: " + data.tds.toFixed(2) + " ppm";
          var tdsGaugeWidth = document.getElementById("tdsGauge").offsetWidth;
          var tdsPointer = document.getElementById("tdsPointer");
          var tdsValue = Math.min(Math.max(data.tds, 0), 1000);
          var tdsPos = (tdsValue / 1000) * tdsGaugeWidth;
          tdsPointer.style.left = tdsPos + "px";
        }
      };
      xhr.open("GET", "/sensor", true);
      xhr.send();
    }
    setInterval(fetchData, 1000);
    window.onload = fetchData;
  </script>
</head>
<body>
  <div class="container">
    <!-- Card Status Pompa + Debit -->
    <div class="card">
      <div class="header">Status Pompa</div>
      <div id="pump1" class="pump">Pompa 1<br>-</div>
      <div id="pump2" class="pump">Pompa 2<br>-</div>
      <div id="pump3" class="pump">Pompa 3<br>-</div>
      <!-- Ubah label default dari L/m menjadi mL/s -->
      <div id="debit" class="data">Debit: - mL/s</div>
    </div>

    <!-- Gauge Reservoir -->
    <div class="card">
      <div class="header">Tinggi Muka Air Reservoir</div>
      <svg viewBox="0 0 200 100">
        <path d="M180,80 A60,60 0 0,0 20,80" stroke="#555" stroke-width="10" fill="none"/>
        <path id="reservoirArc" d="M180,80 A60,60 0 0,0 20,80" stroke="#76ff03" stroke-width="10" fill="none"
              stroke-dasharray="188.5" stroke-dashoffset="188.5"/>
        <text id="reservoirValue" x="100" y="60" class="gauge-text">-- cm</text>
      </svg>
    </div>

    <!-- Gauge Sumur -->
    <div class="card">
      <div class="header">Tinggi Muka Air Sumur</div>
      <svg viewBox="0 0 200 100">
        <path d="M180,80 A60,60 0 0,0 20,80" stroke="#555" stroke-width="10" fill="none"/>
        <path id="sumurArc" d="M180,80 A60,60 0 0,0 20,80" stroke="#4caf50" stroke-width="10" fill="none"
              stroke-dasharray="188.5" stroke-dashoffset="188.5"/>
        <text id="sumurValue" x="100" y="60" class="gauge-text">-- cm</text>
      </svg>
    </div>

    <!-- TDS -->
    <div class="card">
      <div class="header">Nilai TDS</div>
      <div id="tds" class="data">- ppm</div>
      <div class="gauge-container">
        <div id="tdsGauge" class="gauge"></div>
        <div id="tdsPointer" class="pointer"></div>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";
    
    html.replace("%MAX_RESERVOIR%", String(TANK_HEIGHT));
    html.replace("%MAX_SUMUR%", String(TANK_HEIGHT2));
    return html;
}

void WebServerHandler::handleRoot() {
    server.send(200, "text/html", generateHTML());
}

void WebServerHandler::handleSensor() {
    server.send(200, "application/json", getSensorDataJSON());
}

void WebServerHandler::handleControl() {
    if (server.hasArg("pump")) {
        String pumpArg = server.arg("pump");
        if (pumpArg == "1") {
            pump1State = !pump1State;
            digitalWrite(PUMP1_PIN, pump1State ? LOW : HIGH);
        } else if (pumpArg == "2") {
            pump2State = !pump2State;
            digitalWrite(PUMP2_PIN, pump2State ? LOW : HIGH);
        } else if (pumpArg == "3") {
            pump3State = !pump3State;
            digitalWrite(PUMP3_PIN, pump3State ? LOW : HIGH);
        }
        server.send(200, "text/plain", "Pompa " + pumpArg + " toggled");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}
