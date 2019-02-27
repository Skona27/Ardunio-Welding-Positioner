#include <LiquidCrystal.h>

// LCD pins
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pins to motor controller (Pulse, direction, on/off)
#define PUL_OUT     11
#define DIR_OUT     10
#define EN_OUT      9

// Pause pin
#define PAUSE_IN    8

// Keyboard keys
#define KEY_UP      14
#define KEY_DOWN    15
#define KEY_LEFT    16
#define KEY_RIGHT   17
#define KEY_SELECT  18

// 1.8 degree step increments
const int stepsPerRevolution = 200; 

enum {BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_SELECT, BTN_NONE};
enum {DIS_NONE, DIS_VALUE, DIS_YESNO, DIS_T1F0, DIS_DIR, DIS_POW};
enum {READY, PAUSED, RUN};
enum {SET_RADIUS, SET_RPM,
      SET_PAUSE, SET_TURN, SET_DIR,
      SET_RATIO, SET_MICROSTEP, SET_COUNT };

static int lcd_key  = BTN_NONE;
static int lcd_key_last  = BTN_NONE;
static int run_state = READY;
static int last_run_state = RUN;

int button_timer = 0;
int system_timer = 0;
int start_paused_time = 0;
int paused_time = 0;
int togglePulse = LOW;
int startStatePause = LOW;
bool home_display = true;
bool quick_adjust_rpm = true;
int settings_sub_menu = 0;
int keyboardTemp;
int keyboardCurrent;

typedef struct
{
  int currentValue;
  int previousValue;
  int minValue;
  int maxValue;
  int divider;
  int stepValue;
  int displayType;
  String topLine;
  String bottomLine;
} settings_s;

settings_s settings[SET_COUNT];

int readKeyboard()
{
  keyboardTemp = digitalRead(KEY_UP);
  if (keyboardTemp == HIGH) {
    return BTN_UP;
  }

  keyboardTemp = digitalRead(KEY_DOWN);
  if (keyboardTemp == HIGH) {
    return BTN_DOWN;
  }

  keyboardTemp = digitalRead(KEY_LEFT);
  if (keyboardTemp == HIGH) {
    return BTN_LEFT;
  }

  keyboardTemp = digitalRead(KEY_RIGHT);
  if (keyboardTemp == HIGH) {
    return BTN_RIGHT;
  }

  keyboardTemp = digitalRead(KEY_SELECT);
  if (keyboardTemp == HIGH) {
    return BTN_SELECT;
  }

  return BTN_NONE;
}

void readPauseButton() {
  keyboardTemp = digitalRead(PAUSE_IN);

  switch (keyboardTemp)
  {
    case HIGH:
      run_state = RUN;
      break;

    case LOW:
      run_state = PAUSED;
      break;

    default:
      run_state = PAUSED;
      break;
  }
}

void resetSettings()
{ 
                                     // CRNT, PREV, MIN,   MAX, DIV, STP,     Type, "             TOP", " BTM"
  settings[SET_RADIUS]     = (settings_s){ 100,   0,   10,   250,  1,   1, DIS_VALUE, "Promien:    ", "mm"};
  settings[SET_RATIO]     = (settings_s){  123,   0,   10,   150,  10,   1, DIS_VALUE, "Przekladnia:    ", ":1"};
  settings[SET_MICROSTEP] = (settings_s){   4,   0,   1,    32,   1,   2,   DIS_POW, "Mikro kroki:    ", ""};
  settings[SET_PAUSE]     = (settings_s){   0,   0,   0,  5000,   1, 250, DIS_VALUE, "Pauza:         ", "ms"};
  settings[SET_TURN]      = (settings_s){   1,   0,   1,    25,   1,   1, DIS_VALUE, "Kroki:        ", ""};
  settings[SET_RPM]       = (settings_s){   10,   0,  1,  30, 10,  1, DIS_VALUE, "Predkosc spawu:         ", "mm/s"};
  settings[SET_DIR]       = (settings_s){   1,   0,   0,     1,   1,   1,   DIS_DIR, "Kierunek:     ", ""};
}

void Increase(int item)
{
  if (item >= 0 && item < SET_COUNT)
  {
    settings[item].previousValue = settings[item].currentValue;
    if (settings[settings_sub_menu].displayType == DIS_POW)
    {
      settings[item].currentValue *= settings[item].stepValue;
    }
    else
    {
      settings[item].currentValue += settings[item].stepValue;
    }
    settings[item].currentValue = min(settings[item].maxValue, settings[item].currentValue);
  }
}

void Decrease(int item)
{
  if (item >= 0 && item < SET_COUNT)
  {
    settings[item].previousValue = settings[item].currentValue;
    if (settings[settings_sub_menu].displayType == DIS_POW)
    {
      settings[item].currentValue /= settings[item].stepValue;
    }
    else
    {
      settings[item].currentValue -= settings[item].stepValue;
    }
    settings[item].currentValue = max(settings[item].minValue, settings[item].currentValue);
  }
}

void UpdateDisplay()
{
  String bottomLine;
  lcd.clear();

  if (home_display)
  {
    lcd.setCursor(0, 0);
    switch (run_state)
    {
      case READY:
        lcd.print("Gotowy...");
        break;

      case PAUSED:
        lcd.print("Zatrzymany");
        break;

      case RUN:
        lcd.print("Uruchomiony");
        break;
    }

    lcd.setCursor(0, 1);
    if (quick_adjust_rpm)
    {
      bottomLine = "Kroki:" +  String(settings[SET_TURN].currentValue);
    }
    else
    {
      bottomLine = "Pauza:" + String((float)settings[SET_PAUSE].currentValue / (float)settings[SET_PAUSE].divider, 1) + "ms";
    }
    lcd.print(bottomLine);
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print(settings[settings_sub_menu].topLine);

    lcd.setCursor(0, 1);
    switch (settings[settings_sub_menu].displayType)
    {
      case DIS_NONE:
        break;

      case DIS_VALUE:
      case DIS_POW:
        if (settings[settings_sub_menu].divider > 1)
        {
          bottomLine = String((float)settings[settings_sub_menu].currentValue / (float)settings[settings_sub_menu].divider, 1);
        }
        else
        {
          bottomLine = (String(settings[settings_sub_menu].currentValue));
        }
        break;

      case DIS_YESNO:
        (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("YES") : bottomLine = ("NO");
        break;

      case DIS_T1F0:
        (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("TRUE") : bottomLine = ("FALSE");
        break;

      case DIS_DIR:
        (settings[settings_sub_menu].currentValue > 0) ? bottomLine = ("CCW") : bottomLine = ("CW");
        break;
    }
    lcd.print(bottomLine);
    lcd.setCursor(bottomLine.length(), 1);
    lcd.print(settings[settings_sub_menu].bottomLine);
  }
}

bool HandleButton(int button)
{
  bool refresh = true;

  if (home_display)
  {
    switch (button)
    {
      case (BTN_UP):
        if (quick_adjust_rpm)
        {
          Increase(SET_TURN);
        }
        else
        {
          Increase(SET_PAUSE);
        }
        break;

      case (BTN_DOWN):
        if (quick_adjust_rpm)
        {
          Decrease(SET_TURN);
        }
        else
        {
          Decrease(SET_PAUSE);
        }
        break;

      case (BTN_LEFT):
        quick_adjust_rpm = true;
        break;

      case (BTN_RIGHT):
        quick_adjust_rpm = false;
        break;

      case (BTN_SELECT):
        home_display = false;
        break;

      default:
        refresh = false;
        break;
    }
  }
  else
  {
    switch (button)
    {
      case (BTN_UP):
        Increase(settings_sub_menu);
        break;

      case (BTN_DOWN):
        Decrease(settings_sub_menu);
        break;

      case (BTN_LEFT):
        settings_sub_menu--;
        settings_sub_menu = max(0, settings_sub_menu);
        break;

      case (BTN_RIGHT):
        settings_sub_menu++;
        settings_sub_menu = min(SET_COUNT - 1, settings_sub_menu);
        break;

      case (BTN_SELECT):
        home_display = true;
        break;

      default:
        refresh = false;
        break;
    }
  }
  return refresh;
}

void StepperMotor()
{
  static unsigned long micropulse = 0;
  static unsigned long micropause = 0;
  static unsigned long microsteps = 0;

  unsigned long microseconds = 0;
  unsigned long micronow = micros();

  double dmicroseconds =  (double)((double)stepsPerRevolution * (double)settings[SET_MICROSTEP].currentValue) * 
                          (double)((double)settings[SET_RPM].currentValue / (double)settings[SET_RPM].divider) /
                          (double)((double)settings[SET_RADIUS].currentValue / (double)settings[SET_RADIUS].divider / (10 / 0.10472)) *
                          (double)((double)settings[SET_RATIO].currentValue / (double)settings[SET_RATIO].divider);                       

  microseconds = 60L * 1000L * 1000L / (unsigned long)dmicroseconds;

  if (micronow - micropulse > microseconds)
  {
    micropulse = micronow;
    togglePulse == LOW ? togglePulse = HIGH : togglePulse = LOW;
    microsteps += (int)togglePulse;
  }


  if (microsteps < (settings[SET_TURN].currentValue * settings[SET_MICROSTEP].currentValue))
  {
    micropause = micronow;
  }
  else
  {
    if (micronow - micropause < (settings[SET_PAUSE].currentValue * 1000L))
    {
      togglePulse = LOW;
    }
    else
    {
      microsteps = 0;
    }
  }

  if (digitalRead(PAUSE_IN) == startStatePause)
  {
    run_state = PAUSED;
  }
  else
  {
    run_state = RUN;
  }

  digitalWrite(PUL_OUT, togglePulse ? HIGH : LOW);
  digitalWrite(DIR_OUT, settings[SET_DIR].currentValue > 0 ? HIGH : LOW);
  digitalWrite(EN_OUT, run_state == PAUSED ? HIGH : LOW);
}

void setup()
{
  lcd.begin(16, 2);
  
  pinMode(DIR_OUT, OUTPUT);
  pinMode(EN_OUT, OUTPUT);
  pinMode(PUL_OUT, OUTPUT);
  pinMode(PAUSE_IN, INPUT);

  digitalWrite(DIR_OUT, LOW);
  digitalWrite(EN_OUT, HIGH);
  digitalWrite(PUL_OUT, LOW);

  pinMode(KEY_UP, INPUT);
  pinMode(KEY_DOWN, INPUT);
  pinMode(KEY_LEFT, INPUT);
  pinMode(KEY_RIGHT, INPUT);
  pinMode(KEY_SELECT, INPUT);

  resetSettings();
  UpdateDisplay();

  run_state = READY;
  startStatePause = digitalRead(PAUSE_IN);
}

void loop()
{
  StepperMotor();
    
  if ((millis() / 100) == system_timer) return;
  system_timer = millis() / 100;

  readPauseButton();

  lcd_key = readKeyboard();

  if (lcd_key != BTN_NONE)
  {
    button_timer++;
    lcd_key_last = lcd_key;
  }
  else
  {
    if (HandleButton(lcd_key_last) || (last_run_state != run_state))
    {
      UpdateDisplay();
      last_run_state = run_state;
    }

    lcd_key_last = lcd_key;

    button_timer = 0;
  }
}
