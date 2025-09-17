/*
  AD8317 → ESP32 Web Logger + Rising-edge Presence + ESP-NOW TX + Status RX
  Core: ESP32 by Espressif Systems 2.0.4
  Board: ESP32 Dev Module (DevKit v1)

  Upgrades:
  - 5-point median filter on dBm (spike killer)
  - Fast/slow EMA split tightened (A_FAST=0.50, A_SLOW=0.005)
  - Adaptive trigger: dyn_thr = max(MIN_FLOOR_DB, K_NOISE * MAD)
  - Trigger fires when delta_db ≥ dyn_thr for 5 consecutive seconds
  - CSV and /json include dyn_thr & mad_est for tuning
  - NEW: ESP-NOW status RX (OCCUPIED/VACANT + confidence) → UI + /json
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <memory>
#include <driver/adc.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include "esp_bt.h"
#include "time.h"

/* ===== USER CONFIG ===== */
constexpr const char* WIFI_SSID = "Dunsmuir"; //change the SSID here (wifi name)
constexpr const char* WIFI_PASS = "N3w-F1@_2025!"; //change the wifi password here
// constexpr const char* WIFI_SSID = "Caste's Phone";
// constexpr const char* WIFI_PASS = "nunu585858"; //change the wifi password here

constexpr const char* AP_SSID   = "AD8317-Logger";
constexpr const char* AP_PASS   = "esp32logger";   // >=8 chars or "" for open

// ESP-NOW camera peer MAC (ESP32-CAM station MAC)
uint8_t CAM_MAC[6] = { 0x78, 0x42, 0x1C, 0x6D, 0xBD, 0x68 };

/* ===== SIGNAL / LOGGER ===== */
constexpr int   PIN_VOUT   = 34;
constexpr float VREF       = 3.3f;
constexpr int   ADC_MAX    = 4095;
constexpr adc_attenuation_t ADC_ATTEN = ADC_11db;
constexpr uint32_t PERIOD_MS = 1000;     // 1 Hz sampling

// AD8317 calibration (tune these!)
float slope_V_per_dB = -0.022f;          // −22 mV/dB
float intercept_dBm  =  15.0f;           // +15 dBm

// Oversample smoothing (voltage EMA before dBm)
constexpr float ALPHA_V = 0.2f;
float emaV = NAN;

// Fast/slow EMA (on median-filtered dBm)
constexpr float A_FAST = 0.50f;          // faster foreground
constexpr float A_SLOW = 0.005f;         // slower baseline
float dbm_fast = NAN, dbm_slow = NAN;
float delta_db = 0.0f;

// Median filter (5-point) over dBm
float ring_dbm[5]; int ring_i = 0; bool ring_full = false;
static float median5(const float *a) {
  float b[5]; memcpy(b, a, 5*sizeof(float));
  for (int i=1;i<5;i++){ float key=b[i]; int j=i-1; while(j>=0 && b[j]>key){ b[j+1]=b[j]; j--; } b[j+1]=key; }
  return b[2];
}

// Adaptive threshold via MAD proxy (abs dev from slow baseline)
float mad_est = 0.0f;
constexpr float ALPHA_MAD   = 0.05f;     // MAD update speed
constexpr float K_NOISE     = 7.0f;      // how many MADs above noise
constexpr float MIN_FLOOR_DB= 0.70f;     // absolute floor (dB)
float dyn_thr = MIN_FLOOR_DB;            // computed each loop

// Rising-edge trigger: need N_ON consecutive samples above dyn_thr
constexpr int   N_ON         = (int)(5000UL / PERIOD_MS);  // ~5 s
int on_streak = 0;

// ESP-NOW cooldown
constexpr uint32_t NOW_COOLDOWN_MS = 30000; // Cool down for 30s (demo only)
uint32_t last_now_ms = 0;

// Minute stats
uint32_t minuteCount  = 0;
double   minuteSumDbm = 0.0;
float    minuteMinDbm = 1e9;
float    minuteMaxDbm = -1e9;
uint32_t lastMinuteTick = 0;

// Time bases
uint32_t bootMs = 0;
bool     timeOK = false;
uint32_t epochBase = 0;

// Logging / Web
WebServer server(80);
File logFile;
uint32_t lineCounter = 0;
constexpr const char* CSV_PATH = "/ad8317_log.csv";
String pendingNote = "";

// Live JSON snapshot
int      g_adc      = 0;
float    g_v        = 0.0f;
float    g_dbm      = 0.0f;   // median dBm
float    g_emaDbm   = 0.0f;   // from emaV
String   g_iso      = "";
uint32_t g_epoch    = 0;
uint32_t g_uptimeMs = 0;
float    g_dyn_thr  = 0.0f;
float    g_mad_est  = 0.0f;

// Interactive window
volatile uint32_t lastHttpMs = 0;
constexpr uint32_t INTERACTIVE_MS = 5000;

/* ===== ESP-NOW message formats ===== */
typedef struct {
  float    delta_db;
  uint32_t ms;
} trigger_msg_t;

/* ===== ESP-NOW status RX (define FIRST to avoid autoproto issues) ===== */
struct StatusMsgPacked {
  uint8_t  version;     // 1
  uint8_t  occupied;    // 1=OCCUPIED, 0=VACANT
  float    car_conf;    // confidence
  uint32_t ms;          // sender's millis()
} __attribute__((packed));

// Global camera status
volatile bool      cam_present = false;
volatile float     cam_conf    = 0.0f;
volatile uint32_t  cam_last_change_ms = 0;   // sender's millis at change
volatile uint32_t  cam_last_recv_ms   = 0;   // our millis when received

// Explicit prototype so Arduino doesn't invent a conflicting one
static bool parseStatusFrame(const uint8_t* data, int len, StatusMsgPacked& out);

/* ===== ESP-NOW ===== */
void onNowSent(const uint8_t* mac, esp_now_send_status_t status) {
  Serial.printf("ESP-NOW sent: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// Supports both 10B packed and 12B padded layouts
static bool parseStatusFrame(const uint8_t* data, int len, StatusMsgPacked& out) {
  if (len == 10) { // packed
    out.version  = data[0];
    out.occupied = data[1];
    memcpy(&out.car_conf, data+2, 4);
    memcpy(&out.ms,       data+6, 4);
    return true;
  } else if (len == 12) { // padded (2B after u8,u8)
    out.version  = data[0];
    out.occupied = data[1];
    memcpy(&out.car_conf, data+4, 4);
    memcpy(&out.ms,       data+8, 4);
    return true;
  }
  return false;
}

static void onNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
  StatusMsgPacked st{};
  if (parseStatusFrame(data, len, st)) {
    cam_present = (st.occupied != 0);
    cam_conf    = st.car_conf;
    cam_last_change_ms = st.ms;
    cam_last_recv_ms   = millis();
    Serial.printf("CAM STATUS RX: %s (conf=%.2f) sender_ms=%u\n",
                  cam_present ? "OCCUPIED" : "VACANT", cam_conf, st.ms);
    return;
  }
  Serial.printf("ESP-NOW RX unknown len=%d\n", len);
}

bool initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  esp_now_register_send_cb(onNowSent);
  esp_now_register_recv_cb(onNowRecv);   // <-- receive status frames

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, CAM_MAC, 6);
  peer.channel = 0;      // follow STA channel
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("ESP-NOW add_peer failed");
    return false;
  }
  Serial.println("ESP-NOW ready");
  return true;
}

bool sendTriggerNow(float ddb) {
  uint32_t now = millis();
  if (now - last_now_ms < NOW_COOLDOWN_MS) return false;
  trigger_msg_t msg{ ddb, now };
  esp_err_t r = esp_now_send(CAM_MAC, (uint8_t*)&msg, sizeof(msg));
  if (r == ESP_OK) {
    last_now_ms = now;
    Serial.printf("Trigger → CAM: Δ=%.3f dB (thr=%.3f)\n", ddb, dyn_thr);
    return true;
  } else {
    Serial.printf("Trigger send ERR=%d\n", r);
    return false;
  }
}

/* ===== UTILS ===== */
float adcToVolts(int adc) { return (adc * VREF) / ADC_MAX; }
float voltsToDbm(float v) { return (v / slope_V_per_dB) + intercept_dBm; }

String isoTimeNow(uint32_t* epochOut = nullptr) {
  struct tm tminfo;
  if (getLocalTime(&tminfo, 10)) {
    timeOK = true;
    time_t now = mktime(&tminfo);
    if (epochOut) *epochOut = (uint32_t)now;
    char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tminfo);
    return String(buf);
  }
  timeOK = false;
  uint32_t sec = (epochBase == 0) ? 0 : (epochBase + (millis() - bootMs) / 1000);
  if (epochOut) *epochOut = sec;
  if (sec == 0) return String("1970-01-01T00:00:00+") + String(millis());
  time_t tt = (time_t)sec;
  struct tm* g = gmtime(&tt);
  if (!g) return String("1970-01-01T00:00:00+") + String(millis());
  char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", g);
  return String(buf);
}

void writeHeaderIfEmpty(File& f) {
  if (!f) return;
  if (f.size() == 0) {
    f.println(F("iso_time,epoch_sec,uptime_ms,adc_code,vout_volts,dbm_med,dbm_ema,minute_min_dbm,minute_max_dbm,minute_avg_dbm,delta_db,dyn_thr,mad_est,trigger_ready,note"));
    f.flush();
  }
}

void writeCSVLine(const String& iso, uint32_t epoch, uint32_t uptimeMs, int adc,
                  float v, float dbm_med, float emaDbm,
                  float minDbm, float maxDbm, float avgDbm,
                  float delta_db, float dyn_thr, float mad_est,
                  const String& note, bool trigger_ready) {
  static char line[420];
  snprintf(line, sizeof(line),
           "%s,%u,%lu,%d,%.4f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%d,%s",
           iso.c_str(), epoch, (unsigned long)uptimeMs, adc, v, dbm_med, emaDbm,
           minDbm, maxDbm, avgDbm, delta_db, dyn_thr, mad_est,
           trigger_ready ? 1 : 0, note.c_str());
  if (logFile) {
    logFile.println(line);
    if ((++lineCounter % 10) == 0) logFile.flush();
  }
  Serial.println(line);
}

String tailFile(const char* path, size_t lines) {
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return String("ERROR: cannot open file\n");
  size_t fsize = f.size();
  if (fsize == 0) { f.close(); return String(""); }
  const size_t CHUNK = 512;
  long pos = (long)fsize;
  size_t found = 0;
  String out = "";
  while (pos > 0 && found <= lines) {
    size_t toRead = (pos >= (long)CHUNK) ? CHUNK : pos;
    pos -= toRead;
    f.seek(pos, SeekSet);
    std::unique_ptr<char[]> buf(new char[toRead + 1]);
    f.readBytes(buf.get(), toRead); buf[toRead] = '\0';
    for (int i = (int)toRead - 1; i >= 0; --i) {
      if (buf[i] == '\n') { found++; if (found > lines) { pos += i + 1; goto done; } }
    }
  }
done:
  f.seek(pos, SeekSet);
  while (f.available()) out += (char)f.read();
  f.close(); return out;
}

/* ===== WEB (PROGMEM HTML) ===== */
const char HTML_PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AD8317 Logger</title>
<style>
body{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif;margin:1rem;line-height:1.4}
.card{border:1px solid #ddd;border-radius:12px;padding:1rem;margin-bottom:1rem;box-shadow:0 1px 3px rgba(0,0,0,.05)}
h1{font-size:1.25rem;margin:0 0 .5rem}table{border-collapse:collapse;width:100%}
td{padding:.25rem .4rem;border-bottom:1px solid #eee}
.input{padding:.4rem .5rem;border:1px solid #ccc;border-radius:8px}
.btn{padding:.4rem .8rem;border:1px solid #888;border-radius:10px;background:#f7f7f7;cursor:pointer}
.row{display:flex;gap:.6rem;align-items:center;flex-wrap:wrap}
.mono{font-family:ui-monospace,Menlo,Consolas,monospace}.small{font-size:.9rem;color:#555}
.badge{display:inline-block;padding:.1rem .5rem;border-radius:999px;border:1px solid #aaa}
.ok{background:#e6f6e6;border-color:#6c6}.err{background:#ffecec;border-color:#c66}
</style></head><body>
<div class="card"><h1>AD8317 Logger</h1>
<div class="small">Time source: <span id="timeOK"></span></div></div>

<div class="card"><h1>Live Sample</h1><table class="mono"><tbody id="live"></tbody></table>
<div class="row"><a class="btn" href="/trigger">Send test trigger</a></div></div>

<div class="card"><h1>Camera Status</h1>
<table class="mono"><tbody id="cam"></tbody></table>
<div class="small">Latest status received via ESP-NOW from ESP32-CAM.</div>
</div>

<div class="card"><h1>Actions</h1><div class="row">
<form action="/addnote" method="get" class="row">
<label for="note">Add note:</label>
<input class="input" id="note" name="note" placeholder="car passed">
<button class="btn" type="submit">Add</button></form>
<a class="btn" href="/tail?lines=200">View last 200</a>
<a class="btn" href="/download">Download CSV</a>
<a class="btn" href="/clear" onclick="return confirm('Clear CSV?')">Clear CSV</a>
</div></div>

<script>
function fmtAge(ms){ if(ms<0) ms=0; const s=Math.floor(ms/1000); return s+'s'; }
async function refresh(){
  try{
    const r=await fetch('/json'); const j=await r.json();
    document.getElementById('timeOK').textContent=j.time_ok?'NTP (OK)':'Uptime (no NTP)';
    const rows=[
      ['iso_time',j.iso_time],['epoch_sec',j.epoch_sec],['uptime_ms',j.uptime_ms],
      ['adc_code',j.adc_code],['vout_volts',Number(j.vout_volts).toFixed(4)],
      ['dbm_med',Number(j.dbm_med).toFixed(2)],['dbm_ema',Number(j.dbm_ema).toFixed(2)],
      ['minute_min_dbm',Number(j.minute_min_dbm).toFixed(2)],['minute_max_dbm',Number(j.minute_max_dbm).toFixed(2)],
      ['minute_avg_dbm',Number(j.minute_avg_dbm).toFixed(2)],['delta_db',Number(j.delta_db).toFixed(3)],
      ['dyn_thr',Number(j.dyn_thr).toFixed(3)],['mad_est',Number(j.mad_est).toFixed(3)],
      ['trigger_ready',j.trigger_ready]
    ];
    let t=''; for(const [k,v] of rows){ t+=`<tr><td>${k}</td><td>${v}</td></tr>`; }
    document.getElementById('live').innerHTML=t;

    const badge = j.cam_present ? '<span class="badge ok">OCCUPIED</span>' :
                                  '<span class="badge err">VACANT</span>';
    const camRows = [
      ['present', badge],
      ['confidence', Number(j.cam_conf).toFixed(2)],
      ['age_since_rx', fmtAge(j.cam_age_ms)],
      ['sender_ms_at_change', j.cam_sender_ms]
    ];
    let c=''; for(const [k,v] of camRows){ c+=`<tr><td>${k}</td><td>${v}</td></tr>`; }
    document.getElementById('cam').innerHTML=c;
  }catch(e){}
}
setInterval(refresh,1000); refresh();
</script></body></html>
)HTML";

inline void touchHttp(){ lastHttpMs = millis(); }

void handleRoot(){ touchHttp(); server.send_P(200, "text/html", HTML_PAGE); }
void handleDownload(){
  touchHttp();
  if (logFile) logFile.flush();
  File f = SPIFFS.open(CSV_PATH, FILE_READ);
  if (!f) { server.send(404, "text/plain", "No log file"); return; }
  server.streamFile(f, "text/csv"); f.close();
}
void handleTail(){
  touchHttp();
  size_t lines = 200;
  if (server.hasArg("lines")) { lines = (size_t)server.arg("lines").toInt(); if (!lines) lines = 200; }
  if (logFile) logFile.flush();
  server.send(200, "text/plain", tailFile(CSV_PATH, lines));
}
void handleAddNote(){
  touchHttp();
  if (!server.hasArg("note")) { server.send(400, "text/plain", "Missing ?note=..."); return; }
  pendingNote = server.arg("note");
  server.sendHeader("Location", "/"); server.send(303);
}
void handleClear(){
  touchHttp();
  if (logFile) { logFile.close(); }
  SPIFFS.remove(CSV_PATH);
  logFile = SPIFFS.open(CSV_PATH, FILE_APPEND);
  if (logFile) writeHeaderIfEmpty(logFile);
  server.sendHeader("Location", "/"); server.send(303);
}
void handleEpoch(){
  touchHttp();
  if (!server.hasArg("sec")) { server.send(400, "text/plain", "Missing ?sec=<unix_epoch>"); return; }
  uint32_t sec = (uint32_t)server.arg("sec").toInt();
  if (sec < 100000000U) { server.send(400, "text/plain", "Bad epoch"); return; }
  epochBase = sec; server.send(200, "text/plain", "OK");
}
void handleTrigger(){ touchHttp(); bool ok = sendTriggerNow(delta_db); server.send(200, "text/plain", ok ? "Trigger sent\n" : "Trigger blocked/failed\n"); }

void handleJson(){
  touchHttp();
  double mAvg = (minuteCount ? minuteSumDbm / minuteCount : NAN);
  uint32_t age_ms = (cam_last_recv_ms==0) ? 0 : (millis() - cam_last_recv_ms);
  char buf[768];
  int len = snprintf(buf, sizeof(buf),
    "{\"iso_time\":\"%s\",\"epoch_sec\":%u,\"uptime_ms\":%lu,"
    "\"adc_code\":%d,\"vout_volts\":%.6f,\"dbm_med\":%.3f,"
    "\"dbm_ema\":%.3f,\"minute_min_dbm\":%.3f,\"minute_max_dbm\":%.3f,"
    "\"minute_avg_dbm\":%.3f,\"delta_db\":%.3f,\"dyn_thr\":%.3f,\"mad_est\":%.3f,"
    "\"trigger_ready\":%s,\"time_ok\":%s,"
    "\"cam_present\":%s,\"cam_conf\":%.3f,\"cam_age_ms\":%u,\"cam_sender_ms\":%u}",
    g_iso.c_str(), g_epoch, (unsigned long)g_uptimeMs,
    g_adc, g_v, g_dbm, g_emaDbm, minuteMinDbm, minuteMaxDbm,
    (float)mAvg, delta_db, g_dyn_thr, g_mad_est,
    (on_streak >= N_ON) ? "true" : "false",
    timeOK ? "true" : "false",
    cam_present ? "true" : "false", cam_conf, age_ms, cam_last_change_ms);
  server.send(200, "application/json", buf);
}

/* ===== SETUP ===== */
void setup() {
  delay(3000);
  Serial.begin(115200);
  delay(100);
  Serial.println(F("\nAD8317 ESP32 Web Logger + ESP-NOW TX (adaptive trigger) + Status RX"));

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VOUT, ADC_ATTEN);

  if (!SPIFFS.begin(true)) Serial.println(F("SPIFFS mount failed."));
  logFile = SPIFFS.open(CSV_PATH, FILE_APPEND);
  if (logFile) writeHeaderIfEmpty(logFile);

  // Wi-Fi STA first, AP fallback
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to SSID: %s ...\n", WIFI_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL) { delay(200); Serial.print('.'); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nSTA connected: %s (ch=%d)\n", WiFi.localIP().toString().c_str(), WiFi.channel());
    configTime(0, 0, "pool.ntp.org");    // UTC
    WiFi.setSleep(true);                 // modem sleep
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // balanced
    btStop();                            // disable BT
    setCpuFrequencyMhz(80);              // baseline
  } else {
    Serial.println("\nSTA connect failed; enabling SoftAP fallback.");
    WiFi.mode(WIFI_MODE_AP);
    bool apOK = WiFi.softAP(AP_SSID, strlen(AP_PASS) ? AP_PASS : nullptr);
    Serial.printf("SoftAP %s at %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    if (!apOK) Serial.println("SoftAP start failed.");
  }

  // ESP-NOW (TX triggers + RX camera status)
  if (!initEspNow()) Serial.println("ESP-NOW unavailable; continuing without triggers/status.");

  // Routes
  server.on("/",        HTTP_GET, handleRoot);
  server.on("/download",HTTP_GET, handleDownload);
  server.on("/tail",    HTTP_GET, handleTail);
  server.on("/addnote", HTTP_GET, handleAddNote);
  server.on("/clear",   HTTP_GET, handleClear);
  server.on("/epoch",   HTTP_GET, handleEpoch);
  server.on("/trigger", HTTP_GET, handleTrigger);
  server.on("/json",    HTTP_GET, handleJson);
  server.begin();

  bootMs = millis();
  lastMinuteTick = bootMs;

  Serial.println(F("Setup complete."));
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("Open http://%s/\n", WiFi.localIP().toString().c_str());
  if (WiFi.getMode() & WIFI_MODE_AP)
    Serial.printf("Open http://%s/ (AP)\n", WiFi.softAPIP().toString().c_str());
}

/* ===== LOOP ===== */
void loop() {
  uint32_t t0 = millis();

  // Oversample for noise reduction
  const int N = 64;
  uint32_t acc = 0;
  for (int i = 0; i < N; ++i) { acc += analogRead(PIN_VOUT); delayMicroseconds(300); }
  int   adc = acc / N;
  float v   = adcToVolts(adc);

  if (isnan(emaV)) emaV = v;
  emaV = (1 - ALPHA_V) * emaV + ALPHA_V * v;

  float dbm_raw = voltsToDbm(v);

  // Median filter on dBm
  ring_dbm[ring_i++] = dbm_raw;
  if (ring_i == 5) { ring_i = 0; ring_full = true; }
  float dbm_med = ring_full ? median5(ring_dbm) : dbm_raw;

  // Fast/slow EMA on median dBm
  if (isnan(dbm_fast)) { dbm_fast = dbm_med; dbm_slow = dbm_med; }
  dbm_fast = (1.0f - A_FAST) * dbm_fast + A_FAST * dbm_med;
  dbm_slow = (1.0f - A_SLOW) * dbm_slow + A_SLOW * dbm_med;
  delta_db = fabsf(dbm_fast - dbm_slow);

  // MAD proxy (abs dev from slow baseline) → adaptive threshold
  float dev = fabsf(dbm_med - dbm_slow);
  mad_est = (1.0f - ALPHA_MAD) * mad_est + ALPHA_MAD * dev;
  dyn_thr = max(MIN_FLOOR_DB, K_NOISE * mad_est);

  // Rising-edge streak logic using dyn_thr
  if (delta_db >= dyn_thr) on_streak++; else on_streak = 0;

  if (on_streak == N_ON) {  // fire once per bout; cooldown inside send
    Serial.printf("AUTO trigger: Δ=%.3f ≥ dyn_thr=%.3f for %d s\n", delta_db, dyn_thr, N_ON);
    (void)sendTriggerNow(delta_db);
  }

  // Time & stats
  uint32_t epochNow; String iso = isoTimeNow(&epochNow);
  uint32_t uptimeMs = millis() - bootMs;

  minuteCount++; minuteSumDbm += dbm_med;
  if (dbm_med < minuteMinDbm) minuteMinDbm = dbm_med;
  if (dbm_med > minuteMaxDbm) minuteMaxDbm = dbm_med;
  double minuteAvg = (minuteCount ? minuteSumDbm / minuteCount : NAN);

  // Log (now includes dyn_thr & mad_est)
  bool trigger_ready = (on_streak >= N_ON);
  String noteToWrite = pendingNote; pendingNote = "";
  writeCSVLine(iso, epochNow, uptimeMs, adc, v, dbm_med, voltsToDbm(emaV),
               minuteMinDbm, minuteMaxDbm, (float)minuteAvg,
               delta_db, dyn_thr, mad_est,
               noteToWrite, trigger_ready);

  // Update snapshot for /json
  g_adc = adc; g_v = v; g_dbm = dbm_med; g_emaDbm = voltsToDbm(emaV);
  g_iso = iso; g_epoch = epochNow; g_uptimeMs = uptimeMs;
  g_dyn_thr = dyn_thr; g_mad_est = mad_est;

  // Optional: slow debug print
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 5000) {
    lastDbg = millis();
    Serial.printf("[DBG] dbm_med=%.2f delta=%.3f dyn_thr=%.3f mad=%.3f streak=%d | CAM:%s conf=%.2f age=%ums\n",
                  dbm_med, delta_db, dyn_thr, mad_est, on_streak,
                  cam_present ? "OCCUPIED" : "VACANT", cam_conf,
                  (cam_last_recv_ms==0)?0:(millis()-cam_last_recv_ms));
  }

  // Interactive power knobs
  bool interactive = (millis() - lastHttpMs) < INTERACTIVE_MS;
  if (interactive) { esp_wifi_set_ps(WIFI_PS_NONE); setCpuFrequencyMhz(160); }
  else             { esp_wifi_set_ps(WIFI_PS_MIN_MODEM); setCpuFrequencyMhz(80); }

  // Service clients up to 1 s boundary
  uint32_t targetEnd = t0 + PERIOD_MS;
  while ((int32_t)(millis() - targetEnd) < 0) { server.handleClient(); delay(1); }

  // Optional: end-of-minute summary
  if (millis() - lastMinuteTick >= 60000UL) {
    writeCSVLine(isoTimeNow(&epochNow), epochNow, millis() - bootMs,
                 adc, v, dbm_med, voltsToDbm(emaV),
                 minuteMinDbm, minuteMaxDbm, (float)minuteAvg,
                 delta_db, dyn_thr, mad_est,
                 "MINUTE_SUMMARY", (on_streak >= N_ON));
    minuteCount = 0; minuteSumDbm = 0.0;
    minuteMinDbm = 1e9; minuteMaxDbm = -1e9;
    lastMinuteTick += 60000UL;
  }
}