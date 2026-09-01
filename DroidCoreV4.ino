#define LGFX_USE_V1

#include <LovyanGFX.hpp>

// ============================================================
// R2D2 COMPLEX MKIV
// ESP32-C3 1.28" 240x240 GC9A01
//
// Offline autonomous astromech diagnostic display.
//
// DESIGN PRINCIPLE:
// The display is divided into fixed zones.
//
// HEADER     : y 38..55
// ANIMATION  : y 62..142
// STATUS     : y 151..183
// FOOTER     : y 188..201
//
// NO animation is allowed to draw in the text zones.
// ============================================================


// ============================================================
// DISPLAY CONFIGURATION
// ============================================================

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Bus_SPI _bus;
  lgfx::Panel_GC9A01 _panel;

public:

  LGFX()
  {
    auto bus_cfg = _bus.config();

    bus_cfg.spi_host = SPI2_HOST;
    bus_cfg.spi_mode = 0;
    bus_cfg.freq_write = 80000000;
    bus_cfg.freq_read = 20000000;
    bus_cfg.spi_3wire = false;
    bus_cfg.use_lock = true;
    bus_cfg.dma_channel = SPI_DMA_CH_AUTO;

    bus_cfg.pin_sclk = 6;
    bus_cfg.pin_mosi = 7;
    bus_cfg.pin_miso = -1;
    bus_cfg.pin_dc   = 2;

    _bus.config(bus_cfg);
    _panel.setBus(&_bus);

    auto panel_cfg = _panel.config();

    panel_cfg.pin_cs = 10;
    panel_cfg.pin_rst = -1;
    panel_cfg.pin_busy = -1;

    panel_cfg.panel_width = 240;
    panel_cfg.panel_height = 240;

    panel_cfg.memory_width = 240;
    panel_cfg.memory_height = 240;

    panel_cfg.offset_x = 0;
    panel_cfg.offset_y = 0;
    panel_cfg.offset_rotation = 0;

    panel_cfg.dummy_read_pixel = 8;
    panel_cfg.dummy_read_bits = 1;

    panel_cfg.readable = false;
    panel_cfg.invert = true;
    panel_cfg.rgb_order = false;
    panel_cfg.dlen_16bit = false;
    panel_cfg.bus_shared = false;

    _panel.config(panel_cfg);

    setPanel(&_panel);
  }
};


// ============================================================
// OBJECTS
// ============================================================

LGFX lcd;
LGFX_Sprite frame(&lcd);


// ============================================================
// DISPLAY CONSTANTS
// ============================================================

const int SCREEN_W = 240;
const int SCREEN_H = 240;

const int CX = 120;
const int CY = 120;


// ============================================================
// SAFE DESIGN AREA
// ============================================================
//
// The blue bezel occupies approximately radius 105-116.
//
// We deliberately keep ALL UI inside radius 91.
//
// No graphic uses the outer region.
// ============================================================

const int UI_RADIUS = 91;


// ============================================================
// ZONES
// ============================================================

const int HEADER_Y = 46;

const int ANIM_TOP = 61;
const int ANIM_BOTTOM = 139;

const int STATUS_Y = 154;
const int VALUE_Y = 169;

const int FOOTER_Y = 190;


// ============================================================
// COLORS
// ============================================================

#define BLACK       0x0000

#define DEEP_NAVY   0x0108
#define NAVY        0x0114

#define BLUE        0x041F
#define BLUE2       0x03BF
#define CYAN        0x07FF

#define GREEN       0x07E0
#define DARK_GREEN  0x0340

#define YELLOW      0xFFE0
#define ORANGE      0xFD20

#define RED         0xF800
#define DARK_RED    0x5000

#define GREY        0x630C
#define LIGHT_GREY  0xB596
#define WHITE       0xFFFF


// ============================================================
// TIMING
// ============================================================

unsigned long lastFrame = 0;

const unsigned long FRAME_TIME = 33;


// ============================================================
// STATE MACHINE
// ============================================================

enum Screen
{
  SCREEN_IDLE,
  SCREEN_POWER,
  SCREEN_MEMORY,
  SCREEN_OPTICS,
  SCREEN_NAV,
  SCREEN_THERMAL,
  SCREEN_MOTORS,
  SCREEN_SIGNAL,
  SCREEN_PROCESSOR,
  SCREEN_CALIBRATION,
  SCREEN_SCAN
};

Screen currentScreen = SCREEN_IDLE;

unsigned long screenStarted = 0;
unsigned long screenDuration = 6000;


// ============================================================
// RANDOM
// ============================================================

float randomFloat(float minVal, float maxVal)
{
  return minVal +
         (float)random(0, 10000) /
         10000.0f *
         (maxVal - minVal);
}


// ============================================================
// SCREEN HELPERS
// ============================================================

float screenTime()
{
  return (millis() - screenStarted) / 1000.0f;
}


void textCenter(
  const char *text,
  int x,
  int y,
  uint16_t color,
  int size = 1)
{
  frame.setTextDatum(middle_center);
  frame.setTextColor(color);
  frame.setTextSize(size);

  frame.drawString(
    text,
    x,
    y
  );
}


void textLeft(
  const char *text,
  int x,
  int y,
  uint16_t color,
  int size = 1)
{
  frame.setTextDatum(middle_left);
  frame.setTextColor(color);
  frame.setTextSize(size);

  frame.drawString(
    text,
    x,
    y
  );
}


// ============================================================
// HEADER
// ============================================================

void drawHeader()
{
  // Very restrained.
  // This is deliberately NOT an animation area.

  textCenter(
    "R2D2",
    120,
    HEADER_Y,
    CYAN,
    1
  );

  frame.drawLine(
    69,
    HEADER_Y + 8,
    103,
    HEADER_Y + 8,
    NAVY
  );

  frame.drawLine(
    137,
    HEADER_Y + 8,
    171,
    HEADER_Y + 8,
    NAVY
  );

  // Small status lamps

  frame.fillCircle(
    61,
    HEADER_Y,
    2,
    BLUE
  );

  frame.fillCircle(
    179,
    HEADER_Y,
    2,
    BLUE
  );
}


// ============================================================
// FOOTER
// ============================================================

void drawFooter()
{
  textCenter(
    "R2D2 COMPLEX MKIV",
    CX,
    FOOTER_Y,
    BLUE,
    1
  );
}


// ============================================================
// STATUS AREA
// ============================================================

void drawStatus(
  const char *system,
  const char *value,
  uint16_t statusColor = GREEN)
{
  // Top status line

  textCenter(
    system,
    CX,
    STATUS_Y,
    LIGHT_GREY
  );

  // Main status value

  textCenter(
    value,
    CX,
    VALUE_Y,
    statusColor
  );
}


// ============================================================
// ANIMATION FRAME
// ============================================================
//
// The animation zone is intentionally small.
//
// x = 45..195
// y = 62..139
//
// Nothing else is allowed here.
// ============================================================

void clearAnimation()
{
  frame.fillRect(
    38,
    ANIM_TOP,
    164,
    ANIM_BOTTOM - ANIM_TOP,
    BLACK
  );
}


// ============================================================
// IDLE ANIMATION
// ============================================================
//
// A small central "astromech processor".
// ============================================================

void animationIdle()
{
  float t = screenTime();

  int cx = 120;
  int cy = 101;

  // Outer processor rings

  frame.drawCircle(
    cx,
    cy,
    32,
    DEEP_NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    25,
    NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    18,
    BLUE
  );

  // Rotating segments

  for (int i = 0; i < 8; i++)
  {
    float a =
      t * 0.8f +
      i * TWO_PI / 8.0f;

    int x1 =
      cx + cos(a) * 27;

    int y1 =
      cy + sin(a) * 27;

    int x2 =
      cx + cos(a) * 34;

    int y2 =
      cy + sin(a) * 34;

    frame.drawLine(
      x1,
      y1,
      x2,
      y2,
      i == 0
        ? CYAN
        : NAVY
    );
  }

  // Central eye

  float pulse =
    4 +
    sin(t * 3.0f) * 1.5f;

  frame.fillCircle(
    cx,
    cy,
    pulse + 4,
    BLUE
  );

  frame.fillCircle(
    cx,
    cy,
    pulse,
    CYAN
  );

  // Tiny orbiting indicators

  for (int i = 0; i < 4; i++)
  {
    float a =
      -t * 1.5f +
      i * TWO_PI / 4;

    int x =
      cx + cos(a) * 43;

    int y =
      cy + sin(a) * 30;

    frame.fillCircle(
      x,
      y,
      2,
      i == 0
        ? CYAN
        : BLUE
    );
  }
}


// ============================================================
// POWER ANIMATION
// ============================================================

void animationPower()
{
  float t = screenTime();

  // Battery

  int x = 62;
  int y = 76;
  int w = 116;
  int h = 28;

  frame.drawRoundRect(
    x,
    y,
    w,
    h,
    5,
    BLUE
  );

  frame.fillRect(
    x + w,
    y + 8,
    5,
    12,
    BLUE
  );

  int charge =
    70 +
    sin(t * 1.4f) * 7;

  int fill =
    (w - 6) *
    charge /
    100;

  frame.fillRoundRect(
    x + 3,
    y + 3,
    fill,
    h - 6,
    3,
    BLUE2
  );

  // Energy pulses

  int pulseX =
    x + 5 +
    ((int)(t * 80) %
     (w - 10));

  frame.fillCircle(
    pulseX,
    y + h / 2,
    2,
    CYAN
  );

  // Voltage marker

  textCenter(
    "3.3V BUS",
    CX,
    122,
    LIGHT_GREY
  );
}


// ============================================================
// MEMORY ANIMATION
// ============================================================

void animationMemory()
{
  float t = screenTime();

  const int cols = 8;
  const int rows = 4;

  const int cellW = 13;
  const int cellH = 11;

  int startX = 72;
  int startY = 70;

  int total =
    cols * rows;

  int active =
    (int)(t * 9) % total;

  for (int i = 0; i < total; i++)
  {
    int col = i % cols;
    int row = i / cols;

    int x =
      startX +
      col * cellW;

    int y =
      startY +
      row * cellH;

    uint16_t color;

    if (i < active)
    {
      color =
        i % 9 == 0
        ? CYAN
        : BLUE;
    }
    else
    {
      color = DEEP_NAVY;
    }

    frame.fillRect(
      x,
      y,
      9,
      7,
      color
    );
  }

  textCenter(
    "MEMORY MAP",
    CX,
    125,
    BLUE
  );
}


// ============================================================
// OPTICS ANIMATION
// ============================================================

void animationOptics()
{
  float t = screenTime();

  int cx = 120;
  int cy = 101;

  // Radar

  frame.drawCircle(
    cx,
    cy,
    18,
    DEEP_NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    31,
    DEEP_NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    43,
    NAVY
  );

  frame.drawLine(
    cx - 47,
    cy,
    cx + 47,
    cy,
    DEEP_NAVY
  );

  frame.drawLine(
    cx,
    cy - 47,
    cx,
    cy + 47,
    DEEP_NAVY
  );

  // Sweep

  float a =
    t * 2.0f;

  int x =
    cx + cos(a) * 44;

  int y =
    cy + sin(a) * 44;

  frame.drawLine(
    cx,
    cy,
    x,
    y,
    CYAN
  );

  // Moving target

  float ta =
    t * .65f;

  float tr =
    22 +
    sin(t * .9f) * 14;

  int tx =
    cx + cos(ta) * tr;

  int ty =
    cy + sin(ta) * tr;

  frame.fillCircle(
    tx,
    ty,
    3,
    CYAN
  );
}


// ============================================================
// NAVIGATION ANIMATION
// ============================================================

void animationNavigation()
{
  float t = screenTime();

  int cx = 120;
  int cy = 101;

  frame.drawCircle(
    cx,
    cy,
    37,
    NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    28,
    DEEP_NAVY
  );

  // Compass ticks

  for (int i = 0; i < 16; i++)
  {
    float a =
      i * TWO_PI / 16.0f;

    int x1 =
      cx + cos(a) * 31;

    int y1 =
      cy + sin(a) * 31;

    int x2 =
      cx + cos(a) * 37;

    int y2 =
      cy + sin(a) * 37;

    frame.drawLine(
      x1,
      y1,
      x2,
      y2,
      i % 4 == 0
        ? CYAN
        : BLUE
    );
  }

  // Heading

  float heading =
    t * .35f;

  int x =
    cx + cos(heading) * 25;

  int y =
    cy + sin(heading) * 25;

  frame.drawLine(
    cx,
    cy,
    x,
    y,
    CYAN
  );

  frame.fillCircle(
    cx,
    cy,
    4,
    BLUE
  );

  textCenter(
    "NAV LOCK",
    CX,
    129,
    GREEN
  );
}


// ============================================================
// THERMAL ANIMATION
// ============================================================

void animationThermal()
{
  float t = screenTime();

  int previousX = 45;
  int previousY = 101;

  for (int x = 49;
       x <= 195;
       x += 4)
  {
    float wave =
      sin(
        x * .10f +
        t * 2.5f
      ) * 8;

    wave +=
      sin(
        x * .23f -
        t * 1.4f
      ) * 3;

    int y =
      101 +
      wave;

    frame.drawLine(
      previousX,
      previousY,
      x,
      y,
      x % 20 == 0
        ? CYAN
        : BLUE
    );

    previousX = x;
    previousY = y;
  }

  // Threshold lines

  frame.drawLine(
    45,
    86,
    195,
    86,
    DEEP_NAVY
  );

  frame.drawLine(
    45,
    116,
    195,
    116,
    DEEP_NAVY
  );
}


// ============================================================
// MOTOR ANIMATION
// ============================================================

void animationMotors()
{
  float t = screenTime();

  const char *labels[] =
  {
    "A",
    "B",
    "C",
    "D"
  };

  for (int i = 0; i < 4; i++)
  {
    int y =
      70 +
      i * 15;

    textLeft(
      labels[i],
      53,
      y,
      LIGHT_GREY
    );

    frame.drawRoundRect(
      68,
      y - 4,
      108,
      8,
      3,
      DEEP_NAVY
    );

    int value =
      40 +
      sin(
        t * 1.5f +
        i * 1.1f
      ) * 30;

    frame.fillRoundRect(
      70,
      y - 2,
      104 * value / 100,
      4,
      2,
      BLUE
    );

    if (value > 70)
    {
      frame.fillCircle(
        182,
        y,
        2,
        CYAN
      );
    }
  }

  textCenter(
    "ACTUATOR ARRAY",
    CX,
    130,
    BLUE
  );
}


// ============================================================
// SIGNAL ANIMATION
// ============================================================

void animationSignal()
{
  float t = screenTime();

  for (int wave = 0;
       wave < 2;
       wave++)
  {
    int baseY =
      89 +
      wave * 27;

    int prevX = 45;
    int prevY = baseY;

    for (int x = 49;
         x <= 195;
         x += 4)
    {
      float signal =
        sin(
          x * .11f +
          t * 3.0f +
          wave
        );

      signal +=
        sin(
          x * .27f -
          t * 2.0f
        ) * .35f;

      int y =
        baseY +
        signal * 7;

      frame.drawLine(
        prevX,
        prevY,
        x,
        y,
        wave == 0
          ? CYAN
          : BLUE
      );

      prevX = x;
      prevY = y;
    }
  }
}


// ============================================================
// PROCESSOR ANIMATION
// ============================================================

void animationProcessor()
{
  float t = screenTime();

  int cx = 120;
  int cy = 100;

  // Chip

  frame.drawRoundRect(
    86,
    69,
    68,
    62,
    5,
    BLUE
  );

  frame.drawRoundRect(
    91,
    74,
    58,
    52,
    3,
    DEEP_NAVY
  );

  // Pins

  for (int i = 0; i < 6; i++)
  {
    int y =
      78 +
      i * 9;

    frame.drawLine(
      80,
      y,
      86,
      y,
      BLUE
    );

    frame.drawLine(
      154,
      y,
      160,
      y,
      BLUE
    );
  }

  for (int i = 0; i < 5; i++)
  {
    int x =
      95 +
      i * 12;

    frame.drawLine(
      x,
      63,
      x,
      69,
      BLUE
    );

    frame.drawLine(
      x,
      131,
      x,
      137,
      BLUE
    );
  }

  // Internal processor activity

  int active =
    (int)(t * 8) % 12;

  for (int i = 0; i < 12; i++)
  {
    int x =
      96 +
      (i % 4) * 13;

    int y =
      82 +
      (i / 4) * 13;

    frame.fillCircle(
      x,
      y,
      3,
      i == active
        ? CYAN
        : BLUE
    );
  }

  textCenter(
    "CPU",
    cx,
    118,
    LIGHT_GREY
  );
}


// ============================================================
// CALIBRATION ANIMATION
// ============================================================

void animationCalibration()
{
  float t = screenTime();

  int cx = 120;
  int cy = 101;

  frame.drawCircle(
    cx,
    cy,
    38,
    NAVY
  );

  frame.drawCircle(
    cx,
    cy,
    26,
    DEEP_NAVY
  );

  // Crosshair

  frame.drawLine(
    cx - 45,
    cy,
    cx + 45,
    cy,
    DEEP_NAVY
  );

  frame.drawLine(
    cx,
    cy - 45,
    cx,
    cy + 45,
    DEEP_NAVY
  );

  // Moving calibration point

  float a =
    t * 1.3f;

  float r =
    10 +
    sin(t * 1.7f) * 15;

  int x =
    cx + cos(a) * r;

  int y =
    cy + sin(a) * r;

  frame.fillCircle(
    x,
    y,
    4,
    CYAN
  );

  frame.drawCircle(
    x,
    y,
    9,
    BLUE
  );

  textCenter(
    "CALIBRATING",
    CX,
    130,
    YELLOW
  );
}


// ============================================================
// SYSTEM SCAN
// ============================================================

void animationScan()
{
  float t = screenTime();

  const int count = 6;

  for (int i = 0; i < count; i++)
  {
    int y =
      66 +
      i * 11;

    // Left node

    frame.fillCircle(
      55,
      y,
      2,
      BLUE
    );

    // Connection

    frame.drawLine(
      58,
      y,
      174,
      y,
      DEEP_NAVY
    );

    // Right node

    bool active =
      i ==
      ((int)(t * 2) % count);

    frame.fillCircle(
      178,
      y,
      3,
      active
        ? CYAN
        : BLUE
    );

    // Signal traveling

    if (active)
    {
      int px =
        60 +
        ((int)(t * 90) % 110);

      frame.fillCircle(
        px,
        y,
        2,
        CYAN
      );
    }
  }

  textCenter(
    "SYSTEM BUS SCAN",
    CX,
    132,
    BLUE
  );
}


// ============================================================
// ANIMATION DISPATCH
// ============================================================

void drawAnimation()
{
  clearAnimation();

  switch (currentScreen)
  {
    case SCREEN_IDLE:
      animationIdle();
      break;

    case SCREEN_POWER:
      animationPower();
      break;

    case SCREEN_MEMORY:
      animationMemory();
      break;

    case SCREEN_OPTICS:
      animationOptics();
      break;

    case SCREEN_NAV:
      animationNavigation();
      break;

    case SCREEN_THERMAL:
      animationThermal();
      break;

    case SCREEN_MOTORS:
      animationMotors();
      break;

    case SCREEN_SIGNAL:
      animationSignal();
      break;

    case SCREEN_PROCESSOR:
      animationProcessor();
      break;

    case SCREEN_CALIBRATION:
      animationCalibration();
      break;

    case SCREEN_SCAN:
      animationScan();
      break;
  }
}


// ============================================================
// STATUS DISPATCH
// ============================================================

void drawStatusForScreen()
{
  switch (currentScreen)
  {
    case SCREEN_IDLE:
      drawStatus(
        "ASTROMECH SYSTEM",
        "MONITORING",
        CYAN
      );
      break;

    case SCREEN_POWER:
      drawStatus(
        "POWER BUS",
        "NOMINAL",
        GREEN
      );
      break;

    case SCREEN_MEMORY:
      drawStatus(
        "MEMORY MATRIX",
        "STABLE",
        GREEN
      );
      break;

    case SCREEN_OPTICS:
      drawStatus(
        "OPTICAL ARRAY",
        "TRACKING",
        CYAN
      );
      break;

    case SCREEN_NAV:
      drawStatus(
        "NAVIGATION",
        "LOCKED",
        GREEN
      );
      break;

    case SCREEN_THERMAL:
      drawStatus(
        "THERMAL ARRAY",
        "NOMINAL",
        GREEN
      );
      break;

    case SCREEN_MOTORS:
      drawStatus(
        "MOTOR CONTROL",
        "READY",
        GREEN
      );
      break;

    case SCREEN_SIGNAL:
      drawStatus(
        "SIGNAL ANALYSIS",
        "CLEAN",
        GREEN
      );
      break;

    case SCREEN_PROCESSOR:
      drawStatus(
        "LOGIC PROCESSOR",
        "ACTIVE",
        CYAN
      );
      break;

    case SCREEN_CALIBRATION:
      drawStatus(
        "SENSOR ARRAY",
        "CALIBRATING",
        YELLOW
      );
      break;

    case SCREEN_SCAN:
      drawStatus(
        "SYSTEM SCAN",
        "RUNNING",
        CYAN
      );
      break;
  }
}


// ============================================================
// BEZEL
// ============================================================
//
// This is intentionally completely separate from UI.
// It is always drawn after everything else.
// ============================================================

void drawBezel()
{
  // Dark outer edge

  frame.drawCircle(
    CX,
    CY,
    116,
    DEEP_NAVY
  );

  frame.drawCircle(
    CX,
    CY,
    113,
    NAVY
  );

  // Blue ring

  frame.drawCircle(
    CX,
    CY,
    109,
    BLUE
  );

  frame.drawCircle(
    CX,
    CY,
    106,
    DEEP_NAVY
  );

  // Mechanical marks

  for (int i = 0; i < 48; i++)
  {
    float a =
      i * TWO_PI / 48.0f;

    int r1 =
      i % 4 == 0
      ? 106
      : 108;

    int r2 =
      i % 4 == 0
      ? 101
      : 105;

    int x1 =
      CX + cos(a) * r1;

    int y1 =
      CY + sin(a) * r1;

    int x2 =
      CX + cos(a) * r2;

    int y2 =
      CY + sin(a) * r2;

    frame.drawLine(
      x1,
      y1,
      x2,
      y2,
      i % 4 == 0
        ? BLUE2
        : NAVY
    );
  }

  // Three permanent "R2" indicator lights

  frame.fillCircle(
    120,
    12,
    2,
    CYAN
  );

  frame.fillCircle(
    31,
    120,
    2,
    BLUE
  );

  frame.fillCircle(
    209,
    120,
    2,
    BLUE
  );
}


// ============================================================
// FULL FRAME
// ============================================================

void renderFrame()
{
  // Entire framebuffer is constructed before sending it.

  frame.fillScreen(
    BLACK
  );

  // -----------------------------------------
  // UI
  // -----------------------------------------

  drawHeader();

  drawAnimation();

  drawStatusForScreen();

  drawFooter();

  // -----------------------------------------
  // Decorative bezel
  // -----------------------------------------

  drawBezel();

  // -----------------------------------------
  // ONE COMPLETE FRAME -> DISPLAY
  // -----------------------------------------

  frame.pushSprite(
    0,
    0
  );
}


// ============================================================
// CHANGE SCREEN
// ============================================================

void changeScreen(
  Screen newScreen,
  unsigned long duration)
{
  currentScreen = newScreen;

  screenStarted = millis();

  screenDuration = duration;
}


// ============================================================
// NEXT SCREEN
// ============================================================
//
// Some screens are deliberately more common than others.
// This should feel like an R2 unit doing diagnostics rather
// than a slideshow.
// ============================================================

void nextScreen()
{
  int r =
    random(0, 100);

  if (r < 22)
  {
    changeScreen(
      SCREEN_IDLE,
      random(4000, 7000)
    );
  }
  else if (r < 32)
  {
    changeScreen(
      SCREEN_SCAN,
      random(4500, 7000)
    );
  }
  else if (r < 41)
  {
    changeScreen(
      SCREEN_POWER,
      random(3500, 5500)
    );
  }
  else if (r < 50)
  {
    changeScreen(
      SCREEN_MEMORY,
      random(3500, 5500)
    );
  }
  else if (r < 60)
  {
    changeScreen(
      SCREEN_OPTICS,
      random(4000, 6000)
    );
  }
  else if (r < 69)
  {
    changeScreen(
      SCREEN_NAV,
      random(4000, 6000)
    );
  }
  else if (r < 77)
  {
    changeScreen(
      SCREEN_THERMAL,
      random(3500, 5500)
    );
  }
  else if (r < 85)
  {
    changeScreen(
      SCREEN_MOTORS,
      random(4000, 6000)
    );
  }
  else if (r < 91)
  {
    changeScreen(
      SCREEN_SIGNAL,
      random(4000, 6000)
    );
  }
  else if (r < 96)
  {
    changeScreen(
      SCREEN_PROCESSOR,
      random(3000, 5000)
    );
  }
  else
  {
    changeScreen(
      SCREEN_CALIBRATION,
      random(3500, 5000)
    );
  }
}


// ============================================================
// BOOT
// ============================================================

void boot()
{
  for (int progress = 0;
       progress <= 100;
       progress += 2)
  {
    frame.fillScreen(
      BLACK
    );

    textCenter(
      "R2D2",
      CX,
      70,
      CYAN,
      2
    );

    textCenter(
      "COMPLEX MKIV",
      CX,
      91,
      BLUE,
      1
    );

    textCenter(
      "INITIALIZING",
      CX,
      116,
      LIGHT_GREY
    );

    // Progress bar

    frame.drawRoundRect(
      50,
      132,
      140,
      9,
      4,
      NAVY
    );

    frame.fillRoundRect(
      53,
      135,
      134 * progress / 100,
      3,
      1,
      CYAN
    );

    char buf[16];

    sprintf(
      buf,
      "%03d%%",
      progress
    );

    textCenter(
      buf,
      CX,
      154,
      WHITE
    );

    textCenter(
      "ASTROMECH UNIT",
      CX,
      177,
      BLUE
    );

    drawBezel();

    frame.pushSprite(
      0,
      0
    );

    delay(20);
  }

  delay(500);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  randomSeed(
    micros()
  );

  // Backlight

  pinMode(
    3,
    OUTPUT
  );

  digitalWrite(
    3,
    HIGH
  );

  // Display

  lcd.init();

  lcd.setRotation(
    0
  );

  lcd.setColorDepth(
    16
  );

  // Full framebuffer.
  //
  // 240*240*2 = 115200 bytes.
  //
  // This gives us a complete frame that can be
  // transferred in one pushSprite() operation.

  frame.setColorDepth(
    16
  );

  if (!frame.createSprite(
        SCREEN_W,
        SCREEN_H))
  {
    lcd.fillScreen(
      BLACK
    );

    lcd.setTextDatum(
      middle_center
    );

    lcd.setTextColor(
      RED
    );

    lcd.drawString(
      "SPRITE ERROR",
      CX,
      CY
    );

    while (true)
    {
      delay(1000);
    }
  }

  boot();

  changeScreen(
    SCREEN_IDLE,
    6000
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now =
    millis();

  // Fixed ~30 FPS.
  //
  // This prevents unnecessary display writes and
  // keeps the animation smooth.

  if (
    now - lastFrame <
    FRAME_TIME
  )
  {
    return;
  }

  lastFrame = now;

  // Change diagnostic system

  if (
    now - screenStarted >=
    screenDuration
  )
  {
    nextScreen();
  }

  // Construct and send ONE frame.

  renderFrame();
}
