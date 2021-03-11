#include <LiquidCrystal.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

//Author: Jeong Bin Lee
//SID: 12935084
//Date: 6/10/2020



/**** C++ ****/
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


/*** ENUM ***/

enum MENU //used for main menu
{
  MAIN,
  CONTROL,
  SWEEP,
  WALL,
  NAV
};

enum BUTTONS //reads users inputs
{
  NOTHING,
  RIGHT,
  UP,
  DOWN,
  LEFT,
  SELECT
};

enum ORIGIN //orientation of the rover
{
  OLEFT,
  OFLEFT,
  OFRONT,
  OFRIGHT,
  ORIGHT,
  OBRIGHT,
  OBACK,
  OBLEFT,
};



/*** GLOBAL VARIABLES ***/
volatile unsigned long int Delay = 0;

uint8_t Main_Select = MAIN;
uint8_t Menu_Select = MAIN;

uint16_t Button;

int Adc_Read;

float IR_Read;
int32_t pos = 0;

bool Move_ISR  = false;
uint32_t MoveDelay_ISR = 750; //default delay for continuous movement
float Move_Speed = 0.2; //default speed


ISR(TIMER2_OVF_vect)
{
  static uint32_t counter = 0;
  static uint32_t counter2 = 0;
  Delay++; //Global delay counter (1ms)


  //WALL moving forward
  counter++;
  if(Move_ISR  == true && counter >= MoveDelay_ISR)
  {
    counter = 0; //reset counter
    Serial.print("CMD_ACT_LAT_1_" + (String)(Move_Speed));
    Serial.write(13); //carriage return character (ASCII 13, or '\r')
    Serial.write(10); //newline character (ASCII 10, or '\n')
  }
  
  //reading buttons
  counter2++;
  if(counter2 >= 100)
  {
    //TEST//
    ReadUserButton();
    counter2 = 0;
  }
}

//to initialise delay
void DelayInit() {
  TCCR2A = 0;
  // Prescaler for 64
  TCCR2B &= ~(1<<CS20);
  TCCR2B &= ~(1<<CS21);
  TCCR2B |= (1<<CS22);
  TIMSK2 |= (1<<TOIE2); //Enable Overflow interrupt
  TIFR2 |= (1<<TOV2);
}

//to get global delay variable
volatile unsigned long int getDelay() {
  return Delay;
}

//my delay fuction
void DelayMilli(uint64_t delayTime) {
  uint64_t count = getDelay();
  while(getDelay() <= (delayTime + count)) {
  }
}

//ADC setup
void ADCSetup(void)
{
  ADCSRA = (1<<ADEN); // Enable ADC
  ADMUX |= (1<<REFS0); // Internal Vcc 5v
}

//read buttons from LCD shield
void ReadUserButton(void)
{
  //read the user input
  if(Adc_Read != ADCsingleREAD(0))
  {
    Adc_Read = ADCsingleREAD(0);
  }
}

//check what the user pressed
void CheckUserButton(void)
{
  if(Adc_Read <= 50) //Right  = 0
  {
    Serial.println("Right");
    Button = RIGHT;
    DelayMilli(200); //debounce
  }
  else if(600 <= Adc_Read && Adc_Read <= 650) //Left   = 626 or 625
  {
    Serial.println("Left");
    Button = LEFT;
    DelayMilli(200); //debounce
  }
  else if(150 <= Adc_Read && Adc_Read <= 350) //Up = 208
  {
    Serial.println("Up");
    Button = UP;
    DelayMilli(200); //debounce
  }
  else if(350 <= Adc_Read && Adc_Read <= 450)//Down = 410 or 409
  {
    Serial.println("Down");
    Button = DOWN;
    DelayMilli(200); //debounce
  }
  else if(750 <= Adc_Read && Adc_Read <= 900)//Select = 824
  {
    Serial.println("Select");
    Button = SELECT;
    DelayMilli(250); //debounce
  }
  else
  {
//    Serial.println("Nothing");
    Button = NOTHING;
//    DelayMilli(150);
  }
}


//read ADC pin once
int ADCsingleREAD(uint8_t pin) 
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
  ADCSRA |= (1<<ADSC); // start conversion
  

  // wait for conversion to complete
  while (!(ADCSRA &(1<<ADIF)));
  
  ADCSRA |= (1<<ADIF);
  return ADC;
}



void setup(void)
{
  Serial.begin(9600);
  Serial.setTimeout(300); //set timeout to 300ms
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("12935084");
  
//  TimerSetup();
  DelayInit();
  ADCSetup();

  PrintMessage("CMD_START"); // Start the robot
  
  sei(); // enable interrupts
}

void loop()
{
  //MAIN LOOP  
  DelayMilli(100);
  while (1)
  {
    Display(Main_Select);
  }
}


//sweep 360 deg by 5deg increment, and face the shortest distance
void Sweep(void)
{
  String IRValue;
  
  IR_Read = 100;
  pos = -1;
  
  for(int i=360; i>=0; i-=5)
  {
    String mess = ("CMD_SEN_ROT_" + (String)i);
    PrintMessage(mess); //Rotate CW by 1deg
    DelayMilli(100);
    PrintMessage("CMD_SEN_IR"); // Query the sensor reading
    DelayMilli(100);
    IRValue = Serial.readString(); // Read the incoming value

    if((IRValue.equals("NaN\r\n") == true) || (IRValue.toFloat() == 0))
    {
      
    }
    else if(IR_Read > IRValue.toFloat())
    {
      IR_Read = IRValue.toFloat();
      pos = i;
    }
  }
  
  //turn CCW to face the wall
  if(pos > 0)
  {
    for(int j=pos; j>=0; j-=5)
    {
      PrintMessage("CMD_ACT_ROT_0_5"); //Rotate CCW by 1deg
      DelayMilli(100);
    }
  }
  pos=-1;
}


//check IR sensor by 45deg increments using ENUM
//see ORIGIN
String CheckIR(int origin)
{
  switch(origin)
  {
    case OLEFT:
    {
      PrintMessage("CMD_SEN_ROT_90"); //look 90 left
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OFLEFT:
    {
      PrintMessage("CMD_SEN_ROT_45"); //look 45 left
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OFRONT:
    {
      PrintMessage("CMD_SEN_ROT_0"); //look 0 front
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OFRIGHT:
    {
      PrintMessage("CMD_SEN_ROT_315"); //look 45 right
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case ORIGHT:
    {
      PrintMessage("CMD_SEN_ROT_270"); //look 90 right
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OBRIGHT:
    {
      PrintMessage("CMD_SEN_ROT_225"); //look 135 right
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OBACK:
    {
      PrintMessage("CMD_SEN_ROT_180"); //look 0 back
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
    case OBLEFT:
    {
      PrintMessage("CMD_SEN_ROT_135"); //look 135 left
      PrintMessage("CMD_SEN_IR"); // Query the sensor reading
      return Serial.readString(); // Read the incoming value
    }
  }
}


//send a message from arduino to computer using Serial
void PrintMessage(String message)
{
 Serial.print(message);
 Serial.write(13); //carriage return character (ASCII 13, or '\r')
 Serial.write(10); //newline character (ASCII 10, or '\n')
 DelayMilli(200);
}

//check sensor with int angle as input
float CheckSensor(int angle)
{
  String tmpStr;
  float tmpFlo = 0;

  PrintMessage("CMD_SEN_ROT_" + (String)angle);
  PrintMessage("CMD_SEN_IR"); //check the distance
  tmpStr = Serial.readString();
  if(tmpStr.equals("NaN\r\n") == true)
  {
    return 6;
  }
  return tmpStr.toFloat();;
}


//main part of the code where everything comes together
void Display(uint8_t mode)
{
  switch(Main_Select)
  {
    case MAIN: //main mode
      while(Main_Select == MAIN)
      {
        CheckUserButton();
        
        if(Button == DOWN)
        {
          Menu_Select++;
          if(Menu_Select > NAV)
          {
            Menu_Select = MAIN;
          }
        }
        else if(Button == SELECT)
        {
          Main_Select = Menu_Select;
          Menu_Select = 0;
        }
        
        switch(Menu_Select)
        {
          case MAIN:
            lcd.setCursor(0, 1);
            lcd.print("Main menu       ");
            break;
          case CONTROL:
            lcd.setCursor(0, 1);
            lcd.print("Control         ");
            break;
          case SWEEP:
            lcd.setCursor(0, 1);
            lcd.print("Sweep           ");
            break;
          case WALL:
            lcd.setCursor(0, 1);
            DelayMilli(100);
            lcd.print("Wall follow   ");
            break;
          case NAV:
            lcd.setCursor(0, 1);
            lcd.print("Navigation      ");
            break;
        }
      }
      break;
    case CONTROL: //control mode
      lcd.setCursor(0, 1);
      lcd.print("Control         ");
      while(Main_Select == CONTROL)
      {
        CheckUserButton();
        
        if(Button == SELECT) //Exit control
        {
          Main_Select = MAIN;
        }
        else if(Button == LEFT) //rotate left
        {
          PrintMessage("CMD_ACT_ROT_0_15"); // Rotate A-Clockwise 15Deg
        }
        else if(Button == RIGHT)//rotate right
        {
          PrintMessage("CMD_ACT_ROT_1_15"); // Rotate Clockwise 15Deg
        }
        else if(Button == UP) //Move forward
        {
          PrintMessage("CMD_ACT_LAT_1_1"); // Move Foward 1m
          DelayMilli(300);
        }
        else if(Button == DOWN) //Move backwards
        {
          PrintMessage("CMD_ACT_LAT_0_1"); // Move Foward 1m
          DelayMilli(300);
        }
      }
      break;
      
    case SWEEP: //sweep mode
      lcd.setCursor(0, 1);
      lcd.print("Sweep           ");
      while(Main_Select == SWEEP)
      {
        CheckUserButton();
        
        if(Button == SELECT) //Exit control
        {
          Main_Select = MAIN;
        }
        else if (Button == UP)
        {
          Sweep(); //sweep with +-10deg accuracy
        }
      }
      break;
      
    case WALL: //wall follow mode
    {
      lcd.setCursor(0, 1);
      lcd.print("Wall follow    ");
      String WallValue;
      float WallValue1, WallValue3, error;
      String front, left;
      float left1;
      int CWcount = 0;
      int CCWcount = 0;
      bool turnFront = false;
      MoveDelay_ISR = 750;

      //initial sweep to make sure a wall is near
      Sweep(); //sweep with +-5deg accuracy
      WallValue = CheckIR(OFRONT);
      CheckUserButton();
      if(WallValue.equals("NaN\r\n") == true)
      {
        while(WallValue.equals("NaN\r\n") == true)
        {
          WallValue = CheckIR(OFRONT);
          PrintMessage("CMD_ACT_LAT_1_3"); //move forward by 3m
        }
        Sweep(); //sweep with 5deg accuracy
      }
      else if(Button == SELECT)
      {
        Main_Select = MAIN;
        Move_ISR  = false;
      }

      //check the front distance from the wall
      //&& move that distance accordingly >2m or <2m 
      WallValue = CheckIR(OFRONT);
      CheckUserButton();
      if(WallValue.toFloat() > 2) //if distance from wall is > 2
      {
        PrintMessage("CMD_ACT_LAT_1_" + (String)(WallValue.toFloat() - 2)); //move up 0.1m closer until 2m is achieved
      }
      else if(WallValue.toFloat() < 2 && WallValue.toFloat() > 0) //if distance from wall is < 2
      {
        PrintMessage("CMD_ACT_LAT_0_" + (String)(2 - WallValue.toFloat())); //move up 0.1m closer until 2m is achieved
      }
      else if(WallValue.equals("NaN\r\n") == true)
      {
        PrintMessage("CMD_ACT_LAT_1_2"); //move forward by 2m
        WallValue = CheckIR(OFRONT);
        CheckUserButton();
        if(WallValue.toFloat() > 2) //if distance from wall is > 2
        {
          PrintMessage("CMD_ACT_LAT_1_" + (String)(WallValue.toFloat() - 2)); //move up 0.1m closer until 2m is achieved
        }
        else if(WallValue.toFloat() < 2 && WallValue.toFloat() > 0) //if distance from wall is < 2
        {
          PrintMessage("CMD_ACT_LAT_0_" + (String)(2 - WallValue.toFloat())); //move up 0.1m closer until 2m is achieved
        }
        else if (Button == SELECT)
        {
          Main_Select = MAIN;
          Move_ISR  = false;
        }
      }
      else if (Button == SELECT)
      {
        Main_Select = MAIN;
        Move_ISR  = false;
      }
      PrintMessage("CMD_ACT_ROT_1_90");
    
      //find perpendicular to the wall by scanning 75deg and 105deg
      do
      {
        WallValue1 = CheckSensor(75);
    
        WallValue3 = CheckSensor(105);
        CheckUserButton();
        if(WallValue1 > WallValue3)
        {
          PrintMessage("CMD_ACT_ROT_0_1");
          error = WallValue1 - WallValue3;
        }
        else if(WallValue3 > WallValue1)
        {
          PrintMessage("CMD_ACT_ROT_1_1");
          error = WallValue3 - WallValue1;
        }
        else if(Button == SELECT)
        {
          Main_Select = MAIN;
          Move_ISR  = false;
        }
      } while(error > 0.04);

      //the main wall follow, by scanning 30deg to the left
      //maintain the distance 2m (because 1/cos(30) = 2m
      //the wall follow moves forward using interrupt.
      //SEE ISR function
      while(Main_Select == WALL)
      {
        Move_Speed = 0.2;
        Move_ISR  = true;
        
        left1 = CheckSensor(30);//left.toFloat();
        CheckUserButton();
        if(left1 < 4 && left1 > 0)
        {
          //PrintMessage("CMD_ACT_LAT_1_0.1");
          //rotate CW by 0.5deg
          PrintMessage("CMD_ACT_ROT_1_0.5");
          turnFront = true;
          CCWcount--;
          CWcount++;
        }
        else if(left1 > 4 && left1 < 6)
        {
          //rotate CCW
          PrintMessage("CMD_ACT_ROT_0_0.5");
          turnFront = true;
          CCWcount++;
          CWcount--;
        }
        else if(left1 <= 4.1 && left1 >= 3.9)
        {
          CheckUserButton();
          if(CCWcount > 0)
          {
            PrintMessage("CMD_ACT_ROT_1_" + (String)(CCWcount*0.3));
            CCWcount = 0;
          }
          else if(CWcount > 0)
          {
            PrintMessage("CMD_ACT_ROT_1_" + (String)(CWcount*0.3));
            CWcount = 0;
          }
          else if(Button == SELECT)
          {
            Main_Select = MAIN;
            Move_ISR  = false;
          }
        }
        else if(left1 == 0) //NaN
        {
          PrintMessage("CMD_ACT_ROT_0_5"); //rotate 5deg CCW
          left1 = CheckSensor(30);
          while(left1 == 0)
          {
            PrintMessage("CMD_ACT_LAT_1_0.2"); //move 0.2m
            DelayMilli(700);
          }
          PrintMessage("CMD_ACT_ROT_1_3"); //rotate 3deg CW
        }
        else if(Button == SELECT)
        {
          Main_Select = MAIN;
          Move_ISR  = false;
        }

        CheckUserButton();
        left1 = CheckSensor(30);
        
        if(left1 < 2.5 && left1 > 0)
        {
          front = CheckIR(OFRONT);
          if(front.toFloat() < 3 && front.toFloat() > 0 && front.equals("NaN\r\n") == false)
          {
            if(front.toFloat() > 2) //if distance from wall is > 2
            {
              PrintMessage("CMD_ACT_LAT_1_" + (String)(front.toFloat() - 2)); //move up 0.1m closer until 2m is achieved
              PrintMessage("CMD_ACT_ROT_1_90");
              turnFront = false;
            }
            else if(front.toFloat() < 2) //if distance from wall is < 2
            {
              PrintMessage("CMD_ACT_LAT_0_" + (String)(2 - front.toFloat())); //move up 0.1m closer until 2m is achieved
              PrintMessage("CMD_ACT_ROT_1_90");
              turnFront = false;
            }
          }
        }
        else if(left1 < 1 && left1 > 0)
        {
          front = CheckIR(OFRONT);
          if(front.toFloat() <= 1.5 && front.toFloat() > 0 && front.equals("NaN\r\n") == false)
          {
            PrintMessage("CMD_ACT_ROT_1_90");
          }
        }
        else if(Button == SELECT)
        {
          Main_Select = MAIN;
          Move_ISR  = false;
        }
      }
      break;
    }
    
    case NAV:
      lcd.setCursor(0, 1);
      lcd.print("Navigation      ");
      String goal1, goal2, goal3;
      String frontStr, tmpStr;
      float front,left,right;
      float angle;
      bool goal = false;

      //move forward using interrupt
      //check left 30deg, front 0 deg, right 30deg
      //change angle accordingly (so that the rover doesnt crash into wall)
      //if the goal is found use arccos rule to find the angle at which the goal is at.
      while(Main_Select == NAV)
      {
        CheckUserButton();
        PrintMessage("CMD_SEN_PING");
        DelayMilli(200);
        goal1 = Serial.readString();

        if(goal1.toFloat() <= 3 && goal1.toFloat() > 0)
        {
          Move_ISR = false;
          frontStr = CheckIR(OFRONT);
          if(frontStr.toFloat() > 0.7 && frontStr.equals("NaN\r\n"))
          {
            PrintMessage("CMD_ACT_LAT_1_0.5");//move 0.5m forward (acutally 0.52)
            DelayMilli(200);
          }
          
//          do
//          {
            PrintMessage("CMD_SEN_PING");
            DelayMilli(200);
            goal2 = Serial.readString();
  
//            PrintMessage("CMD_SEN_PING");
//            DelayMilli(200);
//            tmpStr = Serial.readString();
//          } while(goal2.equals(tmpStr) == false);
        
          DelayMilli(200);
          if(goal2.toFloat() == 0)
          {
            PrintMessage("CMD_SEN_PING");
            goal2 = Serial.readString();
            DelayMilli(200);
            PrintMessage("CMD_ACT_LAT_0_0.5"); //go back
            DelayMilli(200);
            PrintMessage("CMD_ACT_ROT_1_45"); //change angle by 45deg
          }
          else if(goal2.toFloat() > 0 && goal2.toFloat() <= 5) //goal2 is valid
          {
            float tmp1, tmp2;
            
            //find the angle
            angle = (180 - ((180.0/PI)*acos((pow(goal2.toFloat(),2) + pow(0.52,2) - pow(goal1.toFloat(),2))/(2*0.52*goal2.toFloat()))));

            String tmpAngStr = "" + (String)angle;

            //if angle is error
            while(tmpAngStr.indexOf("NAN") > 0 || tmpAngStr.indexOf("nan") > 0)
            {
              float tmp = (goal2.toFloat() + 0.1);
              angle = (((180.0/PI)*acos((pow(tmp,2) + pow(0.52,2) - pow(goal1.toFloat(),2))/(2*0.52*tmp))));
              tmpAngStr = "" + (String)angle;
            }

            angle = 180 - angle;

            PrintMessage("CMD_ACT_ROT_1_" + (String)(angle));
            frontStr = CheckIR(OFRONT);
            if(frontStr.toFloat() > 0.5 && frontStr.equals("NaN\r\n"))
            {
              PrintMessage("CMD_ACT_LAT_1_0.1");
            }
            DelayMilli(200);

//attemp to scan twice (to account for error read)
//            do
//            {
              PrintMessage("CMD_SEN_PING");
              DelayMilli(200);
              goal3 = Serial.readString();
//              PrintMessage("CMD_SEN_PING");
//              DelayMilli(200);
//              tmpStr = Serial.readString();
//            } while(goal3.equals(tmpStr) == false);
            
            if(goal3.toFloat() < goal2.toFloat() && goal3.toFloat() != 0)
            {
              frontStr = CheckIR(OFRONT);
              if(frontStr.toFloat() < goal2.toFloat()) //check if theres something in the way
              {
              }
              else
              {
                //move toward the goal
                PrintMessage("CMD_ACT_LAT_1_" + (String)(goal2.toFloat() - 0.2));
              }
            }
            else
            {
              //move back, turn and move
              PrintMessage("CMD_ACT_LAT_1_0.1");
              PrintMessage("CMD_ACT_ROT_0_" + (String)(2*angle));
              
              frontStr = CheckIR(OFRONT);
              if(frontStr.toFloat() < goal2.toFloat()) //check if theres something in the way
              {
              }
              else
              {
                //move toward the goal
                PrintMessage("CMD_ACT_LAT_1_" + (String)(goal2.toFloat() - 0.2));
              }
            }
            PrintMessage("CMD_SEN_PING");
            goal3 = Serial.readString();
            if(goal3.toFloat() <= 0.5 && goal3.toFloat() != 0)
            {
              goal = true;
              lcd.setCursor(0,0);
              lcd.print("Finished      ");
              DelayMilli(5000);
              PrintMessage("CMD_CLOSE");
              DelayMilli(5000);
              PrintMessage("CMD_CLOSE");
            }
          }
        }
        else //roam around
        {
          MoveDelay_ISR = 800;

          left = CheckSensor(30); //returns 0 if NaN
          if(left < 1)
          {
            Move_ISR = false;
          }
          if(left < right)
          {
            if(left < 4 && left >= 3.5)
            {
              PrintMessage("CMD_ACT_ROT_1_10");
            }
            else if(left < 3.5 && left >= 3)
            {
              PrintMessage("CMD_ACT_ROT_1_12.5");
            }
            else if(left < 3 && left >= 2.5)
            {
              PrintMessage("CMD_ACT_ROT_1_15");
            }
            else if(left < 2.5 && left >= 2)
            {
              PrintMessage("CMD_ACT_ROT_1_17.5");
            }
            else if(left < 2 && left >= 1.5)
            {
              PrintMessage("CMD_ACT_ROT_1_20");
            }
            else if(left < 1.5 && left >= 1)
            {
              //turn 25
              PrintMessage("CMD_ACT_ROT_1_25");
            }
            else
            {
              //turn 30deg
              PrintMessage("CMD_ACT_ROT_1_30");
            }
          }
          
          front = CheckSensor(0);
          if(front < 1 && front > 0)
          {
            Move_ISR = false;
          }
          if(front > 0)
          {
            if(front < 1 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_60");
            }
            else if(front < 1 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_60");
            }

            if(front < 2 && front >=1 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_30");
            }
            else if(front < 2 && front >=1 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_30");
            }

            if(front < 3 && front >=2 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_10");
            }
            else if(front < 3 && front >=2 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_10");
            }

            if(front > 3)
            {
              Move_Speed = 0.4;
            }
            else if(front < 3)
            {
              Move_Speed = 0.2;
            }
          }
          
          right = CheckSensor(330);
          if(right < 1)
          {
            Move_ISR = false;
          }
          if(right < left)
          {
            if(right < 4 && right >= 3.5)
            {
              PrintMessage("CMD_ACT_ROT_0_10");
            }
            else if(right < 3.5 && right >= 3)
            {
              PrintMessage("CMD_ACT_ROT_0_12.5");
            }
            else if(right < 3 && right >= 2.5)
            {
              PrintMessage("CMD_ACT_ROT_0_15");
            }
            else if(right < 2.5 && right >= 2)
            {
              PrintMessage("CMD_ACT_ROT_0_17.5");
            }
            else if(right < 2 && right >= 1.5)
            {
              PrintMessage("CMD_ACT_ROT_0_20");
            }
            else if(right < 1.5 && right >= 1)
            {
              //turn 25
              PrintMessage("CMD_ACT_ROT_0_25");
            }
            else
            {
              //turn 30deg
              PrintMessage("CMD_ACT_ROT_0_30");
            }
          }
          
          front = CheckSensor(0);
          if(front < 1 && front > 0)
          {
            Move_ISR = false;
          }
          if(front > 0)
          {
            if(front < 1 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_60");
            }
            else if(front < 1 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_60");
            }

            if(front < 2 && front >=1 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_30");
            }
            else if(front < 2 && front >=1 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_30");
            }

            if(front < 3 && front >=2 && left > right)
            {
              PrintMessage("CMD_ACT_ROT_0_10");
            }
            else if(front < 3 && front >=2 && left < right)
            {
              PrintMessage("CMD_ACT_ROT_1_10");
            }

            if(front > 3)
            {
              Move_Speed = 0.4; 
            }
            else if(front < 3)
            {
              Move_Speed = 0.2;
            }
          }
          
          if(left > 1 && right > 1 && front > 1)
          {
            Move_ISR = true;
          }
        }
      }
      break;
  }
}
