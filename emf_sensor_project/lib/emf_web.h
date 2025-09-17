#ifndef EMF_WEB_H
#define EMF_WEB_H

#include <Arduino.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "emf_config.h"

// =================== WEB SERVER FUNCTIONS ===================
void init_web_server();
void handle_web_clients();
void update_json_snapshot(int adc, float v, float dbm_med, float emaDbm,
                         const String& iso, uint32_t epoch, uint32_t uptimeMs);

// =================== LOGGING FUNCTIONS ===================
void init_logging();
void writeCSVLine(const String& iso, uint32_t epoch, uint32_t uptimeMs, int adc,
                  float v, float dbm_med, float emaDbm,
                  float minDbm, float maxDbm, float avgDbm,
                  float delta_db, float dyn_thr, float mad_est,
                  const String& note, bool trigger_ready);
void writeHeaderIfEmpty(File& f);
String tailFile(const char* path, size_t lines);

// =================== TIME FUNCTIONS ===================
String isoTimeNow(uint32_t* epochOut = nullptr);
void setup_time_sync();

// =================== GLOBAL VARIABLES ===================
extern WebServer server;
extern File logFile;
extern uint32_t lineCounter;
extern String pendingNote;
extern volatile uint32_t lastHttpMs;

// Time management
extern uint32_t bootMs;
extern bool timeOK;
extern uint32_t epochBase;

// Live JSON snapshot variables
extern int g_adc;
extern float g_v;
extern float g_dbm;
extern float g_emaDbm;
extern String g_iso;
extern uint32_t g_epoch;
extern uint32_t g_uptimeMs;
extern float g_dyn_thr;
extern float g_mad_est;

// Camera status (received via ESP-NOW)
extern volatile bool cam_present;
extern volatile float cam_conf;
extern volatile uint32_t cam_last_change_ms;
extern volatile uint32_t cam_last_recv_ms;

// =================== UTILITY FUNCTIONS ===================
void touchHttp();

#endif // EMF_WEB_H