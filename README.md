# 🌿 Pollencuboid

> **⚠️ Beta-Version:** Dieses Projekt befindet sich noch in aktiver Entwicklung. Fehler und Breaking Changes sind möglich. / This project is still in active development. Bugs and breaking changes may occur.

Eine kompakte WLAN-Pollenanzeige auf Basis des **ESP32-S3 Super Mini**, die aktuelle Pollenflugdaten und Wetterdaten auf einem 2,4"-Farbdisplay anzeigt.
Dieses Projekt ist ein Geschwisterprojekt des [WetterCube](https://github.com/JPPeterson-lab/wettercube) und richtet sich speziell an Pollenallergiker.

Pollenflugdaten kommen kostenlos vom [Deutschen Wetterdienst (DWD)](https://opendata.dwd.de/), Wetterdaten von [Open-Meteo](https://open-meteo.com) – kein API-Key nötig.

---

## ✨ Funktionen

- **Pollenflug** für 8 Arten: Birke, Hasel, Erle, Esche, Gräser, Roggen, Beifuß, Ambrosia
- **Farbkodierte Belastungsanzeige** — keine / gering / mittel / hoch / sehr hoch
- **Aktuelle Temperatur** und Wetterbedingungen
- **Pollen-Warnscreen** – blinkt bei erhöhter Belastung, per Tastendruck quittierbar
- **Captive-Portal-Setup:** WLAN + Standort bequem per Browser einstellen
- **DWD-Pollenregion** frei wählbar (alle deutschen Bundesländer/Regionen)
- **Automatisches Dimmen** nach konfigurierbarem Timeout
- **Web-Konfiguration** unter `http://pollencuboid.local` – kein Flashen nötig
- **OTA-Firmware-Update** direkt aus der Web-Oberfläche per Knopfdruck

---

## ⚙️ Web-Konfiguration

Nach dem Start ist die Konfigurationsoberfläche per Browser erreichbar.

### Zugang

**Option 1 – mDNS (empfohlen)**
```
http://pollencuboid.local
```
Funktioniert auf macOS, iPhone und Android direkt. Unter Windows 11 ebenfalls.
Ältere Windows-Versionen benötigen [Bonjour](https://support.apple.com/kb/DL999) (Apple, kostenlos).

**Option 2 – IP-Adresse**

Die IP wird nach dem WLAN-Connect kurz auf dem Boot-Screen angezeigt. Diese Adresse im Browser eingeben.

### Was sich konfigurieren lässt

| Bereich | Einstellung |
|---|---|
| **Standort** | Stadt (für Wetterdaten) |
| | DWD-Pollenregion (Dropdown aller Regionen) |
| **Display** | Helligkeit (Schieberegler) |
| | Dimm-Timeout (Minuten, 0 = deaktiviert) |
| **Warnungen** | Pollen-Warnscreen ein/aus |
| **Firmware** | OTA-Update per Knopfdruck |

---

## 🔌 Pinbelegung (ESP32-S3 Super Mini N8R8)

| Funktion | GPIO |
|---|---|
| Display MOSI (SPI) | 11 |
| Display SCK (SPI) | 12 |
| Display CS | 10 |
| Display DC | 9 |
| Display RST | 4 |
| Display BL (Backlight) | 13 |
| Touch-Taster | 5 |

> Display: **ST7789V** · 2,4" · 320×240 px · SPI (4-wire)
>
> RST kann alternativ fest an **3,3 V** angeschlossen werden (dann `pin_rst = -1` im Code).

---

## 🔄 OTA-Firmware-Update

`http://pollencuboid.local` öffnen → Bereich „Firmware-Update" → Button klicken.

Das Gerät prüft automatisch `version.json` in diesem Repository und lädt die neue `.bin` aus dem Releases-Bereich herunter und flasht sich selbst — kein USB-Kabel nötig.

### Neues Release einspielen (für Entwickler)

1. Arduino IDE → **Sketch → Compiled Binary exportieren** → `Pollencuboid.ino.bin` umbenennen in `Pollencuboid-X.Y.Z.bin`
2. `version.json` aktualisieren: `{ "version": "X.Y.Z" }`
3. `git tag vX.Y.Z && git push origin vX.Y.Z`
4. `gh release create vX.Y.Z firmware/Pollencuboid-X.Y.Z.bin`

---

## 🛠 Verwendete Bibliotheken

| Bibliothek | Zweck |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display-Treiber |
| [LVGL 8.x](https://lvgl.io) | GUI-Framework |
| [WiFiManager (tzapu)](https://github.com/tzapu/WiFiManager) | WLAN-Captive-Portal |
| [ArduinoJson](https://arduinojson.org) | JSON-Parsing |
| WiFi / HTTPClient / HTTPUpdate / WebServer | Arduino ESP32 Core 3.x |
| Preferences | Einstellungen dauerhaft im Flash speichern |
| ESPmDNS | Erreichbarkeit unter `pollencuboid.local` |

---

## 🌐 Datenquellen

| Quelle | Zweck |
|---|---|
| [DWD GeoServer](https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json) | Pollenflugdaten (kostenlos, kein Key) |
| [Open-Meteo](https://open-meteo.com) | Wetterdaten (kostenlos, kein Key) |
| [Nominatim / OpenStreetMap](https://nominatim.org) | Geocoding Stadtname → Koordinaten |

---

## 🙏 Credits

| Ressource | Urheber | Lizenz / Link |
|---|---|---|
| Wetter-Icons | [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/) | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Wetterdaten | [Open-Meteo](https://open-meteo.com) | Open-Meteo API (kostenlos) |
| Pollenflugdaten | [Deutscher Wetterdienst](https://opendata.dwd.de/) | Open Data (GeoNutzV) |

---

## 📄 Lizenz

Der Quellcode dieses Projekts steht unter der **MIT-Lizenz** – frei nutzbar, veränderbar und weitergabefähig. Weitere Details in [LICENSE](LICENSE).

Die enthaltenen Wetter-Icons unterliegen der **CC BY-SA 4.0**-Lizenz ([Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/)).

---
---

# 🌿 Pollencuboid (English)

A compact Wi-Fi pollen display based on the **ESP32-S3 Super Mini**, showing current pollen levels and weather data on a 2.4" color display.
This is a sister project of [WetterCube](https://github.com/JPPeterson-lab/wettercube), designed specifically for pollen allergy sufferers.

Pollen data is provided free of charge by the [German Weather Service (DWD)](https://opendata.dwd.de/), weather data by [Open-Meteo](https://open-meteo.com) – no API key required.

---

## ✨ Features

- **Pollen levels** for 8 types: birch, hazel, alder, ash, grasses, rye, mugwort, ragweed
- **Color-coded severity** — none / low / medium / high / very high
- **Current temperature** and weather conditions
- **Pollen warning screen** – flashes on high levels, acknowledged via button press
- **Captive Portal Setup:** Configure Wi-Fi and location conveniently in a browser
- **DWD pollen region** selectable (all German federal states / regions)
- **Automatic dimming** after a configurable timeout
- **Web configuration** at `http://pollencuboid.local` – no reflashing required
- **OTA firmware update** directly from the web interface with one click

---

## ⚙️ Web Configuration

After startup the configuration interface is accessible via browser.

### Access

**Option 1 – mDNS (recommended)**
```
http://pollencuboid.local
```
Works on macOS, iPhone, and Android out of the box. Also on Windows 11.
Older Windows versions require [Bonjour](https://support.apple.com/kb/DL999) (free from Apple).

**Option 2 – IP Address**

The IP is briefly shown on the boot screen after connecting to Wi-Fi.

### Available Settings

| Category | Setting |
|---|---|
| **Location** | City (for weather data) |
| | DWD pollen region (dropdown of all regions) |
| **Display** | Brightness (slider) |
| | Dimming timeout (minutes, 0 = disabled) |
| **Warnings** | Pollen warning screen on/off |
| **Firmware** | OTA update via button |

---

## 🔌 Pin Assignment (ESP32-S3 Super Mini N8R8)

| Function | GPIO |
|---|---|
| Display MOSI (SPI) | 11 |
| Display SCK (SPI) | 12 |
| Display CS | 10 |
| Display DC | 9 |
| Display RST | 4 |
| Display BL (Backlight) | 13 |
| Touch Button | 5 |

> Display: **ST7789V** · 2.4" · 320×240 px · SPI (4-wire)
>
> RST can be connected directly to **3.3 V** instead (set `pin_rst = -1` in code).

---

## 🔄 OTA Firmware Update

Open `http://pollencuboid.local` → Firmware Update section → click the button.

The device automatically checks `version.json` in this repository and downloads and flashes the new `.bin` from the Releases section — no USB cable required.

---

## 🛠 Libraries Used

| Library | Purpose |
|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display driver |
| [LVGL 8.x](https://lvgl.io) | GUI framework |
| [WiFiManager (tzapu)](https://github.com/tzapu/WiFiManager) | Wi-Fi captive portal |
| [ArduinoJson](https://arduinojson.org) | JSON parsing |
| WiFi / HTTPClient / HTTPUpdate / WebServer | Arduino ESP32 Core 3.x |
| Preferences | Persistent settings in flash |
| ESPmDNS | Access via `pollencuboid.local` |

---

## 🌐 Data Sources

| Source | Purpose |
|---|---|
| [DWD GeoServer](https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json) | Pollen data (free, no key) |
| [Open-Meteo](https://open-meteo.com) | Weather data (free, no key) |
| [Nominatim / OpenStreetMap](https://nominatim.org) | Geocoding city name → coordinates |

---

## 🙏 Credits

| Resource | Author | License / Link |
|---|---|---|
| Weather Icons | [Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/) | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |
| Weather Data | [Open-Meteo](https://open-meteo.com) | Open-Meteo API (free) |
| Pollen Data | [German Weather Service](https://opendata.dwd.de/) | Open Data (GeoNutzV) |

---

## 📄 License

The source code is licensed under the **MIT License** – free to use, modify, and redistribute. See [LICENSE](LICENSE).

The included weather icons are licensed under **CC BY-SA 4.0** ([Dovora Weather Icons](https://www.dovora.com/resources/weather-icons/)).
