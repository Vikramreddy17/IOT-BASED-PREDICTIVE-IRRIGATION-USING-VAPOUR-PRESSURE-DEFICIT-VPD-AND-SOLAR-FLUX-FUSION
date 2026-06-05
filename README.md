<div align="center">
  <h1>🌱 IoT-Based Predictive Irrigation System</h1>
  <h3>Using Vapor Pressure Deficit (VPD) & Solar Flux Fusion</h3>
</div>

<p align="center">
  <em>An intelligent, IoT-powered irrigation system using Arduino/ESP32 to predict and automate watering schedules. It analyzes real-time environmental data to conserve water, featuring a live web dashboard for seamless agricultural monitoring and management.</em>
</p>

---

## 📖 About the Project

This project aims to bridge the gap between agriculture and technology by utilizing advanced predictive algorithms. Instead of relying solely on simple timers or basic soil moisture sensors, this system calculates the **Vapor Pressure Deficit (VPD)** and fuses it with **Solar Flux (Light Intensity)** data to make intelligent decisions about when and how much to irrigate crops.

### Key Features
*   **VPD Calculation:** Measures temperature and humidity to determine the exact atmospheric demand for water.
*   **Solar Flux Fusion:** Adjusts watering logic based on ambient sunlight via a BH1750 light sensor.
*   **Live Web Dashboard:** A responsive, beautifully designed local web server UI hosted directly on the microcontroller to monitor stats and toggle pump modes.
*   **Tri-Mode Control System:** Auto mode (sensor-driven), Manual ON, and Manual OFF overrides.
*   **OLED On-Device Display:** Real-time feedback right on the device screen for immediate diagnostics.

## 🛠 Hardware Components
*   **Microcontroller:** ESP32 (acting as Wi-Fi Access Point and Web Server)
*   **Sensors:** 
    *   DHT11 (Temperature & Humidity)
    *   BMP085 (Atmospheric Pressure & Temperature)
    *   BH1750 (Light Intensity / Solar Flux)
    *   Capacitive Soil Moisture Sensor
    *   DS3231 RTC (Real-Time Clock)
*   **Display:** 1.3" OLED (SH1106)
*   **Actuators:** Relay Module & Water Pump

## 📂 Documentation

*   **[Full Project Report (PDF)](./IOT%20-%20BASED%20PREDICTIVE%20IRRIGATION%20REPORT.pdf)**: Detailed methodology, code explanation, and results.
*   **[Project Presentation (PPTX)](./IOT%20-%20BASED%20PREDICTIVE%20IRRIGATION%20PPT.pptx)**: Slides summarizing the project's goals, hardware, and software architecture.

## 🚀 How to Run

1. Open `IOT_BASED_PREDICTIVE_IRRIGATION/IOT_BASED_PREDICTIVE_IRRIGATION.ino` in the Arduino IDE.
2. Install the required libraries (`U8g2`, `BH1750`, `Adafruit BMP085`, `DHT sensor library`, `RTClib`).
3. Upload the code to your ESP32 board.
4. Connect your phone or PC to the Wi-Fi network `VPD_Irrigation` (Password: `12345678`).
5. Open your web browser and navigate to `http://192.168.4.1` to view the dashboard.

---

## 📸 Project Showcase

<div align="center">
  <img src="FULL%20PROJECT.jpeg" alt="Full Project Hardware" width="450"/>
  <p><i>The complete hardware setup featuring the ESP32, OLED Display, Relays, and Environmental Sensors working in unison.</i></p>
</div>

<br>

<div align="center">
  <img src="WEB%20DASHBOARD.jpeg" alt="Web Dashboard Interface" width="450"/>
  <p><i>The real-time live web dashboard allowing users to monitor Temperature, Humidity, Light Intensity, VPD, and Soil Moisture, alongside manual motor controls.</i></p>
</div>

<br>

<div align="center">
  <img src="Circuit%20Diagram.png" alt="Circuit Diagram" width="450"/>
  <p><i>The complete electrical schematic and wiring diagram showing the connections between the microcontroller and the sensors.</i></p>
</div>
