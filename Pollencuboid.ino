/*
 * Pollencuboid — ESP32-S3 Super Mini + ST7789V 2.4" (320x240)
 *
 * Pinbelegung:
 *   MOSI  → GPIO 11    SCK   → GPIO 12
 *   CS    → GPIO 10    DC    → GPIO 9
 *   RST   → GPIO 4     BL    → GPIO 13 (PWM)
 *   Touch → GPIO 5 (Taster gegen GND, interner Pullup)
 *
 * Bibliotheken: LovyanGFX, lvgl (8.x), WiFiManager (tzapu),
 *               ESPAsyncWebServer, AsyncTCP, ArduinoJson
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdate.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <time.h>

#include "src/ui/ui.h"
#include "src/ui/screens.h"
#include "src/ui/images.h"
// ── Version ───────────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION "0.2.0-beta"
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/JPPeterson-lab/Pollencuboid/main/version.json"
#define OTA_BIN_BASE_URL "https://github.com/JPPeterson-lab/Pollencuboid/releases/download/"

// ── Pins ─────────────────────────────────────────────────────────────────────
#define PIN_TOUCH_BTN    5

// ── LovyanGFX Konfiguration ───────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX() {
        { auto c = _bus.config();
          c.spi_host    = SPI2_HOST;
          c.spi_3wire   = false;
          c.use_lock    = true;
          c.dma_channel = SPI_DMA_CH_AUTO;
          c.pin_sclk    = 12;
          c.pin_mosi    = 11;
          c.pin_miso    = -1;
          c.pin_dc      = 9;
          c.freq_write  = 40000000;
          _bus.config(c);
          _panel.setBus(&_bus); }

        { auto c = _panel.config();
          c.pin_cs      = 10;
          c.pin_rst     = 4;
          c.pin_busy    = -1;
          c.memory_width  = 240;  c.memory_height  = 320;
          c.panel_width   = 240;  c.panel_height   = 320;
          c.offset_rotation = 0;
          c.readable    = false;
          c.invert      = true;   // ST7789V braucht Color-Inversion
          c.rgb_order   = false;
          c.dlen_16bit  = false;
          c.bus_shared  = false;
          _panel.config(c); }

        { auto c = _light.config();
          c.pin_bl      = 13;
          c.invert      = false;
          c.freq        = 5000;
          c.pwm_channel = 0;
          _light.config(c);
          _panel.setLight(&_light); }

        setPanel(&_panel);
    }
};

// ── LVGL Display Buffer ───────────────────────────────────────────────────────
#define SCREEN_W 320
#define SCREEN_H 240
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvgl_buf[SCREEN_W * 10];
static LGFX               lcd;

// ── Konfiguration (NVS) ───────────────────────────────────────────────────────
Preferences prefs;
String  cfg_city          = "Berlin";
float   cfg_lat           = 52.52f;
float   cfg_lon           = 13.41f;
int     cfg_dwd_region    = 31;   // 31 = Berlin
int     cfg_brightness    = 200;  // 0-255
int     cfg_dim_timeout   = 5;    // Minuten bis Dimmen (0 = aus)
bool    cfg_pollen_warn   = true;

// ── Pollen-Daten (DWD, Skala 0–3) ────────────────────────────────────────────
struct PollenData {
    // heute
    float hasel = -1, erle = -1, esche = -1, birke = -1;
    float graeser = -1, roggen = -1, beifuss = -1, ambrosia = -1;
    // morgen
    float hasel_t = -1, erle_t = -1, esche_t = -1, birke_t = -1;
    float graeser_t = -1, roggen_t = -1, beifuss_t = -1, ambrosia_t = -1;
    // übermorgen
    float hasel_d = -1, erle_d = -1, esche_d = -1, birke_d = -1;
    float graeser_d = -1, roggen_d = -1, beifuss_d = -1, ambrosia_d = -1;
};
PollenData pollen;

// ── Wetterdaten ───────────────────────────────────────────────────────────────
float  wetter_temp = -99.0f;
int    wetter_code = 0;

// ── Warnscreen-State ──────────────────────────────────────────────────────────
bool   pollenWarnAktiv      = false;
bool   pollenWarnBestaetigt = false;
bool   pollenBlinkState     = false;
unsigned long lastBlink     = 0;
String warnText             = "";

// ── Timing ───────────────────────────────────────────────────────────────────
unsigned long lastDataFetch = 0;
unsigned long lastUiUpdate  = 0;
unsigned long lastActivity  = 0;
bool          isDimmed      = false;
const unsigned long FETCH_INTERVAL = 10UL * 60 * 1000; // 10 Minuten

// ── Web-Server ────────────────────────────────────────────────────────────────
WebServer webServer(80);

// =============================================================================
// LVGL Display-Flush
// =============================================================================
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((lgfx::rgb565_t*)&color_p->full, w * h);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

// =============================================================================
// Hintergrundbeleuchtung
// =============================================================================
void setBrightness(int val) {
    lcd.setBrightness(constrain(val, 0, 255));
}

void wakeDisplay() {
    lastActivity = millis();
    if (isDimmed) {
        setBrightness(cfg_brightness);
        isDimmed = false;
    }
}

// =============================================================================
// Hilfsfunktionen Farbe & Labels
// =============================================================================
static lv_color_t pollenColor(float val) {
    if (val <= 0)   return lv_color_hex(0x888888); // keine / unbekannt
    if (val <= 1.0) return lv_color_hex(0x00CC44); // gering
    if (val <= 2.0) return lv_color_hex(0xFFCC00); // mäßig
    if (val <= 2.5) return lv_color_hex(0xFF8800); // hoch
    return               lv_color_hex(0xFF3300);   // sehr hoch
}

static lv_color_t tempColor(float t) {
    if (t < 0)  return lv_color_hex(0x00AAFF);
    if (t < 10) return lv_color_hex(0x00CC44);
    if (t < 25) return lv_color_hex(0xFFCC00);
    return           lv_color_hex(0xFF3300);
}

// DWD-Wertestring → float ("0-1" → 0.5, "2-3" → 2.5, etc.)
static float parseDwdWert(const char *s) {
    if (!s || strlen(s) == 0) return -1;
    String str(s);
    str.trim();
    if (str == "-1") return -1;
    int dash = str.indexOf('-');
    if (dash > 0) {
        float a = str.substring(0, dash).toFloat();
        float b = str.substring(dash + 1).toFloat();
        return (a + b) / 2.0f;
    }
    return str.toFloat();
}

// =============================================================================
// UI aktualisieren
// =============================================================================
static const char* pollenText(float val) {
    if (val < 0)    return "--";
    if (val == 0)   return "keine";
    if (val <= 1.0) return "gering";
    if (val <= 2.0) return "mittel";
    if (val <= 2.5) return "hoch";
    return                 "stark";
}

void updatePollenLabel(lv_obj_t *lbl, float val) {
    if (!lbl) return;
    lv_label_set_text(lbl, pollenText(val));
    lv_obj_set_style_text_color(lbl, pollenColor(val), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void updateUI() {
    if (!objects.screen_1) return;

    // Temperatur
    if (wetter_temp > -99.0f) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f°C", wetter_temp);
        lv_label_set_text(objects.labeltemp, buf);
        lv_obj_set_style_text_color(objects.labeltemp, tempColor(wetter_temp),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Wetter-Icon nach WMO-Code
    if (objects.imagewetter) {
        const lv_img_dsc_t *icon;
        if (wetter_code == 0 || wetter_code == 1)
            icon = &day_clear;
        else if (wetter_code == 45 || wetter_code == 48)
            icon = &ui_img_fog_png;
        else if ((wetter_code >= 51 && wetter_code <= 67) ||
                 (wetter_code >= 80 && wetter_code <= 82))
            icon = &ui_img_rain_png;
        else if ((wetter_code >= 71 && wetter_code <= 77) ||
                 wetter_code == 85 || wetter_code == 86)
            icon = &ui_img_snow_png;
        else if (wetter_code >= 95)
            icon = &ui_img_thunder_png;
        else
            icon = &ui_img_overcast_png;
        lv_img_set_src(objects.imagewetter, icon);
    }

    // Uhrzeit
    struct tm ti;
    if (getLocalTime(&ti)) {
        char t[6];
        strftime(t, sizeof(t), "%H:%M", &ti);
        lv_label_set_text(objects.labeltime, t);
    }

    // Pollenwerte mit Farbkodierung
    updatePollenLabel(objects.labelbirkewert,    pollen.birke);
    updatePollenLabel(objects.labelgraeserwert,  pollen.graeser);
    updatePollenLabel(objects.labelerlewert,     pollen.erle);
    updatePollenLabel(objects.labelbeifusswert,  pollen.beifuss);
    updatePollenLabel(objects.labelambrosiawert, pollen.ambrosia);
    updatePollenLabel(objects.labelhaselwert,    pollen.hasel);
    updatePollenLabel(objects.labeleschewert,    pollen.esche);
    updatePollenLabel(objects.labelroggenwert,   pollen.roggen);
}

void updateForecastUI() {
    // Screen SCREENFORECAST: Birke, Gräser, Erle, Beifuß
    updatePollenLabel(objects.labelbirkeday1,   pollen.birke);
    updatePollenLabel(objects.labelbirkeday2,   pollen.birke_t);
    updatePollenLabel(objects.labelbirkeday3,   pollen.birke_d);
    updatePollenLabel(objects.labelgraeserday1, pollen.graeser);
    updatePollenLabel(objects.labelgraeserday2, pollen.graeser_t);
    updatePollenLabel(objects.labelgraeserday3, pollen.graeser_d);
    updatePollenLabel(objects.labelerleday1,    pollen.erle);
    updatePollenLabel(objects.labelerleday2,    pollen.erle_t);
    updatePollenLabel(objects.labelerleday3,    pollen.erle_d);
    updatePollenLabel(objects.labelbeifussday1, pollen.beifuss);
    updatePollenLabel(objects.labelbeifussday2, pollen.beifuss_t);
    updatePollenLabel(objects.labelbeifussday3, pollen.beifuss_d);

    // Screen SCREENFORECAST2: Ambrosia, Hasel, Esche, Roggen
    updatePollenLabel(objects.labelambrosiaday1, pollen.ambrosia);
    updatePollenLabel(objects.labelambrosiaday2, pollen.ambrosia_t);
    updatePollenLabel(objects.labelambrosiaday3, pollen.ambrosia_d);
    updatePollenLabel(objects.labelhaselday1,    pollen.hasel);
    updatePollenLabel(objects.labelhaselday2,    pollen.hasel_t);
    updatePollenLabel(objects.labelhaselday3,    pollen.hasel_d);
    updatePollenLabel(objects.labelescheday1,    pollen.esche);
    updatePollenLabel(objects.labelescheday2,    pollen.esche_t);
    updatePollenLabel(objects.labelescheday3,    pollen.esche_d);
    updatePollenLabel(objects.labelroggenday1,   pollen.roggen);
    updatePollenLabel(objects.labelroggenday2,   pollen.roggen_t);
    updatePollenLabel(objects.labelroggenday3,   pollen.roggen_d);
}

// =============================================================================
// Warnscreen-Logik (identisch zu WetterCube)
// =============================================================================
String buildWarnText() {
    struct { const char *name; float val; } liste[] = {
        {"Birke",    pollen.birke},
        {"Gräser",   pollen.graeser},
        {"Ambrosia", pollen.ambrosia},
        {"Beifuß",   pollen.beifuss},
        {"Esche",    pollen.esche},
        {"Erle",     pollen.erle},
        {"Hasel",    pollen.hasel},
        {"Roggen",   pollen.roggen},
    };
    for (auto &p : liste) {
        if (p.val > 2.5f) return String(p.name) + ": Sehr hoch";
    }
    return "";
}

void checkPollenWarnung() {
    warnText = buildWarnText();

    if (warnText == "") {
        pollenWarnBestaetigt = false; // Reset wenn Pollen wieder sinken
        return;
    }

    if (cfg_pollen_warn && !pollenWarnBestaetigt && !pollenWarnAktiv) {
        if (objects.labelpollenwarnart)
            lv_label_set_text(objects.labelpollenwarnart, warnText.c_str());
        pollenWarnAktiv = true;
        loadScreen(SCREEN_ID_UISCREENWARNUNGPOLLEN);
    }
}

void tickPollenBlink() {
    if (!pollenWarnAktiv) return;
    if (millis() - lastBlink < 500) return;
    lastBlink = millis();
    pollenBlinkState = !pollenBlinkState;
    if (objects.uiscreenwarnungpollen) {
        lv_obj_set_style_bg_color(objects.uiscreenwarnungpollen,
            pollenBlinkState ? lv_color_hex(0xFF8800) : lv_color_hex(0xCC5500),
            LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// =============================================================================
// DWD Pollenflug-API
// =============================================================================
bool fetchPollenDWD() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, "https://opendata.dwd.de/climate_environment/health/alerts/s31fg.json");
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    // Filter: region_id + gesamtes Pollen-Objekt (today-Felder intern)
    // Pollen als true statt einzelne Keys — vermeidet UTF-8-Key-Größenprobleme im Filter
    StaticJsonDocument<128> filter;
    JsonArray fc = filter.createNestedArray("content");
    JsonObject fe = fc.createNestedObject();
    fe["region_id"] = true;
    fe["Pollen"]    = true;

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();
    if (err) return false;

    JsonArray content = doc["content"].as<JsonArray>();
    for (JsonObject region : content) {
        if (region["region_id"].as<int>() != cfg_dwd_region) continue;
        JsonObject p = region["Pollen"];
        pollen.hasel    = parseDwdWert(p["Hasel"]["today"]      | "");
        pollen.erle     = parseDwdWert(p["Erle"]["today"]       | "");
        pollen.esche    = parseDwdWert(p["Esche"]["today"]      | "");
        pollen.birke    = parseDwdWert(p["Birke"]["today"]      | "");
        pollen.graeser  = parseDwdWert(p["Graeser"]["today"]    | "");
        pollen.roggen   = parseDwdWert(p["Roggen"]["today"]     | "");
        pollen.beifuss  = parseDwdWert(p["Beifuss"]["today"]    | "");
        pollen.ambrosia = parseDwdWert(p["Ambrosia"]["today"]   | "");

        pollen.hasel_t    = parseDwdWert(p["Hasel"]["tomorrow"]    | "");
        pollen.erle_t     = parseDwdWert(p["Erle"]["tomorrow"]     | "");
        pollen.esche_t    = parseDwdWert(p["Esche"]["tomorrow"]    | "");
        pollen.birke_t    = parseDwdWert(p["Birke"]["tomorrow"]    | "");
        pollen.graeser_t  = parseDwdWert(p["Graeser"]["tomorrow"]  | "");
        pollen.roggen_t   = parseDwdWert(p["Roggen"]["tomorrow"]   | "");
        pollen.beifuss_t  = parseDwdWert(p["Beifuss"]["tomorrow"]  | "");
        pollen.ambrosia_t = parseDwdWert(p["Ambrosia"]["tomorrow"] | "");

        pollen.hasel_d    = parseDwdWert(p["Hasel"]["dayafter_to"]    | "");
        pollen.erle_d     = parseDwdWert(p["Erle"]["dayafter_to"]     | "");
        pollen.esche_d    = parseDwdWert(p["Esche"]["dayafter_to"]    | "");
        pollen.birke_d    = parseDwdWert(p["Birke"]["dayafter_to"]    | "");
        pollen.graeser_d  = parseDwdWert(p["Graeser"]["dayafter_to"]  | "");
        pollen.roggen_d   = parseDwdWert(p["Roggen"]["dayafter_to"]   | "");
        pollen.beifuss_d  = parseDwdWert(p["Beifuss"]["dayafter_to"]  | "");
        pollen.ambrosia_d = parseDwdWert(p["Ambrosia"]["dayafter_to"] | "");
        return true;
    }
    return false;
}

// =============================================================================
// Open-Meteo Wetter + Geocoding
// =============================================================================
bool fetchWetter() {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=";
    url += String(cfg_lat, 4) + "&longitude=" + String(cfg_lon, 4);
    url += "&current=temperature_2m,weather_code&timezone=Europe%2FBerlin";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);
    int wcode = http.GET();
    if (wcode != 200) { http.end(); return false; }
    String body = http.getString();
    http.end();
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
    wetter_temp = doc["current"]["temperature_2m"] | -99.0f;
    wetter_code = doc["current"]["weather_code"]   | 0;
    return true;
}

bool geocodeCity(const String &city, float &lat, float &lon) {
    String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
    url += city + "&count=1&language=de&format=json";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    if (http.GET() != 200) { http.end(); return false; }
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getStream());
    http.end();
    if (!doc["results"][0].is<JsonObject>()) return false;
    lat = doc["results"][0]["latitude"]  | 52.52f;
    lon = doc["results"][0]["longitude"] | 13.41f;
    return true;
}

// =============================================================================
// NVS Konfiguration
// =============================================================================
void loadConfig() {
    prefs.begin("pollencuboid", true);
    cfg_city        = prefs.getString("city",        "Berlin");
    cfg_lat         = prefs.getFloat ("lat",          52.52f);
    cfg_lon         = prefs.getFloat ("lon",          13.41f);
    cfg_dwd_region  = prefs.getInt   ("dwd_region",   31);
    cfg_brightness  = prefs.getInt   ("brightness",   200);
    cfg_dim_timeout = prefs.getInt   ("dim_timeout",  5);
    cfg_pollen_warn = prefs.getBool  ("pollen_warn",  true);
    prefs.end();
}

void saveConfig() {
    prefs.begin("pollencuboid", false);
    prefs.putString("city",        cfg_city);
    prefs.putFloat ("lat",         cfg_lat);
    prefs.putFloat ("lon",         cfg_lon);
    prefs.putInt   ("dwd_region",  cfg_dwd_region);
    prefs.putInt   ("brightness",  cfg_brightness);
    prefs.putInt   ("dim_timeout", cfg_dim_timeout);
    prefs.putBool  ("pollen_warn", cfg_pollen_warn);
    prefs.end();
}

// =============================================================================
// Web-UI (GitHub Dark)
// =============================================================================
static const char *DWD_REGIONS[] = {
    "10=Schleswig-Holstein & Hamburg",
    "20=Mecklenburg-Vorpommern",
    "30=Niedersachsen & Bremen",
    "31=Nordrhein (Tiefland)",
    "40=Nordrhein-Westfalen",
    "41=Westfalen",
    "50=Hessen",
    "51=Saarland & Mittelrhein",
    "52=Rheinland-Pfalz",
    "60=Baden-Württemberg (Oberrhein)",
    "61=Baden-Württemberg (Rest)",
    "70=Bayern (Alpen)",
    "71=Bayern (Rest)",
    "80=Brandenburg & Berlin",
    "90=Sachsen-Anhalt",
    "91=Sachsen",
    "100=Thüringen",
    nullptr
};

// =============================================================================
// OTA-Update
// =============================================================================
String otaCheckLatestVersion() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, OTA_VERSION_URL);
    http.setTimeout(8000);
    int code = http.GET();
    if (code != 200) { http.end(); return ""; }
    String body = http.getString();
    http.end();
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) return "";
    return doc["version"] | "";
}

void doOtaUpdate(const String &version) {
    String url = String(OTA_BIN_BASE_URL) + "v" + version + "/Pollencuboid-" + version + ".bin";
    WiFiClientSecure *client = new WiFiClientSecure();
    client->setInsecure();
    httpUpdate.setLedPin(-1);
    httpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = httpUpdate.update(*client, url);
    delete client;
    if (ret == HTTP_UPDATE_FAILED) {
        Serial.printf("[OTA] Fehler: %s\n", httpUpdate.getLastErrorString().c_str());
    }
}

String buildWebPage() {
    String h;
    h.reserve(4096);
    h += "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'>";
    h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    h += "<title>Pollencuboid</title><style>";
    h += "body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:20px;}";
    h += "h1{color:#58a6ff;font-size:1.4em;margin-bottom:4px;}";
    h += "p.sub{color:#8b949e;font-size:.85em;margin-top:0;margin-bottom:20px;}";
    h += ".card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:16px;max-width:480px;margin-bottom:16px;}";
    h += ".card h2{color:#58a6ff;font-size:1em;margin-top:0;}";
    h += "label{display:block;margin-bottom:4px;font-size:.9em;color:#8b949e;}";
    h += "input[type=text],input[type=number],select{width:100%;padding:8px 10px;margin-bottom:14px;border-radius:5px;border:1px solid #30363d;background:#21262d;color:#e6edf3;box-sizing:border-box;font-size:.95em;}";
    h += ".row{display:flex;align-items:center;gap:12px;margin-bottom:14px;}";
    h += ".row label{margin:0;flex:1;}";
    h += "input[type=checkbox]{width:18px;height:18px;accent-color:#58a6ff;}";
    h += "input[type=range]{width:100%;accent-color:#58a6ff;margin-bottom:4px;}";
    h += "button{background:#238636;color:#fff;border:none;padding:12px;border-radius:6px;font-size:1em;cursor:pointer;width:100%;max-width:480px;margin-top:4px;}";
    h += "button:hover{background:#2ea043;}";
    h += ".pgrid{display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:.85em;}";
    h += ".pval{background:#21262d;border-radius:4px;padding:6px 10px;border:1px solid #30363d;}";
    h += "</style></head><body>";
    h += "<h1>Pollencuboid</h1><p class='sub'>by Jan Althaus &nbsp;·&nbsp; Firmware " FIRMWARE_VERSION "</p>";

    // Aktuelle Werte
    h += "<div class='card'><h2>Aktuelle Werte</h2><div class='pgrid'>";
    struct { const char *n; float v; } vals[] = {
        {"Birke",pollen.birke},{"Hasel",pollen.hasel},
        {"Erle",pollen.erle},{"Esche",pollen.esche},
        {"Gräser",pollen.graeser},{"Roggen",pollen.roggen},
        {"Beifuß",pollen.beifuss},{"Ambrosia",pollen.ambrosia}
    };
    for (auto &v : vals) {
        h += "<div class='pval'><b>" + String(v.n) + "</b>: ";
        h += (v.v < 0) ? "--" : String(v.v, 1);
        h += "</div>";
    }
    h += "</div>";
    h += "<p style='margin-top:10px;color:#8b949e;font-size:.8em;'>Temp: ";
    h += (wetter_temp > -99) ? String(wetter_temp, 1) + " °C" : "--";
    h += " | Wettercode: " + String(wetter_code) + "</p></div>";

    // Einstellungen
    h += "<form method='POST' action='/save'>";

    h += "<div class='card'><h2>Standort</h2>";
    h += "<label>Stadt (für Wetterdaten)</label>";
    h += "<input type='text' name='city' value='" + cfg_city + "'>";
    h += "<label>DWD-Pollenregion</label><select name='dwd_region'>";
    for (int i = 0; DWD_REGIONS[i]; i++) {
        String entry = DWD_REGIONS[i];
        int eq  = entry.indexOf('=');
        int rid = entry.substring(0, eq).toInt();
        h += "<option value='" + String(rid) + "'";
        if (rid == cfg_dwd_region) h += " selected";
        h += ">" + entry.substring(eq + 1) + "</option>";
    }
    h += "</select></div>";

    h += "<div class='card'><h2>Anzeige</h2>";
    h += "<label>Helligkeit (" + String(cfg_brightness) + " / 255)</label>";
    h += "<input type='range' name='brightness' min='20' max='255' value='" + String(cfg_brightness) + "'>";
    h += "<label>Dimmen nach (Minuten, 0 = deaktiviert)</label>";
    h += "<input type='number' name='dim_timeout' min='0' max='60' value='" + String(cfg_dim_timeout) + "'>";
    h += "</div>";

    h += "<div class='card'><h2>Warnungen</h2>";
    h += "<div class='row'><label>Pollenwarnung aktivieren</label>";
    h += "<input type='checkbox' name='pollen_warn' value='1'";
    if (cfg_pollen_warn) h += " checked";
    h += "></div>";
    h += "<p style='color:#8b949e;font-size:.8em;margin:0'>Warnscreen erscheint bei DWD-Wert &gt; 2.5 (mittel-hoch bis hoch)</p>";
    h += "</div>";

    h += "<button type='submit'>Speichern</button></form>";

    // OTA-Update
    h += "<div class='card' style='margin-top:16px;'><h2>Firmware-Update</h2>";
    h += "<p style='font-size:.85em;color:#8b949e;margin-top:0;'>Aktuelle Version: <b>" FIRMWARE_VERSION "</b></p>";
    h += "<form method='POST' action='/doupdate'>";
    h += "<button type='submit' style='background:#1f6feb;'>Auf neueste Version aktualisieren</button>";
    h += "</form></div>";

    h += "<div style='margin-top:20px;text-align:center;color:#8b949e;font-size:.75em;'>";
    h += "IP: " + WiFi.localIP().toString() + " | RSSI: " + String(WiFi.RSSI()) + " dBm</div>";
    h += "</body></html>";
    return h;
}

void setupWebServer() {
    webServer.on("/", HTTP_GET, []() {
        webServer.send(200, "text/html", buildWebPage());
    });

    webServer.on("/save", HTTP_POST, []() {
        if (webServer.hasArg("city")) {
            String city = webServer.arg("city");
            float lat, lon;
            if (geocodeCity(city, lat, lon)) {
                cfg_city = city;
                cfg_lat  = lat;
                cfg_lon  = lon;
            }
        }
        if (webServer.hasArg("dwd_region"))
            cfg_dwd_region = webServer.arg("dwd_region").toInt();
        if (webServer.hasArg("brightness")) {
            cfg_brightness = webServer.arg("brightness").toInt();
            setBrightness(cfg_brightness);
        }
        if (webServer.hasArg("dim_timeout"))
            cfg_dim_timeout = webServer.arg("dim_timeout").toInt();
        cfg_pollen_warn = webServer.hasArg("pollen_warn") &&
                          webServer.arg("pollen_warn") == "1";
        saveConfig();
        lastDataFetch = 0;
        webServer.sendHeader("Location", "/");
        webServer.send(303);
    });

    webServer.on("/update", HTTP_GET, []() {
        String latest = otaCheckLatestVersion();
        String msg;
        if (latest.isEmpty()) {
            msg = "Versionscheck fehlgeschlagen.";
        } else if (latest == FIRMWARE_VERSION) {
            msg = "Bereits auf dem neuesten Stand (" + latest + ").";
        } else {
            msg = "Neue Version verfugbar: " + latest + " (aktuell: " FIRMWARE_VERSION ")";
        }
        webServer.send(200, "text/plain", msg);
    });

    webServer.on("/doupdate", HTTP_POST, []() {
        String latest = otaCheckLatestVersion();
        if (latest.isEmpty() || latest == FIRMWARE_VERSION) {
            webServer.send(200, "text/html",
                "<html><body style='background:#0d1117;color:#e6edf3;font-family:Arial;padding:20px'>"
                "<h2>Kein Update verfugbar oder Versionscheck fehlgeschlagen.</h2>"
                "<a href='/'>Zuruck</a></body></html>");
            return;
        }
        webServer.send(200, "text/html",
            "<html><body style='background:#0d1117;color:#e6edf3;font-family:Arial;padding:20px'>"
            "<h2>Update auf " + latest + " wird gestartet...</h2>"
            "<p>Das Gerat startet nach dem Update automatisch neu.</p>"
            "</body></html>");
        webServer.client().flush();
        delay(500);
        doOtaUpdate(latest);
    });

    webServer.begin();
}

// =============================================================================
// WiFi + NTP
// =============================================================================
void setupWiFi() {
    if (objects.labelstatus)
        lv_label_set_text(objects.labelstatus, "Verbinde mit WLAN...");
    lv_timer_handler();

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setAPCallback([](WiFiManager *) {
        if (objects.labelstatus)
            lv_label_set_text(objects.labelstatus, "Config: PollencuboidAP");
        lv_timer_handler();
    });

    if (!wm.autoConnect("PollencuboidAP")) {
        if (objects.labelstatus)
            lv_label_set_text(objects.labelstatus, "WLAN fehlgeschlagen...");
        lv_timer_handler();
        delay(3000);
        ESP.restart();
    }

    // WiFi-Stärke in Bar
    int barVal = constrain(map(WiFi.RSSI(), -90, -40, 0, 100), 0, 100);
    if (objects.barwifi) lv_bar_set_value(objects.barwifi, barVal, LV_ANIM_ON);

    if (objects.labelstatus)
        lv_label_set_text(objects.labelstatus, "Lade Daten...");
    lv_timer_handler();

    // NTP (MEZ/MESZ)
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
}

// =============================================================================
// Touch-Button ISR
// =============================================================================
volatile bool btnPressed = false;
void IRAM_ATTR onBtnPress() { btnPressed = true; }

// =============================================================================
// setup()
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[1] Serial OK");

    // LovyanGFX init (SPI, RST-Puls, Backlight-PWM intern verwaltet)
    lcd.init();
    lcd.setRotation(1);   // Landscape 320x240
    lcd.fillScreen(TFT_BLACK);
    setBrightness(200);
    Serial.println("[2] LCD OK");

    // LVGL init
    lv_init();
    Serial.println("[4] lv_init OK");

    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, nullptr, SCREEN_W * 10);
    Serial.println("[5] draw_buf OK");

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("[6] disp_drv OK");

    // UI → Boot-Screen
    ui_init();
    Serial.println("[7] ui_init OK");
    loadScreenNow(SCREEN_ID_SCREENBOOT);   // sofort, ohne Animation
    lv_timer_handler();
    Serial.println("[8] Boot-Screen OK");

    // Touch-Button
    pinMode(PIN_TOUCH_BTN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_BTN), onBtnPress, FALLING);

    // Config laden
    loadConfig();
    setBrightness(cfg_brightness);

    // WiFi + WebServer
    setupWiFi();
    setupWebServer();
    MDNS.begin("pollencuboid");

    // Erstmalig Daten laden
    fetchWetter();
    fetchPollenDWD();
    updateUI();
    updateForecastUI();
    checkPollenWarnung();

    if (!pollenWarnAktiv)
        loadScreen(SCREEN_ID_SCREEN_1);

    lastDataFetch = millis();
    lastActivity  = millis();
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    webServer.handleClient();
    lv_timer_handler();
    ui_tick();

    // Touch-Button: Warnscreen bestätigen oder Screens durchschalten
    if (btnPressed) {
        btnPressed = false;
        wakeDisplay();
        if (pollenWarnAktiv) {
            pollenWarnAktiv      = false;
            pollenWarnBestaetigt = true;
            loadScreen(SCREEN_ID_SCREEN_1);
        } else {
            lv_obj_t *cur = lv_scr_act();
            if (cur == objects.screen_1)
                loadScreen(SCREEN_ID_SCREENFORECAST);
            else if (cur == objects.screenforecast)
                loadScreen(SCREEN_ID_SCREENFORECAST2);
            else
                loadScreen(SCREEN_ID_SCREEN_1);
        }
    }

    // Blink-Animation Warnscreen
    tickPollenBlink();

    // Daten alle 10 Minuten aktualisieren
    if (lastDataFetch == 0 || millis() - lastDataFetch >= FETCH_INTERVAL) {
        lastDataFetch = millis();
        fetchWetter();
        fetchPollenDWD();
        updateUI();
        updateForecastUI();
        checkPollenWarnung();
    }

    // Uhrzeit jede Minute aktualisieren
    if (millis() - lastUiUpdate >= 60000) {
        lastUiUpdate = millis();
        updateUI();
        updateForecastUI();
    }

    // Display dimmen nach Inaktivität
    if (cfg_dim_timeout > 0 && !isDimmed) {
        if (millis() - lastActivity >= (unsigned long)cfg_dim_timeout * 60 * 1000) {
            setBrightness(20);
            isDimmed = true;
        }
    }

    delay(5);
}
