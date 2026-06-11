# Devlog — Pollencuboid

---

## 0.1.0-beta — 2026-06-11

**Erster öffentlicher Release / First public release**

### Hardware
- ESP32-S3 Super Mini N8R8 (8 MB Flash, kein PSRAM)
- ST7789V 2.4" SPI-Display (GMT024-08-SPI8P), 320×240 px
- Touch-Taster an GPIO 5

### Implementiert
- Display-Ansteuerung über LovyanGFX (SPI2_HOST, 4-wire SPI, invert=true)
- LVGL 8.3.x UI, erstellt mit PicoPixel.io
- Boot-Screen mit Statusmeldungen
- WLAN via WiFiManager (Captive Portal)
- Pollenflugdaten vom DWD (8 Pollenarten, farbkodiert)
- Wetterdaten von Open-Meteo (Temperatur + Wettercode)
- Geocoding über Nominatim
- Web-UI unter pollencuboid.local (Einstellungen, Live-Werte)
- Warnscreen bei hohem Pollenflug (blinkend, quittierbar)
- Display-Dimmen nach konfigurierbarem Timeout
- Helligkeit einstellbar per Web-UI
- OTA-Firmware-Update über GitHub Releases

### Bekannte Einschränkungen
- Kein PSRAM → kein Kartendisplay möglich
- 3-Tage-Pollenforecast UI noch in Planung
- ESPAsyncWebServer inkompatibel mit IDF 5 → Standard WebServer in Verwendung

### Technische Notizen
- DWD JSON-Schlüssel: "Graeser" und "Beifuss" (keine Umlaute trotz Anzeige in der Dokumentation)
- HTTPClient benötigt WiFiClientSecure + setInsecure() für HTTPS auf ESP32 Arduino Core 3.x
- http.getString() statt getStream() nötig für zuverlässiges JSON-Parsen
- PicoPixel-Export: .c Bilddateien → .inc umbenennen, includes anpassen
