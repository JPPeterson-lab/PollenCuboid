# Pollencuboid

> **⚠️ Beta-Version / Beta Version:** Dieses Projekt befindet sich noch in der Entwicklung. Fehler und Breaking Changes sind möglich. / This project is still in active development. Bugs and breaking changes may occur.

---

## Deutsch

**Pollencuboid** ist ein ESP32-basiertes Display-Gerät, das aktuelle Pollenwerte und Wetterdaten auf einem 2,4"-TFT-Display anzeigt. Pollenflugdaten stammen vom Deutschen Wetterdienst (DWD), Wetterdaten von Open-Meteo. Das Gerät wird über ein WLAN-Captive-Portal konfiguriert und bietet eine Web-Oberfläche zur Einstellung von Ort, Pollenregion, Helligkeit und Warnungen.

### Funktionen
- Anzeige von Pollenflug für 8 Pollenarten (Birke, Hasel, Erle, Esche, Gräser, Roggen, Beifuß, Ambrosia)
- Aktuelle Temperatur und Wetterbedingungen
- Farbkodierte Warnanzeige bei erhöhtem Pollenflug
- Web-UI erreichbar unter `http://pollencuboid.local`
- OTA-Firmware-Update direkt aus der Web-Oberfläche

### Pinbelegung (ESP32-S3 Super Mini)

| Funktion       | GPIO |
|----------------|------|
| Display MOSI   | 11   |
| Display SCK    | 12   |
| Display CS     | 10   |
| Display DC     | 9    |
| Display RST    | 4    |
| Display BL     | 13   |
| Touch-Taster   | 5    |

> RST des Displays kann alternativ fest an 3,3 V angeschlossen werden (dann `pin_rst = -1` im Code).

### Verwendete Bibliotheken
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [lvgl 8.x](https://github.com/lvgl/lvgl)
- [WiFiManager (tzapu)](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/)
- HTTPClient, HTTPUpdate (ESP32 Arduino Core 3.x / IDF 5.x)

### Datenquellen / APIs
- **Pollenflug:** [DWD GeoServer](https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json)
- **Wetter:** [Open-Meteo](https://open-meteo.com/)
- **Geocoding:** [Nominatim / OpenStreetMap](https://nominatim.org/)

### OTA-Firmware-Update
`http://pollencuboid.local` öffnen → Bereich „Firmware-Update" → Button klicken.  
Das Gerät prüft `version.json` in diesem Repository und lädt die neue `.bin` aus dem Releases-Bereich automatisch herunter und flasht sich selbst.

---

## English

**Pollencuboid** is an ESP32-based display device showing current pollen levels and weather data on a 2.4" TFT display. Pollen data is sourced from the German Weather Service (DWD), weather data from Open-Meteo. The device is configured via a captive portal Wi-Fi setup and offers a web interface for location, pollen region, brightness, and alert settings.

### Features
- Pollen display for 8 pollen types (birch, hazel, alder, ash, grasses, rye, mugwort, ragweed)
- Current temperature and weather conditions
- Color-coded warning screen for high pollen levels
- Web UI at `http://pollencuboid.local`
- OTA firmware update directly from the web interface

### Pin Assignment (ESP32-S3 Super Mini N8R8)

| Function       | GPIO |
|----------------|------|
| Display MOSI   | 11   |
| Display SCK    | 12   |
| Display CS     | 10   |
| Display DC     | 9    |
| Display RST    | 4    |
| Display BL     | 13   |
| Touch Button   | 5    |

> Display RST can be connected directly to 3.3 V instead (set `pin_rst = -1` in code).

### Libraries
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [lvgl 8.x](https://github.com/lvgl/lvgl)
- [WiFiManager (tzapu)](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/)
- HTTPClient, HTTPUpdate (ESP32 Arduino Core 3.x / IDF 5.x)

### Data Sources / APIs
- **Pollen:** [DWD GeoServer](https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json)
- **Weather:** [Open-Meteo](https://open-meteo.com/)
- **Geocoding:** [Nominatim / OpenStreetMap](https://nominatim.org/)

### OTA Firmware Update
Open `http://pollencuboid.local` → Firmware Update section → click the button.  
The device checks `version.json` in this repository and automatically downloads and flashes the new `.bin` from the Releases section.
