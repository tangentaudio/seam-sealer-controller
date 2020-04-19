/////////////////////////////////////////////////////////////////////////////////
// IMPULSE SEAM SEALER CONTROLLER
// Copyright (C) 2020 Stephen Richardson (steve.richardson@makeitlabs.com)
//
// Part of MakeIt Labs COVID-19 Response Effort
// see http://www.makeitlabs.com/covid19 for more details
//
// Released under GPL v3, see https://www.gnu.org/licenses/gpl-3.0.en.html
//
// Disclaimer of Warranty.
// THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED 
// IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM “AS IS” WITHOUT WARRANTY OF ANY KIND,
// EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
// FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.
// SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.
//
// Limitation of Liability.
// IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER, OR ANY OTHER
// PARTY WHO MODIFIES AND/OR CONVEYS THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY 
// GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM 
// (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD 
// PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS 
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Platform: 
//  PJRC Teensy 3.5, Arduino environment 1.8.5
//  HX711 load cell amplifier/ADC
//  ST7735 TFT display 160x128
//  buttons, encoder knob
//  piezo beeper
//  high current DC solid state relay, PWM-capable
//
/////////////////////////////////////////////////////////////////////////////////

// N.B. Yes, this is a mess.  Not intended to represent best practice or good form.
// Written hastily for R&D purposes during the COVID-19 crisis.

#include <HX711.h>
#include <SPI.h>
#include <ST7735_t3.h>
#include <Bounce.h>
#include <limits.h>

#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

#define POT0 33
#define POT1 34

#define BUTTON_CFG0 33
#define BUTTON_CFG1 34

#define BUTTON0 35
#define BUTTON1 36
#define BUTTON2 37
#define BEEPER 38

#define ENC_A 25
#define ENC_B 24

#define PWM_OUT 30

#define HX711_CLK 0
#define HX711_DATA 1

#define TFT_CS 10
#define TFT_DC 3
#define TFT_RESET 2

#define HEATER_PWM_OFF 0

unsigned long cfg_timer_run_time = 3300;
unsigned long cfg_timer_cool_time = 5000;
unsigned long cfg_heater_pwm_run = 16384;
unsigned long cfg_load_ok_low = 400;
unsigned long cfg_load_ok_high = 500;

ST7735_t3 tft = ST7735_t3(TFT_CS, TFT_DC, TFT_RESET);
HX711 loadcell;
Encoder enc(ENC_A, ENC_B);
elapsedMillis milli;

Bounce button0 = Bounce(BUTTON0, 10);
Bounce button_cfg0 = Bounce(BUTTON_CFG0, 10);
Bounce button_cfg1 = Bounce(BUTTON_CFG1, 10);

bool config_mode = false;

void tft_init(bool);

void setup() {
  Serial.begin(9600);

  loadcell.begin(HX711_DATA, HX711_CLK);
  loadcell.set_scale(6726);
  //loadcell.tare();  

  tft_init(true);

  analogWriteResolution(16);

  pinMode(BUTTON0, INPUT_PULLUP);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);

  pinMode(BUTTON_CFG0, INPUT_PULLUP);
  pinMode(BUTTON_CFG1, INPUT_PULLUP);

}

#define BG_COLOR ST7735_BLACK
#define LABEL_COLOR ST7735_WHITE
#define LABEL_BG_COLOR ST7735_BLUE
#define LABEL_BG_COLOR_CFG ST7735_MAGENTA

#define LOAD_X_LABEL_POS 0
#define LOAD_X_POS 32
#define LOAD_Y_POS 0
#define LOAD_COLOR_OK ST7735_GREEN
#define LOAD_COLOR_BAD ST7735_RED

#define LOAD_X_BAR_POS LOAD_X_POS
#define LOAD_Y_BAR_POS 22
#define LOAD_BAR_COLOR_BAD ST7735_RED
#define LOAD_BAR_COLOR_OK ST7735_GREEN
#define LOAD_BAR_COLOR ST7735_WHITE

#define PWM_X_LABEL_POS 0
#define PWM_X_POS 32
#define PWM_Y_POS 46
#define PWM_COLOR_ON ST7735_GREEN
#define PWM_COLOR_OFF ST7735_WHITE

#define TIMER_X_LABEL_POS 0
#define TIMER_X_POS 32
#define TIMER_Y_POS 70
#define TIMER_COLOR_STOP ST7735_WHITE
#define TIMER_COLOR_RUN ST7735_GREEN
#define TIMER_COLOR_COOL ST7735_BLUE

#define STATUS_X_POS 0
#define STATUS_Y_POS 102
#define STATUS_COLOR ST7735_YELLOW

void label_load_cell(bool cfg = false, bool high = true)
{
  tft.fillRect(LOAD_X_LABEL_POS, LOAD_Y_POS, 30, 44, cfg ? LABEL_BG_COLOR_CFG : LABEL_BG_COLOR);
  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(1);
  tft.setCursor(LOAD_X_LABEL_POS + 2, LOAD_Y_POS + 2);
  tft.println("LOAD");
  tft.setCursor(LOAD_X_LABEL_POS + 2, LOAD_Y_POS + 12);
  tft.println("CELL");
  
  if (cfg) {
    if (high) {
      tft.setCursor(LOAD_X_LABEL_POS + 2, LOAD_Y_POS + 22);
      tft.println("HIGH");
    } else {
      tft.setCursor(LOAD_X_LABEL_POS + 2, LOAD_Y_POS + 22);
      tft.println("LOW");
    }
  }
}

void label_pwm(bool cfg = false)
{
  tft.fillRect(PWM_X_LABEL_POS, PWM_Y_POS, 30, 22, cfg ? LABEL_BG_COLOR_CFG : LABEL_BG_COLOR);
  tft.setCursor(PWM_X_LABEL_POS + 2, PWM_Y_POS + 2);
  tft.println("PWM");
  tft.setCursor(PWM_X_LABEL_POS + 2, PWM_Y_POS + 12);
  tft.println("OUT");
}

void label_timer(bool cfg = false, bool cool = false)
{
  tft.fillRect(TIMER_X_LABEL_POS, TIMER_Y_POS, 30, 22, cfg ? LABEL_BG_COLOR_CFG : LABEL_BG_COLOR);
  if (cool) {
    tft.setCursor(TIMER_X_LABEL_POS + 2, TIMER_Y_POS + 2);
    tft.println("COOL");
    tft.setCursor(TIMER_X_LABEL_POS + 2, TIMER_Y_POS + 12);
    tft.println("TIME");
  } else {
    tft.setCursor(TIMER_X_LABEL_POS + 2, TIMER_Y_POS + 2);
    tft.println("RUN");
    tft.setCursor(TIMER_X_LABEL_POS + 2, TIMER_Y_POS + 12);
    tft.println("TIME");
  }
}

void tft_init(bool init = false)
{
  if (init) {
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(3);
  }

  tft.fillScreen(ST7735_BLACK);
  tft.setTextWrap(false);

  label_load_cell();
  label_pwm();
  label_timer();

  
}

#define PRESSURE_LOW -1
#define PRESSURE_OK 0
#define PRESSURE_HIGH 1

int tft_update_load_cell(unsigned long int val, bool cfg = false, unsigned long int low = 0, unsigned long int high = 0)
{
  static char sv[10] = "";
  static unsigned long int last;
  static bool first = true;
  int ok = PRESSURE_LOW;

  float val_max = 1200.0;
  unsigned int bar_width = tft.width() - LOAD_X_BAR_POS;

  unsigned int low_val = (low != 0) ? low : cfg_load_ok_low;
  unsigned int high_val = (high != 0) ? high : cfg_load_ok_high;

  unsigned int load_scaled = (unsigned int)(( (float) val / val_max ) * bar_width);
  unsigned int low_scaled =  (unsigned int)(( (float) low_val / val_max ) * bar_width);
  unsigned int high_scaled = (unsigned int)(( (float) high_val / val_max ) * bar_width);

  if (load_scaled < low_scaled)
    ok = PRESSURE_LOW;
  else if (load_scaled > high_scaled)
    ok = PRESSURE_HIGH;
  else
    ok = PRESSURE_OK;

  if (val != last || first) {
    first = false;

    if (cfg) {
      tft.fillRect(LOAD_X_BAR_POS, LOAD_Y_BAR_POS, bar_width, 20, LOAD_BAR_COLOR_BAD);
      tft.fillRect(LOAD_X_BAR_POS + low_scaled, LOAD_Y_BAR_POS, high_scaled - low_scaled, 20, LOAD_BAR_COLOR_OK);
    } else {
      tft.fillRect(LOAD_X_BAR_POS, LOAD_Y_BAR_POS, load_scaled, 10, LOAD_BAR_COLOR);
      tft.fillRect(LOAD_X_BAR_POS + load_scaled, LOAD_Y_BAR_POS, bar_width - load_scaled, 10, BG_COLOR);
      tft.fillRect(LOAD_X_BAR_POS, LOAD_Y_BAR_POS + 10, bar_width, 8, LOAD_BAR_COLOR_BAD);
      tft.fillRect(LOAD_X_BAR_POS + low_scaled, LOAD_Y_BAR_POS + 10, high_scaled - low_scaled, 8, LOAD_BAR_COLOR_OK);
    }
        
    tft.setCursor(LOAD_X_POS, LOAD_Y_POS);
    tft.setTextColor(BG_COLOR);
    tft.setTextSize(3);
    tft.println(sv);
  
    sprintf(sv, "%lu", val);
  
    tft.setCursor(LOAD_X_POS, LOAD_Y_POS);
    if (ok == PRESSURE_OK) 
      tft.setTextColor(LOAD_COLOR_OK);
    else
      tft.setTextColor(LOAD_COLOR_BAD);
    
    tft.setTextSize(3);
    tft.println(sv);
  }
  last = val;  

  return ok;
}


void tft_update_timer(unsigned long int timer, bool cool = false)
{
  static char sv[10] = "";
  static unsigned long int last;
  static bool first = true;

  if (timer != last || first) {
    first = false;
    
    tft.setCursor(TIMER_X_POS, TIMER_Y_POS);
    tft.setTextColor(BG_COLOR);
    tft.setTextSize(3);
    tft.println(sv);
  
    unsigned long int secs = timer / 10;
    unsigned long int tenths = timer % 10;
  
    sprintf(sv, "%lu.%lu", secs, tenths);
  
    tft.setCursor(TIMER_X_POS, TIMER_Y_POS);
    if (timer > 0)
      if (cool)
        tft.setTextColor(TIMER_COLOR_COOL);
      else 
        tft.setTextColor(TIMER_COLOR_RUN);
    else
      tft.setTextColor(TIMER_COLOR_STOP);
    tft.setTextSize(3);
    tft.println(sv);
  }
  last = timer;  
}

void tft_update_pwm(unsigned int value)
{
  static char sv[10] = "";
  static unsigned long int last;
  static bool first = true;

  if (value != last || first) {
    first = false;
   
    tft.setCursor(PWM_X_POS, PWM_Y_POS);
    tft.setTextColor(BG_COLOR);
    tft.setTextSize(3);
    tft.println(sv);
  
    unsigned long int pct_fixed = (value * 1000) / 65535;
    unsigned long int pct = pct_fixed / 10;
    unsigned long int tenths = pct_fixed % 10;
  
    sprintf(sv, "%lu.%lu%%", pct, tenths);
  
    tft.setCursor(PWM_X_POS, PWM_Y_POS);

    if (value > 0)
      tft.setTextColor(PWM_COLOR_ON);
    else
      tft.setTextColor(PWM_COLOR_OFF);
    
    tft.setTextSize(3);
    tft.println(sv);
  }
  last = value;  
}

void tft_update_status(const char* status)
{
  static char sv[20] = "";
  static bool first = true;
  
  if (strcmp(status, sv) != 0 || first) {
    first = false;
   
    tft.setCursor(STATUS_X_POS, STATUS_Y_POS);
    tft.setTextColor(BG_COLOR);
    tft.setTextSize(2);
    tft.println(sv);
  
    tft.setCursor(STATUS_X_POS, STATUS_Y_POS);

    tft.setTextColor(STATUS_COLOR);
    
    tft.setTextSize(2);
    tft.println(status);
  }
  strcpy(sv, status);
}


void heater_set_pwm(unsigned int value) {
  analogWrite(PWM_OUT, value);

  tft_update_pwm(value);
}


void loop() {

  if (config_mode) {
    loop_config();
  } else {
    loop_operate();
  }
  
}


#define STATE_CFG_INIT 0
#define STATE_CFG_LOAD_CELL_LOW 1
#define STATE_CFG_LOAD_CELL_HIGH 2
#define STATE_CFG_PWM 3
#define STATE_CFG_RUN_TIME 4
#define STATE_CFG_COOL_TIME 5

void loop_config() 
{
  static unsigned short int state = STATE_CFG_INIT;
  static long pos_enc = -999;

  long new_enc = enc.read() / 4;
    
  if (new_enc != pos_enc) {
    Serial.print("enc=");
    Serial.println(new_enc);
    pos_enc = new_enc;
  }

  if (button_cfg1.update()) {
    if (button_cfg1.fallingEdge()) {
      config_mode = false;
      state = STATE_CFG_INIT;
      tft_init();
      label_load_cell(false);
      label_pwm(false);
      label_timer(false);
      return;
    }
  }

  switch (state) {
    case STATE_CFG_INIT:
    {
      state = STATE_CFG_LOAD_CELL_LOW;
      label_load_cell(true, false);
      label_pwm(false);
      label_timer(false, false);
      break;
    }

    case STATE_CFG_LOAD_CELL_LOW:
    {
      long newval = cfg_load_ok_low + (pos_enc * 10);
      if (newval < 1)
        newval = 1;
      else if (newval > (long)cfg_load_ok_high-10)
        newval = cfg_load_ok_high-10;
        
      tft_update_load_cell(newval, true, newval, cfg_load_ok_high);

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          cfg_load_ok_low = newval;
          
          enc.write(0);
          pos_enc = 0;
          state = STATE_CFG_LOAD_CELL_HIGH;
          label_load_cell(true, true);
          label_pwm(false);
          label_timer(false, false);
        }
      }
      break;
    }

    case STATE_CFG_LOAD_CELL_HIGH:
    {
      long newval = cfg_load_ok_high + (pos_enc * 10);
      if (newval < (long)cfg_load_ok_low + 10)
        newval = cfg_load_ok_low + 10;
      else if (newval > 1200)
        newval = 1200;
        
      tft_update_load_cell(newval, true, cfg_load_ok_low, newval);

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          cfg_load_ok_high = newval;
          
          enc.write(0);
          pos_enc = 0;
          state = STATE_CFG_PWM;
          label_load_cell(false);
          label_pwm(true);
          label_timer(false, false);
        }
      }
      break;
    }
    
    case STATE_CFG_PWM:
    {
      long int newval = cfg_heater_pwm_run + (pos_enc * 64);
      if (newval < 0)
        newval = 0;
      else if (newval > 65535)
        newval = 65535;
        
      tft_update_pwm(newval);

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          cfg_heater_pwm_run = newval;
          
          enc.write(0);
          pos_enc = 0;
          state = STATE_CFG_RUN_TIME;
          label_load_cell(false);
          label_pwm(false);
          label_timer(true, false);
        }
      }
      break;
    }

    case STATE_CFG_RUN_TIME:
    {
      long int newval = cfg_timer_run_time + (pos_enc * 100);
      if (newval < 0)
        newval = 0;
      else if (newval > 65535)
        newval = 65535;
        
      tft_update_timer(newval / 100);

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          cfg_timer_run_time = newval;
          enc.write(0);
          pos_enc = 0;
          state = STATE_CFG_COOL_TIME;
          label_load_cell(false);
          label_pwm(false);
          label_timer(true, true);
        }
      }
      break;
    }

    case STATE_CFG_COOL_TIME:
    {
      long int newval = cfg_timer_cool_time + (pos_enc * 100);
      if (newval < 0)
        newval = 0;
      else if (newval > 65535)
        newval = 65535;
        
      tft_update_timer(newval / 100, true);

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          cfg_timer_cool_time = newval;
          enc.write(0);
          pos_enc = 0;
          state = STATE_CFG_LOAD_CELL_LOW;
          label_load_cell(true, false);
          label_pwm(false);
          label_timer(false, false);
        }
      }
      break;
    }
  }
}


#define STATE_INIT 0
#define STATE_OFF 1
#define STATE_ON 2
#define STATE_COOL 3
#define STATE_DONE 4

void loop_operate() {
  static unsigned short int state = STATE_INIT;
  static unsigned long int tcap = milli;
  static int last_load_ok = INT_MAX;
  int load_ok = INT_MAX;
  static char load_status[20];


  unsigned int load_raw = loadcell.get_units(1);
  load_ok = tft_update_load_cell(load_raw);

  if (load_ok != last_load_ok) {
    if (load_ok == PRESSURE_LOW) {
      tone(BEEPER, 800, 50);
      strcpy(load_status, "Low Pressure");
    } else if (load_ok == PRESSURE_HIGH) {
      tone(BEEPER, 800, 50);
      strcpy(load_status, "Pressure High");
    } else {
      tone(BEEPER, 1600, 50);
      strcpy(load_status, "Pressure OK");
    }
  }

  last_load_ok = load_ok;

  switch (state) {
  case STATE_INIT:
    {
      heater_set_pwm(HEATER_PWM_OFF);
      tft_update_timer(0);
      state = STATE_OFF;
      break;
    }
    
  case STATE_OFF:
    {
      tft_update_status(load_status);
      if (button0.update()) {
        if (button0.fallingEdge()) {
          if (load_ok == PRESSURE_OK) {
            state = STATE_ON;
            tone(BEEPER, 1200, 500);
            
            tcap = milli + cfg_timer_run_time;
            heater_set_pwm(cfg_heater_pwm_run);
          } else {
            tone(BEEPER, 400, 1000);
          }
        }
      }

      if (button_cfg0.update()) {
        if (button_cfg0.fallingEdge()) {
          config_mode = true;
          tone(BEEPER, 2000, 250);
          tft_init();
          tft_update_status("CONFIG");
        }
      }
      break;
    }

  case STATE_ON:
    {
      tft_update_status("HEATING");

      unsigned long int t = (tcap - milli) / 100;
  
      if (t <= 0)
        t = 0;
        
      tft_update_timer(t);
  
      if (t == 0) {
        tcap = milli + cfg_timer_cool_time;
        state = STATE_COOL;
        tone(BEEPER, 1500, 100);
      }
      break;
    }

  case STATE_COOL:
    {
      tft_update_status("COOLING");
      
      heater_set_pwm(HEATER_PWM_OFF);
      
      unsigned long int t = (tcap - milli) / 100;
  
      if (t <= 0)
        t = 0;
        
      tft_update_timer(t, true);
  
      if (t == 0) {
        tone(BEEPER, 1000, 1000);

        state = STATE_DONE;
      }
      break;
    }

  case STATE_DONE:
    {
        tft_update_status("DONE, RELEASE");
        if (load_ok == PRESSURE_LOW) {
          state = STATE_INIT;
        }

      break;
    }
  }  
}
