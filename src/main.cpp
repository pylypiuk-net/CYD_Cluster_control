#include <lvgl.h>
#include <TFT_eSPI.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <stdlib.h>
#include <ArduinoJson.h>


// Including secrets, remove it if you want to put them directly into this file
#include "private.h"
/* contents are:                          
const char* ssid = "ssid";             
const char* password = "password";     
const char* mqtt_server = "broker_ip"; 
int pin = 1111;                        */



WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
const char* commandTopic = "cluster/screen/command";
const char* responseTopic = "cluster/screen/response";

#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <AsyncTCP.h>

#include <XPT2046_Touchscreen.h>

// A library for interfacing with the touch screen
//
// Can be installed from the library manager (Search for "XPT2046")
//https://github.com/PaulStoffregen/XPT2046_Touchscreen
// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
#define _UI_EVENTS_H

/*in my case HSPI haven't done a great job, please check with your board*/
SPIClass mySpi = SPIClass(VSPI);

XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);


/*Change to your screen resolution*/
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

AsyncWebServer server(80);
unsigned long ota_progress_millis = 0;

boolean entered_pin = false;
boolean start_action = false;
boolean action_failed = false;
boolean action_completed = false;


extern "C" void pin_received()
{
    Serial.println("got pin");
    entered_pin = true;
}


extern "C" void play_actions()
{
    Serial.println("triggering action");
    start_action = true;
}

void onOTAStart() {
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");

  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  if (String(topic) == responseTopic) { 
    if (messageTemp == "completed")
    {
      action_completed = true;
    }
    else if (messageTemp == "failed")
    {
     action_failed = true;
    }
    
  }
    
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("Cluster-Screen-Touch-TST")) {
      Serial.println("connected");
      client.subscribe(responseTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
        }
    } 
}

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp_drv );
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;

    //bool touched = tft.getTouch( &touchX, &touchY, 600 );
    //bool touched = false;
    bool touched = (ts.tirqTouched() && ts.touched());

    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        TS_Point p = ts.getPoint();
        touchX = map(p.x,200,3700,1,screenWidth); /* Touchscreen X calibration */
        touchY = map(p.y,240,3800,1,screenHeight); /* Touchscreen X calibration */
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;

        // Serial.print( "Data x " );
        // Serial.println( touchX );

        // Serial.print( "Data y " );
        // Serial.println( touchY );
    }
}

#include "ui.h"

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Cluster's Screen Touch OTA Update Server.");
  });

  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println( LVGL_Arduino );
    // Serial.println( "I am LVGL_Arduino" );

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
    ts.begin(mySpi); /* Touchscreen init */
    ts.setRotation(1); /* Landscape orientation */

    tft.begin();          /* TFT init */
    tft.setRotation( 1 ); /* Landscape orientation */

    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register( &indev_drv );
  
    ui_init();   
    Serial.println( "Setup done" );
}

void loop()
{
    ElegantOTA.loop();

    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    long now = millis();
    if (now - lastMsg > 5000) {
        lastMsg = now;
    }
    
    lv_timer_handler(); /* let the GUI do its work */
    delay( 5 );
    if (entered_pin == true)
    {
        String received_pin;
        received_pin = lv_textarea_get_text(ui_textpin);
        if (pin == received_pin.toInt())
        {
            _ui_screen_change(&ui_control, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_control_screen_init);
        }
        else {
             _ui_screen_change(&ui_denied, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_denied_screen_init);
        }
        lv_textarea_set_text(ui_textpin, "");
        entered_pin = false;
    }

    if (start_action == true)
    {

      char buf1 [32];
      char buf2 [32];
      lv_dropdown_get_selected_str(ui_hostsdropdown, buf1, sizeof(buf1));
      lv_dropdown_get_selected_str(ui_actiondropdown, buf2, sizeof(buf2));
      Serial.println(buf1);
      Serial.println(buf2);
      DynamicJsonDocument doc(1024);
      doc["host"] = buf1;
      doc["action"] = buf2;
      char out[128];
      int b =serializeJson(doc, out);
      boolean rc = client.publish(commandTopic, out);

      start_action = false;
    }

    if (action_completed == true) {
      lv_obj_add_flag(ui_progressSpinner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_failed, LV_OBJ_FLAG_HIDDEN);
      delay(50);
      lv_obj_clear_flag(ui_completed, LV_OBJ_FLAG_HIDDEN);
      action_completed = false;
    }
    if (action_failed == true )
    {
      lv_obj_add_flag(ui_progressSpinner, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_completed, LV_OBJ_FLAG_HIDDEN);
      delay(50);
      lv_obj_clear_flag(ui_failed, LV_OBJ_FLAG_HIDDEN);
      action_failed = false;
    }
}