#include "EasyNextionLibrary.h"
#include "Arduino.h"
#include "control.h"
#include "PCF8575.h"
#include <Preferences.h>
#include <ezButton.h>
#include "AiEsp32RotaryEncoder.h"

Preferences Settings;

char *secondsToHHMMSS(int total_seconds)
{
    int hours, minutes, seconds;

    hours = total_seconds / 3600;         // Divide by number of seconds in an hour
    total_seconds = total_seconds % 3600; // Get the remaining seconds
    minutes = total_seconds / 60;         // Divide by number of seconds in a minute
    seconds = total_seconds % 60;         // Get the remaining seconds

    // Format the output string
    static char hhmmss_str[7]; // 6 characters for HHMMSS + 1 for null terminator
    sprintf(hhmmss_str, "%02d%02d%02d", hours, minutes, seconds);
    return hhmmss_str;
}

EasyNex myNex(Serial2);
PCF8575 pcf8575(0x25);

// Declaration of LCD Variables
const int NUM_SETTING_ITEMS = 8;
const int NUM_TEST_SCREEN = 4;

int currentSettingScreen = 0;
int currentTestMenuScreen = 0;

bool settingFlag = false;
bool refreshScreenFlag = false;
bool RunAutoFlag, RunTestFlag = false;

String setting_items[NUM_SETTING_ITEMS][2] = { // array with item names
    {"Starch", "SEC"},
    {"Bleach", "SEC"},
    {"Water", "MIN"},
    {"Mixer", "MIN"},
    {"Discharge", "MIN"},
    {"Length", "CM"},
    {"Drying", "MIN"},
    {"MoveToEnd", "SEC"}};

double parametersTimer[NUM_SETTING_ITEMS] = {20, 20, 20, 20, 20, 30, 30, 30};
double parametersTimerMaxValue[NUM_SETTING_ITEMS] = {1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200};

Control Water(secondsToHHMMSS(4));
Control Starch(secondsToHHMMSS(3));
Control Bleach(secondsToHHMMSS(6));
Control Mixer(secondsToHHMMSS(4));
Control DischargeValve(secondsToHHMMSS(4));
Control Drying(secondsToHHMMSS(4));
Control RollToEndDryer(secondsToHHMMSS(4));

const int controlPin[16] = {P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15};

int rStarch = P8;
int rBleach = P9;
int rWater = P10;
int rMixer = P14;
int rValve = P11;
int rAgitator = P6;
int rConveyor = P15;
int rSubPump = P13;
int rHeater = P0;
int rFan = P1;
int rCutter = P12;

int PaperSensorPin = 32;
bool PaperFeedbackStatus = false;
ezButton FeedBackPaper(PaperSensorPin);

double lengthCalibration = 115;
double lengthDisplayCalibration = 70;
int RotaryValue = 0;

unsigned long currentMillisCutter;
unsigned long previousMillisCutter = 0;
const long intervalCutter = 300;

void setFeedbackPins()
{
    FeedBackPaper.setDebounceTime(50);
}

void readFeedbackStatus()
{
    FeedBackPaper.loop();
    // Serial.println(FeedBackPaper.getState());
    if (FeedBackPaper.getState() == 1)
    {
        PaperFeedbackStatus = true;
    }
    else
    {
        PaperFeedbackStatus = false;
    }

    // if (PaperFeedbackStatus == true)
    // {
    //     Serial.println("Detected Paper");
    // }
}

bool triggerType = LOW;

#define ROTARY_ENCODER_A_PIN 13
#define ROTARY_ENCODER_B_PIN 14
#define ROTARY_ENCODER_BUTTON_PIN 25
#define ROTARY_ENCODER_STEPS 1

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);

void IRAM_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}

void setRotaryEncoder()
{
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    // rotaryEncoder.setBoundaries(0, 1000, false);
    rotaryEncoder.setAcceleration(0);
}

void saveSettings()
{
    Settings.putDouble("waterpump", parametersTimer[0]);
    Settings.putDouble("bleachpump", parametersTimer[1]);
    Settings.putDouble("starchpump", parametersTimer[2]);
    Settings.putDouble("mixertimer", parametersTimer[3]);
    Settings.putDouble("discharge", parametersTimer[4]);
    Settings.putDouble("length", parametersTimer[5]);
    Settings.putDouble("dryingTime", parametersTimer[6]);
    Settings.putDouble("movingTime", parametersTimer[7]);
}
void loadSettings()
{
    Serial.println("---- Start Reading Settings ----");
    parametersTimer[2] = Settings.getDouble("waterpump", 0);
    parametersTimer[1] = Settings.getDouble("bleachpump", 0);
    parametersTimer[0] = Settings.getDouble("starchpump", 0);
    parametersTimer[3] = Settings.getDouble("mixertimer", 0);
    parametersTimer[4] = Settings.getDouble("discharge", 0);
    parametersTimer[5] = Settings.getDouble("length", 0);
    parametersTimer[6] = Settings.getDouble("dryingTime", 0);
    parametersTimer[7] = Settings.getDouble("movingTime", 0);

    Serial.println("Pump Timer : " + String(parametersTimer[2]));
    Serial.println("Pump Bleach Timer : " + String(parametersTimer[1]));
    Serial.println("Pump Starch Timer : " + String(parametersTimer[0]));
    Serial.println("Mixer Timer : " + String(parametersTimer[3]));
    Serial.println("Discharge Timer : " + String(parametersTimer[4]));
    Serial.println("Length : " + String(parametersTimer[5]));
    Serial.println("Drying Time : " + String(parametersTimer[6]));
    Serial.println("To End : " + String(parametersTimer[7]));
    Serial.println("---- End Reading Settings ----");

    Starch.setTimer(secondsToHHMMSS(parametersTimer[0]));
    Bleach.setTimer(secondsToHHMMSS(parametersTimer[1]));
    Water.setTimer(secondsToHHMMSS(parametersTimer[2] * 60));
    Mixer.setTimer(secondsToHHMMSS(parametersTimer[3] * 60));
    DischargeValve.setTimer(secondsToHHMMSS(parametersTimer[4] * 60));
    Drying.setTimer(secondsToHHMMSS(parametersTimer[6] * 60));
    RollToEndDryer.setTimer(secondsToHHMMSS(parametersTimer[7]));
}

void setPinsForRelay()
{
    for (int i = 0; i < 16; i++)
    {
        pcf8575.pinMode(controlPin[i], OUTPUT); // set pin as output
    }

    Serial.begin(115200);

    pcf8575.begin();

    for (int i = 0; i < 16; i++)
    {
        if (triggerType == LOW)
        {
            pcf8575.digitalWrite(controlPin[i], HIGH);
        }
        else
        {
            pcf8575.digitalWrite(controlPin[i], LOW);
        }
    }
}

void StopAllTimer()
{
    Water.stop();
    Bleach.stop();
    Water.stop();
    Mixer.stop();
    DischargeValve.stop();
    RollToEndDryer.stop();
    Drying.stop();
}

void StopAllMotor()
{

    pcf8575.digitalWrite(rStarch, HIGH);
    pcf8575.digitalWrite(rBleach, HIGH);
    pcf8575.digitalWrite(rWater, HIGH);
    pcf8575.digitalWrite(rMixer, HIGH);
    pcf8575.digitalWrite(rValve, HIGH);
    pcf8575.digitalWrite(rAgitator, HIGH);
    pcf8575.digitalWrite(rConveyor, HIGH);
    pcf8575.digitalWrite(rSubPump, HIGH);
    pcf8575.digitalWrite(rHeater, HIGH);
    pcf8575.digitalWrite(rFan, HIGH);
    pcf8575.digitalWrite(rCutter, HIGH);
}

unsigned long currentMillis;
unsigned long previousMillis = 0;
const long interval = 1000;

void runTest()
{
    // Serial.println("Test Function - Top");
}
int RunAutoStatus = 0;
int RunPaperMakingStatus = 0;
int RunDryingStatus = 0;

void runPaperMaking()
{

    switch (RunDryingStatus)
    {
    case 1:
        if (RollToEndDryer.isStopped() == false)
        {
            RollToEndDryer.run();
            if (RollToEndDryer.isTimerCompleted() == true)
            {
                pcf8575.digitalWrite(rConveyor, HIGH);
                pcf8575.digitalWrite(rSubPump, HIGH);
                Drying.start();
                RunDryingStatus = 2;
            }
            else
            {
                pcf8575.digitalWrite(rConveyor, LOW);
                pcf8575.digitalWrite(rSubPump, LOW);
            }
        }
        break;
    case 2:
        if (Drying.isStopped() == false)
        {
            Drying.run();
            if (Drying.isTimerCompleted() == true)
            {
                RollToEndDryer.start();
                RunDryingStatus = 1;
            }
            else
            {
                pcf8575.digitalWrite(rConveyor, HIGH);
            }
        }
        break;
    default:
        break;
    }
    /*
    1 - Count Step
    2 - Cut and Reset
    */

    switch (RunPaperMakingStatus)
    {
    case 1:
        if (PaperFeedbackStatus == true)
        {
            rotaryEncoder.reset();
            RunPaperMakingStatus = 2;
        }
        break;
    case 2:
        if (rotaryEncoder.encoderChanged())
        {
            if ((double)rotaryEncoder.readEncoder() >= parametersTimer[5] * lengthCalibration)
            {
                RunPaperMakingStatus = 3;
                previousMillisCutter = millis();
                rotaryEncoder.reset();
            }
        }
        break;
    case 3:
        currentMillisCutter = millis();
        if (currentMillisCutter - previousMillisCutter >= intervalCutter)
        {
            previousMillisCutter = currentMillisCutter;
            RunPaperMakingStatus = 2;
            pcf8575.digitalWrite(rCutter, HIGH);
        }
        else
        {
            pcf8575.digitalWrite(rCutter, LOW);
        }
        break;
    default:
        break;
    }
}

void runAuto()
{
    /*
    PRE HEAT (On HEATER On FAN)
    1 - Fill Mixer (MIXER AGITATOR On)
    2 - Pump Starch ()
    3 - Pump Bleach ()
    4 - Timer Mixer
    5 - Timer Open Valve (AGITATOR ON CONVEYOR ON)
    6 - Sense Paper
    7 - Count and Cut
    */

    switch (RunAutoStatus)
    {
    case 1:
        if (Water.isStopped() == false)
        {
            Water.run();
            if (Water.isTimerCompleted() == true)
            {
                pcf8575.digitalWrite(rWater, HIGH);
                RunAutoStatus = 2;
                Starch.start();
            }
            else
            {
                pcf8575.digitalWrite(rMixer, LOW);
                pcf8575.digitalWrite(rWater, LOW);
            }
        }
        break;
    case 2:
        if (Starch.isStopped() == false)
        {
            Starch.run();
            if (Starch.isTimerCompleted() == true)
            {
                pcf8575.digitalWrite(rStarch, HIGH);
                RunAutoStatus = 3;
                Bleach.start();
            }
            else
            {
                pcf8575.digitalWrite(rStarch, LOW);
            }
        }
        break;
    case 3:
        if (Bleach.isStopped() == false)
        {
            Bleach.run();
            if (Bleach.isTimerCompleted() == true)
            {
                pcf8575.digitalWrite(rBleach, HIGH);
                RunAutoStatus = 4;
                Mixer.start();
            }
            else
            {
                pcf8575.digitalWrite(rBleach, LOW);
            }
        }
        break;
    case 4:
        if (Mixer.isStopped() == false)
        {
            Mixer.run();
            if (Mixer.isTimerCompleted() == true)
            {
                RunAutoStatus = 5;
                DischargeValve.start();
            }
        }
        break;
    case 5:
        if (DischargeValve.isStopped() == false)
        {
            DischargeValve.run();
            if (DischargeValve.isTimerCompleted() == true)
            {
                pcf8575.digitalWrite(rValve, HIGH);
                pcf8575.digitalWrite(rMixer, HIGH);
                RunAutoStatus = 6;
            }
            else
            {
                pcf8575.digitalWrite(rValve, LOW);
            }
        }
        break;
    case 6:
        pcf8575.digitalWrite(rConveyor, LOW);
        pcf8575.digitalWrite(rAgitator, LOW);
        RollToEndDryer.start();
        RunDryingStatus = 1;
        RunPaperMakingStatus = 1;
        RunAutoStatus = 7;
        break;
    case 7:
        runPaperMaking();
        break;
    default:
        break;
    }
}

void refreshScreen()
{
    if (refreshScreenFlag == true)
    {
        refreshScreenFlag = false;
        if (settingFlag == true)
        {
            myNex.writeStr("item.txt", setting_items[currentSettingScreen][0]);
            String valueToPrint = String(parametersTimer[currentSettingScreen]) + " " + setting_items[currentSettingScreen][1];
            myNex.writeStr("value.txt", valueToPrint);
        }
        else if (RunAutoFlag == true)
        {
            /*
            PRE HEAT (On HEATER On FAN)
            1 - Fill Mixer (MIXER AGITATOR On)
            2 - Pump Starch ()
            3 - Pump Bleach ()
            4 - Timer Mixer
            5 - Timer Open Valve (AGITATOR ON CONVEYOR ON)
            6 - Sense Paper
            7 - Count and Cut
            */
            switch (RunAutoStatus)
            {
            case 1:
                myNex.writeStr("txtbCurrentL.txt", "  " + String((double)rotaryEncoder.readEncoder() / lengthDisplayCalibration) + "  CM");
                myNex.writeStr("txtbProcess.txt", "   Filling Mixer");
                myNex.writeStr("txtbRemainTime.txt", "   " + String(Water.getTimeRemaining()));
                break;
            case 2:
                myNex.writeStr("txtbProcess.txt", "   Pumping Starch");
                myNex.writeStr("txtbRemainTime.txt", "   " + String(Starch.getTimeRemaining()));
                break;
            case 3:
                myNex.writeStr("txtbProcess.txt", "   Pumping Bleach");
                myNex.writeStr("txtbRemainTime.txt", "   " + String(Bleach.getTimeRemaining()));
                break;
            case 4:
                myNex.writeStr("txtbProcess.txt", "   Mixing");
                myNex.writeStr("txtbRemainTime.txt", "   " + String(Mixer.getTimeRemaining()));
                break;
            case 5:
                myNex.writeStr("txtbProcess.txt", "   Discharging");
                myNex.writeStr("txtbRemainTime.txt", "   " + String(DischargeValve.getTimeRemaining()));
                break;
            case 6:
                myNex.writeStr("txtbProcess.txt", "   Waiting for Paper");
                myNex.writeStr("txtbRemainTime.txt", "  N/A");
                break;
            case 7:
                switch (RunDryingStatus)
                {
                case 1:
                    myNex.writeStr("txtbProcess.txt", "   Moving To Dryer");
                    myNex.writeStr("txtbRemainTime.txt", "  " + String(RollToEndDryer.getTimeRemaining()));
                    break;
                case 2:
                    myNex.writeStr("txtbProcess.txt", "   Drying");
                    myNex.writeStr("txtbRemainTime.txt", "  " + String(Drying.getTimeRemaining()));
                    break;
                default:
                    break;
                }
                myNex.writeStr("txtbSecondary.txt", "  " + String((double)rotaryEncoder.readEncoder() / lengthDisplayCalibration) + "  CM");
                break;
            default:
                break;
            }
        }
        else if (RunTestFlag == true)
        {
        }
    }
}

void setup()
{
    myNex.begin(9600);
    setPinsForRelay();
    StopAllTimer();
    Settings.begin("timerSetting", false);
    setRotaryEncoder();
    // saveSettings();
    loadSettings();
    myNex.writeStr("page home");
    setFeedbackPins();
}

void loop()
{
    if (rotaryEncoder.encoderChanged())
    {
        Serial.println(rotaryEncoder.readEncoder());
    }

    myNex.NextionListen();
    readFeedbackStatus();
    if (refreshScreenFlag == true)
    {
        refreshScreen();
    }

    if (RunAutoFlag == true)
    {
        runAuto();
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            previousMillis = currentMillis;
            refreshScreenFlag = true;
        }
    }

    if (RunTestFlag == true)
    {
        // Serial.println("Main Loop - Test");
        runTest();

        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            previousMillis = currentMillis;
            refreshScreenFlag = true;
        }
    }
}

void selectTestScreen()
{
    if (currentTestMenuScreen == 0)
    {
        myNex.writeStr("page pTest1");
    }
    else if (currentTestMenuScreen == 1)
    {
        myNex.writeStr("page pTest2");
    }
    else if (currentTestMenuScreen == 2)
    {
        myNex.writeStr("page pTest3");
    }
}

// Select Settings - 00
void trigger0()
{
    myNex.writeStr("page pSettings");
    settingFlag = true;
    refreshScreenFlag = true;
}

// Save Settings - 01
void trigger1()
{
    myNex.writeStr("page home");
    settingFlag = false;
    refreshScreenFlag = true;
    saveSettings();
    loadSettings();
}

// Prev Setting - 02
void trigger2()
{
    if (currentSettingScreen == 0)
    {
        currentSettingScreen = NUM_SETTING_ITEMS - 1;
    }
    else
    {
        currentSettingScreen--;
    }
    refreshScreenFlag = true;
}

// Next Settings - 03
void trigger3()
{
    if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
    {
        currentSettingScreen = 0;
    }
    else
    {
        currentSettingScreen++;
    }
    refreshScreenFlag = true;
}

// + Small Settings - 04
void trigger4()
{
    if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
    {
        parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
    }
    else
    {
        parametersTimer[currentSettingScreen] += 1;
        if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
        {
            parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
        }
    }
    refreshScreenFlag = true;
}

// - Small Settings - 05
void trigger5()
{
    if (parametersTimer[currentSettingScreen] <= 0)
    {
        parametersTimer[currentSettingScreen] = 0;
    }
    else
    {
        parametersTimer[currentSettingScreen] -= 1;

        if (parametersTimer[currentSettingScreen] <= 0)
        {
            parametersTimer[currentSettingScreen] = 0;
        }
    }
    refreshScreenFlag = true;
}

// + 10 Settings - 06
void trigger6()
{
    if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
    {
        parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
    }
    else
    {
        parametersTimer[currentSettingScreen] += 10;

        if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
        {
            parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
        }
    }
    refreshScreenFlag = true;
}

// -10 Settings - 07
void trigger7()
{
    if (parametersTimer[currentSettingScreen] <= 0)
    {
        parametersTimer[currentSettingScreen] = 0;
    }
    else
    {
        parametersTimer[currentSettingScreen] -= 10;

        if (parametersTimer[currentSettingScreen] <= 0)
        {
            parametersTimer[currentSettingScreen] = 0;
        }
    }
    refreshScreenFlag = true;
}

// Test Machine Item 1 - 08
void trigger8()
{
    if (currentTestMenuScreen == 0)
    {
        if (pcf8575.digitalRead(rStarch) == HIGH)
        {
            pcf8575.digitalWrite(rStarch, LOW);
            myNex.writeNum("btnItem1.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rStarch, HIGH);
            myNex.writeNum("btnItem1.val", 0);
        }
    }
    else if (currentTestMenuScreen == 1)
    {
        if (pcf8575.digitalRead(rAgitator) == HIGH)
        {
            pcf8575.digitalWrite(rAgitator, LOW);
            myNex.writeNum("btnItem1.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rAgitator, HIGH);
            myNex.writeNum("btnItem1.val", 0);
        }
    }
    else if (currentTestMenuScreen == 2)
    {
        if (pcf8575.digitalRead(rFan) == HIGH)
        {
            pcf8575.digitalWrite(rFan, LOW);
            myNex.writeNum("btnItem1.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rFan, HIGH);
            myNex.writeNum("btnItem1.val", 0);
        }
    }
}

// Test Machine Item 2 - 09
void trigger9()
{
    if (currentTestMenuScreen == 0)
    {
        if (pcf8575.digitalRead(rBleach) == HIGH)
        {
            pcf8575.digitalWrite(rBleach, LOW);
            myNex.writeNum("btnItem2.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rBleach, HIGH);
            myNex.writeNum("btnItem2.val", 0);
        }
    }
    else if (currentTestMenuScreen == 1)
    {
        if (pcf8575.digitalRead(rConveyor) == HIGH)
        {
            pcf8575.digitalWrite(rConveyor, LOW);
            myNex.writeNum("btnItem2.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rConveyor, HIGH);
            myNex.writeNum("btnItem2.val", 0);
        }
    }
    else if (currentTestMenuScreen == 2)
    {
        if (pcf8575.digitalRead(rCutter) == HIGH)
        {
            pcf8575.digitalWrite(rCutter, LOW);
            myNex.writeNum("btnItem2.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rCutter, HIGH);
            myNex.writeNum("btnItem2.val", 0);
        }
    }
}

// Test Machine Item 3 - 0A
void trigger10()
{
    if (currentTestMenuScreen == 0)
    {
        if (pcf8575.digitalRead(rWater) == HIGH)
        {
            pcf8575.digitalWrite(rWater, LOW);
            myNex.writeNum("btnItem3.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rWater, HIGH);
            myNex.writeNum("btnItem3.val", 0);
        }
    }
    else if (currentTestMenuScreen == 1)
    {
        if (pcf8575.digitalRead(rSubPump) == HIGH)
        {
            pcf8575.digitalWrite(rSubPump, LOW);
            myNex.writeNum("btnItem3.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rSubPump, HIGH);
            myNex.writeNum("btnItem3.val", 0);
        }
    }
    else if (currentTestMenuScreen == 2)
    {
        if (pcf8575.digitalRead(rValve) == HIGH)
        {
            pcf8575.digitalWrite(rValve, LOW);
            myNex.writeNum("btnItem3.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rValve, HIGH);
            myNex.writeNum("btnItem3.val", 0);
        }
    }
    else
    {
    }
}

// Test Machine Item 4 - 0B
void trigger11()
{
    if (currentTestMenuScreen == 0)
    {
        if (pcf8575.digitalRead(rMixer) == HIGH)
        {
            pcf8575.digitalWrite(rMixer, LOW);
            myNex.writeNum("btnItem4.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rMixer, HIGH);
            myNex.writeNum("btnItem4.val", 0);
        }
    }
    else if (currentTestMenuScreen == 1)
    {
        if (pcf8575.digitalRead(rHeater) == HIGH)
        {
            pcf8575.digitalWrite(rHeater, LOW);
            myNex.writeNum("btnItem4.val", 1);
        }
        else
        {
            pcf8575.digitalWrite(rHeater, HIGH);
            myNex.writeNum("btnItem4.val", 0);
        }
    }
    else if (currentTestMenuScreen == 2)
    {
        // if (pcf8575.digitalRead(rFinishLinear) == HIGH)
        // {
        //     pcf8575.digitalWrite(rFinishLinear, LOW);
        //     myNex.writeNum("btnItem4.val", 1);
        // }
        // else
        // {
        //     pcf8575.digitalWrite(rFinishLinear, HIGH);
        //     myNex.writeNum("btnItem4.val", 0);
        // }
    }
}

// Test Machine Prev - 0C
void trigger12()
{
    if (currentTestMenuScreen == 0)
    {
        currentTestMenuScreen = NUM_TEST_SCREEN - 1;
    }
    else
    {
        currentTestMenuScreen--;
    }
    selectTestScreen();
}

// Test Machine Next - 0D
void trigger13()
{
    if (currentTestMenuScreen == NUM_TEST_SCREEN - 1)
    {
        currentTestMenuScreen = 0;
    }
    else
    {
        currentTestMenuScreen++;
    }
    selectTestScreen();
}

// Test Machine Exit - 0E
void trigger14()
{
    currentTestMenuScreen = 0;
    myNex.writeStr("page home");
    RunTestFlag = false;
}

// RunAuto - 0F
void trigger15()
{
    StopAllTimer();
    StopAllMotor();
    myNex.writeStr("page pRun");
    RunAutoFlag = true;
    RunAutoStatus = 1;
    RunPaperMakingStatus = 0;
    pcf8575.digitalWrite(rHeater, LOW);
    pcf8575.digitalWrite(rFan, LOW);
    Water.start();
}

// RunAuto Stop - 10
void trigger16()
{
    myNex.writeStr("page home");
    RunAutoFlag = false;
    StopAllMotor();
    StopAllTimer();
    RunAutoStatus = 0;
    RunPaperMakingStatus = 0;
    RunDryingStatus = 0;
}

// Test Machine Open - 11
void trigger17()
{
    currentTestMenuScreen = 0;
    selectTestScreen();
    RunTestFlag = true;
    Serial.println("Open Test Menu");
}



