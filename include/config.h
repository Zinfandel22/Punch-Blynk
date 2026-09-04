#pragma once

#include <EEPROM.h>
extern unsigned long lastPunchCompleteTime;

// --- FIRMWARE DETAILS ---
#define Version "(c)2023-26 PIO-26-09-03"
#define Concept0 "Concept, Design & Build"
#define Concept1 "Tom Moller"
#define Concept2 "Dave Williams"

// --- HARDWARE PIN DEFINITIONS ---
#define ONE_WIRE_BUS 23  // Serial data wire is on pin 22 for dallas temp sensor
#define Sol1 25          // Solenoid 1 for Act1
#define Sol2 27          // Solenoid 2 for Act2
#define Sol3 29          // Solenoid 3 for Act3
#define Sol4 31          // Solenoid 4 for Act4

// --- LCD ANALOG CONTROL PINS ---
#define LCD_CS A3     // Chip Select goes to Analog 3
#define LCD_CD A2     // Command/Data goes to Analog 2
#define LCD_WR A1     // LCD Write goes to Analog 1
#define LCD_RD A0     // LCD Read goes to Analog 0
#define LCD_RESET A4  // Can alternately just connect to Arduino's reset pin
#define YP A3         // must be an analog pin, use "An" notation!
#define XM A2         // must be an analog pin, use "An" notation!
#define YM 9          // can be a digital pin
#define XP 8          // can be a digital pin

//DEFINE TOUCH POINTS FOR ILI9341 PANEL
//UPDATE AFTER RUNNING CALIBRATION ROUTINE
/*-------------------------------------------------------------------------------------------------*/
#define TS_LEFT 110
#define TS_RIGHT 920
#define TS_BOT 90
#define TS_TOP 920
#define MINPRESSURE 20
#define MAXPRESSURE 2000

// --- UI COLORS ---
#define BLACK 0x0000       /*   0,   0,   0 */
#define NAVY 0x000F        /*   0,   0, 128 */
#define DARKGREEN 0x03E0   /*   0, 128,   0 */
#define DARKCYAN 0x03EF    /*   0, 128, 128 */
#define MAROON 0x7800      /* 128,   0,   0 */
#define PURPLE 0x780F      /* 128,   0, 128 */
#define OLIVE 0x7BE0       /* 128, 128,   0 */
#define LIGHTGREY 0xC618   /* 192, 192, 192 */
#define DARKGREY 0x7BEF    /* 128, 128, 128 */
#define BLUE 0x001F        /*   0,   0, 255 */
#define GREEN 0x07E0       /*   0, 255,   0 */
#define CYAN 0x07FF        /*   0, 255, 255 */
#define RED 0xF800         /* 255,   0,   0 */
#define MAGENTA 0xF81F     /* 255,   0, 255 */
#define YELLOW 0xFFE0      /* 255, 255,   0 */
#define WHITE 0xFFFF       /* 255, 255, 255 */
#define ORANGE 0xFD20      /* 255, 165,   0 */
#define GREENYELLOW 0xAFE5 /* 173, 255,  47 */

// --- UI DIMENSIONS ---
#define BUTTON_W 80
#define BUTTON_H 50