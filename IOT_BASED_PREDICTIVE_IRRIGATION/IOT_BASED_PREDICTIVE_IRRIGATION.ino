#include <Wire.h>
#include <U8g2lib.h>
#include <BH1750.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>

// -------- OLED --------
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// -------- DHT --------
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------- Sensors --------
BH1750 lightMeter;
Adafruit_BMP085 bmp;
RTC_DS3231 rtc;

// -------- Relay --------
#define RELAY_PIN  5
// ✅ FIX 1: Your relay is ACTIVE-HIGH
#define RELAY_ON   HIGH
#define RELAY_OFF  LOW

// -------- Soil Sensor --------
#define SOIL_PIN 34

// -------- Physical Manual Button --------
#define MANUAL_BTN_PIN 18   // One pin → GPIO18, other pin → GND

// -------- WiFi AP --------
const char* ap_ssid     = "VPD_Irrigation";
const char* ap_password = "12345678";

WebServer server(80);

// -------- Global Sensor Values --------
float g_T = 0, g_RH = 0, g_lux = 0, g_VPD = 0;
int   g_soilPercent = 0;

// -------- 3-Mode System --------
// 0 = AUTO, 1 = MANUAL ON, 2 = MANUAL OFF
int  g_mode   = 0;
bool g_pumpON = false;

// -------- Button Debounce --------
unsigned long lastBtnPress = 0;
const unsigned long DEBOUNCE_MS = 300;

// -------- Sensor Read Timer --------
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 1000;

// ================================================================
//  setRelay() — THE ONLY PLACE THAT TOUCHES THE RELAY PIN
// ================================================================
void setRelay(bool turnOn) {
  g_pumpON = turnOn;
  digitalWrite(RELAY_PIN, turnOn ? RELAY_ON : RELAY_OFF);
}

// ================================================================
//  HTML Dashboard
// ================================================================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>VPD Irrigation Dashboard</title>
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:wght@300;400;500;600&family=Space+Mono:wght@400;700&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:#f0f4ff; --card:#ffffff; --text:#1e293b;
    --muted:#64748b; --border:#e2e8f0;
    --shadow:0 4px 24px rgba(59,130,246,0.10);
  }
  *{box-sizing:border-box;margin:0;padding:0;}
  body{background:var(--bg);font-family:'DM Sans',sans-serif;color:var(--text);min-height:100vh;padding:0 0 32px;}

  .header{background:linear-gradient(135deg,#1d4ed8 0%,#3b82f6 60%,#38bdf8 100%);padding:28px 20px 36px;position:relative;overflow:hidden;}
  .header::before{content:'';position:absolute;width:200px;height:200px;border-radius:50%;background:rgba(255,255,255,0.08);top:-60px;right:-40px;}
  .header::after{content:'';position:absolute;width:120px;height:120px;border-radius:50%;background:rgba(255,255,255,0.06);bottom:-30px;left:20px;}
  .header-title{font-size:1.05rem;font-weight:600;color:rgba(255,255,255,0.85);letter-spacing:.04em;text-transform:uppercase;margin-bottom:4px;}
  .header-sub{font-size:1.5rem;font-weight:600;color:#fff;margin-bottom:12px;}
  .header-time{font-family:'Space Mono',monospace;font-size:2.3rem;font-weight:700;color:#fff;display:flex;align-items:baseline;gap:8px;}
  .header-ampm{font-size:1rem;font-weight:400;color:rgba(255,255,255,0.7);}
  .live-badge{display:inline-flex;align-items:center;gap:6px;background:rgba(255,255,255,0.18);border-radius:20px;padding:4px 12px;font-size:.75rem;color:#fff;margin-top:10px;}
  .live-dot{width:7px;height:7px;border-radius:50%;background:#4ade80;animation:pulse 1.4s infinite;display:inline-block;}

  @keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.5;transform:scale(1.4)}}
  @keyframes iconFloat{0%,100%{transform:translateY(0)}50%{transform:translateY(-4px)}}
  @keyframes cardIn{from{opacity:0;transform:translateY(18px)}to{opacity:1;transform:translateY(0)}}
  @keyframes flashUpdate{0%{background:#dbeafe}100%{background:#fff}}

  .status-banner{margin:-18px 16px 0;border-radius:16px;padding:16px 20px;display:flex;align-items:center;gap:14px;position:relative;z-index:2;transition:background .4s,border .4s;}
  .s-auto-off {background:linear-gradient(135deg,#f0f9ff,#e0f2fe);border:1.5px solid #7dd3fc;}
  .s-auto-on  {background:linear-gradient(135deg,#ecfdf5,#d1fae5);border:1.5px solid #86efac;}
  .s-manual-on{background:linear-gradient(135deg,#fff7ed,#ffedd5);border:1.5px solid #fdba74;}
  .s-manual-off{background:linear-gradient(135deg,#fdf2f8,#fce7f3);border:1.5px solid #f9a8d4;}
  .status-icon{font-size:2rem;animation:iconFloat 3s ease-in-out infinite;}
  .status-text-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);font-weight:500;}
  .status-text-value{font-size:1.15rem;font-weight:600;color:var(--text);}
  .status-indicator{margin-left:auto;display:flex;align-items:center;gap:6px;font-size:.85rem;font-weight:600;}
  .status-dot{width:10px;height:10px;border-radius:50%;}
  .s-auto-on   .status-dot{background:#22c55e;animation:pulse 1s infinite;}
  .s-auto-off  .status-dot{background:#93c5fd;}
  .s-manual-on .status-dot{background:#f97316;animation:pulse 1s infinite;}
  .s-manual-off .status-dot{background:#f472b6;}

  .grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;padding:22px 16px 0;}
  .card{background:var(--card);border-radius:20px;padding:18px 16px 16px;box-shadow:var(--shadow);border:1.5px solid var(--border);animation:cardIn .5s ease both;overflow:hidden;}
  .card:nth-child(1){animation-delay:.05s}.card:nth-child(2){animation-delay:.10s}
  .card:nth-child(3){animation-delay:.15s}.card:nth-child(4){animation-delay:.20s}
  .card-icon-wrap{width:38px;height:38px;border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:1.25rem;margin-bottom:12px;}
  .card-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);font-weight:500;margin-bottom:4px;}
  .card-value{font-family:'Space Mono',monospace;font-size:1.6rem;font-weight:700;color:var(--text);line-height:1;}
  .card-unit{font-size:.8rem;font-weight:400;color:var(--muted);margin-left:2px;}
  .card-bar-wrap{height:5px;border-radius:10px;margin-top:12px;overflow:hidden;}
  .card-bar-fill{height:100%;border-radius:10px;transition:width .8s cubic-bezier(.4,0,.2,1);}

  .card-temp .card-icon-wrap{background:#fff7ed;} .card-temp .card-bar-wrap{background:#fed7aa;} .card-temp .card-bar-fill{background:linear-gradient(90deg,#fb923c,#f97316);}
  .card-hum  .card-icon-wrap{background:#e0f2fe;} .card-hum  .card-bar-wrap{background:#bae6fd;} .card-hum  .card-bar-fill{background:linear-gradient(90deg,#38bdf8,#0ea5e9);}
  .card-light .card-icon-wrap{background:#fefce8;} .card-light .card-bar-wrap{background:#fef08a;} .card-light .card-bar-fill{background:linear-gradient(90deg,#facc15,#eab308);}
  .card-vpd  .card-icon-wrap{background:#ede9fe;} .card-vpd  .card-bar-wrap{background:#ddd6fe;} .card-vpd  .card-bar-fill{background:linear-gradient(90deg,#a78bfa,#8b5cf6);}
  .card-soil .card-icon-wrap{background:#ecfdf5;} .card-soil .card-bar-wrap{background:#bbf7d0;} .card-soil .card-bar-fill{background:linear-gradient(90deg,#4ade80,#22c55e);}
  .card-soil{grid-column:1 / -1;}
  .card-soil-inner{display:flex;gap:16px;align-items:center;}
  .card-soil .card-value{font-size:2rem;}

  .card-manual{grid-column:1 / -1;background:linear-gradient(135deg,#fafafa,#f8faff);border:1.5px solid #c7d7f7;}
  .manual-title-row{display:flex;align-items:center;gap:10px;margin-bottom:16px;}
  .manual-title{font-size:.8rem;text-transform:uppercase;letter-spacing:.1em;color:#1e3a8a;font-weight:700;}
  .mode-badge{font-size:.68rem;border-radius:8px;padding:3px 10px;font-weight:700;transition:background .3s,color .3s;}
  .mode-auto      {background:#dbeafe;color:#1d4ed8;}
  .mode-manual-on {background:#fef3c7;color:#b45309;}
  .mode-manual-off{background:#fce7f3;color:#9d174d;}

  .btn-row{display:flex;gap:10px;flex-wrap:wrap;}
  .btn{padding:12px 0;border:none;border-radius:14px;font-family:'DM Sans',sans-serif;font-size:.92rem;font-weight:600;cursor:pointer;transition:all .18s;flex:1;min-width:90px;}
  .btn-auto{background:linear-gradient(135deg,#3b82f6,#1d4ed8);color:#fff;box-shadow:0 4px 14px rgba(59,130,246,.35);}
  .btn-auto:hover{transform:translateY(-1px);box-shadow:0 6px 18px rgba(59,130,246,.45);}
  .btn-on  {background:linear-gradient(135deg,#22c55e,#16a34a);color:#fff;box-shadow:0 4px 14px rgba(34,197,94,.35);}
  .btn-on:hover{transform:translateY(-1px);box-shadow:0 6px 18px rgba(34,197,94,.45);}
  .btn-off {background:linear-gradient(135deg,#ef4444,#dc2626);color:#fff;box-shadow:0 4px 14px rgba(239,68,68,.35);}
  .btn-off:hover{transform:translateY(-1px);box-shadow:0 6px 18px rgba(239,68,68,.45);}
  .btn.active-btn{outline:3px solid #fbbf24;outline-offset:2px;}

  .refresh-note{text-align:center;font-size:.72rem;color:var(--muted);margin-top:20px;letter-spacing:.04em;}
  .updated{animation:flashUpdate .7s ease;}
</style>
</head>
<body>

<div class="header">
  <div class="header-title">IoT-Based Predictive</div>
  <div class="header-sub">VPD Irrigation Monitor</div>
  <div class="header-time"><span id="disp-time">--:--</span><span class="header-ampm" id="disp-ampm">--</span></div>
  <div class="live-badge"><div class="live-dot"></div> Live &bull; Auto-refreshing</div>
</div>

<div class="status-banner s-auto-off" id="status-banner">
  <div class="status-icon" id="status-icon">&#127807;</div>
  <div>
    <div class="status-text-label">Motor / Pump</div>
    <div class="status-text-value" id="status-text">Irrigation OFF</div>
  </div>
  <div class="status-indicator">
    <div class="status-dot"></div>
    <span id="status-label">Standby</span>
  </div>
</div>

<div class="grid">
  <div class="card card-temp" id="card-temp">
    <div class="card-icon-wrap">&#127777;&#65039;</div>
    <div class="card-label">Temperature</div>
    <div class="card-value" id="val-temp">--<span class="card-unit">&deg;C</span></div>
    <div class="card-bar-wrap"><div class="card-bar-fill" id="bar-temp" style="width:0%"></div></div>
  </div>
  <div class="card card-hum" id="card-hum">
    <div class="card-icon-wrap">&#128167;</div>
    <div class="card-label">Humidity</div>
    <div class="card-value" id="val-hum">--<span class="card-unit">%</span></div>
    <div class="card-bar-wrap"><div class="card-bar-fill" id="bar-hum" style="width:0%"></div></div>
  </div>
  <div class="card card-light" id="card-light">
    <div class="card-icon-wrap">&#9728;&#65039;</div>
    <div class="card-label">Light Intensity</div>
    <div class="card-value" id="val-lux">--<span class="card-unit">lux</span></div>
    <div class="card-bar-wrap"><div class="card-bar-fill" id="bar-lux" style="width:0%"></div></div>
  </div>
  <div class="card card-vpd" id="card-vpd">
    <div class="card-icon-wrap">&#127787;&#65039;</div>
    <div class="card-label">Vapor Pressure</div>
    <div class="card-value" id="val-vpd">--<span class="card-unit">kPa</span></div>
    <div class="card-bar-wrap"><div class="card-bar-fill" id="bar-vpd" style="width:0%"></div></div>
  </div>
  <div class="card card-soil" id="card-soil">
    <div class="card-soil-inner">
      <div class="card-icon-wrap" style="width:44px;height:44px;flex-shrink:0">&#127807;</div>
      <div style="flex:1">
        <div class="card-label">Soil Moisture</div>
        <div class="card-value" id="val-soil">--<span class="card-unit">%</span></div>
        <div class="card-bar-wrap" style="margin-top:10px"><div class="card-bar-fill" id="bar-soil" style="width:0%"></div></div>
      </div>
    </div>
  </div>

  <div class="card card-manual">
    <div class="manual-title-row">
      <span style="font-size:1.3rem">&#9881;&#65039;</span>
      <span class="manual-title">Motor Control</span>
      <span class="mode-badge mode-auto" id="mode-badge">AUTO MODE</span>
    </div>
    <div class="btn-row">
      <button class="btn btn-auto active-btn" id="btn-auto" onclick="setMode(0)">&#129504; Auto</button>
      <button class="btn btn-on"              id="btn-mon"  onclick="setMode(1)">&#9889; Manual ON</button>
      <button class="btn btn-off"             id="btn-moff" onclick="setMode(2)">&#9646; Manual OFF</button>
    </div>
  </div>
</div>

<div class="refresh-note">Data updates every 2 seconds &nbsp;&bull;&nbsp; VPD Irrigation v1.0</div>

<script>
function flashCard(id){var e=document.getElementById(id);e.classList.remove('updated');void e.offsetWidth;e.classList.add('updated');}

async function setMode(m){
  try{ await fetch('/mode?v='+m); } catch(e){ console.warn(e); }
}

async function fetchData(){
  try{
    var res=await fetch('/data');
    if(!res.ok) return;
    var d=await res.json();

    document.getElementById('disp-time').textContent=d.time;
    document.getElementById('disp-ampm').textContent=d.ampm;

    document.getElementById('val-temp').innerHTML=d.temp+'<span class="card-unit">&deg;C</span>';
    document.getElementById('bar-temp').style.width=Math.min(d.temp/50*100,100)+'%';
    flashCard('card-temp');

    document.getElementById('val-hum').innerHTML=d.humidity+'<span class="card-unit">%</span>';
    document.getElementById('bar-hum').style.width=d.humidity+'%';
    flashCard('card-hum');

    document.getElementById('val-lux').innerHTML=Math.round(d.lux)+'<span class="card-unit">lux</span>';
    document.getElementById('bar-lux').style.width=Math.min(d.lux/20000*100,100)+'%';
    flashCard('card-light');

    document.getElementById('val-vpd').innerHTML=d.vpd+'<span class="card-unit">kPa</span>';
    document.getElementById('bar-vpd').style.width=Math.min(d.vpd/3*100,100)+'%';
    flashCard('card-vpd');

    document.getElementById('val-soil').innerHTML=d.soil+'<span class="card-unit">%</span>';
    document.getElementById('bar-soil').style.width=d.soil+'%';
    flashCard('card-soil');

    var banner=document.getElementById('status-banner');
    var icon=document.getElementById('status-icon');
    var stxt=document.getElementById('status-text');
    var slbl=document.getElementById('status-label');
    var badge=document.getElementById('mode-badge');
    var bA=document.getElementById('btn-auto');
    var bOn=document.getElementById('btn-mon');
    var bOff=document.getElementById('btn-moff');

    bA.classList.remove('active-btn');
    bOn.classList.remove('active-btn');
    bOff.classList.remove('active-btn');

    if(d.mode===0){
      badge.textContent='AUTO MODE'; badge.className='mode-badge mode-auto';
      bA.classList.add('active-btn');
      if(d.pump){
        banner.className='status-banner s-auto-on';
        icon.innerHTML='&#128167;'; stxt.textContent='Irrigation ACTIVE (Auto)'; slbl.textContent='Auto ON';
      } else {
        banner.className='status-banner s-auto-off';
        icon.innerHTML='&#127807;'; stxt.textContent='Irrigation OFF (Auto)'; slbl.textContent='Standby';
      }
    } else if(d.mode===1){
      badge.textContent='MANUAL ON'; badge.className='mode-badge mode-manual-on';
      bOn.classList.add('active-btn');
      banner.className='status-banner s-manual-on';
      icon.innerHTML='&#128167;'; stxt.textContent='Motor ON (Manual)'; slbl.textContent='Manual ON';
    } else {
      badge.textContent='MANUAL OFF'; badge.className='mode-badge mode-manual-off';
      bOff.classList.add('active-btn');
      banner.className='status-banner s-manual-off';
      icon.innerHTML='&#9940;'; stxt.textContent='Motor OFF (Manual)'; slbl.textContent='Manual OFF';
    }
  } catch(e){ console.warn('Fetch error:',e); }
}

fetchData();
setInterval(fetchData,2000);
</script>
</body>
</html>
)rawliteral";

// ================================================================
//  Web Handlers
// ================================================================
void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleData() {
  DateTime now = rtc.now();
  int hour = now.hour();
  String ampm = (hour >= 12) ? "PM" : "AM";
  int displayHour = hour % 12;
  if (displayHour == 0) displayHour = 12;
  char minBuf[3];
  sprintf(minBuf, "%02d", now.minute());

  String json = "{";
  json += "\"time\":\""   + String(displayHour) + ":" + String(minBuf) + "\",";
  json += "\"ampm\":\""   + ampm + "\",";
  json += "\"temp\":"     + String(g_T, 1)  + ",";
  json += "\"humidity\":" + String(g_RH, 1) + ",";
  json += "\"lux\":"      + String(g_lux, 1) + ",";
  json += "\"vpd\":"      + String(g_VPD, 2) + ",";
  json += "\"soil\":"     + String(g_soilPercent) + ",";
  json += "\"pump\":"     + String(g_pumpON ? "true" : "false") + ",";
  json += "\"mode\":"     + String(g_mode);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleMode() {
  if (server.hasArg("v")) {
    int val = server.arg("v").toInt();
    if (val == 0) {
      g_mode = 0;
      Serial.println("WEB → Mode: AUTO");
    }
    else if (val == 1) {
      g_mode = 1;
      setRelay(true);
      Serial.println("WEB → Mode: MANUAL ON → Relay ON");
    }
    else if (val == 2) {
      g_mode = 2;
      setRelay(false);
      Serial.println("WEB → Mode: MANUAL OFF → Relay OFF");
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

// ================================================================
//  setup()
// ================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  u8g2.begin();
  dht.begin();
  bmp.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  rtc.begin();

  // Uncomment ONCE to sync RTC, then comment back and re-upload:
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // ✅ FIX 2: Relay pin set LOW immediately — motor guaranteed OFF at boot
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);   // direct write before anything else
  g_pumpON = false;
  g_mode   = 0;

  pinMode(MANUAL_BTN_PIN, INPUT_PULLUP);

  // Title screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 20, "IoT-Based Predictive");
  u8g2.drawStr(0, 35, "Irrigation System");
  u8g2.drawStr(0, 50, "Using VPD & Solar");
  u8g2.drawStr(0, 63, "Flux Fusion");
  u8g2.sendBuffer();
  delay(3000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 15, "WiFi: VPD_Irrigation");
  u8g2.drawStr(0, 30, "Pass: 12345678");
  u8g2.drawStr(0, 45, "Open browser:");
  u8g2.drawStr(0, 60, "192.168.4.1");
  u8g2.sendBuffer();
  delay(4000);

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/mode", handleMode);
  server.begin();
  Serial.println("Server ready → 192.168.4.1");
}

// ================================================================
//  loop()
// ================================================================
void loop() {
  server.handleClient();

  // ── Physical Button: cycles AUTO → MANUAL ON → MANUAL OFF → AUTO ──
  if (digitalRead(MANUAL_BTN_PIN) == LOW) {
    unsigned long now_ms = millis();
    if (now_ms - lastBtnPress > DEBOUNCE_MS) {
      lastBtnPress = now_ms;
      g_mode = (g_mode + 1) % 3;
      if (g_mode == 0) {
        Serial.println("BTN → Mode: AUTO");
        // Relay decided by sensor logic on next cycle
      } else if (g_mode == 1) {
        setRelay(true);
        Serial.println("BTN → Mode: MANUAL ON");
      } else {
        setRelay(false);
        Serial.println("BTN → Mode: MANUAL OFF");
      }
    }
  }

  // ── Sensor Read every 1 second ──
  unsigned long now_ms = millis();
  if (now_ms - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now_ms;

    float T   = bmp.readTemperature();
    float RH  = dht.readHumidity();
    float lux = lightMeter.readLightLevel();
    int soilRaw     = analogRead(SOIL_PIN);
    int soilPercent = constrain(map(soilRaw, 4095, 1500, 0, 100), 0, 100);

    if (isnan(RH)) return;

    float es  = 0.6108 * exp((17.27 * T) / (T + 237.3));
    float VPD = (1 - RH / 100.0) * es;

    // ✅ FIX 3: AUTO mode — thresholds lowered for realistic triggering
    // For final deployment restore: VPD > 1.2, lux > 8000, soilPercent < 40
    if (g_mode == 0) {
      if (VPD > 0.5 && lux > 1000 && soilPercent < 60) {
        setRelay(true);
      } else {
        setRelay(false);
      }
    }
    // Mode 1 or 2 → relay already set, do NOT touch it here

    g_T           = T;
    g_RH          = RH;
    g_lux         = lux;
    g_VPD         = VPD;
    g_soilPercent = soilPercent;

    // Serial debug
    DateTime now = rtc.now();
    int hour = now.hour();
    String ampm = (hour >= 12) ? "PM" : "AM";
    int dH = hour % 12; if (dH == 0) dH = 12;
    Serial.println("------ LIVE DATA ------");
    Serial.printf("Time : %d:%02d %s\n", dH, now.minute(), ampm.c_str());
    Serial.printf("Temp : %.1f C\n", T);
    Serial.printf("Hum  : %.1f %%\n", RH);
    Serial.printf("Light: %.0f lux\n", lux);
    Serial.printf("VPD  : %.2f kPa\n", VPD);
    Serial.printf("Soil : %d %%\n", soilPercent);
    Serial.printf("Mode : %s\n", g_mode==0?"AUTO":(g_mode==1?"MANUAL ON":"MANUAL OFF"));
    Serial.printf("Pump : %s\n", g_pumpON?"ON":"OFF");
    Serial.println("----------------------\n");

    // OLED
    DateTime t  = rtc.now();
    int dH2 = t.hour() % 12; if (dH2 == 0) dH2 = 12;
    String ap2  = (t.hour() >= 12) ? "PM" : "AM";

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);

    u8g2.setCursor(0, 10); u8g2.print("VPD IRRIGATION");
    u8g2.setCursor(80, 10);
    u8g2.print(dH2); u8g2.print(":");
    if (t.minute() < 10) u8g2.print("0");
    u8g2.print(t.minute()); u8g2.print(ap2);
    u8g2.drawLine(0, 13, 128, 13);

    u8g2.setCursor(0,  26); u8g2.print("TEMP:"); u8g2.print(T, 1); u8g2.print("C");
    u8g2.setCursor(64, 26); u8g2.print("HUM:");  u8g2.print(RH, 0); u8g2.print("%");
    u8g2.setCursor(0,  39); u8g2.print("LUX:");  u8g2.print((int)lux);
    u8g2.setCursor(64, 39); u8g2.print("VPD:");  u8g2.print(VPD, 2);
    u8g2.setCursor(0,  52); u8g2.print("SOIL:"); u8g2.print(soilPercent); u8g2.print("%");
    u8g2.setCursor(64, 52);
    if      (g_mode == 1) u8g2.print("M:ON  ");
    else if (g_mode == 2) u8g2.print("M:OFF ");
    else                  u8g2.print(g_pumpON ? "AUTO:ON " : "AUTO:OFF");
    u8g2.sendBuffer();
  }
}