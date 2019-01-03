/*
 * ideas for monosynth with patch memory using teensy 3.6
 *
 * general plan -> LCD screen shows 4 active parameters, 4 knobs below   
 * control those parameters. scroll through "pages" of parameters with up/down
 * buttons or encoder or something.
 *
 * parameters are 8 bit. (?)
 * 
 * LFO and ADSRs generated in software, VCOs, VCF, VCA all hardware
 * 
 * ^^^^^^^ MESSAGE FOR NICK AND ALEJANDRO HERE ^^^^^^^
 * 
 * this file is from when I started messing around with programming, and everything
 * about it is pretty trash. I didn't know about oop when I started this. there are 
 * some things in here that I don't remember why I did it that way. Basically, it's 
 * just a really really rough starting point.
 */

#include <Encoder.h>
#include <ADC.h>

Encoder myEnc(15, 14);

const uint8_t       NUM_PAGES               =       11;       // how many pages of parameters?
const uint8_t       NUM_ITEMS               =       4;        // how many items per page?
const uint8_t       NUM_PATCHES             =       255;



//------------ LCD SERIAL OUTPUT PIN -----------------------------------------------------------------------------

const uint8_t       LCD_PIN                 =       1;        // pin for LCD serial communication using serial port 1, pin # 1, TX1



//------------ 8 BIT PWM DAC OUTPUT PINS -----------------------------------------------------------------------------

const uint8_t       PULSE_1_PIN             =       2;        // pwm output to control pulse width 1, 0 = PW of 5%, 255 = PW of 95%
const uint8_t       PULSE_2_PIN             =       3;        // pwm output to control pulse width 2, same as above
const uint8_t       VCO_1_LEV_PIN           =       4;        // controls mixer level, 0 = 0dB, 255 = -120dB
const uint8_t       VCO_2_LEV_PIN           =       5;        // same as above
const uint8_t       NOISE_LEV_PIN           =       6;        // same as above
const uint8_t       EXT_LEV_PIN             =       7;        // same as above
const uint8_t       VCF_Q_PIN               =       8;        // controls VCF Q, 0 = minimum Q, 255 = max Q
const uint8_t       NOISE_PIN               =       9;        // 8 bit random number noise, for audio



//------------ LOGIC OUTPUT PINS -----------------------------------------------------------------------------

const uint8_t       VCO1_TRI_PIN            =       11;       // low = tri, high = saw or square
const uint8_t       VCO1_SAW_SQR_PIN        =       12;       // low = saw, high = square
const uint8_t       SYNC_PIN                =       24;       // high = VCO1 acts as master to VCO2 slave, low = VCO2 is free running
const uint8_t       VCO2_TRI_PIN            =       25;       // low = tri, high = saw or square
const uint8_t       VCO2_SAW_SQR_PIN        =       26;       // low = saw, high = square



//------------ POT ADC INPUT PINS -----------------------------------------------------------------------------

const uint8_t       POT_NUMS[4]             =       {A9, A8, A7, A6};




volatile uint8_t    pot_values[4]           =       {0, 0, 0, 0};
volatile uint8_t    last_values[4]          =       {0, 0, 0, 0};
volatile boolean    pot_grabbed[4]          =       {false, false, false, false};
volatile boolean    pot_control[4]          =       {false, false, false, false};


uint8_t             parameter_page          =       0;
uint8_t             last_page               =       0;

boolean             patch_up                =       false;
boolean             patch_down              =       false;
boolean             patch_change            =       false;

const String        PAGE[NUM_PAGES]         =      {
                                                     "      VCO 1     pg 1",
                                                     "      VCO 2     pg 2",
                                                     "      MIXER     pg 3",
                                                     "       VCF      pg 4",
                                                     "   VCF ENVELOPE pg 5",
                                                     "   VCA ENVELOPE pg 6",
                                                     " LFO PARAMETERS pg 7",
                                                     "  LFO QUANTIZE  pg 8",
                                                     "   LFO ASSIGN   pg 9",
                                                     "TRIGGER ASSIGN pg 10",
                                                     "     MISC.     pg 11"
                                                   };

const String        PARAMETERS[NUM_PAGES * NUM_ITEMS] = {     
                                                          //"VCO 1 octave: ", "VCO 1 wave: ", "VCO 1 Pulse width: ", "sync: ",                                                   // VCO 1
                                                          " OCT",             "WAVE",         "SYNC",                "  PW",
                                                          //"VCO 2 octave: ", "VCO 2 interval: ", "VCO 2 wave: ", "VCO 2 Pulse width: ",                                         // VCO 2
                                                          " OCT",             "WAVE",             "TUNE",         "  PW",
                                                          //"VCO 1 level: ", "VCO 2 level: ", "Noise level: ", "Ext level: ",                                                    // mixer
                                                          "VCO1",             "VCO2",           "NOISE",         " EXT",
                                                          //"VCF frequency: ", "VCF resonance: ", "VCF tracking: ", "VCF env level: ",                                           // VCF
                                                          "FREQ",             "EMPH",             "TRACK",           " ENV",
                                                          //"VCF attack: ", "VCF decay: ", "VCF sustain: ", "VCF release: ",                                                     // VCF envelope
                                                          " ATT",           " DEC",         " SUS",         " REL",
                                                          //"VCA attack: ", "VCA decay: ", "VCA sustain: ", "VCA release: ",                                                     // VCA envelope
                                                          " ATT",           " DEC",         " SUS",         " REL",
                                                          //"LFO rate: ", "LFO wave: ", "LFO sample: ", "sample rate: ",                                                         // LFO parameters
                                                          "RATE",         "WAVE",        "SAMP",        "SRATE",
                                                          //"VCO 1 FM level: ", "VCO 2 FM level: ", "PWM level: ", "VCF FM level: ",                                             // LFO assign
                                                          "QUANT",           " WBWB WWBW BWBW",         "",         "",
                                                          //"Delay time: ", "Delay repeats: ", "Delay level: ", "Reverb level: ",                                                // FX  
                                                          " FM1",           " FM2",           " PWM",         " VCF",
                                                          //trigger mode,     lfo trig,       , samp trig,      ext trig,
                                                          "MODE",           " LFO",           "SAMP",         " EXT",
                                                          //"Glide: ", "Master volume: ", "Master tune: ", "A 440:"                                                              // misc.
                                                          "GLIDE",     " VOL",           "TUNE",           "A440"
                                                        };
      
uint8_t              current_values[NUM_PAGES * NUM_ITEMS] =   {    
                                                          0, 100, 0, 128,                                                                                               // VCO 1
                
                                                          0, 100, 128, 128,                                                                                                     // VCO 2
                                                         
                                                          255, 255, 0, 0,                                                                                                      // mixer
                                                         
                                                          255, 0, 255, 0,                                                                                                      // VCF
                                                         
                                                          0, 64, 128, 64,                                                                                                      // VCF envelope
                                                         
                                                          0, 64, 128, 64,                                                                                                      // VCA envelope
                                                         
                                                          128, 0, 0, 255,                                                                                               // LFO parameters
                
                                                          0, 0, 0, 0,                                                                                                       // LFO quantize
                                                         
                                                          0, 0, 0, 0,                                                                                                      // LFO assign
                                                         
                                                          0, 0, 0, 255,                                                                                                       // trigger assign
                                                          
                                                          0, 255, 128, 0                                                                                                            // misc
                                                       };

String              display_values[NUM_PAGES * NUM_ITEMS] =   {};




//const int     DAC_ONE                 =     A21;        // DAC 0 pin on teensy
//const int     DAC_TWO                 =     A22;        // DAC 1 pin on teensy

//IntervalTimer accumulator_sample_timer; 

void setup() {

pinMode(LCD_PIN, OUTPUT);

pinMode(VCO1_TRI_PIN, OUTPUT);
pinMode(VCO1_SAW_SQR_PIN, OUTPUT);
pinMode(SYNC_PIN, OUTPUT);
pinMode(VCO2_TRI_PIN, OUTPUT);
pinMode(VCO2_SAW_SQR_PIN, OUTPUT);


pinMode(PULSE_1_PIN, OUTPUT);
analogWriteFrequency(PULSE_1_PIN, 187500);

pinMode(PULSE_2_PIN, OUTPUT);
analogWriteFrequency(PULSE_2_PIN, 187500);

pinMode(VCO_1_LEV_PIN, OUTPUT);
analogWriteFrequency(VCO_1_LEV_PIN, 187500);

pinMode(VCO_2_LEV_PIN, OUTPUT);
analogWriteFrequency(VCO_2_LEV_PIN, 187500);

pinMode(NOISE_LEV_PIN, OUTPUT);
analogWriteFrequency(NOISE_LEV_PIN, 187500);

pinMode(EXT_LEV_PIN, OUTPUT);
analogWriteFrequency(EXT_LEV_PIN, 187500);

pinMode(VCF_Q_PIN, OUTPUT);
analogWriteFrequency(VCF_Q_PIN, 187500);

pinMode(NOISE_PIN, OUTPUT);
analogWriteFrequency(NOISE_PIN, 187500);

for (uint8_t i = 0; i < 4; i ++)
{
  pinMode(POT_NUMS[i], INPUT);                                // setup the four pots as inputs
}

//Serial.begin(9600);   while (!Serial) { }

Serial1.begin(9600);

delay(1000);

initialize_display();

//accumulator_sample_timer.begin(update_phase_accum, 50);               // update the phase accumulator LFOs & ADSRs at 20kHz ???

//accumulator_sample_timer.priority(0);                                 // give the accumulator high priority


}

void loop() 
{

  //noInterrupts();

  write_page();

  scan_pots();

  interpret_values();

  refresh_values();
  
  //interrupts();
}

/*
uint16_t  update_phase_accum()
{
  // do phase accumulator stuff here
}
*/


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ SCAN POTS +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void scan_pots()
{
  
  static uint32_t pot_timer;
  
  if ((millis() - pot_timer) >= 100)
  {
    pot_timer = millis();
    
    for (uint8_t i = 0; i < 4; i ++)
    {
      pot_values[i] = analogRead(POT_NUMS[i]) >> 2;               // read the pots, take just the upper 8 bits of the 10 bit reading
      
      if (abs(pot_values[i] - last_values[i] > 15))               // it you change a pot by a significant amount 
      {
        pot_grabbed[i] = true;                                    // you "grabbed" it
      } 
  
      if (pot_grabbed[i])                                                       // if you've grabbed the pot
      {
        if (last_values[i] <= current_values[i + parameter_page * 4])           // if the last pot value was smaller than the value stored in the array
        {                                                                       
          if (pot_values[i] >= current_values[i + parameter_page * 4])          // if you've turned the pot up to at least the value stored in the array 
          {                                                                     
            pot_control[i] = true;                                              // hand control over to the pot, 
          }
        } else if (last_values[i] > current_values[i + parameter_page * 4]) {   // likewise, if the last pot value was larger than the value stored in the array
          if (pot_values[i] <= current_values[i + parameter_page * 4])          // if you've turned the pot down to at least the value stored in the array
          {
            pot_control[i] = true;                                              // hand over control to the pot
          }
        }
      }
  
      if (pot_control[i])
      {
        current_values[i + parameter_page * 4] = pot_values[i];                 // if the pot is controlling this parameter, update the value 
      } 
      
      last_values[i] = pot_values[i];
    
    }
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++ INTERPRET VALUES +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void interpret_values()
{
  for (uint8_t i = 0; i < NUM_PAGES * NUM_ITEMS; i ++)
  {
    switch(i)
    {
      case 0:                                                                 // octave 1, 0 to 5
      {
        display_values[i] = get_int_string(current_values[i] / 46);
        // vco1_ctrl += current_values[i] / 46;
      }
      break;

      case 1:                                                                 // wave 1, tri, saw or square
      {
        if (current_values[i] < 85)
        {
          display_values[i] = " TRI";
          digitalWrite(VCO1_TRI_PIN, LOW);
          digitalWrite(VCO1_SAW_SQR_PIN, LOW);
        } 
        else if (current_values[i] < 170) 
        {
          display_values[i] = " SAW";
          digitalWrite(VCO1_TRI_PIN, HIGH);
          digitalWrite(VCO1_SAW_SQR_PIN, LOW);
          
        } 
        else 
        {
          display_values[i] = " SQR";
          digitalWrite(VCO1_TRI_PIN, HIGH);
          digitalWrite(VCO1_SAW_SQR_PIN, HIGH);
        }
      }
      break;

      case 2:                                                                         // sync, on or off
      {
        if (current_values[i] < 128)
        {
          display_values[i] = " OFF";
          digitalWrite(SYNC_PIN, LOW);
        }
        else
        {
          display_values[i] = "  ON";
          digitalWrite(SYNC_PIN, HIGH);
        }
        break;
      }

      case 3:                                                                         // pulse width 1, 5 to 95
      {
        display_values[i] = get_int_string(((current_values[i] * 1000) / 2833) + 5);
        //vco1_pw += (current_values[i] * 1000) / 2833) + 5;
        analogWrite(PULSE_1_PIN, current_values[i]);
      }
      break;

      case 4:                                                                         // octave 2, 0 to 5
      {
        display_values[i] = get_int_string(current_values[i] / 46);
      }
      break;

      case 5:                                                                         // wave 2, tri, saw or square
      {
        if (current_values[i] < 85)
        {
          display_values[i] = " TRI";
          digitalWrite(VCO2_TRI_PIN, LOW);
          digitalWrite(VCO2_SAW_SQR_PIN, LOW);
        } 
        else if (current_values[i] < 170) 
        {
          display_values[i] = " SAW";
          digitalWrite(VCO2_TRI_PIN, HIGH);
          digitalWrite(VCO2_SAW_SQR_PIN, LOW);
        } 
        else 
        {
          display_values[i] = " SQR";
          digitalWrite(VCO2_TRI_PIN, HIGH);
          digitalWrite(VCO2_SAW_SQR_PIN, HIGH);
        }
      }
      break;

      case 6:                                                                         // tune 2, -128 to 127
      {
        display_values[i] = get_int_string(current_values[i] - 128);
      }
      break;

      case 7:                                                                         // pulse width 2, 5 to 95
      {
        display_values[i] = get_int_string(((current_values[i] * 1000) / 2833) + 5);
        analogWrite(PULSE_2_PIN, current_values[i]);
      }
      break;

      case 8:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vco 1 level, 0 to 255
        analogWrite(VCO_1_LEV_PIN, 255 - current_values[i]);
      }
      break;

      case 9:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vco 2 level, 0 to 255
        analogWrite(VCO_2_LEV_PIN, 255 - current_values[i]);
      }
      break;


      case 10:
      {
        display_values[i] = get_int_string(current_values[i]);                        // noise level, 0 to 255
        analogWrite(NOISE_LEV_PIN, 255 - current_values[i]);
      }
      break;


      case 11:
      {
        display_values[i] = get_int_string(current_values[i]);                        // external level, 0 to 255
        analogWrite(EXT_LEV_PIN, 255 - current_values[i]);
      }
      break;

      case 12:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf frequency level, 0 to 255
      }
      break;

      case 13:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf emphasis level, 0 to 255
        analogWrite(VCF_Q_PIN, current_values[i]);
      }
      break;


      case 14:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf key tracking, 0 to 255
      }
      break;


      case 15:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf envelope modulation, 0 to 255
      }
      break;

      case 16:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf adsr attack, 0 to 255
      }
      break;

      case 17:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf adsr decay, 0 to 255
      }
      break;


      case 18:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf adsr sustain, 0 to 255
      }
      break;


      case 19:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vcf adsr release, 0 to 255
      }
      break;

      case 20:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vca adsr attack, 0 to 255
      }
      break;

      case 21:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vca adsr decay, 0 to 255
      }
      break;


      case 22:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vca adsr sustain, 0 to 255
      }
      break;


      case 23:
      {
        display_values[i] = get_int_string(current_values[i]);                        // vca adsr release, 0 to 255
      }
      break;

      case 24:
      {
        display_values[i] = get_int_string(current_values[i]);                        // lfo rate, 0 to 255
      }
      break;

      case 25:                                                                        // lfo wave, sine, tri, up saw, down saw, square, noise
      {
        if (current_values[i] < 42)
        {
          display_values[i] = "SINE";  
        }
        else if (current_values[i] < 84)
        {
          display_values[i] = " TRI";
        }
        else if (current_values[i] < 126)
        {
          display_values[i] = "USAW";
        }
        else if (current_values[i] < 168)
        {
          display_values[i] = "DSAW";
        }
        else if (current_values[i] < 210)
        {
          display_values[i] = " SQR";
        }
        else
        {
          display_values[i] = "NOIS";
        }
      }
      break;


      case 26:
      {
        if (current_values[i] < 128)                              // lfo sample, on or off
        {
          display_values[i] = " OFF";
        }
        else
        {
          display_values[i] = "  ON";
        }
      }
      break;


      case 27:
      {
        display_values[i] = get_int_string(current_values[i]);    // lfo sample rate, 0 to 255
      }
      break;

       case 28:
      {
        if (current_values[i] < 128)                              // quantize lfo on/off
        {
          display_values[i] = " OFF";
        }
        else 
        {
          display_values[i] = "  ON";
        }
      }
      break;

      case 29:
      {
        display_values[i] = " " + get_binary_string(current_values[i]);                              // first 4 notes quantize
      }
      break;

      case 30:
      {
        display_values[i] =  " " + get_binary_string(current_values[i]);                             // second 4
      }
      break;


      case 31:
      {
        display_values[i] = " " + get_binary_string(current_values[i]);                              // third 4
      }
      break;

       case 32:
      {
        display_values[i] = get_int_string(current_values[i]);                                       // lfo vco 1 mod, 0 to 255
      }
      break;

      case 33:
      {
        display_values[i] = get_int_string(current_values[i]);                                       // lfo vco 2 mod, 0 to 255
      }
      break;

      case 34:
      {
        display_values[i] = get_int_string(current_values[i]);                                       // lfo pwm, 0 to 255
      }
      break;

            case 35:
      {
        display_values[i] = get_int_string(current_values[i]);                                       // lfo vcf mod, 0 to 255
      }
      break;

       case 36:
      {
        if (current_values[i] < 128)                                                                 // trigger mode, OR or AND
        {
          display_values[i] = "  OR";
        }
        else 
        {
          display_values[i] = " AND";
        }                              
      }
      break;

      case 37:
      {
        if (current_values[i] < 128)                                                                 // lfo trigger, off or on
        {
          display_values[i] = " OFF";
        }
        else 
        {
          display_values[i] = "  ON";
        } 
      }
      break;

      case 38:
      {
        if (current_values[i] < 128)                                                                 // sample clock trigger, off or on
        {
          display_values[i] = " OFF";
        }
        else 
        {
          display_values[i] = "  ON";
        } 
      }
      break;


      case 39:
      {
        if (current_values[i] < 128)                                                                 // external trigger, off or on
        {
          display_values[i] = " OFF";
        }
        else 
        {
          display_values[i] = "  ON";
        } 
      }
      break;
  

       case 40:
      {
        display_values[i] = get_int_string(current_values[i]);                              // glide, 0 to 255
      }
      break;

      case 41:
      {
        display_values[i] = get_int_string(current_values[i]);                              // master volume, 0 to 255
      }
      break;

      case 42:
      {
        display_values[i] = get_int_string(current_values[i] - 128);                        // master tune, -128 to 127
      }
      break;


      case 43:
      {
        if (current_values[i] < 128)                                                        // a440, on or off
        {
          display_values[i] = " OFF";
        }
        else 
        {
          display_values[i] = "  ON";
        }
      }
      break;
    }
  }
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ WRITE TO SCREEN +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void display_write(int column, int row, String message)    // prepares and writes to the wireless display, WORKS WITH NEW HAVEN 4 x 20 LCD WITH ADAFRUIT BACKPACK
{
  Serial1.write(0xFE);                                     // Command
  Serial1.write(0x47);                                     // Move cursor to
  Serial1.write(column);                                   // column
  Serial1.write(row);                                      // row
  delay(1);                                                // short delay may be required
  Serial1.print(message);                                  // and here we write the actual message
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ CLEAR SCREEN +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void clear_screen()                                         // just clears the screen
{
  Serial1.write(0xFE);
  Serial1.write(0x58);                                                                                                 
  delay(10);  
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ GET INT STRING +++++++++++++++++++++++++++++++++++++++++++++++++++++//

String get_int_string(int input)                          // returns a string version of an integer, for printing to the LCD
                                                          // works for a range of 999 to -999
                                                          // adds leading " " blank spaces to numbers so they come out right justified
{
  if (input < -100)
  {
    return String(input);
  }
  else if (input <= - 10) 
  {
     return " " + String(input);
  }
  else if (input < 0)
  {
    return "  " + String(input);
  }
  else if (input < 10)
  {
    return "   " + String(input); 
  } 
  else if (input < 100) 
  {
    return "  " + String(input);
  } 
  else 
  {
    return " " + String(input);
  }
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ GET BINARY STRING +++++++++++++++++++++++++++++++++++++++++++++++++++++//

String get_binary_string(uint8_t input)                    // takes an 8 bit integer as input and returns a four bit binary representation of an integer, as a string, for printing to the LCD
                                                           // ex. an input of 5 returns the string "0101"
{
  String output_string = "";
  input >>= 4;                                             // shift the input over to get a 4 bit number

  if (input & B1000)
  {
    output_string += "1";
  }
  else 
  {
    output_string += "0";
  }

  if (input & B0100)
  {
    output_string += "1";
  }
  else
  {
    output_string += "0";
  }

  if (input & B0010)
  {
    output_string += "1";
  }
  else 
  {
    output_string += "0";
  }

  if (input & B0001)
  {
    output_string += "1";
  }
  else
  {
    output_string += "0";
  }

  return output_string;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ INITIALIZE DISPLAY +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void initialize_display()
{

  delay(1000);
  
  Serial1.write(0xFE);                    //initialize display size
  Serial1.write(0xD1);
  Serial1.write(20);                      // 20 columns
  Serial1.write(4);                       // 4 rows
  delay(10);       
  
  Serial1.write(0xFE); //splash screen
  Serial1.write(0x40);
  Serial1.write("                    !#$%#$%&%&*^(@#%^$%^                       ");
  delay(10);
  
  // set the contrast, 200 is a good place to start, adjust as desired
  Serial1.write(0xFE); //contrast
  Serial1.write(0x50);
  Serial1.write(230);
  delay(10);       
  
  // set the brightness - (255 is max brightness)
  Serial1.write(0xFE); //brightness
  Serial1.write(0x99);
  Serial1.write(255);
  delay(10);       
  
  // turn off cursors
  Serial1.write(0xFE);
  Serial1.write(0x4B);
  Serial1.write(0xFE);
  Serial1.write(0x54);
  
  //DISABLE AUTOSCROLL SO LAST CHARACTER WILL DISPLAY CORRECTLY  
  Serial1.write(0xFE);
  Serial1.write(0x52);

  Serial1.write(0xFE);
  Serial1.write(0x58);                                                            // clear the screen                                          
  delay(10);  
  
  
  display_write(5, 1, PAGE[parameter_page]);                                      // write the title of the page
  for (uint8_t i = 0; i < 4; i ++)
  {
    display_write(i * 5 + 1, 3, PARAMETERS[i + parameter_page * 4]);
  }
}



//+++++++++++++++++++++++++++++++++++++++++++++++++++++ GET PAGE +++++++++++++++++++++++++++++++++++++++++++++++++++++//

uint8_t get_page()                                  // this function reads the encoder and returns the parameter page
                                                    // turning the encoder 1 detent clockwise increments the page by 1 
                                                    // turning the encoder 1 detent counter clockwise decrements the page by 1
                                                    // we don't want to be able to turn the page below 0 or above the highest page number
                                                    // when we are in a detent and wiggle the encoder, we don't want the page to jump around
                                                    // we only want the page to change when we firmly move to a new detent

{  
  static int16_t encoder_position;
  
  encoder_position = myEnc.read() + 1;              // we add 1 to the encoder position here to stop the page from jumping up and down while we're in a detent
  
  if (encoder_position <= 0)                        // don't go lower than page zero
  {
    encoder_position = 0;
    myEnc.write(0);
  } 
  else if (encoder_position >= (NUM_PAGES << 2))    // and don't go higher than the number of pages, the encoder counts 4 ticks per detent, so we multiply the number of pages by 4 to make it work out
  {
    encoder_position = (NUM_PAGES - 1) << 2;        // we subtract 1 from the number of pages here because of zero indexing, ex. there are 11 pages, we want to count from 0 to 10 and not go above 10 
    myEnc.write(encoder_position);
  }

  return encoder_position >> 2;                     // since the encoder counts 4 ticks per detent, we divide by 4 here to get the correct page number
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++ WRITE PAGE +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void write_page() 
{
  //uint8_t parameter_page;
  //uint8_t last_page;

  parameter_page = get_page();
  
  if (parameter_page != last_page)
  {
    clear_screen(); 

    display_write(1, 1, PAGE[parameter_page]);                                      // write the title of the page
    
    for (uint8_t i = 0; i < 4; i ++)                                                // go through each of the four knobs and parameters
    {                                                                              
      pot_grabbed[i] = false;                                                       // when we first write a new page
      pot_control[i] = false;                                                       // we don't want the pots to control anything
      display_write(i * 5 + 1, 3, PARAMETERS[i + parameter_page * 4]);              // fill in the list of parameters on that page
    }                                                                               // (i * 5 + 1) gives us the column number to print to, each entry takes up 5 columns
                                                                                    // the 3 is the 3rd row 
                                                                                    // [i + parameter_page * 4] gives us the parameter to write, 4 parameters per page              
    last_page = parameter_page;
  }
}



//+++++++++++++++++++++++++++++++++++++++++++++++++++++ WRITE VALUES +++++++++++++++++++++++++++++++++++++++++++++++++++++//

void refresh_values()
{

  static uint32_t value_refresh_timer;

  if ((millis() - value_refresh_timer) >= 33)
  {
    value_refresh_timer = millis();
    for (uint8_t i = 0; i < 4; i ++)
    {                                          
      display_write(i * 5 + 1, 4, display_values[i + parameter_page * 4]);
    } 
  }
}
  
