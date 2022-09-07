// =================================================================================================
//   SHADOW :  Small Handheld Arduino Droid Operating Wand
// =================================================================================================
//   Originally Written By KnightShade
//   Heavily Modified By Ian Martin
//   Inspired by the PADAWAN by danf
//   Totally hacked by Eebel 22 Aug 2019 - Added Tabs, NeoPixel Dome Lighting, and MP3 Trigger
//    Adding ServoEasing library for realtime servo control
//      I also cleaned up some compiler Warnings due to different number type being compared like unsigned long to millis()
//
//   PS3 Bluetooth library - developed by Kristian Lauszus
//
//   Sabertooth (Foot Drive): Set Sabertooth 2x32 or 2x25 Dip Switches: 1 and 2 Down, All Others Up
//   SyRen 10 Dome Drive: For SyRen packetized Serial Set Switches: 1, 2 and 4 Down, All Others Up
// =================================================================================================
//  Last Hacked 9 Apr 2021

/*
 Works with the following versions
 NeoPatterns 2.0.0
 ServoEasing 1.4.1
 Servo 1.1.1
 Adafruit NeoPixel 1.3.2
 Adafruit PWM ServoDriver Libray 2.3.0
 USB HostSHield 1.5.0
 */

#include <Arduino.h>
// --------------------------------------------------------
//                      User Settings
// --------------------------------------------------------
//#define PrimaryPS3Controller

#ifdef PrimaryPS3Controller
   String PS3MoveNavigatonPrimaryMAC = "00:06:F5:13:C6:D5"; //controller 1. If using multiple controlers, designate a primary (MIKES: 00:07:04:EF:56:80
#else
   String PS3MoveNavigatonPrimaryMAC = "00:07:04:BA:6F:DF"; //controller 2.
#endif

//String PS3MoveNavigatonPrimaryMAC = "00:07:04:BA:6F:DF";//Controller 2

byte drivespeed1 = 50; //was 50;set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
byte drivespeed2 = 90; //was 80.  Recommend beginner: 50 to 75

byte turnspeed = 50; //was 58.  50 - the higher this number the faster it will spin in place, lower - easier to control.

byte domespeedfast = 127; // Use a number up to 127
byte domespeedslow = 80; // Use a number up to 127
byte domespeed = domespeedfast; // Use a number up to 127 was domespeedslow...I like it faster

byte ramping = 3; //was 7 3 - Ramping, the lower this number the longer R2 will take to speedup or slow down,

byte joystickFootDeadZoneRange = 18;  //15 For controllers that centering problems, use the lowest number with no drift
byte joystickDomeDeadZoneRange = 12;  //10 For controllers that centering problems, use the lowest number with nfo drift
byte driveDeadBandRange = 10; // Used to set the Sabertooth DeadZone for foot motors

int invertTurnDirection = -1;   //This may need to be set to 1 for some configurations

#define SHADOW_DEBUG       //uncomment this for console DEBUG output
#define EEBEL_TEST    //Uncomment to Change things back from my test code
#define SERVO_EASING //Comment out to use Adafruit PWM code instead.

#define NEOPIXEL_TEST //Uncomment to not use test NEOPIXEL test code

// --------------------------------------------------------
//                Drive Controller Settings
// --------------------------------------------------------

int motorControllerBaudRate = 9600; // Set the baud rate for the Syren motor controller. For packetized options are: 2400, 9600, 19200 and 38400

#define SYREN_ADDR         129      // Serial Address for Dome Syren
#define SABERTOOTH_ADDR    128      // Serial Address for Foot Sabertooth
#define ENABLE_UHS_DEBUGGING 1

#define smDirectionPin  2 // Zapper Direction pin
#define smStepPin  3 //Zapper Stepper pin
#define smEnablePin  4 //Zapper Enable pin
#define PIN_DOMENEOPIXELS 6
#define PIN_ZAPPER 7
#define PIN_BUZZSAW 8 
#define  PIN_BADMOTIVATOR 9
#define PIN_DOMECENTER 49
#define PIN_BUZZSLIDELIMIT 24

#define SHADOW_VERBOSE     //uncomment this for console VERBOSE output


// --------------------------------------------------------
//                       Libraries
// --------------------------------------------------------

//#include <Arduino.h>
#include <PS3BT.h>  //PS3 Bluetooth Files
#include <SPP.h>
#include <usbhub.h>
#include <Sabertooth.h>
#include <Servo.h>
#include <Wire.h>
//#include <Adafruit_PWMServoDriver.h> // I think I don't need this anymore due to ServoEasing library
//#include <Adafruit_Soundboard.h>

#include <NeoPatterns.h>
#include <MP3Trigger.h>
/*
 * !!! Uncomment use PCA9865 in ServoEasing.h to make the expander work !!!
 * Otherwise you will see errors like: "PCA9685_Expander:44:46: error: 'Wire' was not declared in this scope"
 */
#include "ServoEasing.h"
/*
 Works with the following versions
 NeoPatterns 2.0.0
 ServoEasing 1.4.1
 Servo 1.1.1
 Adafruit NeoPixel 1.3.2
 Adafruit PWM ServoDriver Libray 2.3.0
 USB HostSHield 1.5.0
 */

#pragma mark -
#pragma mark Declarations

//Declarations-----------------------------------------------
void swapPS3NavControllers(void);
void moveUtilArms(int arm, int direction, int position, int setposition);

//Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, NULL);

// --------------------------------------------------------
//                        Variables
// --------------------------------------------------------

unsigned long currentMillis = millis();
unsigned long flutterMillis = 0;
unsigned long previousDomeMillis = millis();
unsigned long previousFootMillis = millis();
unsigned long stoppedDomeMillis = 0;
unsigned long startDomeMillis = 0;
boolean domeSpinning = false;
int lastDomeRotationSpeed = 0;
int startDomeRotationSpeed = 0;
int autoDomeSetSpeed = 0;
unsigned long autoDomeSetTime = 0;
boolean head_right = true;
boolean flutterActive = false;
int lastSoundPlayed = 0;
int lastLastSoundPlayed = 0; // Im an idiot, I know.

//int serialLatency = 25;   //This is a delay factor in ms to prevent queueing of the Serial data. 25ms seems approprate for HardwareSerial
unsigned long serialLatency = 25; //removes compiler warning for comparisons of differnt number types set it to 30..Does this make for more reliable PS3 connections?
unsigned long motivatorMillis = millis();
unsigned long smokeMillis = millis();
unsigned long blinkMillis = millis();
unsigned long lastBlinkMillis;
boolean smokeTriggered = false;
boolean motivatorTriggered = false;

int centerDomeSpeed = 0;
int centerDomeHighCount = 0;
int centerDomeLowCount = 0;
int centerDomeRotationCount = 0;
boolean domeCentered = false;
boolean domeCentering = false;

boolean droidFreakOut = false;
boolean droidFreakingOut = false;
boolean droidFreakedOut = false;
boolean droidFreakOutComplete = false;
boolean droidScream = false;
boolean gripperActivated = false;
boolean interfaceActivated = false;
boolean interfaceOut = false;
boolean utilArmClosed = true;
boolean drawerActivated = false;

boolean piePanelsOpen = false;
boolean sidePanelsOpen = false;
boolean midPanelsOpen = false;
boolean buzzDoorOpen = false;
boolean utilityGrab = false;

unsigned long freakOutMillis = millis();

boolean lifeformActivated = false;
unsigned long lifeformMillis = millis();
unsigned long lifeformLEDMillis = millis();
int lifeformPosition = 0;
int lifeformDir = 0;
int lifeformLED = 0;

int lastUtilStickPos = 127;
unsigned long lastUtilArmMillis = millis();

#ifdef SERVO_EASING
    int utilArmMax0 = 180; //180, 450, 600 Fully Open UPPER
    int utilArmMin0 = 0; //43, 87, 275 Fully Closed UPPER
    int utilArmMax1 = 180; //43, 87 Fully Open  LOWER
    int utilArmMin1 = 0;  //180, 450, 320 Fully Closed LOWER
#else
    int utilArmMax0 = 450; //450, 600 Fully Open UPPER
    int utilArmMin0 = 75; //87, 275 Fully Closed UPPER
    int utilArmMax1 = 75; //87 Fully Open  LOWER
    int utilArmMin1 = 450;  //450, 320 Fully Closed LOWER
#endif
    int lastUtilArmPos0 = utilArmMin0;
    int lastUtilArmPos1 = utilArmMin1;


#ifdef SERVO_EASING
    struct ServoInfo {
    uint16_t minDeg;
    uint16_t maxDeg;
    int pinNum;
    int pwmNum;
    String servoName;
    };

    ServoInfo pwm3_1Info;
    ServoInfo pwm3_2Info;
    ServoInfo pwm3_3Info;
    ServoInfo pwm3_4Info;
    ServoInfo pwm3_5Info;
    ServoInfo pwm3_6Info;
    ServoInfo pwm3_7Info;
    ServoInfo pwm3_8Info;
    ServoInfo pwm3_9Info;

    ServoInfo pwm2_1Info;
    ServoInfo pwm2_4Info;
    ServoInfo pwm2_6Info;
    ServoInfo pwm2_7Info;
    ServoInfo pwm2_8Info;
    ServoInfo pwm2_9Info;
    ServoInfo pwm2_11Info;
    ServoInfo pwm2_12Info;
    ServoInfo pwm2_13Info;
    ServoInfo pwm2_14Info;
    ServoInfo pwm2_15Info;

    ServoInfo pwm1_4Info;


    //SERVO_EASING Parameters//////////////////////
//    const int pwm3Pie1_Pin = 1;
//    const int pwm3Pie2_Pin = 2;
//    const int pwm3Pie3_Pin = 3;
//    const int pwm3Pie4_Pin = 4;
//    const int pwm3Pie5_Pin = 5;
//    const int pwm3Pie6_Pin = 6;
//    const int pwm3Pie7_Pin = 7;
//    const int pwm3LifeSlide_Pin = 8;
//    const int pwm3LifeTurn_Pin = 9;
//
//    const int pwm2BuzzSlide_Pin = 1;
//    const int pwm2LeftBreadPan_Pin = 4;
//    const int pwm2RightBreadPan_Pin = 6;
//    const int pwm2CBIDoor_Pin = 7;
//    const int pwm2LowerUtil_Pin = 8;
//    const int pwm2UpperUtil_Pin = 9;
//    const int pwm2BuzzDoor_Pin = 11;
//    const int pwm2GripperLift_Pin = 12;
//    const int pwm2Gripper_Pin = 13;
//    const int pwm2InterfaceLift_Pin = 14;
//    const int pwm2Interface_Pin = 15;
//
//    const int pwm1DataPanel_Pin = 4;
//
//    //const int pwm1_DataPanel = 4;
//    //const int pwm2_RightBreadPanDoor = 6;
//
//    /////0-180 Degrees
//    int pwm3_1_max = 123; // Pie 1 -
//    int pwm3_1_min = 73; // Pie 1 - Higher Number is lower position
//    int pwm3_2_max = 115; // Pie 2
//    int pwm3_2_min = 70; // Pie 2
//    int pwm3_3_max = 115; // Pie 3
//    int pwm3_3_min = 70; // Pie 3
//    int pwm3_4_max = 140 ; // Pie 4
//    int pwm3_4_min = 79; // Pie 4
//    int pwm3_5_max = 127; // Pie 5
//    int pwm3_5_min = 74; // Pie 5
//    int pwm3_6_max = 108; // Pie 6
//    int pwm3_6_min = 68; // Pie 6
//    int pwm3_7_max = 174;  // Pie 7
//    int pwm3_7_min = 92; // Pie 7
//    int pwm3_8_max = 174;  // Lifeform Slide
//    int pwm3_8_min = 92; // Lifeform Slide
//    int pwm3_9_max = 174;  // Lifeform Turn
//    int pwm3_9_min = 92; // Lifeform Turn
////    int pwm3_10_max = 2047; // HEAD LED
////    int pwm3_11_max = 2047; // HEAD LED
////    int pwm3_12_max = 2047; // HEAD LED
////    int pwm3_13_max = 2047; // HEAD LED
////    int pwm3_14_max = 2047; // HEAD LED
////    int pwm3_15_max = 2047; // LIFEFORM LED
//
//    int pwm2_0_max = 275; // ZAPPER SLIDE
//    int pwm2_0_min = 600; // 600ZAPPER SLIDE
//    int pwm2_1_max = 182; // BUZZ SLIDE
//    int pwm2_1_min = 56; // 800 BUZZ SLIDE
////    int pwm2_2_max = 50;  //
////    int pwm2_2_min = 250; //
////    int pwm2_3_max = 380; //
////    int pwm2_3_min = 850; //
//    int pwm2_4_max = 60; // Left BreadPan Door 359
//    int pwm2_4_min = 90; // Left BreadPanDoor 540
////    int pwm2_5_max = 310; //
////    int pwm2_5_min = 550; //
//    int pwm2_6_max = 93; // 410 Right BreadPan Door
//    int pwm2_6_min = 61; // 560 Right BreadPan Door
//    int pwm2_7_max = 63; // 410, 370,410 CBI Door
//    int pwm2_7_min = 47; // 500,520,540,550,580 CBI Door
//    //  pwm2_8 = Lower Arm
//    //  pwm2_9 = Uppare Arm
//    //  int pwm2_10_max = 270; // CARD DISPENSER
//    int pwm2_11_max = 165; // 250 BUZZ DOOR 370
//    int pwm2_11_min = 94; // BUZZ DOOR 700
//    int pwm2_12_max = 130; // 545,200 GRIPPER LIFTER
//    int pwm2_12_min = 68; // 200,545 GRIPPER LIFTER
//    int pwm2_13_max = 120; // 400 GRIPPER
//    int pwm2_13_min = 68; // 800 GRIPPER
//    int pwm2_14_max = 11; // 500 INTERFACE LIFTER
//    int pwm2_14_min = 169; // 30 INTERFACE LIFTER
//    int pwm2_15_max = 123; // 550 NTERFACE
//    int pwm2_15_min = 82; // 120 INTERFACE
//
////    int pwm1_14_max = 350; // LEGO
////    int pwm1_14_min = 100; // LEGO
////    int pwm1_15_max = 270; // EXTINGUISHER
////    int pwm1_15_min = 600; // EXTINGUISHER
////    int pwm1_12_max = 275; // Panel Drawer
////    int pwm1_12_min = 600; // Panel Drawer
////    int pwm1_11_max = 2047; // Panel Drawer LED
////    int pwm1_10_max = 600; // Rear Bolt Door
////    int pwm1_10_min = 100; // Rear Bolt Door
//    int pwm1_4_max = 37; // 475, 540 DATA PANEL 359
//    int pwm1_4_min = 47; // 375,350DATA PANEL 540

#else
#pragma mark -
#pragma mark AdafruitServoLimits
    ////Adafruit PWM Parameters///////////////////
    int pwm3_1_max = 220; // 200Pie 1 -
    int pwm3_1_min = 370; // 350 Pie 1 - Higher Number is lower position
    int pwm3_2_max = 235; // 220 185 Pie 2
    int pwm3_2_min = 385; // 370 Pie 2
    int pwm3_3_max = 220; // 195 175 140 Pie 3
    int pwm3_3_min = 360; // 350 Pie 3
    int pwm3_4_max = 205 ; // Pie 4
    int pwm3_4_min = 365; // 350 Pie 4
    int pwm3_5_max = 220; // Pie 5
    int pwm3_5_min = 375; // 360 350 Pie 5
    int pwm3_6_max = 275; // Pie 6
    int pwm3_6_min = 440; // Pie 6
    int pwm3_7_max = 140; // Pie 7
    int pwm3_7_min = 285; // 275 Pie 7
    int pwm3_8_max = 275; // Lifeform Slide
    int pwm3_8_min = 600; // Lifeform Slide
    int pwm3_9_max = 380; // Lifeform Turn
    int pwm3_9_min = 130; // Lifeform Turn
    int pwm3_10_max = 2047; // HEAD LED
    int pwm3_11_max = 2047; // HEAD LED
    int pwm3_12_max = 2047; // HEAD LED
    int pwm3_13_max = 2047; // HEAD LED
    int pwm3_14_max = 2047; // HEAD LED
    int pwm3_15_max = 2047; // LIFEFORM LED

    int pwm2_0_max = 275; // ZAPPER SLIDE
    int pwm2_0_min = 600; // 600ZAPPER SLIDE
    int pwm2_1_max = 275; // BUZZ SLIDE
    int pwm2_1_min = 600; // 400, 800 BUZZ SLIDE
    int pwm2_2_max = 50;  //
    int pwm2_2_min = 250; //
    int pwm2_3_max = 380; //
    int pwm2_3_min = 850; //
    int pwm2_4_max = 565; // 540,Left BreadPan Door 359 Gripper
    int pwm2_4_min = 359; // Left BreadPanDoor 540
    int pwm2_5_max = 310; //
    int pwm2_5_min = 550; //
    int pwm2_6_max = 390; // 400,350,410 Right BreadPan Door Interface
    int pwm2_6_min = 600; // 540,530, 560 Right BreadPan Door
    int pwm2_7_max = 370; // 365, 410, 370,410 CBI Door
    int pwm2_7_min = 580; // 495,500,520,540,550,580 CBI Door
    //  pwm2_8 = Lower Arm
    //  pwm2_9 = Uppare Arm
    int pwm2_10_max = 270; // CARD DISPENSER
    int pwm2_11_max = 220; // 250 BUZZ DOOR 370
    int pwm2_11_min = 475; // 460, BUZZ DOOR 700
    int pwm2_12_max = 545; // 545,200 GRIPPER LIFTER
    int pwm2_12_min = 150; // 200,545 GRIPPER LIFTER
    int pwm2_13_max = 390; // 400 GRIPPER
    int pwm2_13_min = 650; // 800 GRIPPER
    int pwm2_14_max = 30; // 500 INTERFACE LIFTER
    int pwm2_14_min = 500; // 30 INTERFACE LIFTER
    int pwm2_15_max = 525; // 550 NTERFACE
    int pwm2_15_min = 165; // 120 INTERFACE

    int pwm1_14_max = 350; // LEGO
    int pwm1_14_min = 100; // LEGO
    int pwm1_15_max = 270; // EXTINGUISHER
    int pwm1_15_min = 600; // EXTINGUISHER
    int pwm1_12_max = 275; // Panel Drawer
    int pwm1_12_min = 600; // Panel Drawer
    int pwm1_11_max = 2047; // Panel Drawer LED
    int pwm1_10_max = 600; // Rear Bolt Door
    int pwm1_10_min = 100; // Rear Bolt Door
    int pwm1_4_max = 530; // 485, 475, 540 DATA PANEL 359
    int pwm1_4_min = 385; // 375,350DATA PANEL 540
#endif

boolean buzzActivated = false;
boolean buzzSpinning = false;
unsigned long buzzSawMillis = millis();
unsigned long buzzSawSlideInMillis = millis();
boolean buzzSawSliding = false;

/*
 * I Moved these variables to #define statements so they would all be in the same place
int smDirectionPin = 2; // Zapper Direction pin
int smStepPin = 3; //Zapper Stepper pin
int smEnablePin = 4; //Zapper Enable pin
 */
boolean zapperActivated = false;

boolean isStickEnabled = true;
byte isAutomateDomeOn = true;

unsigned long automateSoundMillis = 0;
unsigned long automateDomeMillis = 0;
byte automateAction = 0;

Sabertooth *ST=new Sabertooth(SABERTOOTH_ADDR, Serial2);
Sabertooth *SyR=new Sabertooth(SYREN_ADDR, Serial2);

const int HOLO_DELAY = 5000; //up to 20 second delay

uint32_t holoFrontRandomTime = 0;
uint32_t holoFrontLastTime = 0;

  #ifdef SERVO_EASING
      ///ServoEasingSetup////////////////////////////////////////////////
//      ServoEasing pwm1_Servo1(0x40, &Wire);
//      ServoEasing pwm1_Servo2(0x40, &Wire);
//      ServoEasing pwm1_Servo3(0x40, &Wire);
      ServoEasing pwm1_Servo4(0x40, &Wire); //Data Panel Door


      ServoEasing pwm2_Servo1(0x41, &Wire); //BuzzSlide
//      ServoEasing pwm2_Servo2(0x41, &Wire); //Motivator
//      ServoEasing pwm2_Servo3(0x41, &Wire); //Zapper 3-7
      ServoEasing pwm2_Servo4(0x41, &Wire); //Left BreadPan Door
//      ServoEasing pwm2_Servo5(0x41, &Wire);
      ServoEasing pwm2_Servo6(0x41, &Wire); //RightBread Pan Door
      ServoEasing pwm2_Servo7(0x41, &Wire); //CBI Door
      ServoEasing pwm2_Servo8(0x41, &Wire); //Lower Utility Arm
      ServoEasing pwm2_Servo9(0x41, &Wire); //Upper Utility Arm
      ServoEasing pwm2_Servo11(0x41, &Wire); //Buzz Door
      ServoEasing pwm2_Servo12(0x41, &Wire); //GripperLift
      ServoEasing pwm2_Servo13(0x41, &Wire); //Gripper
      ServoEasing pwm2_Servo14(0x41, &Wire); //Interface Lift
      ServoEasing pwm2_Servo15(0x41, &Wire); //Interface

      ServoEasing pwm3_Servo1(0x42, &Wire); //Pie 1
      ServoEasing pwm3_Servo2(0x42, &Wire); //Pie 2
      ServoEasing pwm3_Servo3(0x42, &Wire); //Pie 3
      ServoEasing pwm3_Servo4(0x42, &Wire); //Pie 4
      ServoEasing pwm3_Servo5(0x42, &Wire); //Pie 5
      ServoEasing pwm3_Servo6(0x42, &Wire); //Pie 6
      ServoEasing pwm3_Servo7(0x42, &Wire); //Pie 7
      ServoEasing pwm3_Servo8(0x42, &Wire); //LifeSlide
      ServoEasing pwm3_Servo9(0x42, &Wire); //LifeTurn

      ///////////////////////////////////////////////////////////////////
  #else
//      Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
//      Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);
//      Adafruit_PWMServoDriver pwm3 = Adafruit_PWMServoDriver(0x42);
      ServoEasing pwm1(0x40, &Wire);
      ServoEasing pwm2(0x41, &Wire);
      ServoEasing pwm3(0x42, &Wire);
  #endif

/////// Setup for USB and Bluetooth Devices

USB Usb;
BTD Btd(&Usb); // You have to create the Bluetooth Dongle instance like so
PS3BT *PS3Nav=new PS3BT(&Btd);
PS3BT *PS3Nav2=new PS3BT(&Btd);

//Used for PS3 Fault Detection
uint32_t msgLagTime = 0;
uint32_t lastMsgTime = 0;
uint32_t currentTime = 0;
uint32_t lastLoopTime = 0;
int badPS3Data = 0;

SPP SerialBT(&Btd,"Astromech:R5","1977"); // Create a BT Serial device(defaults: "Arduino" and the pin to "0000" if not set)
boolean firstMessage = true;
String output = "";

boolean isFootMotorStopped = true;
boolean isDomeMotorStopped = true;

boolean isPS3NavigatonInitialized = false;
boolean isSecondaryPS3NavigatonInitialized = false;

//------------------------NeoPixel Setup-------------------------------------
#ifdef NEOPIXEL_TEST
  void ownPatterns(NeoPatterns * aLedsPtr);
  NeoPatterns bar16 = NeoPatterns(8, PIN_DOMENEOPIXELS, NEO_GRB + NEO_KHZ800, &ownPatterns);
#else
  //onComplete callback functions
  void allPatterns(NeoPatterns * aLedsPtr);
  NeoPatterns bar16 = NeoPatterns(8, PIN_DOMENEOPIXELS, NEO_GRB + NEO_KHZ800, &allPatterns);
#endif


//----------------MP3 Trigger Sound------------------------------------------
MP3Trigger trigger;//make trigger object
int droidSoundLevel = 80; //60Amount of attenuation  higher=LOWER volume..ten is pretty loud 0-127

///////////////////////////////////////////////////////////////===============================================================
void silenceMiddleServos(){
  #ifdef SERVO_EASING
   delay(400);
   pwm1_Servo4.detach();
   pwm2_Servo7.detach();
   pwm2_Servo11.detach();
   #endif
}
void silenceServos(){
  #ifdef SERVO_EASING
      /////////////////////////////////////////////////
      //TODO:  stopAllServos(); does this work?
   //stopAllServos();
      pwm3_Servo1.detach(); //Pie1
      pwm3_Servo2.detach(); //Pie2
      pwm3_Servo3.detach(); //Pie3
      pwm3_Servo4.detach(); //Pie4
      pwm3_Servo5.detach(); //Pie5
      pwm3_Servo6.detach(); //Pie6
      pwm3_Servo7.detach(); //Pie7

      pwm2_Servo1.detach(); //BuzzSlide
      pwm2_Servo4.detach(); //LeftBreadPan
      pwm2_Servo6.detach(); //RightBreadPan
      pwm2_Servo7.detach(); //CBI Door
      pwm2_Servo8.detach(); //LowerUtil
      pwm2_Servo9.detach(); //UpperUtil
      pwm2_Servo11.detach(); //Buzz Door
      pwm2_Servo12.detach();  //GripperLift
      pwm2_Servo13.detach(); //Gripper
      pwm2_Servo14.detach(); //Interface Lift
      pwm2_Servo15.detach(); //Interface

//if (!pwm1_Servo4.isMoving()){
      pwm1_Servo4.detach();  //DataPanel Door
//}
      ////////////////////////////////////////////////
  #else
    pwm2.setPWM(2, 0, 0); // MOTIVATOR
    pwm2.setPWM(3, 0, 0); // Zapper
    pwm2.setPWM(4, 0, 0); // Zapper
    pwm2.setPWM(5, 0, 0); // Zapper
    pwm2.setPWM(6, 0, 0); // Zapper
    pwm2.setPWM(7, 0, 0); // Zapper
    pwm2.setPWM(8, 0, 0); // Lower Utilty Arm
    pwm2.setPWM(9, 0, 0); // Upper Utilty Arm
    pwm2.setPWM(11, 0, 0); // Buzz Door
    pwm2.setPWM(12, 0, 0); // GRIPPER
    pwm2.setPWM(13, 0, 0); // GRIPPER
    pwm2.setPWM(14, 0, 0); // GRIPPER
    pwm2.setPWM(15, 0, 0); // GRIPPER

    pwm3.setPWM(0, 0, 0); // TOP PANNEL
    pwm3.setPWM(1, 0, 0); // PIE PANEL 1
    pwm3.setPWM(2, 0, 0); // PIE PANEL 2
    pwm3.setPWM(3, 0, 0); // PIE PANEL 3
    pwm3.setPWM(4, 0, 0); // PIE PANEL 4
    pwm3.setPWM(5, 0, 0); // PIE PANEL 5
    pwm3.setPWM(6, 0, 0); // SIDE PANEL 7
    pwm3.setPWM(7, 0, 0); // SIDE PANEL 7

    pwm1.setPWM(15, 0, 0); // EXTINGUISHER
    pwm1.setPWM(14, 0, 0); // Rear Door Lego
    pwm1.setPWM(10, 0, 0); // Rear Door Bolt
    pwm1.setPWM(4,0, 0);// DataPanel
   #endif
}



///////////////////////////////////////////////////////////////================================================================
// =======================================================================================
// //////////////////////////Process PS3 Controller Fault Detection///////////////////////
// =======================================================================================
boolean criticalFaultDetect(){
    if (PS3Nav->PS3NavigationConnected || PS3Nav->PS3Connected){
        lastMsgTime = PS3Nav->getLastMessageTime();
        currentTime = millis();
        if ( currentTime >= lastMsgTime){
          msgLagTime = currentTime - lastMsgTime;
        }else{
             #ifdef SHADOW_DEBUG
               output += "Waiting for PS3Nav Controller Data\r\n";
             #endif
             badPS3Data++;
             msgLagTime = 0;
        }

        if (msgLagTime > 100 && !isFootMotorStopped){
            #ifdef SHADOW_DEBUG
              output += "It has been 100ms since we heard from the PS3 Controller\r\n";
              output += "Shut downing motors, and watching for a new PS3 message\r\n";
            #endif
            ST->stop();
            SyR->stop();
            isFootMotorStopped = true;
            return true;
        }
        if ( msgLagTime > 30000 ){
            //was 30000 I set to 3
            #ifdef SHADOW_DEBUG
              output += "It has been 30s since we heard from the PS3 Controller\r\n";
              output += "msgLagTime:";
              output += msgLagTime;
              output += "  lastMsgTime:";
              output += lastMsgTime;
              output += "  millis:";
              output += millis();
              output += "\r\nDisconnecting the controller.\r\n";
            #endif
            PS3Nav->disconnect();
        }

        //Check PS3 Signal Data
        if(!PS3Nav->getStatus(Plugged) && !PS3Nav->getStatus(Unplugged)){
            // We don't have good data from the controller.
            //Wait 10ms, Update USB, and try again
            delay(10);
            Usb.Task();
            if(!PS3Nav->getStatus(Plugged) && !PS3Nav->getStatus(Unplugged)){
                badPS3Data++;
                #ifdef SHADOW_DEBUG
                    output += "\r\nInvalid data from PS3 Controller.";
                #endif
                return true;
            }
        }else if (badPS3Data > 0){
            //output += "\r\nPS3 Controller  - Recovered from noisy connection after: ";
            //output += badPS3Data;
            badPS3Data = 0;
        }

        if ( badPS3Data > 10 ){
            #ifdef SHADOW_DEBUG
                output += "Too much bad data coming fromo the PS3 Controller\r\n";
                output += "Disconnecting the controller.\r\n";
            #endif
            PS3Nav->disconnect();
        }
    }else if (!isFootMotorStopped){
        #ifdef SHADOW_DEBUG
            output += "No Connected Controllers were found\r\n";
            output += "Shuting downing motors, and watching for a new PS3 message\r\n";
        #endif
        ST->stop();
        SyR->stop();
        isFootMotorStopped = true;
        return true;
    }
    return false;
}
// =======================================================================================
// //////////////////////////END of PS3 Controller Fault Detection///////////////////////
// =======================================================================================




boolean readUSB(){


    Usb.Task();
    #ifdef EEBEL_TEST
    //Serial.print(F("\r\nEebelUSB8"));
          //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
          if (PS3Nav->PS3NavigationConnected)
          {
              if (criticalFaultDetect())
              {
                  //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
                  //printOutput();
                  return false;
              }

          } else if (!isFootMotorStopped)
          {
              #ifdef SHADOW_DEBUG
                  output += "No foot controller was found\r\n";
                  output += "Shuting down motors, and watching for a new PS3 foot message\r\n";
              #endif
              ST->stop();
              isFootMotorStopped = true;
              drivespeed1 = 0;
              //WaitingforReconnect = true;
          }

//          if (PS3Nav2->PS3NavigationConnected)
//          {
//
//              if (criticalFaultDetectDome())
//              {
//                 //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
//                 printOutput();
//                 return false;
//              }
//          }

          return true;
  #else
        //Old R5 code
        Serial.println("\n\rRead USB");
        //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
        Usb.Task();
        if (PS3Nav->PS3NavigationConnected) Usb.Task();
        if (PS3Nav2->PS3NavigationConnected) Usb.Task();
        if (criticalFaultDetect()){
          //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
          flushAndroidTerminal();
          Serial.print(F("\r\nPS3 Controller Fault!"));
          return false;
        }
        return true;

    #endif
}









// ==============================================================
//                          SOUND STUFF
// ==============================================================

void processSoundCommand(int soundCommand, int soundCommand2 = 0){
  // Takes two track numbers and selects a random track.
  //Then, it builds a string to call for that track name.
    if (soundCommand2 != 0){ //If it is == 0, then there is only one track in the list
      soundCommand = random(soundCommand,soundCommand2);
      if (soundCommand == lastSoundPlayed || soundCommand2 == lastLastSoundPlayed) soundCommand = random(soundCommand,soundCommand2);
      //No idea why this is here three times.  Also, why compares to the same thing on either side of the or.
      //if (soundCommand == lastSoundPlayed || soundCommand == lastLastSoundPlayed) soundCommand = random(soundCommand,soundCommand2);
      //if (soundCommand == lastSoundPlayed || soundCommand == lastLastSoundPlayed) soundCommand = random(soundCommand,soundCommand2);
      if (soundCommand != lastSoundPlayed && soundCommand2 != lastLastSoundPlayed) lastLastSoundPlayed = soundCommand;
    }
    trigger.trigger(soundCommand); //play the track
    lastSoundPlayed = soundCommand;
}

void ps3soundControl(PS3BT* myPS3 = PS3Nav, int controllerNumber = 1){
    if (!(myPS3->getButtonPress(L1)||myPS3->getButtonPress(L2)||myPS3->getButtonPress(PS))){
      if (myPS3->getButtonClick(UP)) {
        processSoundCommand(30,33);
        Serial.println("\n\rSoundCommand Annoyed");//;  // Annoyed
      }
      else if (myPS3->getButtonClick(RIGHT)){
        processSoundCommand(1,12);  // CHAT
        Serial.println("\n\rSoundCommand Chat");
      }
      else if (myPS3->getButtonClick(DOWN)){
        processSoundCommand(50,54); // Sad
        Serial.println("\n\rSoundCommand Sad");
      }
      else if (myPS3->getButtonClick(LEFT)) {
        processSoundCommand(40,47); // Razz
        Serial.println("\n\rSoundCommand Razz");
      }
    }else if (myPS3->getButtonPress(L2)&&!myPS3->getButtonPress(L1)){
      if (myPS3->getButtonClick(RIGHT))  processSoundCommand(7);         //Yes
      else if (myPS3->getButtonClick(DOWN))   processSoundCommand(3);    //Hello
      else if (myPS3->getButtonClick(LEFT))   processSoundCommand(6);    //No
    }else if (myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(L2)){
      if (myPS3->getButtonClick(UP))          processSoundCommand(1,26);  // Random/Chat Auto Sounds
      else if (myPS3->getButtonClick(DOWN))   processSoundCommand(21);  // Hum
      else if (myPS3->getButtonClick(LEFT))   processSoundCommand(99);  // None
      else if (myPS3->getButtonClick(RIGHT))  processSoundCommand(11); // Patrol
    }
}

void soundControl(){
   if (PS3Nav->PS3NavigationConnected) ps3soundControl(PS3Nav,1);
   if (PS3Nav2->PS3NavigationConnected) ps3soundControl(PS3Nav2,2);
}

// ==============================================================
//                         DOME FUNCTIONS
// ==============================================================

void pieOpenAll(){
    #ifdef SERVO_EASING
      Serial.print("pieOpenAll ServoEasing");
     pwm3_Servo7.write(pwm3_7Info.maxDeg); // DOME PIE 7
     pwm3_Servo6.write(pwm3_6Info.maxDeg); // DOME PIE 6
     pwm3_Servo5.write(pwm3_5Info.maxDeg); // DOME PIE 5
     pwm3_Servo4.write(pwm3_4Info.maxDeg); // DOME PIE 4
     pwm3_Servo3.write(pwm3_3Info.maxDeg); // DOME PIE 3
     pwm3_Servo2.write(pwm3_2Info.maxDeg); // DOME PIE 2
     pwm3_Servo1.write(pwm3_1Info.maxDeg); // DOME PIE 1

    #else
       pwm3.setPWM(7, 0, pwm3_7_max); // DOME PIE 7
       pwm3.setPWM(6, 0, pwm3_6_max); // DOME PIE 6
       pwm3.setPWM(5, 0, pwm3_5_max); // DOME PIE 5
       pwm3.setPWM(4, 0, pwm3_4_max); // DOME PIE 4
       pwm3.setPWM(3, 0, pwm3_3_max); // DOME PIE 3
       pwm3.setPWM(2, 0, pwm3_2_max); // DOME PIE 2
       pwm3.setPWM(1, 0, pwm3_1_max); // DOME PIE 1
       //       delay(300);
       Serial.print("pieOpenAll PWM only");
     #endif
      delay(300);
      silenceServos();
    }

void pieCloseAll(){

  #ifdef SERVO_EASING
     pwm3_Servo7.write(pwm3_7Info.minDeg); // DOME PIE 7
     pwm3_Servo6.write(pwm3_6Info.minDeg); // DOME PIE 6
     pwm3_Servo5.write(pwm3_5Info.minDeg); // DOME PIE 5
     pwm3_Servo4.write(pwm3_4Info.minDeg); // DOME PIE 4
     pwm3_Servo3.write(pwm3_3Info.minDeg); // DOME PIE 3
     if (!lifeformActivated){
      pwm3_Servo2.write(pwm3_2Info.minDeg); // DOME PIE 2
     }
     pwm3_Servo1.write(pwm3_1Info.minDeg); // DOME PIE 1--
  #else
     pwm3.setPWM(7, 0, pwm3_7_min); // DOME PIE 7
     pwm3.setPWM(6, 0, pwm3_6_min); // DOME PIE 6
     pwm3.setPWM(5, 0, pwm3_5_min); // DOME PIE 5
     pwm3.setPWM(4, 0, pwm3_4_min); // DOME PIE 4
     pwm3.setPWM(3, 0, pwm3_3_min); // DOME PIE 3
     if (!lifeformActivated){
      pwm3.setPWM(2, 0, pwm3_2_min); // DOME PIE 2
     }
     pwm3.setPWM(1, 0, pwm3_1_min); // DOME PIE 1--
   #endif
     delay(400);//400
     silenceServos();
}

void openAll(boolean motivator = false, boolean openUtil = false){
  #ifdef SERVO_EASING
      pwm3_Servo1.easeTo(pwm3_1Info.maxDeg);//Pie1

      pwm3_Servo2.easeTo(pwm3_2Info.maxDeg);//Pie2

      pwm3_Servo3.easeTo(pwm3_3Info.maxDeg); //Pie3

      pwm3_Servo4.easeTo(pwm3_4Info.maxDeg);//Pie4

      pwm3_Servo5.easeTo(pwm3_5Info.maxDeg);//Pie5

      pwm3_Servo6.easeTo(pwm3_6Info.maxDeg);//Pie6

      pwm3_Servo7.easeTo(pwm3_7Info.maxDeg);//Pie7

      pwm2_Servo8.easeTo(pwm2_8Info.maxDeg); //Lower Util

      pwm2_Servo9.easeTo(pwm3_9Info.maxDeg); //UpperUtil

      pwm2_Servo4.easeTo(pwm2_4Info.maxDeg); //Interface Door

      pwm2_Servo6.easeTo(pwm2_6Info.maxDeg); //Gripper Door

      pwm2_Servo11.easeTo(pwm2_11Info.maxDeg); //Buzz Door
      pwm1_Servo4.easeTo(pwm1_4Info.maxDeg);//DataPanel Door

      pwm2_Servo7.easeTo(pwm2_7Info.maxDeg); //CBI

     if (motivator){
       //pwm2.setPWM(2, 0, 100); // MOTIVATOR
       //processSoundCommand(5);
     }
  #else
     pwm3.setPWM(7, 0, pwm3_7_max); // DOME PIE 7
     pwm3.setPWM(6, 0, pwm3_6_max); // DOME PIE 6
     pwm3.setPWM(5, 0, pwm3_5_max); // DOME PIE 5
     pwm3.setPWM(4, 0, pwm3_4_max); // DOME PIE 4
     pwm3.setPWM(3, 0, pwm3_3_max); // DOME PIE 3
     pwm3.setPWM(2, 0, pwm3_2_max); // DOME PIE 2
     pwm3.setPWM(1, 0, pwm3_1_max); // DOME PIE 1
     //Serial.print(pwm3_1_max);

     pwm2.setPWM(3, 0, pwm2_3_max); // Zapper
     pwm2.setPWM(11, 0, pwm2_11_max); // BuzzSaw
     pwm1.setPWM(4,0, pwm1_4_max);// DataPanel

     if (openUtil){
      moveUtilArms(0,0,0,utilArmMax0);
      moveUtilArms(1,0,0,utilArmMax1);
     }

     pwm2.setPWM(4, 0, pwm2_4_max); // RT
     pwm2.setPWM(5, 0, pwm2_5_max); // RB
     pwm2.setPWM(6, 0, pwm2_6_max); // LT
     pwm2.setPWM(7, 0, pwm2_7_max); // LB
    
     if (motivator){
       pwm2.setPWM(2, 0, 100); // MOTIVATOR
       processSoundCommand(5);
     }
   #endif
     delay(500);
     silenceServos();
}

void closeAll(){
    #ifdef SERVO_EASING
      pwm2_Servo8.write(pwm2_8Info.minDeg); //Lower Util
      delay(300);
      pwm2_Servo9.write(pwm2_9Info.minDeg); //UpperUtil
      delay(300);
      pwm3_Servo1.write(pwm3_1Info.minDeg);
      if (!lifeformActivated){
          pwm3_Servo2.write(pwm3_2Info.minDeg);
      }
      pwm3_Servo3.write(pwm3_3Info.minDeg); //Pie
      pwm3_Servo4.write(pwm3_4Info.minDeg);
      pwm3_Servo5.write(pwm3_5Info.minDeg);
      pwm3_Servo6.write(pwm3_6Info.minDeg);
      pwm3_Servo7.write(pwm3_7Info.minDeg);



      if (!interfaceActivated){
      pwm2_Servo4.write(pwm2_4Info.minDeg); //Interface Door
         delay(200);
      }
      if (!gripperActivated){
        pwm2_Servo6.write(pwm2_6Info.minDeg); //Gripper Door
         delay(200);
      }
      pwm2_Servo7.write(pwm2_7Info.minDeg); //CBI Door
      if (!buzzActivated){
        pwm2_Servo11.write(pwm2_11Info.minDeg); //Buzz Door
         delay(200);
      }
      pwm1_Servo4.write(pwm2_4Info.minDeg);// DataPanel
      //pwm1_Servo4.setEaseTo(pwm2_4Info.minDeg);// DataPanel
      delay(500);
      silenceServos();
    
    #else
      
      pwm3.setPWM(7, 0, pwm3_7_min); // DOME PIE 7
      pwm3.setPWM(6, 0, pwm3_6_min); // DOME PIE 6
      pwm3.setPWM(5, 0, pwm3_5_min); // DOME PIE 5
      pwm3.setPWM(4, 0, pwm3_4_min); // DOME PIE 4
      pwm3.setPWM(3, 0, pwm3_3_min); // DOME PIE 3
      if (!lifeformActivated){
        pwm3.setPWM(2, 0, pwm3_2_min); // DOME PIE 2
      }
      pwm3.setPWM(1, 0, pwm3_1_min); // DOME PIE 1--
      Serial.print("CloseAll pwm3_1_min = ");//--
      Serial.println(pwm3_1_min);//--


      moveUtilArms(0,1,255,utilArmMin0);
      moveUtilArms(1,1,255,utilArmMin1);

      if (!interfaceActivated){
        pwm2.setPWM(4, 0, pwm2_4_min); // RT
        pwm2.setPWM(5, 0, pwm2_5_min); // RB
      }
      if (!gripperActivated){
        pwm2.setPWM(6, 0, pwm2_6_min); // LT
        pwm2.setPWM(7, 0, pwm2_7_min); // LB
      }

      if (!zapperActivated){
        pwm2.setPWM(3, 0, pwm2_3_min); // Zapper
      }
      if (!buzzActivated){
        pwm2.setPWM(11, 0, pwm2_11_min); // Buzz
      }

      pwm1.setPWM(14, 0, pwm1_14_min); // Rear Door Lego
      pwm1.setPWM(10, 0, pwm1_10_min); // Rear Door Bolt
      pwm1.setPWM(4,0, pwm1_4_min);// DataPanel
    #endif
    delay(400);
//    Serial.print("CloseAll pwm3_1_min = ");//--
//    Serial.println(pwm3_1_min);//--
    silenceServos();
}




void moveUtilArms(int arm, int direction, int position, int setposition){
    currentMillis = millis();
    if (currentMillis > (lastUtilArmMillis)){
      if (arm == 1 && direction == 0 && setposition < lastUtilArmPos1){
        #ifdef SERVO_EASING
        //TODO: build a converter for degrees to milleseconds or use a differnt funtion!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //pwm2_Servo8.startEaseTo(setposition, 30);
          #else
            pwm2.setPWM(8, 0, setposition);
          #endif
        if (setposition > utilArmMax1){
          lastUtilArmPos1 = setposition-5;
        }
        lastUtilArmMillis = currentMillis;
      }else if (arm == 1 && direction == 1 && setposition > lastUtilArmPos1){
        #ifdef SERVO_EASING
        //TODO: build a converter for degrees to milleseconds or use a differnt funtion!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //pwm2_Servo8.startEaseTo(setposition, 30);
          #else
            pwm2.setPWM(8, 0, setposition);
          #endif
        if (setposition < utilArmMin1){
          lastUtilArmPos1 = setposition+5;
        }else{
          lastUtilArmPos1 = setposition-10;
        }
        lastUtilArmMillis = currentMillis;
      }else if (arm == 0 && direction == 1 && setposition < lastUtilArmPos0){
//        Serial.print("Arm0 = ");
//        Serial.print(setposition);
        #ifdef SERVO_EASING
        //TODO: build a converter for degrees to milleseconds or use a differnt function!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //pwm2_Servo9.startEaseTo(setposition, 30);
          #else
            pwm2.setPWM(9, 0, setposition);
          #endif
        if (setposition > utilArmMin0){
          lastUtilArmPos0 = setposition-5;
        }else{
          lastUtilArmPos0 = setposition+10;
        }
        lastUtilArmMillis = currentMillis;
      }else if (arm == 0 && direction == 0 && setposition > lastUtilArmPos0){
        #ifdef SERVO_EASING
        //TODO: build a converter for degrees to milleseconds or use a differnt funtion!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //pwm2_Servo9.startEaseTo(setposition, 30);
          #else
            pwm2.setPWM(9, 0, setposition);
          #endif
        if (setposition < utilArmMax0){
          lastUtilArmPos0 = setposition+5;
        }
        lastUtilArmMillis = currentMillis;
      }
      lastUtilStickPos = position;
    }
}

void freakOut(){
  if (droidFreakOut){
    if (!droidFreakingOut){

        processSoundCommand(1);
        centerDomeRotationCount = 1;
        domeCentered = false;
        centerDomeSpeed = 75;
        droidFreakingOut = true;
    }else if (droidFreakingOut && domeCentered && centerDomeRotationCount == 0){
        openAll(true);
        smokeTriggered = true;
        digitalWrite(PIN_BADMOTIVATOR, HIGH);   //Bad Motivator
        smokeMillis = millis() + 3000;
        droidFreakOut = false;
        droidFreakingOut = false;
        droidFreakedOut = true;
        droidFreakOutComplete = true;
    }
  }
}

void centerDomeLoop(){
    if (digitalRead(PIN_DOMECENTER) == 0 && !domeCentered){
      if (centerDomeHighCount >= 200 && !droidFreakingOut){
        centerDomeHighCount = 200;
        if (centerDomeRotationCount == 0){
          centerDomeSpeed = 0;
          domeCentered = true;
          domeCentering = false;
        }
      }else{
        centerDomeHighCount++;
      }
      if (centerDomeHighCount >= 10 && centerDomeLowCount == 30){
        centerDomeHighCount = 0;
        centerDomeLowCount = 0;

        if (centerDomeRotationCount == 0){
          centerDomeSpeed = 0;
          SyR->stop();
          domeCentered = true;
          domeCentering = false;
        }else{
          centerDomeRotationCount = centerDomeRotationCount - 1;
        }
      }
    }else{
      if (centerDomeLowCount >= 30){
        centerDomeLowCount = 30;
      }else{
        centerDomeLowCount++;
        centerDomeHighCount = 0;
      }
      SyR->motor(centerDomeSpeed);
      domeCentered = false;
      domeCentering = true;
    }
}

void blinkEyes(){
  #ifdef SERVO_EASING
    //TODO: Write a NeoPixel Blinky routine for this area!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  #else
      pwm3.setPWM(10,0,0);
      pwm3.setPWM(11,0,0);
      pwm3.setPWM(12,0,0);
      pwm3.setPWM(13,0,0);
      pwm3.setPWM(14,0,0);
      delay(500);
      pwm3.setPWM(10,0,pwm3_10_max);
      pwm3.setPWM(11,0,pwm3_11_max);
      pwm3.setPWM(12,0,pwm3_12_max);
      pwm3.setPWM(13,0,pwm3_13_max);
      pwm3.setPWM(14,0,pwm3_14_max);
      delay(300);
      pwm3.setPWM(10,0,0);
      pwm3.setPWM(11,0,0);
      pwm3.setPWM(12,0,0);
      pwm3.setPWM(13,0,0);
      pwm3.setPWM(14,0,0);
      delay(300);
      pwm3.setPWM(10,0,pwm3_10_max);
      pwm3.setPWM(11,0,pwm3_11_max);
      pwm3.setPWM(12,0,pwm3_12_max);
      pwm3.setPWM(13,0,pwm3_13_max);
      pwm3.setPWM(14,0,pwm3_14_max);
    #endif
}

void badMotivatorLoop(){
     if (millis() > smokeMillis && smokeTriggered){
      digitalWrite(PIN_BADMOTIVATOR, LOW);
      smokeTriggered = false;
    }
    if (millis() > motivatorMillis && motivatorTriggered){
        #ifdef SERVO_EASING
        //TODO: Set bad Motivator!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          //pwm2_Servo2.startEaseTo(pwm2_2Info.maxDeg,30);
        #else
          pwm2.setPWM(2, 0, pwm2_2_max); // MOTIVATOR
        #endif
        delay(100);
        //sfx.playTrack("T05     OGG");
        trigger.trigger(63);
        #ifdef SERVO_EASING
          //TODO: Set Bad Motivator !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          //pwm2_Servo2.startEaseTo(pwm2_2Info.minDeg,30);
        #else
          pwm2.setPWM(2, 0, pwm2_2_min); // MOTIVATOR
        #endif
        silenceServos();
        blinkEyes();
        motivatorTriggered = false;
    }
}

int easingMotor(float time, float startValue, float change, float duration) {
    time /= duration / 2;
    if (time < 1)  {
         return change / 2 * time * time + startValue;
    }
    time--;
    return -change / 2 * (time * (time - 2) - 1) + startValue;
};


void rotateDome(int domeRotationSpeed, int domeRotationLength = 0){
    currentMillis = millis();
    if (((currentMillis - stoppedDomeMillis) < 2000) || ((!isDomeMotorStopped || domeRotationSpeed != 0) && ((currentMillis - previousDomeMillis) > (2*serialLatency)))){

      if (centerDomeSpeed == 0){
        if (domeRotationLength > 0){
           autoDomeSetSpeed = domeRotationSpeed;
           autoDomeSetTime = currentMillis+domeRotationLength;
        }
        if (currentMillis < autoDomeSetTime){
          domeRotationSpeed = autoDomeSetSpeed;
        }
      }

      if (domeRotationSpeed != 0){
        isDomeMotorStopped = false;
      }else{
        isDomeMotorStopped = true;
      }
      previousDomeMillis = currentMillis;

      if (domeSpinning && domeRotationSpeed == 0){
        stoppedDomeMillis = millis();
        domeSpinning = false;
      }else if (!domeSpinning && (domeRotationSpeed > 0 || domeRotationSpeed < 0)){
        domeSpinning = true;
        domeCentered = false;
      }

      if ((currentMillis - stoppedDomeMillis) < 400){
         domeRotationSpeed = easingMotor((currentMillis - stoppedDomeMillis), lastDomeRotationSpeed,(lastDomeRotationSpeed*-1), 400);
      }else{
         lastDomeRotationSpeed = domeRotationSpeed;
      }

      SyR->motor(domeRotationSpeed);
    }
}

void randomDomeMovement(){
  if (!droidFreakOut && !interfaceActivated){
    currentMillis = millis();
    if (currentMillis - automateDomeMillis > 3000){
      automateDomeMillis = millis();
      automateAction = random(6,35);
      if (automateAction < 20 && automateAction > 6 && !domeSpinning && !domeCentering){
        int randomDomeLength = random(500,800);
        centerDomeSpeed = 0;
        if (head_right){
            rotateDome(-automateAction,randomDomeLength);
            head_right = false;
        }else{
            rotateDome(automateAction,randomDomeLength);
            head_right = true;
        }
      }else if (automateAction <= 18 && !domeSpinning && !domeCentered && !domeCentering){
        centerDomeRotationCount = 0;
        if (head_right){
            centerDomeSpeed = -25;
            head_right = false;
        }else{
            centerDomeSpeed = 25;
            head_right = true;
        }
      }
    }
  }
}

void DrivingSoundHead(){
  if (isAutomateDomeOn){
    randomDomeMovement();
    currentMillis = millis();
    if (currentMillis - automateSoundMillis > 3500){
      automateSoundMillis = millis();
      automateAction = random(1,100);
      if (automateAction < 36) processSoundCommand(40,68);
    }
  }
}

// ==============================================================
//                       MOTOR FUNCTIONS
// ==============================================================

boolean ps3FootMotorDrive(PS3BT* myPS3 = PS3Nav){
  //Serial.println("\r\nMotor Funtion");
  int footDriveSpeed = 0;
  int stickSpeed = 0;
  int turnnum = 0;

  if (isPS3NavigatonInitialized) {
      if (!isStickEnabled){ // Additional fault control.  Do NOT send additional commands to Sabertooth if no controllers have initialized.
          if (abs(myPS3->getAnalogHat(LeftHatY)-128) > joystickFootDeadZoneRange) output += "Drive Stick is disabled\r\n";
          ST->stop();
          isFootMotorStopped = true;
      }else if (!myPS3->PS3NavigationConnected){
          ST->stop();
          isFootMotorStopped = true;
      }else if (myPS3->getButtonPress(L1)){
          ST->stop();
          isFootMotorStopped = true;
      }else{
          int joystickPosition = myPS3->getAnalogHat(LeftHatY);
          isFootMotorStopped = false;
          if (myPS3->getButtonPress(L2)){
            int throttle = 0;
            if (joystickPosition < 127){
                throttle = joystickPosition - myPS3->getAnalogButton(L2);
            }else{
                throttle = joystickPosition + myPS3->getAnalogButton(L2);
            }
            stickSpeed = (map(throttle, -255, 510, -drivespeed2, drivespeed2));
          }else{
            stickSpeed = (map(joystickPosition, 0, 255, -drivespeed1, drivespeed1));
          }

          if (abs(joystickPosition-128) < joystickFootDeadZoneRange){
              footDriveSpeed = 0;
          }else if (footDriveSpeed < stickSpeed){
              if (stickSpeed-footDriveSpeed<(ramping+1)){
                  footDriveSpeed+=ramping;
              }else{
                  footDriveSpeed = stickSpeed;
              }
          }else if (footDriveSpeed > stickSpeed){
              if (footDriveSpeed-stickSpeed<(ramping+1)){
                  footDriveSpeed-=ramping;
              }else{
                  footDriveSpeed = stickSpeed;
              }
          }

          turnnum = (myPS3->getAnalogHat(LeftHatX));

          //TODO:  Is there a better algorithm here?
          if (abs(footDriveSpeed) > 50){
              turnnum = (map(myPS3->getAnalogHat(LeftHatX), 54, 200, -(turnspeed/4), (turnspeed/4)));
          }else if (turnnum <= 200 && turnnum >= 54){
              turnnum = (map(myPS3->getAnalogHat(LeftHatX), 54, 200, -(turnspeed/3), (turnspeed/3)));
          }else if (turnnum > 200){
              turnnum = (map(myPS3->getAnalogHat(LeftHatX), 201, 255, turnspeed/3, turnspeed));
          }else if (turnnum < 54){
              turnnum = (map(myPS3->getAnalogHat(LeftHatX), 0, 53, -turnspeed, -(turnspeed/3)));
          }

          currentMillis = millis();
          if ((currentMillis - previousFootMillis) > serialLatency){
            if (footDriveSpeed < -driveDeadBandRange || footDriveSpeed > driveDeadBandRange){
              DrivingSoundHead();
            }

            ST->turn(turnnum * invertTurnDirection);
            ST->drive(footDriveSpeed);
            previousFootMillis = currentMillis;
            return true; //we sent a foot command
          }
      }
  }
  return false;
}

void footMotorDrive(){
  if ((millis() - previousFootMillis) < serialLatency) return;
  if (PS3Nav->PS3NavigationConnected) ps3FootMotorDrive(PS3Nav);
}

int ps3DomeDrive(PS3BT* myPS3 = PS3Nav, int controllerNumber = 1){
    int domeRotationSpeed = 0;
    int joystickPosition = myPS3->getAnalogHat(LeftHatX);
    if ((controllerNumber==1 && myPS3->getButtonPress(L1))&&!myPS3->getButtonPress(L2) || (controllerNumber==1 && myPS3->getButtonPress(L3)) || ( controllerNumber==2 && !myPS3->getButtonPress(L1) && !myPS3->getButtonPress(L2))){
        if (controllerNumber==1 && myPS3->getButtonPress(L3)&&!myPS3->getButtonPress(L1)){
          domespeed = domespeedfast;
          domeRotationSpeed = (map(joystickPosition, 255, 0, -domespeed, domespeed));
        }else if (controllerNumber==1 && myPS3->getButtonPress(L3)&&myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(L2)){
          domespeed = domespeedfast;
          domeRotationSpeed = (map(joystickPosition, 0, 255, -domespeed, domespeed));
        }else{
          domespeed = domespeedslow;
          domeRotationSpeed = (map(joystickPosition, 0, 255, -domespeed, domespeed));
        }
        if (abs(joystickPosition-128) < joystickDomeDeadZoneRange){
          domeRotationSpeed = 0;
        }
    }else{
      domespeed = domespeedslow;
    }

    if (domeRotationSpeed > 0 || domeRotationSpeed < 0){
      domeSpinning = true;
    }
    return domeRotationSpeed;
}





void domeDrive(){
  if ((millis() - previousDomeMillis) < (2*serialLatency) ) return;

  int domeRotationSpeed = 0;
  int ps3NavControlSpeed = 0;
  int ps3Nav2ControlSpeed = 0;
  if (PS3Nav->PS3NavigationConnected) ps3NavControlSpeed = ps3DomeDrive(PS3Nav,1);
  if (PS3Nav2->PS3NavigationConnected) ps3Nav2ControlSpeed = ps3DomeDrive(PS3Nav2,2);

  //In a two controller system, give dome priority to the secondary controller.
  //Only allow the "Primary" controller dome control if the Secondary is NOT spinnning it
  if ( abs(ps3Nav2ControlSpeed) > 0 ){
    domeRotationSpeed = ps3Nav2ControlSpeed;
  }else{
    domeRotationSpeed = ps3NavControlSpeed;
  }
  rotateDome(domeRotationSpeed);
}








// ==============================================================
//                        PS REMOTE COMMANDS
// ==============================================================
void ps3ProcessCommands(PS3BT* myPS3 = PS3Nav){
#pragma mark -
#pragma mark L2 and Arrow Commands
#pragma mark -
#pragma mark    SCREAM
   // SCREAM
   if (myPS3->getButtonPress(L2)&&!myPS3->getButtonPress(L1)){
       if (myPS3->getButtonClick(UP)){
         if (droidScream){
           closeAll();
           droidScream = false;
         }else{
           //processSoundCommand(1);
           trigger.trigger(60);
           openAll();
           droidScream = true;
         }
       }
#pragma mark -
#pragma mark BuzzDoorOnly
            //BuzzDoorOnly
      if (myPS3->getButtonClick(LEFT)){
        if (!buzzDoorOpen){
           pwm2_Servo11.easeTo(pwm2_11Info.maxDeg);
           delay(300);
           pwm2_Servo11.detach();
           buzzDoorOpen = true;
        }else{
           pwm2_Servo11.easeTo(pwm2_11Info.minDeg);
           delay(300);
           pwm2_Servo11.detach();
           buzzDoorOpen = false;
        }
      }
#pragma mark - Utility Grab
      if (myPS3->getButtonClick(RIGHT)){
        if (!utilityGrab){
           pwm2_Servo8.easeTo(pwm2_8Info.maxDeg);
           pwm2_Servo9.easeTo(pwm2_9Info.maxDeg,300);
           delay(300);
           pwm2_Servo8.detach();
           pwm2_Servo9.detach();
           utilityGrab = true;
        }else{
           pwm2_Servo8.easeTo(pwm2_8Info.minDeg);
           pwm2_Servo9.easeTo(pwm2_9Info.minDeg,300);
           delay(300);
           pwm2_Servo8.detach();
           pwm2_Servo9.detach();
           utilityGrab = false;
        }
      }
      
   }

   
    #pragma mark -
    #pragma mark    Bad Motivator
   // BAD MOTIVATOR
   if (myPS3->getButtonPress(PS)){
        if(myPS3->getButtonClick(UP)){
            motivatorTriggered = true;
            smokeTriggered = true;
            digitalWrite(PIN_BADMOTIVATOR, HIGH);
            motivatorMillis = millis() + 1000;
            smokeMillis = millis() + 3000; // was5000;
            silenceServos();
          }
        if(myPS3->getButtonClick(RIGHT)){
            smokeTriggered = true;
            digitalWrite(PIN_BADMOTIVATOR, HIGH);
            smokeMillis = millis() + 3000;
            //my code
            output = "PS + Right Button";
            //printOutput();
          }
        // Rear Bolt Door (Previously Freak Out)
        if(myPS3->getButtonClick(LEFT)){
           /*
           if(droidFreakedOut){
              closeAll();
              processSoundCommand(6);
              droidFreakedOut =false;
           }else{
              droidFreakOut = true;
           }
           */
          processSoundCommand(80,88);
          #ifdef SERVO_EASING

          #else
          pwm1.setPWM(10, 0, pwm1_10_max);  // Rear bolt Door
          delay(800);
          pwm1.setPWM(10, 0, pwm1_10_min);
          delay(300);
          pwm1.setPWM(10, 0, 0);
          #endif
        }
    }

    
    #pragma mark -
    #pragma mark    LifeForm Scanner
    // Lifeform Scanner

    #ifdef SERVO_EASING
    //TODO: Convert this to Servo Easing... Getting lazy as I don't have a scanner yet.
//      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(L3)){
//        if (!lifeformActivated){
//          //pwm3.setPWM(9,0,((pwm3_9_max-pwm3_9_min)/2));
//          pwm3.setPWM(2, 0, pwm3_2_max); // DOME PIE 2
//          delay(200);
//          pwm3.setPWM(8, 0, pwm3_8_max); // Lifeform Slide
//          delay(3000);
//          pwm3.setPWM(8, 0, 0); // Lifeform Slide
//          silenceServos();
//          lifeformActivated = true;
//        }else {
//          pwm3.setPWM(9,0,(pwm3_9_min+30));
//          pwm3.setPWM(8, 0, pwm3_8_min); // Lifeform Slide
//          delay(4000);
//          pwm3.setPWM(8, 0, 0); // Lifeform Slide
//          pwm3.setPWM(15, 0, 0);
//          pwm3.setPWM(2, 0, pwm3_2_min); // DOME PIE 2
//          delay(400);
//          silenceServos();
//          lifeformActivated = false;
//          lifeformPosition = pwm3_9_min;
//          lifeformDir = 0;
//        }
//     }
    #else
       if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(L3)){
          if (!lifeformActivated){
            //pwm3.setPWM(9,0,((pwm3_9_max-pwm3_9_min)/2));
            pwm3.setPWM(2, 0, pwm3_2_max); // DOME PIE 2
            delay(200);
            pwm3.setPWM(8, 0, pwm3_8_max); // Lifeform Slide
            delay(3000);
            pwm3.setPWM(8, 0, 0); // Lifeform Slide
            silenceServos();
            lifeformActivated = true;
          }else {
            pwm3.setPWM(9,0,(pwm3_9_min+30));
            pwm3.setPWM(8, 0, pwm3_8_min); // Lifeform Slide
            delay(4000);
            pwm3.setPWM(8, 0, 0); // Lifeform Slide
            pwm3.setPWM(15, 0, 0);
            pwm3.setPWM(2, 0, pwm3_2_min); // DOME PIE 2
            delay(400);
            silenceServos();
            lifeformActivated = false;
            lifeformPosition = pwm3_9_min;
            lifeformDir = 0;
          }
       }
     #endif

     // Open Pie Panels
     if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(UP)){
        Serial.println("\n\rOpen Pie Panels");
        Serial.print("piePanelsOpen = ");
        Serial.println(piePanelsOpen);
       if (piePanelsOpen){
          piePanelsOpen = false;
          pieCloseAll();
       }else{
          piePanelsOpen = true;
          pieOpenAll();
       }
     }

     // Close All Panels
     if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(DOWN)){

      #ifdef SERVO_EASING
        //pwm3_Servo4.startEaseTo(pwm3_4_max,30); DataPanel
        //TODO: Head blinky thing?
        //Don't need anything here but a light blinky thing?

      #else
        pwm3.begin();
        pwm3.setPWMFreq(62);  // This is the maximum PWM frequency
        pwm3.setPWM(4,0,pwm3_4_max);
        pwm3.setPWM(10,0,pwm3_10_max);//UNUSED HEAD LEDs
        pwm3.setPWM(11,0,pwm3_11_max);
        pwm3.setPWM(12,0,pwm3_12_max);
        pwm3.setPWM(13,0,pwm3_13_max);
        pwm3.setPWM(14,0,pwm3_14_max);
       #endif
        closeAll();
     }
#pragma mark -
#pragma mark Side Panels
     // Open Side Pannels (Or if Gripper Arm is Out, Grip it)///////////////////////////
    #ifdef SERVO_EASING
      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(RIGHT)){
      if (!gripperActivated && !interfaceActivated){
        if (!sidePanelsOpen){
          Serial.println("Open BreadPan Doors");
           pwm2_Servo4.easeTo(pwm2_4Info.maxDeg); // Gripper Door
           pwm2_Servo6.easeTo(pwm2_6Info.maxDeg); // Interface Door
           //silenceServos();
           sidePanelsOpen = true;
        }else{
          Serial.println("Close BreadPan Doors");
           pwm2_Servo4.easeTo(pwm2_4Info.minDeg); // RT
           pwm2_Servo6.easeTo(pwm2_6Info.minDeg); // LT
           //delay(300);
           //silenceServos();
           sidePanelsOpen = false;
        }
      }else if (gripperActivated){
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg); //Gripper
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
            
          //silenceServos();
      }
      delay(300);
      silenceServos();
     }
     #else
       if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(RIGHT)){
        if (!gripperActivated && !interfaceActivated){
          if (!sidePanelsOpen){
             pwm2.setPWM(4, 0, pwm2_4_max); // RT
             pwm2.setPWM(5, 0, pwm2_5_max); // RB
             pwm2.setPWM(6, 0, pwm2_6_max); // LT
             //pwm2.setPWM(7, 0, pwm2_7_max); // CBI Door
             delay(300);
             silenceServos();
             sidePanelsOpen = true;
          }else{
             pwm2.setPWM(4, 0, pwm2_4_min); // RT
             pwm2.setPWM(5, 0, pwm2_5_min); // RB
             pwm2.setPWM(6, 0, pwm2_6_min); // LT
             //pwm2.setPWM(7, 0, pwm2_7_min); // CBI Door
             delay(500);
             silenceServos();
             sidePanelsOpen = false;
          }
        }else if (gripperActivated){
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          silenceServos();
        }
       }
     #endif

    #pragma mark -
    #pragma mark    Middle Panels
     // Open Middle Pannels (Or if Interface Arm is Out, Spin it)//////////////////////
     #ifdef SERVO_EASING
       if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(LEFT)){
        if (!gripperActivated && !interfaceActivated){
          if (!midPanelsOpen){
//             midPanelsOpen = true;
//             Serial.println("midPanelsOpen = True");
             pwm1_Servo4.easeTo(pwm1_4Info.maxDeg);//DataPanel Door
             pwm2_Servo7.easeTo(pwm2_7Info.maxDeg);//CBI Door
             pwm2_Servo11.easeTo(pwm2_11Info.maxDeg);//Buzz Door
             //delay(400);// Delay is built into the silenceMiddleServos() function
             silenceMiddleServos();
             //silenceServos();
             midPanelsOpen = true;
             Serial.println("midPanelsOpen = True");
          }else{
             //midPanelsOpen = false;
             Serial.println("midPanelsOpen = false");
             pwm1_Servo4.easeTo(pwm1_4Info.minDeg);//DataPanel Door
             pwm2_Servo7.easeTo(pwm2_7Info.minDeg);//CBI Door
             pwm2_Servo11.easeTo(pwm2_11Info.minDeg);//Buzz Door
             //delay(400);// Delay is built into the silenceMiddleServos() function
             silenceMiddleServos();
             //silenceServos();
             midPanelsOpen = false;
          }
          //delay(400);
          //silenceServos();
        }else if (interfaceActivated){
          if (interfaceOut){
            pwm2_Servo15.easeTo(pwm2_15Info.minDeg);
            //delay(500);
            interfaceOut = false;
            }else{
            pwm2_Servo15.easeTo(pwm2_15Info.maxDeg);
            //delay(450);
            pwm2_Servo15.easeTo(pwm2_15Info.minDeg);
            //delay(250);
            pwm2_Servo15.easeTo(pwm2_15Info.maxDeg);
            //delay(450);
            pwm2_Servo15.easeTo(pwm2_15Info.minDeg);
            interfaceOut = true;
            }
        }
//       delay(300);
//       silenceServos();
       }
     #else
       if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(LEFT)){
        if (!gripperActivated && !interfaceActivated){
          if (!midPanelsOpen){
             pwm2.setPWM(3, 0, pwm2_3_max); // Zapper Door
             pwm1.setPWM(4, 0, pwm1_4_max); // Data Panel Door
             pwm2.setPWM(7, 0, pwm2_7_max); // CBI Door
             pwm2.setPWM(11, 0, pwm2_11_max); // Buzz Door
             delay(400);
             silenceServos();
             midPanelsOpen = true;
          }else{
             pwm2.setPWM(3, 0, pwm2_3_min); // Zapper Door
             pwm1.setPWM(4, 0, pwm1_4_min); // Data Panel Door
             pwm2.setPWM(7, 0, pwm2_7_min); // CBI Door
             pwm2.setPWM(11, 0, pwm2_11_min); // Buzz Door
             delay(400);
             silenceServos();
             midPanelsOpen = false;
          }
        }else if (interfaceActivated){
          if (interfaceOut){
            pwm2.setPWM(15,0,pwm2_15_min);
            delay(500);
            interfaceOut = false;
            silenceServos();
          }else{
            pwm2.setPWM(15,0,pwm2_15_max);
            delay(450);
            pwm2.setPWM(15,0,pwm2_15_min);
            delay(250);
            pwm2.setPWM(15,0,pwm2_15_max);
            delay(450);
            interfaceOut = true;
            silenceServos();
          }
        }
       }
     #endif

     #ifdef SERVO_EASING

     #else
    // Panel Drawer
      if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(L1)){
          if (!drawerActivated){
            pwm1.setPWM(12, 0, pwm1_12_min); // Panel Drawer
            delay(1600);
            pwm1.setPWM(12, 0, 0); // Panel Drawer
            drawerActivated = true;
          }else{
              pwm1.setPWM(12, 0, pwm1_12_max); // Panel Drawer
              delay(1600);
              pwm1.setPWM(12, 0, 0); // Panel Drawer
              drawerActivated = false;
          }
      }
     #endif

    // Lego
    #ifdef SERVO_EASING
    //ServoEasing code here
     #else
  if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(DOWN)){
        if (zapperActivated){
           digitalWrite(PIN_ZAPPER,HIGH);
           delay(1000);
           digitalWrite(PIN_ZAPPER,LOW);
        }else{

          pwm1.setPWM(13, 0, 300);
          delay(145);
          pwm1.setPWM(13, 0, 580);
          delay(210);
          processSoundCommand(80,88);
          pwm1.setPWM(14, 0, pwm1_14_max);
          delay(800);
          pwm1.setPWM(14, 0, pwm1_14_min);
          delay(300);
          pwm1.setPWM(14, 0, 0);
          //pwm1.setPWM(13, 0, 0);
        }
      }
    #endif

    // Card Dispenser
    #ifdef SERVO_EASING
      //ServoEasingCode here
    #else
      if(myPS3->getButtonPress(L1)&&myPS3->getButtonClick(L3)){
        pwm2.setPWM(10, 0, pwm2_10_max);
        delay(2850);
        //pwm2.setPWM(10, 0, pwm2_10_min);
        //delay(100);
        pwm2.setPWM(10, 0, 0);
    }
    #endif

    #pragma mark -
    #pragma mark    Volume
    // Volume Up
    if(myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(L2)&&myPS3->getButtonClick(CROSS)){
      droidSoundLevel -= 10;
      if (droidSoundLevel < 0) droidSoundLevel = 0;
      trigger.setVolume(droidSoundLevel);
      trigger.trigger(1);
    }
    // Volume Down
    if(!myPS3->getButtonPress(L1)&&myPS3->getButtonPress(L2)&&myPS3->getButtonClick(CROSS)){
      droidSoundLevel += 10;
      if (droidSoundLevel >100) droidSoundLevel = 100;
      trigger.setVolume(droidSoundLevel);
      trigger.trigger(1);
    }

#pragma mark -
#pragma mark Lower UtilityArm
   
        // Lower Utility Arm
#ifdef SERVO_EASING
   if(myPS3->getButtonPress(L1) && !myPS3->getButtonPress(L2)){
    //Key command available
    //This was always coming out by acciendent so I removed it. 
    /*
     int joystickPositionY = myPS3->getAnalogHat(LeftHatY);
     if (joystickPositionY < 95 && !myPS3->getButtonPress(L3)){
       int utilArmPos1 = (map(joystickPositionY, 0, 125, utilArmMax1, utilArmMin1)); //87 -310
        pwm2_Servo8.easeTo(utilArmPos1, 200);
       if (lastUtilArmPos1 > utilArmPos1){
         //moveUtilArms(1,0,joystickPositionY,utilArmPos1);
       }else{
         //moveUtilArms(1,0,joystickPositionY,lastUtilArmPos1);
       }
      }else if (joystickPositionY > 145 && !myPS3->getButtonPress(L3)){
       int utilArmPos1 = (map(joystickPositionY, 127, 225, (lastUtilArmPos1-10), utilArmMin1));
       if (lastUtilArmPos1 < utilArmPos1){
         //moveUtilArms(1,1,joystickPositionY,utilArmPos1);
       }else{
         //moveUtilArms(1,1,joystickPositionY,lastUtilArmPos1);
       }
      }else{
          pwm2_Servo8.detach();
     }
     */
   }
   
#else
   if(myPS3->getButtonPress(L1) && !myPS3->getButtonPress(L2)){
     int joystickPositionY = myPS3->getAnalogHat(LeftHatY);
     if (joystickPositionY < 95 && !myPS3->getButtonPress(L3)){
       int utilArmPos1 = (map(joystickPositionY, 0, 125, utilArmMax1, utilArmMin1)); //87 -310
       if (lastUtilArmPos1 > utilArmPos1){
         moveUtilArms(1,0,joystickPositionY,utilArmPos1);
       }else{
         moveUtilArms(1,0,joystickPositionY,lastUtilArmPos1);
       }
      }else if (joystickPositionY > 145 && !myPS3->getButtonPress(L3)){
       int utilArmPos1 = (map(joystickPositionY, 127, 225, (lastUtilArmPos1-10), utilArmMin1));
       if (lastUtilArmPos1 < utilArmPos1){
         moveUtilArms(1,1,joystickPositionY,utilArmPos1);
       }else{
         moveUtilArms(1,1,joystickPositionY,lastUtilArmPos1);
       }
      }else{
          pwm2.setPWM(8, 0, 0);
     }
   }
#endif
   
   



     // Upper Utility Arm
     if(myPS3->getButtonPress(L2) && !myPS3->getButtonPress(L1)){
       int joystickPositionY = myPS3->getAnalogHat(LeftHatY);
       if (joystickPositionY < 95 && !myPS3->getButtonPress(L3)){
         int utilArmPos0 = (map(joystickPositionY, 0, 125, utilArmMax0, utilArmMin0)); //87 -310
//        Serial.print("\n\rLastArmPos0= ");
//        Serial.print(lastUtilArmPos0);
//        Serial.print("\n\rutilArmPos0= ");
//        Serial.print(utilArmPos0);
         if (lastUtilArmPos0 < utilArmPos0){
//           Serial.print("\n\rUpper Utility Arm Y = ");
//           Serial.print(utilArmPos0);
           moveUtilArms(0,0,joystickPositionY,utilArmPos0);
         }else{
           moveUtilArms(0,0,joystickPositionY,lastUtilArmPos0);
         }
        }else if (joystickPositionY > 145 && !myPS3->getButtonPress(L3)){
         int utilArmPos0 = (map(joystickPositionY, 127, 225, (lastUtilArmPos0+10), utilArmMin0));
         if (lastUtilArmPos0 > utilArmPos0){
           moveUtilArms(0,1,joystickPositionY,utilArmPos0);
         }else{
           moveUtilArms(0,1,joystickPositionY,lastUtilArmPos0);
         }
        }else{
          #ifdef SERVO_EASING
            //pwm2_Servo9.detach();
          #else
            pwm2.setPWM(9, 0, 0);
          #endif
       }
      }

     // UTILITY ARMS
     if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)){
       randomDomeMovement();
       int joystickPositionY = myPS3->getAnalogHat(LeftHatY);
       if (joystickPositionY < 110 && !myPS3->getButtonPress(L3)){
         int utilArmPos0 = (map(joystickPositionY, 0, 125, utilArmMax0, utilArmMin0)); //533? - 300
         int utilArmPos1 = (map(joystickPositionY, 0, 125, utilArmMax1, utilArmMin1)); //87 -310
         if (lastUtilArmPos1 > utilArmPos1){
           moveUtilArms(0,0,joystickPositionY,utilArmPos0);
           moveUtilArms(1,0,joystickPositionY,utilArmPos1);
         }else{
           moveUtilArms(0,0,joystickPositionY,lastUtilArmPos0);
           moveUtilArms(1,0,joystickPositionY,lastUtilArmPos1);
         }
        }else if (joystickPositionY > 125 && !myPS3->getButtonPress(L3)){
         int utilArmPos0 = (map(joystickPositionY, 127, 225, utilArmMin0, (lastUtilArmPos0+10)));
         int utilArmPos1 = (map(joystickPositionY, 127, 225, (lastUtilArmPos1-10), utilArmMin1));
         if (lastUtilArmPos1 < utilArmPos1){
           moveUtilArms(0,1,joystickPositionY,utilArmPos0);
           moveUtilArms(1,1,joystickPositionY,utilArmPos1);
         }else{
           moveUtilArms(0,1,joystickPositionY,lastUtilArmPos0);
           moveUtilArms(1,1,joystickPositionY,lastUtilArmPos1);
         }
        }else{
          #ifdef SERVO_EASING
            //pwm2_Servo8.detach();
            //pwm2_Servo9.detach();
          #else
            pwm2.setPWM(8, 0, 0);
            pwm2.setPWM(9, 0, 0);
          #endif
       }
     }else if (!myPS3->getButtonPress(L1)){
          #ifdef SERVO_EASING
            //pwm2_Servo8.detach();
            //pwm2_Servo9.detach();
          #else
            pwm2.setPWM(8, 0, 0);
            pwm2.setPWM(9, 0, 0);
          #endif
     }

    #pragma mark -
    #pragma mark    Buzz Saw

    // BUZZ SAW
    #ifdef SERVO_EASING
      if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(CIRCLE)){
        if (!buzzActivated){
          processSoundCommand(79);
 //         buzzSawMillis = millis()+15000;
          buzzActivated = true;
           Serial.println("buzzActivated = true");
          buzzSpinning = true;
          digitalWrite(PIN_BUZZSAW, HIGH);//spin Saw
          pwm2_Servo11.easeToD(pwm2_11Info.maxDeg,250); // Buzz Door
          pwm2_Servo11.detach();
          //delay(200);
          //silenceServos();
          pwm2_Servo1.easeTo(-100); // Buzz Slide
          delay(1650);//1800
          pwm2_Servo1.detach(); // Buzz Slide
        }else if (buzzActivated){
          buzzActivated = false;
           Serial.println("buzzActivated = false");

          digitalWrite(PIN_BUZZSAW, LOW);
          pwm2_Servo1.easeTo(100); // Buzz Slide close
          delay(1650);//1800
          buzzSpinning = false;
          pwm2_Servo1.detach(); // Silence Buzz Slide
          pwm2_Servo11.easeTo(pwm2_11Info.minDeg); // Buzz Door
           delay(250);
           pwm2_Servo11.detach();
          //delay(200);
          //pwm2_Servo1.detach(); // Buzz Slide
          //silenceServos();
         //Serial.println("BuzzSAW!");
        }
      }
    #else
      if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(CIRCLE)){
        if (!buzzActivated){
          processSoundCommand(79);
          buzzSawMillis = millis()+15000;
          buzzActivated = true;
          buzzSpinning = true;
          digitalWrite(PIN_BUZZSAW, HIGH);
          pwm2.setPWM(11, 0, pwm2_11_max); // Buzz Door
          delay(200);
          silenceServos();
          pwm2.setPWM(1, 0, pwm2_1_max); // Buzz Slide
          delay(2100);//1800
          pwm2.setPWM(1, 0, 0); // Buzz Slide
        }else if (buzzActivated){
          buzzActivated = false;
          buzzSpinning = false;
          digitalWrite(PIN_BUZZSAW, LOW);
          pwm2.setPWM(1, 0, pwm2_1_min); // Buzz Slide
          delay(2225);//1800
          pwm2.setPWM(1, 0, 0); // Buzz Slide
          pwm2.setPWM(11, 0, pwm2_11_min); // Buzz Door
          delay(200);
          silenceServos();
         Serial.println("BuzzSAW!");
        }
      }
    #endif
    // CENTER DOME
    if (!myPS3->getButtonPress(L2)&&!myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(PS)){
      if (myPS3->getButtonClick(CIRCLE)){
        centerDomeSpeed = 50;
      }else if (myPS3->getButtonClick(CROSS)){
        centerDomeSpeed = -50;
      }
    }

#pragma mark -
#pragma mark Gripper
    // Gripper Arm/////////////////////////////////////////////
    #ifdef SERVO_EASING
      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CIRCLE)){
      //if(myPS3->getButtonClick(CIRCLE)){
        if (!gripperActivated){
          Serial.println("Gripper Activated");
          //trigger.trigger(24);
          //Swap 6 to 4... Ian has his gripper and interface reversed from R-2
          pwm2_Servo4.easeTo(pwm2_4Info.maxDeg); // Door
          pwm2_Servo12.easeTo(pwm2_12Info.maxDeg);  //Gripper Lifter Up
          gripperActivated = true;
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg); //Gripper
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.maxDeg);
          pwm2_Servo13.easeTo(pwm2_13Info.minDeg);
          //silenceServos();
        }else{
          Serial.println("Gripper Arm DeActivated");
          pwm2_Servo12.easeTo(pwm2_12Info.minDeg); //Lower the arm
          delay(500);
          pwm2_Servo4.easeTo(pwm2_4Info.minDeg); // close the door
          //silenceServos();
          gripperActivated = false;
        }
        silenceServos();
      }
    #else
      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CIRCLE)){
      //if(myPS3->getButtonClick(CIRCLE)){
        if (!gripperActivated){
          //trigger.trigger(24);
          //Swap 6 to 4... Ian has his gripper and interface reversed from R-2
          pwm2.setPWM(4, 0, pwm2_4_max); // LT
          //pwm2.setPWM(7, 0, pwm2_7_max); // CBI Door
          delay(300);
          pwm2.setPWM(12, 0, pwm2_12_max);
          gripperActivated = true;
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_max);
          delay(300);
          pwm2.setPWM(13,0,pwm2_13_min);
          delay(300);
          silenceServos();
        }else{
          pwm2.setPWM(12, 0, pwm2_12_min);
          delay(1000);
          pwm2.setPWM(4, 0, pwm2_4_min); // LT
          //pwm2.setPWM(7, 0, pwm2_7_min); // CBI Door
          delay(300);
          silenceServos();
          gripperActivated = false;
        }
      }
    #endif

#pragma mark  -
#pragma mark InterfaceArm
   
    // Interface Arm
    #ifdef SERVO_EASING
      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CROSS)){
      //if(myPS3->getButtonClick(CROSS)){
        Serial.println("Interface Arm");
        if (!interfaceActivated){
          //Swap 4 to 6...Ian has his gripper and interface arms reversed from R2
          pwm2_Servo6.easeTo(pwm2_6Info.maxDeg); // Interface Door
          delay(200); //not sure I need these delays
          //silenceServos();
          pwm2_Servo14.easeTo(pwm2_14Info.maxDeg);//Interface Lifter
          interfaceActivated = true;
          //delay(500);
          pwm2_Servo15.easeTo(pwm2_15Info.maxDeg);
          //delay(500);
          pwm2_Servo15.easeTo(pwm2_15Info.minDeg); //Interface Twister Tool
           pwm2_Servo15.easeTo(pwm2_15Info.maxDeg);
           //delay(500);
           pwm2_Servo15.easeTo(pwm2_15Info.minDeg); //Interface Twister Tool
          //delay(500);
          silenceServos();
        }else{
          pwm2_Servo15.easeTo(pwm2_15Info.minDeg); //Interface Twister Tool
          pwm2_Servo14.easeTo(pwm2_14Info.minDeg); //Interface lifter
          delay(300);
          pwm2_Servo6.easeTo(pwm2_6Info.minDeg); // Door
          delay(300);
          silenceServos();
          interfaceActivated = false;
        }
      }
    #else
      if(myPS3->getButtonPress(L2)&&myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CROSS)){
      //if(myPS3->getButtonClick(CROSS)){
        Serial.println("Interface Arm");
        if (!interfaceActivated){
          //Swap 4 to 6...Ian has his gripper and interface arms reversed from R2
          pwm2.setPWM(6, 0, pwm2_6_max); // LT
          //pwm2.setPWM(5, 0, pwm2_5_max); // LB  i don't use two servos on the doors
          delay(200);
          silenceServos();
          pwm2.setPWM(14, 0, pwm2_14_max);
          interfaceActivated = true;
          delay(500);
          pwm2.setPWM(15,0,pwm2_15_max);
          delay(500);
          pwm2.setPWM(15,0,pwm2_15_min); //Interface Twister Tool
          delay(500);
          silenceServos();
        }else{
          pwm2.setPWM(15,0,pwm2_15_min); //Interface Twister Tool
          pwm2.setPWM(14, 0, pwm2_14_min); //Interface lifter
          delay(1000);
          pwm2.setPWM(6, 0, pwm2_6_min); // LT
          //pwm2.setPWM(5, 0, pwm2_5_min); // LB  not used.  I only have 1 servo per breadpan door
          delay(300);
          silenceServos();
          interfaceActivated = false;
        }
      }
    #endif
    // ZAPPER
    #ifdef SERVO_EASING
      //No Zapper
    #else
      if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(CROSS)){
         if (!zapperActivated){
            pwm2.setPWM(3, 0, 380); // Zapper
            delay(200);
            silenceServos();
            pwm2.setPWM(0, 0, pwm2_0_max); // Zapper Slide
            delay(1800);
            pwm2.setPWM(0, 0, 0); // Zapper Slide
            zapperActivated = true;
         }else if (zapperActivated){
            zapperActivated = false;
            pwm2.setPWM(0, 0, pwm2_0_min); // Zapper Slide
            delay(2000);
            pwm2.setPWM(0, 0, 0); // Zapper Slide
            pwm2.setPWM(3, 0, 850); // Zapper
            delay(500);
            silenceServos();
          }
       }
     #endif

    // Extinguisher
    if(myPS3->getButtonPress(PS)&&myPS3->getButtonClick(L3)){
      #ifdef SERVO_EASING
        //No Extinguisher yet
      #else
        pwm1.setPWM(15, 0, pwm1_15_max);
        delay(300);
        pwm1.setPWM(15, 0, pwm1_15_min);
        delay(100);
        pwm1.setPWM(15, 0, 0);
      #endif
    }

    // Disiabe the DriveStick
    if(myPS3->getButtonPress(L2)&&!myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CROSS)){
      isStickEnabled = false;
      //sfx.playTrack("T07     OGG");
    }

    // Enable the DriveStick
    if(myPS3->getButtonPress(L2)&&!myPS3->getButtonPress(L1)&&myPS3->getButtonClick(CIRCLE)){
      isStickEnabled = true;
      //sfx.playTrack("T08     OGG");;
    }

    // Auto Dome Off
    if(myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(L2)&&myPS3->getButtonClick(CROSS)){
      isAutomateDomeOn = false;
      automateAction = 0;
    }

    // Auto Dome On
    if(myPS3->getButtonPress(L1)&&!myPS3->getButtonPress(L2)&&myPS3->getButtonClick(CIRCLE)){
      isAutomateDomeOn = true;
    }
}

void processCommands(){
   if (PS3Nav->PS3NavigationConnected) ps3ProcessCommands(PS3Nav);
   if (PS3Nav2->PS3NavigationConnected) ps3ProcessCommands(PS3Nav2);
}


// ==============================================================
//             START UP AND OTHER IMPORTANT FUNCTIONS
// ==============================================================


String getLastConnectedBtMAC(){
    String btAddress = "";
    for(int8_t i = 5; i > 0; i--){
        if (Btd.disc_bdaddr[i]<0x10){
            btAddress +="0";
        }
        btAddress += String(Btd.disc_bdaddr[i], HEX);
        btAddress +=(":");
    }
    btAddress += String(Btd.disc_bdaddr[0], HEX);
    btAddress.toUpperCase();
    return btAddress;
}




void onInitPS3(){
    Serial.print(F("\r\nGot to onInitPS3"));
    String btAddress = getLastConnectedBtMAC();
    PS3Nav->setLedOn(LED1);
    isPS3NavigatonInitialized = true;
    badPS3Data = 0;
    //sfx.playTrack("T12     OGG");
    trigger.trigger(23);
    #ifdef SHADOW_DEBUG
      output += "\r\nBT Address of Last connected Device when Primary PS3 Connected: ";
      output += btAddress;
      if (btAddress == PS3MoveNavigatonPrimaryMAC){
          output += "\r\nWe have our primary controller connected.\r\n";

      }else{
          output += "\r\nWe have a controller connected, but it is not designated as \"primary\".\r\n";
      }
    #endif
}




void onInitPS3Nav2(){
    String btAddress = getLastConnectedBtMAC();
    PS3Nav2->setLedOn(LED1);
    isSecondaryPS3NavigatonInitialized = true;
    badPS3Data = 0;
    //sfx.playTrack("T12     OGG");
    
    //TODO:Fix the error when uncommenting line below
    if (btAddress == PS3MoveNavigatonPrimaryMAC) swapPS3NavControllers();
    #ifdef SHADOW_DEBUG
      output += "\r\nBT Address of Last connected Device when Secondary PS3 Connected: ";
      output += btAddress;
      if (btAddress == PS3MoveNavigatonPrimaryMAC){
          output += "\r\nWe have our primary controller connecting out of order.  Swapping locations\r\n";
      }else{
          output += "\r\nWe have a secondary controller connected.\r\n";
      }
    #endif
}

void swapPS3NavControllers(){
    PS3BT* temp = PS3Nav;
    PS3Nav = PS3Nav2;
    PS3Nav2 = temp;
    //Correct the status for Initialization
    boolean tempStatus = isPS3NavigatonInitialized;
    isPS3NavigatonInitialized = isSecondaryPS3NavigatonInitialized;
    isSecondaryPS3NavigatonInitialized = tempStatus;
    //Must relink the correct onInit calls
    PS3Nav->attachOnInit(onInitPS3);
    PS3Nav2->attachOnInit(onInitPS3Nav2);
}



void initAndroidTerminal(){
    //Setup for Bluetooth Serial Monitoring
    if (SerialBT.connected){
        if (firstMessage){
            firstMessage = false;
            SerialBT.println(F("Hello from S.H.A.D.O.W.")); // Send welcome message
        }
    }else{
        firstMessage = true;
    }
}

void flushAndroidTerminal(){
    if (output != ""){
        if (Serial) Serial.println(output);
        if (SerialBT.connected)
            SerialBT.println(output);
            SerialBT.send();
        output = ""; // Reset output string
    }
}

int get_int_len (int value){
  int l=1;
  while(value>9){ l++; value/=10; }
  return l;
}


////////////////NeoPixel Handler////////////////////

#ifdef NEOPIXEL_TEST
 /************************************************************************************************************
   * Put your own pattern code here
   * Provided are sample implementation not supporting initial direction DIRECTION_DOWN
   * Full implementation of this functions can be found in the NeoPatterns.cpp file of the NeoPatterns library
   ************************************************************************************************************/

  /*
   * set all pixel to aColor1 and let a pixel of color2 move through
   * Starts with all pixel aColor1 and also ends with it.
   */
  void UserPattern1(NeoPatterns * aNeoPatterns, color32_t aPixelColor, color32_t aBackgroundColor, uint16_t aIntervalMillis,
          uint8_t aDirection) {
      /*
       * Sample implementation not supporting DIRECTION_DOWN
       */
      aNeoPatterns->ActivePattern = PATTERN_USER_PATTERN1;
      aNeoPatterns->Interval = aIntervalMillis;
      aNeoPatterns->Color1 = aPixelColor;
      aNeoPatterns->LongValue1.BackgroundColor = aBackgroundColor;
      aNeoPatterns->Direction = aDirection;
      aNeoPatterns->TotalStepCounter = aNeoPatterns->numPixels() + 1;
      aNeoPatterns->ColorSet(aBackgroundColor);
      aNeoPatterns->show();
      aNeoPatterns->lastUpdate = millis();
  }

  /*
   * @return - true if pattern has ended, false if pattern has NOT ended
   */
  bool UserPattern1Update(NeoPatterns * aNeoPatterns, bool aDoUpdate) {
      /*
       * Sample implementation not supporting initial direction DIRECTION_DOWN
       */
      if (aDoUpdate) {
          if (aNeoPatterns->decrementTotalStepCounterAndSetNextIndex()) {
              return true;
          }
      }

      for (uint16_t i = 0; i < aNeoPatterns->numPixels(); i++) {
          if (i == aNeoPatterns->Index) {
              aNeoPatterns->setPixelColor(i, aNeoPatterns->Color1);
          } else {
              aNeoPatterns->setPixelColor(i, aNeoPatterns->LongValue1.BackgroundColor);
          }
      }

      return false;
  }

  /*
   * let a pixel of aColor move up and down
   * starts and ends with all pixel cleared
   */
  void UserPattern2(NeoPatterns * aNeoPatterns, color32_t aColor, uint16_t aIntervalMillis, uint16_t aRepetitions,
          uint8_t aDirection) {
      /*
       * Sample implementation not supporting DIRECTION_DOWN
       */
      aNeoPatterns->ActivePattern = PATTERN_USER_PATTERN2;
      aNeoPatterns->Interval = aIntervalMillis;
      aNeoPatterns->Color1 = aColor;
      aNeoPatterns->Direction = aDirection;
      aNeoPatterns->Index = 0;
      // *2 for up and down. (aNeoPatterns->numPixels() - 1) do not use end pixel twice.
      // +1 for the initial pattern with end pixel. + 2 for the first and last clear pattern.
      aNeoPatterns->TotalStepCounter = ((aRepetitions + 1) * 2 * (aNeoPatterns->numPixels() - 1)) + 1 + 2;
      aNeoPatterns->clear();
      aNeoPatterns->show();
      aNeoPatterns->lastUpdate = millis();
  }

  /*
   * @return - true if pattern has ended, false if pattern has NOT ended
   */
  bool UserPattern2Update(NeoPatterns * aNeoPatterns, bool aDoUpdate) {
      /*
       * Sample implementation
       */
      if (aDoUpdate) {
          // clear old pixel
         //aNeoPatterns->setPixelColor(aNeoPatterns->Index, COLOR32_BLACK);
         aNeoPatterns->setPixelColor(aNeoPatterns->Index, COLOR_YELLOW);

          if (aNeoPatterns->decrementTotalStepCounterAndSetNextIndex()) {
              return true;
          }
          /*
           * Next index
           */
          if (aNeoPatterns->Direction == DIRECTION_UP) {
              // do not use top pixel twice
              if (aNeoPatterns->Index == (aNeoPatterns->numPixels() - 1)) {
                  aNeoPatterns->Direction = DIRECTION_DOWN;
              }
          } else {
              // do not use bottom pixel twice
              if (aNeoPatterns->Index == 0) {
                  aNeoPatterns->Direction = DIRECTION_UP;
              }
          }
      }
      /*
       * Refresh pattern
       */
      if (aNeoPatterns->TotalStepCounter != 1) {
          // last pattern is clear
          aNeoPatterns->setPixelColor(aNeoPatterns->Index, aNeoPatterns->Color1);
      }
      return false;
  }

  /*
   * Handler for testing your own patterns
   */
  void ownPatterns(NeoPatterns * aLedsPtr) {
      static int8_t sState = 0;

      uint8_t tDuration = random(20, 120);
      uint8_t tColor = random(255);
      uint8_t tRepetitions = random(2);

      switch (sState) {
      case 0:
          UserPattern1(aLedsPtr, COLOR32_RED_HALF, NeoPatterns::Wheel(tColor), tDuration, FORWARD);
          break;

      case 1:
          UserPattern2(aLedsPtr, NeoPatterns::Wheel(tColor), tDuration, tRepetitions, FORWARD);
          sState = -1; // Start from beginning
          break;

      default:
          Serial.println("ERROR");
          break;
      }

      sState++;
  }
  //*/
//#else
#endif
  /*
   * Handler for all pattern
   */
  void allPatterns(NeoPatterns * aLedsPtr) {
      static int8_t sState = 1; //was 0, but Case 0 is busted

      uint8_t tDuration = random(40, 81);
      uint8_t tColor = random(255);

      switch (sState) {
      case 0:
          // simple scanner
          aLedsPtr->clear();
          aLedsPtr->ScannerExtended(NeoPatterns::Wheel(tColor), 5, tDuration, 2, FLAG_SCANNER_EXT_CYLON);
          break;
      case 1:
          // rocket and falling star - 2 times bouncing
          aLedsPtr->ScannerExtended(NeoPatterns::Wheel(tColor), 7, tDuration, 2,
          FLAG_SCANNER_EXT_ROCKET | FLAG_SCANNER_EXT_START_AT_BOTH_ENDS, (tDuration & DIRECTION_DOWN));
          break;
      case 2:
          // 1 times rocket or falling star
          aLedsPtr->clear();
          aLedsPtr->ScannerExtended(COLOR32_WHITE_HALF, 7, tDuration / 2, 0, FLAG_SCANNER_EXT_VANISH_COMPLETE,
                  (tDuration & DIRECTION_DOWN));
          break;
      case 3:
          aLedsPtr->RainbowCycle(20);
          break;
      case 4:
          aLedsPtr->Stripes(COLOR32_WHITE_HALF, 5, NeoPatterns::Wheel(tColor), 3, tDuration, 2 * aLedsPtr->numPixels(),
                (tDuration & DIRECTION_DOWN));
          break;
      case 5:
          // old TheaterChase
          aLedsPtr->Stripes(NeoPatterns::Wheel(tColor), 1, NeoPatterns::Wheel(tColor + 0x80), 2, tDuration / 2,
                2 * aLedsPtr->numPixels(), (tDuration & DIRECTION_DOWN));
          break;
      case 6:
          aLedsPtr->Fade(COLOR32_RED, COLOR32_BLUE, 32, tDuration);
          break;
      case 7:
          aLedsPtr->ColorWipe(NeoPatterns::Wheel(tColor), tDuration);
          break;
      case 8:
          // clear pattern
          aLedsPtr->ColorWipe(COLOR32_BLACK, tDuration, FLAG_DO_NOT_CLEAR, DIRECTION_DOWN);
          break;
      case 9:
          // Multiple falling star
          initMultipleFallingStars(aLedsPtr, COLOR32_WHITE_HALF, 7, tDuration / 2, 3, &allPatterns);
          break;
      case 10:
          if ((aLedsPtr->PixelFlags & PIXEL_FLAG_GEOMETRY_CIRCLE) == 0) {
              //Fire
              aLedsPtr->Fire(tDuration * 2, tDuration / 2);
          } else {
              // start at both end
              aLedsPtr->ScannerExtended(NeoPatterns::Wheel(tColor), 5, tDuration, 0,
              FLAG_SCANNER_EXT_START_AT_BOTH_ENDS | FLAG_SCANNER_EXT_VANISH_COMPLETE);
          }

          sState = 1;  //Case 0 is busted so skip it-1; // Start from beginning
          break;
      default:
          Serial.println("ERROR");
          break;
      }


  //    Serial.print("Pin=");
  //    Serial.print(aLedsPtr->getPin());
  //    Serial.print(" Length=");
  //    Serial.print(aLedsPtr->numPixels());
  //    Serial.print(" ActivePattern=");
  //    Serial.print(aLedsPtr->ActivePattern);
  //    Serial.print(" State=");
  //    Serial.println(sState);


      sState++;
  }
//#endif


// =======================================================================================
//          Print Output Function
// =======================================================================================

void printOutput()
{
    if (output != "")
    {
        if (Serial) Serial.println(output);
        output = ""; // Reset output string
    }
}


// ==============================================================
//                      START UP SEQUENCE
// ==============================================================
void setup(){
    //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
    if (Usb.Init() == -1){
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }
    Serial.print(F("\r\nBluetooth Library Started"));
    output.reserve(200); // Reserve 200 bytes for the output string

    //Setup for PS3
    PS3Nav->attachOnInit(onInitPS3); // onInit() is called upon a new connection - you can call the function whatever you like
    PS3Nav2->attachOnInit(onInitPS3Nav2);
    Serial1.begin(9600);
    Serial.print(F("\r\nEnd PS3 Connection"));

    //Setup for Serial2:: Motor Controllers - Syren (Dome) and Sabertooth (Feet)
    Serial2.begin(motorControllerBaudRate);

    //SyR->setRamping(1980);

    #ifdef EEBEL_TEST
    //Tring to see if R2-D2 SHADOW-MD code helps.  R5 tends to crash and not connect sometimes
    //Setup for Serial2:: Motor Controllers - Sabertooth (Feet)
      ST->autobaud();          // Send the autobaud command to the Sabertooth controller(s).
      ST->setTimeout(10);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
      ST->setDeadband(driveDeadBandRange);
      ST->stop();
      Serial.print(F("\r\nEnd Eebel SaberTooth SetUp"));
      SyR->autobaud();
      SyR->setTimeout(20);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
      SyR->stop();
      Serial.print(F("\r\nEnd Eebel Syren SetUp"));
      Serial3.begin(9600); //
    #else
      //Original R5 code
      SyR->autobaud();
      //SyR->setRamping(1980);
      SyR->setTimeout(300);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
      Serial.print(F("\r\nEnd Syren SetUp"));

      Serial3.begin(9600); //

      //Setup for Sabertooth / Foot Motors
      ST->autobaud();          // Send the autobaud command to the Sabertooth controller(s).
      ST->setTimeout(300);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
      ST->setDeadband(driveDeadBandRange);
      ST->stop();
      Serial.print(F("\r\nEnd SaberTooth SetUp"));
    #endif


#pragma mark -
#pragma mark ServoLimits
    // START UP SERVO CONTROLLERS
    #ifdef SERVO_EASING
    ////ServoEasing setup////////////////////////////////////////////
   //I used a servo tester with a microseconds readout to set range of servo in the attach command.
   //Then I set the range of each servo from 0 to 180 degrees.
   //I also set reverse rotation at the attach command so 0 is always closed an 180 is always fully open.
    Serial.print(F("\r\nUsing Servo Easing"));
    pwm3_1Info.servoName = "Pie1";
    pwm3_1Info.minDeg = 0;//73
    pwm3_1Info.maxDeg = 180;//140,160, 173
    pwm3_1Info.pinNum = 1;
    pwm3_1Info.pwmNum = 3;
    pwm3_2Info.servoName = "Pie2";
    pwm3_2Info.minDeg = 0;//70
    pwm3_2Info.maxDeg = 180;//130, 135,115
    pwm3_2Info.pinNum = 2;
    pwm3_2Info.pwmNum = 3;
    pwm3_3Info.servoName = "Pie3";
    pwm3_3Info.minDeg = 0;//70
    pwm3_3Info.maxDeg = 180;//145, 115
    pwm3_3Info.pinNum = 3;
    pwm3_3Info.pwmNum = 3;
    pwm3_4Info.servoName = "Pie4";
    pwm3_4Info.minDeg = 0;//79
    pwm3_4Info.maxDeg = 180;//140
    pwm3_4Info.pinNum = 4;
    pwm3_4Info.pwmNum = 3;
    pwm3_5Info.servoName = "Pie5";
    pwm3_5Info.minDeg = 0;//74
    pwm3_5Info.maxDeg = 180;//140, 127
    pwm3_5Info.pinNum = 5;
    pwm3_5Info.pwmNum = 3;
    pwm3_6Info.servoName = "Pie6";
    pwm3_6Info.minDeg = 0;//50, 68
    pwm3_6Info.maxDeg = 180;//125,108
    pwm3_6Info.pinNum = 6;
    pwm3_6Info.pwmNum = 3;
    pwm3_7Info.servoName = "Pie7";
    pwm3_7Info.minDeg = 0;//75, 92
    pwm3_7Info.maxDeg = 180;//180, 174
    pwm3_7Info.pinNum = 7;
    pwm3_7Info.pwmNum = 3;
    pwm3_8Info.servoName = "LifeSlider";
    pwm3_8Info.minDeg = 92;
    pwm3_8Info.maxDeg = 174;
    pwm3_8Info.pinNum = 8;
    pwm3_8Info.pwmNum = 3;
    pwm3_9Info.servoName = "LifeRotate";
    pwm3_9Info.minDeg = 92;
    pwm3_9Info.maxDeg = 174;
    pwm3_9Info.pinNum = 9;
    pwm3_9Info.pwmNum = 3;

    pwm2_1Info.servoName = "BuzzSlide";
    pwm2_1Info.minDeg = 179;
    pwm2_1Info.maxDeg = 20;
    pwm2_1Info.pinNum = 1;
    pwm2_1Info.pwmNum = 2;
    pwm2_4Info.servoName = "LeftBreadPan";//gripper door
    pwm2_4Info.minDeg = 0;//90
    pwm2_4Info.maxDeg = 180;//160,90, 60
    pwm2_4Info.pinNum = 4;
    pwm2_4Info.pwmNum = 2;
    pwm2_6Info.servoName = "RightBreadPan";//Interface door
    pwm2_6Info.minDeg = 0;//15
    pwm2_6Info.maxDeg = 180;//65, 95, 140, 93
    pwm2_6Info.pinNum = 6;
    pwm2_6Info.pwmNum = 2;
    pwm2_7Info.servoName = "CBIDoor";
    pwm2_7Info.minDeg = 0;//36, 40, 35, 30, 25, 15,47
    pwm2_7Info.maxDeg = 180;//70, 75, 85, 90, 105, 115, 140, 85, 64
    pwm2_7Info.pinNum = 7;
    pwm2_7Info.pwmNum = 2;
    pwm2_8Info.servoName = "LowerUtil";
    pwm2_8Info.minDeg = 0;//35
    pwm2_8Info.maxDeg = 180;//145
    pwm2_8Info.pinNum = 8;
    pwm2_8Info.pwmNum = 2;
    pwm2_9Info.servoName = "UpperUtil";
    pwm2_9Info.minDeg = 0;//5
    pwm2_9Info.maxDeg = 180;//145
    pwm2_9Info.pinNum = 9;
    pwm2_9Info.pwmNum = 2;
    pwm2_11Info.servoName = "BuzzDoor";
    pwm2_11Info.minDeg = 51;//51,50, 45, 35,30, 20, 94
    pwm2_11Info.maxDeg = 120;//120,200, 180, 164
    pwm2_11Info.pinNum = 11;
    pwm2_11Info.pwmNum = 2;
    pwm2_12Info.servoName = "GripperLift";
    pwm2_12Info.minDeg = 0;//10, 68
    pwm2_12Info.maxDeg = 180;//140, 130
    pwm2_12Info.pinNum = 12;
    pwm2_12Info.pwmNum = 2;
    pwm2_13Info.servoName = "Gripper";
    pwm2_13Info.minDeg = 0;//-25, 72
    pwm2_13Info.maxDeg = 180;//35, 120
    pwm2_13Info.pinNum = 13;
    pwm2_13Info.pwmNum = 2;
    pwm2_14Info.servoName = "InterfaceLift";
    pwm2_14Info.minDeg = 0;//169
    pwm2_14Info.maxDeg = 180;//11
    pwm2_14Info.pinNum = 14;
    pwm2_14Info.pwmNum = 2;
    pwm2_15Info.servoName = "Interface";
    pwm2_15Info.minDeg = 0;//30
    pwm2_15Info.maxDeg = 180;//140
    pwm2_15Info.pinNum = 15;
    pwm2_15Info.pwmNum = 2;

    pwm1_4Info.servoName = "DataPanel";
    pwm1_4Info.minDeg = 0;//65,77, 47
    pwm1_4Info.maxDeg = 180;//37
    pwm1_4Info.pinNum = 4;
    pwm1_4Info.pwmNum = 1;


//      pwm3_Servo1.attach(pwm3_1Info.pinNum,2400,620);
//      pwm3_Servo1.write(pwm3_1Info.minDeg);
//      pwm3_Servo2.attach(pwm3_2Info.pinNum, 2400,620);
//      pwm3_Servo2.write(pwm3_2Info.minDeg);
//      pwm3_Servo3.attach(pwm3_3Info.pinNum, 2400,620);
//      pwm3_Servo3.write(pwm3_3Info.minDeg);
//      pwm3_Servo4.attach(pwm3_4Info.pinNum,2400,620);
//      pwm3_Servo4.write(pwm3_4Info.minDeg);
//      pwm3_Servo5.attach(pwm3_5Info.pinNum, 2400,620);
//      pwm3_Servo5.write(pwm3_5Info.minDeg);
//      pwm3_Servo6.attach(pwm3_6Info.pinNum, 2400,620);
//      pwm3_Servo6.write(pwm3_6Info.minDeg);
//      pwm3_Servo7.attach(pwm3_7Info.pinNum, 2400, 620);
//      pwm3_Servo7.write(pwm3_7Info.minDeg);


    //const int pwm2BuzzSlide_Pin = 1;
    //const int pwm2LeftBreadPan_Pin = 4;
    //const int pwm2RightBreadPan_Pin = 6;
    //const int pwm2CBIDoor_Pin = 7;
    //const int pwm2LowerUtil_Pin = 8;
    //const int pwm2UpperUtil_Pin = 9;
    //const int pwm2BuzzDoor_Pin = 11;
    //const int pwm2GripperLift_Pin = 12;
    //const int pwm2Gripper_Pin = 13;
    //const int pwm2InterfaceLift_Pin = 14;
    //const int pwm2Interface_Pin = 15;
#pragma mark -
#pragma mark AttachServos
      //pwm1_Servo4.attach(pwm1_4Info.pinNum, 544, 2400);//DataPanel Door
      pwm1_Servo4.attach(pwm1_4Info.pinNum, 1735, 2165);//DataPanel Door
      //pwm1_Servo4.setReverseOperation(true);
      //pwm1_Servo4.write(pwm1_4Info.minDeg); //
      
   
      pwm2_Servo1.attach(pwm2_1Info.pinNum, 544, 2400, -100, 100);//Buzz Slide
    if(digitalRead(PIN_BUZZSLIDELIMIT) == HIGH){
        pwm2_Servo1.startEaseTo(100); //  Only move the Servo if the limit is not met and then only until limit so use non-blocking
    }
      
      pwm2_Servo4.attach(pwm2_4Info.pinNum, 1710, 2150);//Gripper Door 1650-2150
      //pwm2_Servo4.write(pwm2_4Info.minDeg); //
      pwm2_Servo6.attach(pwm2_6Info.pinNum, 1800, 2270);//Interface Door 2258
      pwm2_Servo6.setReverseOperation(true);
      //pwm2_Servo6.write(pwm2_6Info.minDeg); //
      pwm2_Servo7.attach(pwm2_7Info.pinNum, 1600, 2025);//CBI Door, 1575-2025, 1980,1936
      pwm2_Servo7.setReverseOperation(true);
      //pwm2_Servo7.write(pwm2_7Info.minDeg);
      pwm2_Servo8.attach(pwm2_8Info.pinNum, 480, 1848); //Lower UtiltyArm
      //pwm2_Servo8.attach(pwm2_8Info.pinNum, 511, 1848); //Lower UtiltyArm
      pwm2_Servo8.setReverseOperation(true);
      //pwm2_Servo8.write(pwm2_8Info.minDeg);
      pwm2_Servo9.attach(pwm2_9Info.pinNum, 660, 2139);//Upper UtilityArm
      pwm2_Servo9.setReverseOperation(true);
      //pwm2_Servo9.write(pwm2_9Info.minDeg);
      pwm2_Servo11.attach(pwm2_11Info.pinNum, 650,2305);//Buzz Door, 690,935-1797,2295
      //pwm2_Servo11.setEasingType(EASE_LINEAR);
      pwm2_Servo11.setReverseOperation(true);
      //pwm2_Servo11.write(pwm2_11Info.minDeg); //
      pwm2_Servo12.attach(pwm2_12Info.pinNum, 700, 2200);//Gripper Lift
      //pwm2_Servo12.write(pwm2_12Info.minDeg); //
      pwm2_Servo13.attach(pwm2_13Info.pinNum, 2000, 2600);//Gripper
      pwm2_Servo13.setReverseOperation(true);
      //pwm2_Servo13.write(pwm2_13Info.minDeg); //
      pwm2_Servo14.attach(pwm2_14Info.pinNum, 700, 2200);//InterfaceLifter
      pwm2_Servo14.setReverseOperation(true);
      //pwm2_Servo14.write(pwm2_14Info.minDeg); //InterfaceLifter
      pwm2_Servo15.attach(pwm2_15Info.pinNum, 737, 1850);//Interface5
      //pwm2_Servo15.write(pwm2_15Info.minDeg); //Interface

      pwm3_Servo1.attach(pwm3_1Info.pinNum,920,1600);//Pie 1
      pwm3_Servo1.setReverseOperation(true);
      //pwm3_Servo1.write(pwm3_1Info.minDeg);
      pwm3_Servo2.attach(pwm3_2Info.pinNum, 1064,1660);//Pie 2
      pwm3_Servo2.setReverseOperation(true);
      //pwm3_Servo2.write(pwm3_2Info.minDeg);
      pwm3_Servo3.attach(pwm3_3Info.pinNum, 980,1568);//Pie 3
      pwm3_Servo3.setReverseOperation(true);
      //pwm3_Servo3.write(pwm3_3Info.minDeg);
      pwm3_Servo4.attach(pwm3_4Info.pinNum,873,1575);//Pie 4
      pwm3_Servo4.setReverseOperation(true);
      //pwm3_Servo4.write(pwm3_4Info.minDeg);
      pwm3_Servo5.attach(pwm3_5Info.pinNum, 840,1625);//Pie 5
      pwm3_Servo5.setReverseOperation(true);
      //pwm3_Servo5.write(pwm3_5Info.minDeg);
      pwm3_Servo6.attach(pwm3_6Info.pinNum, 1185,1855);//Pie 6
      pwm3_Servo6.setReverseOperation(true);
      //pwm3_Servo6.write(pwm3_6Info.minDeg);
      pwm3_Servo7.attach(pwm3_7Info.pinNum, 608, 1260);//Pie 7
      pwm3_Servo7.setReverseOperation(true);
      //pwm3_Servo7.write(pwm3_7Info.minDeg);

    setSpeedForAllServos(600);//300,make servos fast. Go double since all servos think they are going 180 degrees but they are really only going 90 or less

   
   if(pwm3_Servo7.attach(pwm3_7Info.pinNum, 608, 1260) == INVALID_SERVO) {
       Serial.print(F("Error attaching servo - maybe MAX_EASING_SERVOS="));
       Serial.print(MAX_EASING_SERVOS);
       Serial.println(F(" is to small to hold all servos"));
//       while (true) {
//           digitalWrite(LED_BUILTIN, HIGH);
//           delay(100);
//           digitalWrite(LED_BUILTIN, LOW);
//           delay(100);
//       }
   }
    /////////////////////////////////////////////////////////////////

    #else
      Serial.print(F("\r\nNot Using Servo Easing"));
   
      pwm1.begin();
      pwm1.setPWMFreq(62);  // This is the maximum PWM frequency

      pwm2.begin();
      pwm2.setPWMFreq(62);  // This is the maximum PWM frequency

      pwm3.begin();
      pwm3.setPWMFreq(62);  // This is the maximum PWM frequency

      pwm1.setPWM(15, 0, pwm1_15_min); // Extinguisher
      delay(100);
      pwm1.setPWM(10, 0, 0); //Rear bolt door
    #endif

    Serial.print(F("\r\nEnd PWM SetUp"));



    // ZAPPER and BUZZSAW SETUP
    pinMode(smDirectionPin, OUTPUT);
    pinMode(smStepPin, OUTPUT);
    pinMode(smEnablePin, OUTPUT);
    digitalWrite(smEnablePin, HIGH);
    

    pinMode(PIN_ZAPPER,OUTPUT); // Zapper Relay

    pinMode(PIN_BUZZSAW, OUTPUT); // BUZZ SAW PIN
    digitalWrite(PIN_BUZZSAW, LOW);
    pinMode(PIN_BUZZSLIDELIMIT, INPUT_PULLUP); //BuzzSaw Inner Limit switch

    pinMode(PIN_BADMOTIVATOR, OUTPUT); // MOTIVATOR PIN
    digitalWrite(PIN_BADMOTIVATOR, LOW);

    // DOME CENTER SWITCH
    pinMode(PIN_DOMECENTER, INPUT_PULLUP); // DOME CENTER PIN

    centerDomeLoop();
    closeAll();

    #ifdef SERVO_EASING
      //I don't have the card dispensing drawer, yet
    #else
    pwm1.setPWM(12, 0, pwm1_12_max); // Panel Drawer
      delay(150);
      pwm1.setPWM(12, 0, 0); // Panel Drawer
    #endif

    #ifdef SERVO_EASING

    #else
      //I use a NeoPixel strip  No neeed for this
      //    // TURN ON LEDS
      //    pwm3.setPWM(10,0,pwm3_10_max);
      //    pwm3.setPWM(11,0,pwm3_11_max);
      //    pwm3.setPWM(12,0,pwm3_12_max);
      //    pwm3.setPWM(13,0,pwm3_13_max);
      //    pwm3.setPWM(14,0,pwm3_14_max);
      //    pwm3.setPWM(15,0,0);
      #endif


    //PLay MP3 Trigger sound
    //MP3Trigger trigger;//make trigger object
    // Start serial communication with the trigger (over Serial)
    trigger.setup(&Serial3);
    trigger.setVolume(droidSoundLevel);//Amount of attenuation  higher=LOWER volume..ten is pretty loud 0-127
    trigger.trigger(255);
    //delay(50);
    //trigger.setVolume(10);//Amount of attenuation  higher=LOWER volume..ten is pretty loud
    //sfx.playTrack("T06     OGG");

    //------------NeoPixel Setup--------------------
    bar16.begin(); // This initializes the NeoPixel library.
    bar16.ColorWipe(COLOR32(0, 0, 02), 50, REVERSE); // light Blue
    Serial.print(F("\r\nEnd NeoPixel SetUp"));
}




// ==============================================================
//                          MAIN PROGRAM
// ==============================================================
void loop(){


    //initAndroidTerminal();

    if (!droidFreakOut){
      if (!readUSB()) return; //Check if we have a fault condition. If we do, we want to ensure that we do NOT process any controller data!!!
      footMotorDrive();
      if (!readUSB()) return; // Not sure why he has this twice, and in this order, but better safe then sorry?
      domeDrive();
      //Serial.println(domeSpinning);
      processCommands();
    }

    badMotivatorLoop();
    //flushAndroidTerminal();
    freakOut();
    soundControl();
    //printOutput();

//    if ((centerDomeSpeed > 0 || centerDomeSpeed < 0) && !domeSpinning){
//      centerDomeLoop();
//    }
   
#pragma mark -
#pragma mark BuzzSawMainLoop
   
#ifdef SERVO_EASING
   //Taken Care of by servo Easing.  No need for this?
#else
    if (buzzSawMillis < millis() && buzzSpinning){
        buzzSpinning = false;
        buzzActivated = false;
        digitalWrite(PIN_BUZZSAW, LOW);
        delay(100);
        buzzSawSlideInMillis = (millis()+1500);
        buzzSawSliding = true;
    }
    if (buzzSawSlideInMillis > millis() && buzzSawSliding){
        pwm2.setPWM(1, 0, pwm2_1_min); // Buzz Slide
    }else if (buzzSawSlideInMillis < millis() && buzzSawSliding){
      buzzSawSliding = false;
        pwm2.setPWM(1, 0, 0); // Buzz Slide
        pwm2.setPWM(11, 0, pwm2_11_min); // Buzz Door
       delay(200);
       silenceServos();
    }
#endif
   
    #ifdef SERVO_EASING
      //using NEOPatterns...no blinking.  But maybe set custom blink here
    #else
      if (millis() > blinkMillis && millis() < blinkMillis+1000 && drawerActivated){
        pwm1.setPWM(11, 0, pwm1_11_max);
      }else if (millis() > blinkMillis+1000 && millis() < blinkMillis+2000 && drawerActivated){
        pwm1.setPWM(11, 0, 0);
      }else if (millis() > blinkMillis+2000){
        pwm1.setPWM(11, 0, 0);
        blinkMillis = millis();
      }
    #endif

    #ifdef SERVO_EASING
      //TODO: Convert this code when you get a lifeform scanner !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      if (lifeformActivated && (millis()-lifeformMillis) > 5){
      if ((millis()-lifeformLEDMillis) > 800){
         if (lifeformLED == 0){
            //pwm3.setPWM(15,0,pwm3_15_max); LED???
            //Needs to be a NEOPIXEL?
            lifeformLED = 1;
         }else{
            //pwm3.setPWM(15,0,0);
            //NEOPIXEL CODE HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!
            lifeformLED = 0;
         }
         lifeformLEDMillis = millis();
      }

//      if (lifeformPosition == 0){
//        lifeformPosition = pwm3_9_min;
//      }
//      if (lifeformPosition <= pwm3_9_min){
//        lifeformDir = 0;
//      }else if (lifeformPosition >= pwm3_9_max){
//        lifeformDir = 1;
//      }
//      if (lifeformDir == 0){
//        lifeformPosition++;
//      }else if (lifeformDir == 1){
//        lifeformPosition--;
//      }
//      //pwm3.setPWM(9,0,lifeformPosition);
//      //TODO: MOVELIFESCANNER
      lifeformMillis = millis();
    }

    #else
      if (lifeformActivated && (millis()-lifeformMillis) > 5){
        if ((millis()-lifeformLEDMillis) > 800){
           if (lifeformLED == 0){
              pwm3.setPWM(15,0,pwm3_15_max);
              lifeformLED = 1;
           }else{
              pwm3.setPWM(15,0,0);
              lifeformLED = 0;
           }
           lifeformLEDMillis = millis();
        }

        if (lifeformPosition == 0){
          lifeformPosition = pwm3_9_min;
        }
        if (lifeformPosition <= pwm3_9_min){
          lifeformDir = 0;
        }else if (lifeformPosition >= pwm3_9_max){
          lifeformDir = 1;
        }
        if (lifeformDir == 0){
          lifeformPosition++;
        }else if (lifeformDir == 1){
          lifeformPosition--;
        }
        pwm3.setPWM(9,0,lifeformPosition);
        lifeformMillis = millis();
      }
      #endif

    #ifdef SERVO_EASING
        if (droidFreakOutComplete){
        droidFreakOutComplete = false;
        pwm3_Servo8.easeTo(pwm3_8Info.maxDeg); // Lifeform Slide
        pwm2_Servo14.easeTo(pwm2_14Info.maxDeg); //Interface
        pwm2_Servo12.easeTo(pwm2_12Info.maxDeg); // Gripper
        pwm2_Servo1.easeTo(pwm2_1Info.maxDeg); // Buzz Slide
        //pwm2_Servo0.startEaseTo(pwm2_0_max, 30); // Zapper Slide
        delay(1400);
        pwm2_Servo1.detach(); // Buzz Slide
        //pwm2_Servo0.detach(); // Zapper Slide
        blinkEyes();
        delay(500);
        pwm3_Servo8.detach(); // Lifeform Slide
        //silenceServos();
        //pwm2.setPWM(2, 0, 250); // MOTIVATOR RESET
        //delay(200);
        lifeformActivated = true;
        zapperActivated = true;
        buzzActivated = true;
        gripperActivated = true;
        interfaceActivated = true;
        silenceServos();
      }
    #else
      if (droidFreakOutComplete){
        droidFreakOutComplete = false;
        pwm3.setPWM(8, 0, pwm3_8_max); // Lifeform Slide
        pwm2.setPWM(14,0,pwm2_14_max); //Interface
        pwm2.setPWM(12,0,pwm2_12_max); // Gripper
        pwm2.setPWM(1, 0, pwm2_1_max); // Buzz Slide
        pwm2.setPWM(0, 0, pwm2_0_max); // Zapper Slide
        delay(1400);
        pwm2.setPWM(1, 0, 0); // Buzz Slide
        pwm2.setPWM(0, 0, 0); // Zapper Slide
        blinkEyes();
        delay(500);
        pwm3.setPWM(8, 0, 0); // Lifeform Slide
        silenceServos();
        pwm2.setPWM(2, 0, 250); // MOTIVATOR RESET
        delay(200);
        lifeformActivated = true;
        zapperActivated = true;
        buzzActivated = true;
        gripperActivated = true;
        interfaceActivated = true;
        silenceServos();
      }
    #endif
    //------------NeoPixelUpdate
    bar16.update();
   
    
#pragma mark -
#pragma mark Update servoEasing
#ifdef SERVO_EASING
    if (digitalRead(PIN_BUZZSLIDELIMIT) == HIGH){
//        pwm2_Servo1.update(); //move slide
      }
      else if(buzzSpinning){
         //TODO:  This whole thing is wonky and probably doesn't work
          pwm2_Servo1.detach(); //stop slide movement
//          if (midPanelsOpen){//only close the door if the buzz saw opened it
             Serial.println("StopBuzzSaw Slide with limit switch");
//              pwm2_Servo11.easeTo(pwm2_11Info.minDeg); //close the door
//              pwm2_Servo11.detach();
//          }

      }
#endif

//        Serial.println(F("Move to 135 degree and back nonlinear in one second each using interrupts"));
//    pwm1_Servo1.setEasingType(EASE_CUBIC_IN_OUT);
//
//    for (int i = 0; i < 2; ++i) {
//        pwm1_Servo1.startEaseToD(104, 1000);
//        while (pwm1_Servo1.isMovingAndCallYield()) {
//            ; // no delays here to avoid break between forth and back movement
//        }
//        pwm1_Servo1.startEaseToD(75, 1000);
//        while (pwm1_Servo1.isMovingAndCallYield()) {
//            ; // no delays here to avoid break between forth and back movement
//        }
//    }

}
