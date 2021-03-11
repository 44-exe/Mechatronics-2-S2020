#include <LiquidCrystal.h>
#include <avr/io.h>
#include <avr/interrupt.h>


/**** DEFINES ****/
#define BSIZE 4 //top initialise ButtonHistory[] size
const char USERPOS = 0xFF; //character for user selection

/**** C++ ****/
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/**** ENUMS ****/
enum MODE //jsut for main menu
{
  STARTUP,
  DEBUG,
  IR,
  CM,
  PM,
  SET,
  DRIVE
};


enum BUTTONS
{
  NOTHING,
  RIGHT,
  UP,
  DOWN,
  LEFT,
  SELECT
};

enum MOTOR_DIRECTION
{
  CW, //Clock Wise
  CCW //Counter Clock Wise
};

/**** GLOBAL VARIABLES ****/
int adc_input;

bool debugCheck = false;

uint8_t menuSelect = 0;
uint8_t menuCheck = 0;

uint8_t Steps = 0; //determines which step the motor is on

uint8_t Button = 0; //reads button user input
uint8_t ButtonHistory[BSIZE]; //records the hitory of button pressed (for combo)
uint8_t ButtonIndex = 0; //keeps track of combo

uint8_t DebugUserPos = 0; //should be between 
uint8_t CMUserPos = 0;

bool CM_Dir = CW;

uint32_t totalsec = 0;
uint8_t mins = 0; //for tellimg number of mins
uint8_t secs = 0; //for telling number of secs

uint32_t Delay = 0;

uint8_t M_Speed;
bool M_Start = false;

int16_t PM_Step = 100;
uint16_t PM_Tmp = 100;
bool PM_Start = false;

uint8_t SET_Size = 20; //default

uint8_t Tmp_IR1;
uint8_t Tmp_IR2;
uint8_t IR_Value = 0;
boolean IR_Start = false;

int32_t Drive_Step = 0;
bool Drive_Direct = CW;
bool Drive_Start = false;


/*
 * Setup Timer2 (TC2) in normal mode
 */
void DelaySetup(void)
{
//FOR EVERY 1ms
  OCR2A = 0xFF; //output compare reg 255

  TCCR2A = (1<<WGM21); //CTC Mode

  TIMSK2 = (1<<OCIE2A); //interrupt on compare match

  TCCR2B = (1<<CS22); //set prescaler 64
}

/*
 * set up Timer1 for interrupt every 1 sec (CTC)
 */
void TimerSetup(void)
{
  OCR1A = 0x3D08;
  
  // Mode 4, CTC on OCR1A
  TCCR1B |= (1<<WGM12);

  //Set interrupt on compare match
  TIMSK1 |= (1<<OCIE1A);

  // set prescaler to 1024
  TCCR1B |= (1<<CS12) | (1<<CS10);
}


/*
 * Set a delay using Counter 2
 * 
 * @param milli - user inputs the duration of delay on the program
 */
void DelayMilli(uint32_t milli)
{
  milli = (milli*1000)/1024;
  Delay = 0;
  while(milli >= Delay)
  {
    Serial.print("");
  }
}

void StepperSetup(void)
{
  //set direction of port B: 5, 4, 3 for pins 13, 12, 11 respectively
  DDRB |= (1<<DDB5) | (1<<DDB4) | (1<<DDB3);
  //set direction of port D: 3 for pins 3
  DDRD |= (1<<DDD3);
}

void Motor_Clear()
{
  PORTB &= ~((1<<PORTB3) | (1<<PORTB4) | (1<<PORTB5));
  PORTD &= ~(1<<PORTD3);
}

void MOTOR_OneStep(bool direct)
{
  switch(Steps)
  {
    case 0://pin 9
    PORTB |= (1<<PORTB5); //enable 9
    PORTB &= ~(1<<PORTB4); //disable pins 10, 11, 12
    PORTB &= ~(1<<PORTB3);
    PORTD &= ~(1<<PORTD3);
    break;
    
    case 1://pin 9 & 10
    PORTB |= (1<<PORTB5) | (1<<PORTB4); //enable 9 & 10
    PORTB &= ~(1<<PORTB3); //disable pins 11, 12
    PORTD &= ~(1<<PORTD3);
    break;
    
    case 2://pin 10
    PORTB |= (1<<PORTB4); //enable 10
    PORTB &= ~(1<<PORTB5); //disable pins 9, 11, 12
    PORTB &= ~(1<<PORTB3);
    PORTD &= ~(1<<PORTD3);
    break;
    
    case 3://pin 10 * 11
    PORTB |= (1<<PORTB4) | (1<<PORTB3); //enable 10 & 11
    PORTB &= ~(1<<PORTB5); //disable pins 9, 12
    PORTD &= ~(1<<PORTD3);
//    if(direct == CW)
//      Steps++;
//    else
//      Steps--;
    break;
    
    case 4://pin 11
    PORTB |= (1<<PORTB3); //enable 11
    PORTB &= ~(1<<PORTB5); //disable pins 10, 9, 12
    PORTB &= ~(1<<PORTB4);
    PORTD &= ~(1<<PORTD3);
    break;

    case 5://pin 11 & 12
    PORTB |= (1<<PORTB3);
    PORTD |= (1<<PORTD3); //enable 11 & 12
    PORTB &= ~(1<<PORTB5); //disable pins 10, 9
    PORTB &= ~(1<<PORTB4);
    break;
    
    case 6://pin 12
    PORTD |= (1<<PORTD3); //enable 12
    PORTB &= ~(1<<PORTB5); //disable pins 10, 11, 9
    PORTB &= ~(1<<PORTB4);
    PORTB &= ~(1<<PORTB3);
    break;

    case 7://pin 12 & 9
    PORTD |= (1<<PORTD3);
    PORTB |= (1<<PORTB5); //enable 12 & 9
    PORTB &= ~(1<<PORTB4); //disable pins 10, 11
    PORTB &= ~(1<<PORTB3);
    break;
  }

  //if direction is counter clock wise
  if(direct == false)
  {
    Steps++;
    if(Steps > 7)
      Steps = 0;
  }
  
  //if direction is clock wise
  if(direct == true)
  {
    Steps--;
    if(Steps <= 0 || Steps > 7)
      Steps = 7;
  }
}



int main(void)
{
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();
  TimerSetup();
  DelaySetup();
  StepperSetup();
  ADCSetup();

  
  sei(); // enable interrupts

  //PORTB = (1<<DDB5);
  //MAIN LOOP
  DelayMilli(100);
  while (1)
  {
    if(menuSelect != menuCheck)
    {
      menuCheck = menuSelect;
      lcd.clear();
    }
    
    Display(menuSelect);
  }
}

void ReadUserButton(void)
{
  //read the user input
  if(adc_input != MyAnalogRead(0))
  {
    adc_input = MyAnalogRead(0);
//    Serial.print("ADC = ");
//    Serial.println(MyAnalogRead(0));
  }
}

void CheckUserButton(void)
{
  if(adc_input <= 50) //Right  = 0
  {
    Serial.println("Right");
    Button = RIGHT;
    DelayMilli(200); //debounce
  }
  else if(600 <= adc_input && adc_input <= 650) //Left   = 626 or 625
  {
    Serial.println("Left");
    Button = LEFT;
    DelayMilli(200); //debounce
  }
  else if(150 <= adc_input && adc_input <= 350) //Up = 208
  {
    Serial.println("Up");
    Button = UP;
    DelayMilli(200); //debounce
  }
  else if(350 <= adc_input && adc_input <= 450)//Down = 410 or 409
  {
    Serial.println("Down");
    Button = DOWN;
    DelayMilli(200); //debounce
  }
  else if(750 <= adc_input && adc_input <= 900)//Select = 824
  {
    Serial.println("Select");
    Button = SELECT;
    DelayMilli(250); //debounce
  }
  else
  {
    //Serial.println("Nothing");
    Button = NOTHING;
    DelayMilli(150);
  }
}





/*
 * to display everything for LCD
 */
void Display(uint8_t mode)
{
  menuSelect = mode;
  
  switch(menuSelect)
  {
    case STARTUP: //startup display
      //if LEFT, LEFT, UP, RIGHT, SELECT is pressed, go to "DEBUG mode"
      //ignore any incorrect sequence
      while(menuSelect == STARTUP)
      {
        STARTUP_Display(); //in min:sec format, left most corner, first line
        //display student ID on the second line
        CheckUserButton();
        if(Button != NOTHING)
        {
          //Serial.println("ERROR: menuSelect");
          STARTUP_CheckDebugCombo();
          DelayMilli(50);
        }
        
        if(Button == SELECT && debugCheck == true)
        {
          //Serial.println("ERROR: menuSelect");
          debugCheck = false;
          menuSelect = DEBUG;
          break;
        }
        //at this point, if select is pressed the system should go into "DRIVE mode"
        else if(Button == SELECT && debugCheck == false)
        {
          menuSelect = DRIVE;
          break;
        }
      }
      break;
      
    case DEBUG:
      DEBUG_Display();
      while(menuSelect == DEBUG)
      {
        CheckUserButton(); //Store user button to "Button" as enum
        if(Button == SELECT && DebugUserPos <= 4) //check if user selected an option in DEBUG mode
        {
          switch(DebugUserPos)
          {
            case 0: //IR
              DebugUserPos = 0;
              menuSelect = IR;
              break;

            case 1: //CM
              DebugUserPos = 0;
              menuSelect = CM;
              break;

            case 2: //PM
              DebugUserPos = 0;
              menuSelect = PM;
              break;

            case 3: //SET
              DebugUserPos = 0;
              menuSelect = SET;
              break;

            case 4: //E (exit, go back to STARTUP)
              DebugUserPos = 0;
              menuSelect = STARTUP;
              break;

            default: //if there was an error
              menuSelect = DEBUG;
              break;
          }
        }
        
        if(Button == LEFT || Button == RIGHT)
        {
          DEBUG_UserInput();
        }
        if((secs % 2) == 0)
        {
          DEBUG_DisplayUserInput();
        }
        if((secs % 2) == 1)
        {
          DEBUG_Display(); //Display "DEBUG Mode" on LCD
        }
        
      }
      break;
      
    case IR:
      IR_Value = IR_Read();
      IR_Display(); //display "IR Mode"
      IR_Start = true;
      while(menuSelect == IR)
      {
        ReadUserButton();
        CheckUserButton(); //Store user button to "Button" as enum
        
        //read the value twice, if they match, it is accurate
        
        cli();//disable interupt
        Tmp_IR1 = IR_Read();
        sei(); //enable interrupt
        DelayMilli(10);
        cli();//disable interupt
        Tmp_IR2 = IR_Read();
        sei(); //enable interrupt

        //read two different readings in the IR sensor
        //check if they match, otherwise it is an error
        if(Tmp_IR1 == Tmp_IR2)
        {
          //the change of the IR_Value should be greater than 1
          //this is to stop the flickering due to disturbances
          if((IR_Value - Tmp_IR1) > 1 || (Tmp_IR1 - IR_Value) > 1)
          {
            IR_Value = Tmp_IR1;
          }
        }
        
        IR_Distance();
        DelayMilli(50);
          
//        Serial.print("IR_Value = ");
//        Serial.println(IR_Value);
        
        if(Button == SELECT)
        {
          menuSelect = DEBUG;
          IR_Start = false;
        }
      }
      break;
      
    case CM:
      CM_Display(); //Display "CM Mode" on LCD
      while(menuSelect == CM)
      {
        CheckUserButton(); //Store user button to "Button" as enum
        if(Button == LEFT || Button == RIGHT) //to move users cursor
        {
          CM_UserInput();
        }
        if((secs % 2) == 0) //blink user cursor
        {
          CM_DisplayUserInput();
        }
        if((secs % 2) == 1) //blink display
        {
          CM_Display(); //Display "CM Mode" on LCD
        }
        
        if(Button == SELECT && CMUserPos < 2) //for user selection
        {
          switch(CMUserPos)
          {
            case 0: //Start
              CMUserPos = 0;
              CM_StartMode(); //run motor & display CW or CCW
              Motor_Clear(); //clear output to the motor
              break;
            case 1: //exit
              CMUserPos = 0;
              menuSelect = DEBUG;
              break;
          }
        }
      } //end whileloop
      break;
      
    case PM:
      while(menuSelect == PM)
      {
        CheckUserButton(); //Store user button to "Button" as enum
        PM_Display(); //display "PM Mode on the first line"
        PM_StepDisplay(); //display e.g "Step:100"

        if(Button == UP && PM_Tmp < 60000) //increase number of steps
        {
          PM_Step = PM_Tmp;
          PM_Step += 100;
          PM_Tmp = PM_Step;
        }
        else if(Button == DOWN && PM_Tmp > 100) //decrease number of steps
        {
          PM_Step = PM_Tmp;
          PM_Step -= 100;
          PM_Tmp = PM_Step;
        }
        else if(Button == LEFT)
        {
          PM_Step = 100;
          PM_Tmp = PM_Step;
        }
        else if(Button == RIGHT)
        {
          PM_StartMode(); //runs the motor with the set number of steps
          Motor_Clear(); //clear output to the motor
        }
        else if(Button == SELECT)
        {
          menuSelect = DEBUG;
        }
      }
      break;

    case SET:
      while(menuSelect == SET)
      {
        CheckUserButton(); //Store user button to "Button" as enum
        SET_Display();
        SET_SizeDisplay();
        
        if(Button == UP)
          SET_Size += 10;
        else if(Button == DOWN)
          SET_Size -= 10;
        else if(Button == SELECT)
          menuSelect = DEBUG;
        if(SET_Size >= 100)
          SET_Size = 90;
        else if(SET_Size <= 9)
          SET_Size = 10;
      }
      break;

    case DRIVE:
      DRIVE_Display(); //display "Drive Mode"
      IR_Start = true; //start IR sensor
      IR_Value = IR_Read();
      while(menuSelect == DRIVE)
      {
        CheckUserButton(); //Store user button to "Button" as enum

        //read the value twice, if they match, it is accurate
        Tmp_IR1 = IR_Read();
        DelayMilli(10);
        Tmp_IR2 = IR_Read();
        
        if(Tmp_IR1 == Tmp_IR2)
        {
          if((IR_Value - Tmp_IR1) > 1 || (Tmp_IR1 - IR_Value) > 1)
          {
            IR_Value = Tmp_IR1;
          }
        }
        
        DRIVE_SensorDisplay();//display and update dist, rev, steps.

        if(Button == UP)
        {
//          Serial.println("ERROR: check 1");
          Drive_Direct = CW;
          DRIVE_Motor();
        }
        else if(Button == DOWN)
        {
//          Serial.println("ERROR: check 2");
          Drive_Direct = CCW;
          DRIVE_Motor();
        }
        else if(Button == SELECT)
        {
          menuSelect = STARTUP;
          IR_Start = false;
        }
      }
      break;
  }
}


void STARTUP_Display(void)
{
  //display time
  lcd.setCursor(0,0);
  if(mins < 10)
  {
    lcd.print("0");
  }
  lcd.print(mins);
  
  lcd.print(":");
  
  if(secs < 10)
  {
    lcd.print("0");
  }
  lcd.print(secs);
  
  lcd.setCursor(0,1);
  lcd.print("ID:12935084");
}

/*
 * checks if combo buttons are pressed for DEBUG Mode
 */
void STARTUP_CheckDebugCombo(void)
{
  if(Button == LEFT && ButtonIndex == 0)
  {
    Serial.println("Combo: 1");
    Serial.print("ButtonIndex = ");
    Serial.println(ButtonIndex);
    ButtonHistory[ButtonIndex] = Button;
    ButtonIndex++;
  }
  else if(Button == LEFT && ButtonIndex == 1)
  {
    Serial.println("Combo: 2");
    Serial.print("ButtonIndex = ");
    Serial.println(ButtonIndex);
    ButtonHistory[ButtonIndex] = Button;
    ButtonIndex++;
  }
  else if(Button == UP && ButtonIndex == 2)
  {
    Serial.println("Combo: 3");
    Serial.print("ButtonIndex = ");
    Serial.println(ButtonIndex);
    ButtonHistory[ButtonIndex] = Button;
    ButtonIndex++;
  }
  else if(Button == RIGHT && ButtonIndex == 3)
  {
    Serial.println("Combo: 4");
    Serial.print("ButtonIndex = ");
    Serial.println(ButtonIndex);
    ButtonHistory[ButtonIndex] = Button;
    ButtonIndex++;
    debugCheck = true;
    ButtonIndex = 0;
  }
  else
    ButtonIndex = 0;
}





void DEBUG_Display(void)
{
  lcd.setCursor(0,0);
  lcd.print("DEBUG Mode");
  lcd.setCursor(0,1);
  lcd.print("IR CM PM SET E");
}

void DEBUG_DisplayUserInput(void)
{
  switch(DebugUserPos)
  {
    case 0: //IR
      lcd.setCursor(0, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;
      
    case 1: //CM
      lcd.setCursor(3, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;

    case 2: //PM
      lcd.setCursor(6, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;
      

    case 3: //SET
      lcd.setCursor(9, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;

    case 4: //E
      lcd.setCursor(13, 1); //set cursor on user position
      lcd.print(USERPOS);
      break;
  } 
}

void DEBUG_UserInput(void)
{
  if(Button == LEFT && DebugUserPos != 0)
  {
    DebugUserPos--;
  }
  else if(Button == RIGHT && DebugUserPos < 4)
  {
    DebugUserPos++;
  }
}

void IR_Display(void)
{
  lcd.setCursor(0,0);
  lcd.print("IR Mode");
}

void IR_Distance(void)
{
  lcd.setCursor(0,1);
  lcd.print(IR_Value);
  lcd.print("cm ");
}


uint16_t IR_Read(void)
{
  unsigned int sum = 0;
  float tmp;
  
  MyAnalogRead(5); //dummy read

  for(int i=0; i<10; i++) //buffer 5 samples
  {
    sum += MyAnalogRead(5); //add all the values
  }

  tmp = ((float)sum/10.0) * 0.004887; //convert to voltage
  return 55.9/tmp; //convert to cm
}

void CM_Display(void)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CM Mode");
  lcd.setCursor(0,1);
  lcd.print("Start Exit");
}

void CM_DisplayUserInput(void)
{
  switch(CMUserPos)
  {
    case 0: //Start
      lcd.setCursor(0, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;
      
    case 1: //Exit
      lcd.setCursor(6, 1); //set cursor on user position
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      lcd.print(USERPOS);
      break;
  } 
}

void CM_UserInput(void)
{
  if(Button == LEFT && CMUserPos != 0)
  {
    CMUserPos--;
  }
  else if(Button == RIGHT && CMUserPos < 1)
  {
    CMUserPos++;
  }
}

void CM_StartMode(void)
{
  M_Speed = 3; //mid speed of the motor
  Serial.println(M_Speed);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CM Mode");
  lcd.setCursor(0,1);
  lcd.print("CW  ");
  lcd.print("spd:Mid "); //initial speed of the motor
  M_Start = true; //set motor start to true
  
  while(1)
  {
    CheckUserButton(); //Store user button to "Button" as enum

    if(Button == UP && M_Speed > 0)
    {
      M_Speed -= 3; //increase speed      
//      Serial.print("M_Speed: ");
//      Serial.println(M_Speed);
      lcd.setCursor(4, 1);
      lcd.print("spd:");
      if(M_Speed == 0)
        lcd.print("Fast");
      else
        lcd.print("Mid ");
    }
    else if (Button == DOWN && M_Speed < 6)
    {
      M_Speed += 3; //decrease speed      
//      Serial.print("M_Speed: ");
//      Serial.println(M_Speed);
      lcd.setCursor(4, 1);
      lcd.print("spd:");
      if(M_Speed == 6)
        lcd.print("Slow");
      else
        lcd.print("Mid ");
    }
    else if(Button == LEFT)
    {
      M_Start = false; //disable output to motor (stops it from stalling)
      DelayMilli(100);
      CM_Dir = CCW;
      lcd.setCursor(0,1);
      lcd.print("CCW ");
      M_Start = true; //enable output to motor
      DelayMilli(100);
    }
    else if(Button == RIGHT)
    {
      M_Start = false; //disable output to motor (stops it from stalling)
      DelayMilli(100);
      CM_Dir = CW;
      lcd.setCursor(0,1);
      lcd.print("CW  ");
      M_Start = true; //enable output to motor
      DelayMilli(100);
    }
    else if (Button == SELECT)
    {
      M_Start = false;
      return;
    }
    //Serial.println("ERROR: CHECK2");
    if(M_Speed < 0)
    {
      M_Speed = 0;
    }
    else if(M_Speed > 6)
    {
      M_Speed = 6;
    }
  }
}


void PM_Display(void)
{
  lcd.setCursor(0,0);
  lcd.print("PM Mode");
}

void PM_StepDisplay(void)
{
  lcd.setCursor(0,1);
  

  lcd.print(PM_Tmp);
  lcd.print(" ");
  lcd.print(PM_Step);
  lcd.print("         ");
  
}

void PM_StartMode(void)
{
  
//  M_Speed = 1;
  PM_Start = true;
  while(PM_Step > 1)
  {
    PM_StepDisplay(); //display number of steps left
  }
  PM_Start = false;
  PM_Step = 0;
}



void SET_Display(void)
{
  lcd.setCursor(0,0);
  lcd.print("SETTING Mode");
}

void SET_SizeDisplay(void)
{
  lcd.setCursor(0,1);
  lcd.print("Size:");
  lcd.print(SET_Size);
  lcd.print(" ");
}


void DRIVE_Display(void)
{
  lcd.setCursor(0,0);
  lcd.print("Drive Mode");
}

void DRIVE_SensorDisplay(void)
{
  float tmp;
  uint16_t unit, dp1, dp2;
  //IR Sensor distance in cm
  lcd.setCursor(0,1);
  lcd.print(IR_Value);
  lcd.print("cm ");

  // distance/circumference = rev
  tmp = ((float)IR_Value/(float)SET_Size); 

  unit = tmp;
  dp1 = (tmp*10) - (unit*10);
  dp2 = (tmp*100) - (unit*100 + tmp*10);
  if(dp2 >=5 && dp1 != 9)
  {
    dp1++;
  }
  else if(dp2 >=5 && dp1 == 9)
  {
    unit++;
    dp1 = 0;
  }
    
  lcd.print(unit);
  lcd.print(".");
  lcd.print(dp1);
  lcd.print(" ");

  Drive_Step = tmp * 4096;
  lcd.print(Drive_Step);
  lcd.print("  ");
}


void DRIVE_Motor(void)
{
  Drive_Start = true;
  while(Drive_Step > 0)
  {
    DRIVE_LCD();
  }
  Motor_Clear();
  Drive_Start = false;
  DelayMilli(100);
}

void DRIVE_LCD(void)
{
  lcd.setCursor(9,1);
  lcd.print(Drive_Step);
  lcd.print("  ");
}

void ADCSetup(void)
{
  ADCSRA = (1<<ADEN); // Enable ADC
  ADMUX |= (1<<REFS0); // Internal Vcc 5v
}

int MyAnalogRead(uint8_t pin) 
{
  if(pin == 0)
  {
    ADMUX = 0; //Multiplexer  for which pin to read from
    ADMUX |= (1<<REFS0); // Internal Vcc 5v
  }
  else
  {
    ADMUX = pin;
  }

  // start conversion
  ADCSRA |= (1<<ADSC);
  

  // wait for conversion
  while (!(ADCSRA &(1<<ADIF)));
  
  ADCSRA |= (1<<ADIF);
  return ADC;
}

//interrupt every 1ms
ISR (TIMER2_COMPA_vect)
{
  static uint8_t counter2 = 0;
  static uint8_t counter1 = 0;
  counter2++;
  counter1++;
  if(M_Start == true && counter2 >= M_Speed)
  {
    counter2 = 0; //reset counter
    MOTOR_OneStep(CM_Dir); //one step
  }

  if(PM_Start == true)
  {
    PM_Step -= 1; //decrease step value
    MOTOR_OneStep(CW);
  }

  if(Drive_Start == true)
  {
    Drive_Step--;
    MOTOR_OneStep(Drive_Direct);
  }

  if(counter1 >= 75) //debounce
  {
    ReadUserButton(); //Read user button value
    counter1 = 0;
  }

  Delay++; //increment every 2ms
}

/*
 * Interrupts every 1 sec to update time;
 */
ISR(TIMER1_COMPA_vect)
{
    // action to be done every 1 sec
    totalsec++;
    secs = (totalsec % 3600) % 60;
    mins = (totalsec % 3600) / 60;
}
