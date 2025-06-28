#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiCreds.h> // home-made miniature library containing the personal wifi credentials
#include <yxml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lvgl.h>
#include "ui.h"
#include <Arduino_GFX_Library.h>


const char* ssid     = SSID; // coming from WiFiCreds
const char* password = WIFI_PASSWORD; // coming from WiFiCreds

const char* sonosIP = "192.168.1.212";  // Replace with your Sonos speaker IP
const uint16_t sonosPort = 1400;
const uint16_t serverPort = 8080;

unsigned long lastResubscribe = 0;
const unsigned long RESUBSCRIBE_INTERVAL = 55 * 60 * 1000UL; // 55 minutes

// Parser state
yxml_ret_t r;
yxml_t x[1];
char stack[1024];

// Request state
#define MAX_REQUEST_SIZE 2048
char request[MAX_REQUEST_SIZE];
int request_index = 0;

WiFiServer eventServer(serverPort); // HTTP server to receive NOTIFY events

#define DISPLAY_TIMEOUT_MS 10000  // e.g. 10 seconds

unsigned long lastUpdateTime = 0;
bool displayOn = true;

// #define ROTATION 0
#define ROTATION 1
// #define ROTATION 2
// #define ROTATION 3

#define GFX_BL 23


Arduino_DataBus *bus = new Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus, 22 /* RST */, 0 /* rotation */, false /* IPS */,
  172 /* width */, 320 /* height */,
  34 /*col_offset1*/, 0 /*uint8_t row_offset1*/,
  34 /*col_offset2*/, 0 /*row_offset2*/);

void subscribeToSonos() {
  WiFiClient client;
  if (!client.connect(sonosIP, sonosPort)) {
    Serial.println("Failed to connect to Sonos");
    return;
  }

  String callbackURL = "http://" + WiFi.localIP().toString() + ":" + serverPort + "/";
  String request =
    "SUBSCRIBE /MediaRenderer/RenderingControl/Event HTTP/1.1\r\n" +
    String("HOST: ") + sonosIP + ":" + sonosPort + "\r\n" +
    "CALLBACK: <" + callbackURL + ">\r\n" +
    "NT: upnp:event\r\n" +
    "TIMEOUT: Second-3600\r\n" +
    "\r\n";

  client.print(request);
  delay(100);
  Serial.println("Sent SUBSCRIBE request:");
  Serial.println(request);

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();

  // ✅ Update the timer
  lastResubscribe = millis();
}

void lcd_reg_init(void) {
  static const uint8_t init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,  // 2: Out of sleep mode, no args, w/delay
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23, 

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4,
    0x00, 0x47, 0x00, 0x6F,

    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6,
    0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16, 

    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8,
    0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5,
    0x04, 0x06, 0x6B, 0x0F, 0x00,

    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14, 
    WRITE_C8_D8, 0xDE, 0x01, 

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5,
    0x03, 0x13, 0xEF, 0x35, 0x35,

    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3,
    0x14, 0x15, 0xC0,

    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00, 
    WRITE_C8_D8, 0xDE, 0x02, 

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x01, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00, 
    WRITE_C8_D8, 0x35, 0x00, 
    WRITE_C8_D8, 0x3A, 0x05, 

    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4,
    0x00, 0x22, 0x00, 0xCD,

    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4,
    0x00, 0x00, 0x01, 0x3F,

    WRITE_C8_D8, 0xDE, 0x02, 

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,
    
    WRITE_C8_D8, 0xDE, 0x00, 
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,
    
    DELAY, 10,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,  // 5: Main screen turn on, no args, w/delay
    END_WRITE
  };
  bus->batchOperation(init_operations, sizeof(init_operations));
}

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_disp_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf;
lv_disp_drv_t disp_drv;

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
#ifndef DIRECT_RENDER_MODE
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
#endif // #ifndef DIRECT_RENDER_MODE

  lv_disp_flush_ready(disp_drv);
}


void setup()
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX LVGL_Arduino_v8 example ");
  String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  eventServer.begin();
  subscribeToSonos();

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  lcd_reg_init();
  gfx->setRotation(ROTATION);
  gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

#ifdef DIRECT_RENDER_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 20;
#endif

#ifdef ESP32
#if defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
  disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
#else  // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf)
  {
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
#endif // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
#else // !ESP32
  Serial.println("LVGL disp_draw_buf heap_caps_malloc failed! malloc again...");
  disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
#endif // !ESP32
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
#ifdef DIRECT_RENDER_MODE
    disp_drv.direct_mode = true;
#endif
    lv_disp_drv_register(&disp_drv);

    ui_init();


  }
  pinMode(GFX_BL, OUTPUT);
  setBacklight(true);

  Serial.println("Setup done");
}

void loop()
{
  if (millis() - lastResubscribe > RESUBSCRIBE_INTERVAL) {
    subscribeToSonos();  // re-send SUBSCRIBE request
  }

  lv_timer_handler(); /* let the GUI do its work */

  if (displayOn && millis() - lastUpdateTime > DISPLAY_TIMEOUT_MS) {
    setBacklight(false); // Sleep display
  }

  delay(5);

  WiFiClient client = eventServer.available();
  if (client) {
    memset(request, 0, sizeof(request));
    unsigned long timeout = millis() + 1000;
    request_index = 0;

    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        if (request_index < MAX_REQUEST_SIZE - 1) {
          request[request_index++] = c;
        }
      }
    }
    request[request_index] = '\0';


    // Respond with HTTP 200 OK
    client.print("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    client.stop();

    char decoded[MAX_REQUEST_SIZE];
    memset(decoded, 0, sizeof(decoded));

    decodeEntities(request, decoded, MAX_REQUEST_SIZE);
    int volume = getVolumeFromXML(decoded);
    setVolumeOnScreen(volume);
    lastUpdateTime = millis();
    if (!displayOn) {
      setBacklight(true); // Wake up display
    }
  }
}

void decodeEntities(const char* input, char* output, size_t output_size) {
  size_t out_idx = 0;

  for (size_t i = 0; input[i] != '\0' && out_idx < output_size - 1; ++i) {
    if (strncmp(&input[i], "&lt;", 4) == 0) {
      if (out_idx < output_size - 2) output[out_idx++] = '<';
      i += 3;
    } else if (strncmp(&input[i], "&gt;", 4) == 0) {
      if (out_idx < output_size - 2) output[out_idx++] = '>';
      i += 3;
    } else if (strncmp(&input[i], "&quot;", 6) == 0) {
      if (out_idx < output_size - 2) output[out_idx++] = '"';
      i += 5;
    } else if (strncmp(&input[i], "&apos;", 6) == 0) {
      if (out_idx < output_size - 2) output[out_idx++] = '\'';
      i += 5;
    } else if (strncmp(&input[i], "&amp;", 5) == 0) {
      if (out_idx < output_size - 2) output[out_idx++] = '&';
      i += 4;
    } else {
      output[out_idx++] = input[i];
    }
  }

  output[out_idx] = '\0';  // Null-terminate the string
}

int getVolumeFromXML(const char* xml) {
  yxml_init(x, stack, sizeof(stack));
  char* tag = nullptr;
  char* attr = nullptr;
  const unsigned BUFFER_SIZE = 256;
  char value[BUFFER_SIZE];
  int volume = 0;

  int master_volume = 0;

  while (*xml) {
    r = yxml_parse(x, *xml);
    if (r == YXML_ELEMSTART) {
      tag = x->elem;
    }
    if (r == YXML_ATTRSTART) {
      attr = x->attr;
      memset(value, 0, sizeof(value));
    }
    if (r == YXML_ATTRVAL) {
      strncat(value, x->data, BUFFER_SIZE - strlen(value) - 1);
    }
    if (tag && attr && !strcmp(tag, "Volume") && !strcmp(attr, "channel") && !strcmp(value, "Master")) {
      master_volume = 1;
    }
    if (attr && master_volume && !strcmp(attr, "val") && r == YXML_ATTREND) {
      volume = atoi(value);
      master_volume = 0;
    }
    xml++;
  }
  return volume;
}

void setVolumeOnScreen(int volume)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", volume);
  lv_label_set_text(ui_VolumeLabel, buf);
  int barVolume = map(volume, 0, 56, 0, 100);
  lv_bar_set_value(ui_VolumeBar, barVolume, LV_ANIM_OFF);
}

void setBacklight(bool on) {
  digitalWrite(GFX_BL, on ? HIGH : LOW);
  displayOn = on;
}