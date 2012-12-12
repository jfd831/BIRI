/*
Automated Home Brewing System 
              a.k.a. "BIRI"
              Version : 1.0
              
              Arduino Uno REV 3              
              
        This is an automated home brewing system used for brewing craft beer. It utilizes a stepper motor 
        to control a propane needle valve. The stepper is controlled with the SparkFun Easy Driver board 
        and the related library. This valve is a very commonly used valve for most propane burners, therefore 
        this code could be adapted for many other uses. The valve is controlled based on a temperature
        reading from a digital temperature sensor from Adafruit. This sensor utilizes the OneWire library. 
        There is a hopper using a servo for control, this is driven with the servo library. There is also
        a propane sensor (also from SparkFun) to improve safety by measuring propane gas levels in the 
        environment and force the valve shut in the case of a flame out. It is attached with the gas breakout
        board from Sparkfun. There is a rotary potentiometer on the LPG sensor in order to set the base value
        for the sensor. As of now the user interface is through the serial monitor as well as a piezo speaker
        to alert user during various stages of the brew. 
        
       
	Created: Fall 2012
	By: Jaylyn Daugherty
            Ben Wallace            
            
            
        More documentation, wiring diagrams, design information and links can be found at:
        http://jdaughertytech.blogspot.com/

        Feel free to Edit, adapt, change or change in any way for your own use. We just ask you share what you did with us!
        Information for contacting us can be found on the above website. 
*/




#include <OneWire.h>                          // The library used for the Digital Temperature Sensor
#include <DallasTemperature.h>                // Library for decoding the Digital Sensor Input 
#include <Servo.h>                            // Basic servo library for the hopper servo



int LPG_VALUE;                                // A value for the level of the LPG sensor 
int DESIRED_TEMP;                             // A variable used for setting what temperature is needed for various parts of the recipe
int ACTUAL_TEMP ;                             // The value taken from the Digital Temerature sensor
unsigned long TEMP_COMPARE;                   // A stored value for the difference between the DESIRED_TEMP and ACTUAL_TEMP
long PREVIOUS_SAMPLE;                         // An important variable for non-blocking code which stores the value of millis() between sensor checks
int SAMPLE_RATE = 1000;                       // The time between the non-blocking sensor checks. In milliseconds 
long TIME_BEGIN_BREW;                         // Used for the different steps of the brew. Reset at various points of the brew. 
                                                 // The name is somewhat confusing and most likely will change in future versions
unsigned int STEPPER_POS=0;                   // A variable for storing where the stepper is currently positioned.
                                                 // In future versions this will most likely be replaces with a rotary potentiometer for more accurate information



/* This block of variables are inserted by the user to change between recipes. 
   In the future we would like to replace this with NFC cards which store this data.
   Or with a better user interface which asks for these values. */
   
char  RECIPE[]   = "No Fail Pale";            // The name of the recipe
int   STEEP_TEMP = 150;                       // The temperature for the specialty grains steeping   
                                                 //(all time variables are "long" because the amount of milliseconds would pass the limit of "int")
long  STEEP_TIME = 10000;                     // The amount of time the grains need to steep
int   HOP_1_TEMP = 200;                       // The boil temperature required for the first hop addition
int   HOP_2_TEMP = 200;                       // The second boil temperature 
long  EXTRACT_TIME = 10000;                   // The amount of time required until the addition of Extract
int   EXTRACT_TEMP = 70;                      // Sets the temp low again during extract addition to avoid burning extract to bottom of kettle
long  FINAL_BOIL_TIME = 10000;                // The last amount of time needed for the boil



/* This block is used for setting which pins are used on the Arduino.
   In future versions these may change in order to free up some necessary pins. */
   
#define TEMP_PIN   3                          // The one wire for the digital temp pin
#define LPG_PIN    A5                         // The LPG sensor. Analog sensor
#define DIR_PIN    6                          // The pin used to drive the EasyDriver board. Sets the direction of rotation
#define STEP_PIN   5                          // Another pin used for the EasyDriver board. Tells the board how many steps to go
#define SERVO_PIN  9                          // Attaches the servo control wire
#define ALARM_PIN  A0                         // Attached to the piezo speaker



Servo myservo;                                // Sets up the Servo, necessary for the Servo library
OneWire oneWire(TEMP_PIN);                    // Sets up the one wire library. For the digital temp sensor
DallasTemperature sensors(&oneWire);          // Another set up for the digital temp sensor


/* A block of booleans necessary for the non-blocking code.
   In order to not return to previous parts of the brew.
   Also very useful for testing the setup. */
   
boolean BURNER_LIT    = false;                // Whether the burner is lit or not.     
boolean STEEP_CHECK   = true;                 // If it is time to check for the steeping temperature
boolean GRAIN_CHECK   = false;                // Time for the grain addition
boolean BOIL_CHECK    = false;                // Time for checking the first hop addition
boolean EXTRACT_CHECK = false;                // Extract action
boolean BOIL_CHECK_2  = false;                // Second hop action
boolean BOIL_CHECK_3  = false;                // Third boil action
boolean TIME_TO_BREW  = true;                 // Overall time to run the recipe



/* Function for using the Stepper Driver. 
   Taken from the Easy Driver documentation.
   Utilizing number of steps, speed and direction  */
   
void rotate(int steps, float speed){         // the structure for calling the function
  int dir = (steps > 0)? HIGH:LOW;           // if positive number counter clockwise rotation. negative = clockwise
  steps = abs(steps);                        // number of steps set to a positive value regardless of direction
  digitalWrite(DIR_PIN,dir);                 // Outputs to driver board the direction (HIGH or LOW, i.e. positive or negative or CCW or CW)
  float usDelay = (1/speed) * 70;            // Allows enough time for stepper to catch up to position
  for(int i=0; i < steps; i++){              // Block which actually drives the steps
    digitalWrite(STEP_PIN, HIGH);            // Refer to Easy Driver Spec sheet for info on how this excites coils
    delayMicroseconds(usDelay); 
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(usDelay);
  }
} 
  
  
  
  
  
  
void setup(){
  Serial.begin(9600);                        // Starts the serial monitor for User Interface
  Serial.println(RECIPE);                    // States which recipe is being brewed
  
  pinMode(DIR_PIN,OUTPUT);                   // Sets the Easy Driver pins as outputs 
  pinMode(STEP_PIN, OUTPUT);
 
DESIRED_TEMP = STEEP_TEMP;                   // Starts the DESIRED_TEMP as the STEEP_TEMP

myservo.attach(SERVO_PIN);                   // Attaches the servo pin Per the Servo library
myservo.write(0);                            // Starts the hopper in the first position 
delay(1000);                                 // Gives enough time to get in position
myservo.write(LOW);                          // Turns the servo off once it reaches position
 
sensors.begin();                             // Starts the Temp sensors. Per the oneWire library
 
}






void loop(){
  
  if (!BURNER_LIT ){                        // If at any time the burner is NOT lit. We return to this lighting procedure
                                            // In future Versions this may become a function placed on a separate tab    
                                            
      analogWrite(ALARM_PIN, 255);          // Turns on the piezo to alert user whenever the burner is not lit 
  
      Serial.println("Are you ready to light burner? (y/n)");  // Waits until user is ready 
      char input1 = Serial.read();                             // Reads the input from serial monitor
      delay(3000);                                              
      
        if (input1 == 'y'){                                    // If user is ready 
           Serial.println("Burner open 1/8");
           rotate(200,.01);                                    // Opens valve slightly, just enough to light burner
           STEPPER_POS = (STEPPER_POS+200);                    // Keeps the position of the stepper recorded 
           Serial.println("Is the burner lit? (y/n)");         // Asks user if the burner was successfully lit
           delay(5000);         
           char input2 = Serial.read();                        // Reads the input from serial monitor
         
              if (input2 == 'y'){                              // If burner is lit
                 Serial.println("burner lit!");              
                 analogWrite(ALARM_PIN, LOW);                  // Turn off alarm
                 BURNER_LIT= true;                             // Exits the the lighting procedure
                  }
                  
           delay(1000);
           PREVIOUS_SAMPLE = millis();                          // Sets the first amount of time until the first sensor check and valve adjustment                 
           }
           
      if (STEPPER_POS>=200 &&(!BURNER_LIT)){                    // If the burner is open 
        Serial.println("Turn to off position");
        rotate(-200,.01);                                       // Turns the valve to the off position
        STEPPER_POS= (STEPPER_POS-200);                         // Records the position of the stepper
        }
      
      else {
        Serial.println("Remain in Off Position");               // If the burner is closed and the user is not ready to light
        }
  }
  
  
if (BURNER_LIT && TIME_TO_BREW){                                // If the burner is lit and it is still time to brew

    // This block prints to the serial monitor all the crucial information from the brew 
    Serial.print("Time:  ");
    Serial.print(millis());
    Serial.print("   LPG_VALUE:    ");
    Serial.print(LPG_VALUE);
    Serial.print("   STEPPER_POS:    ");
    Serial.print(STEPPER_POS);
    Serial.print("         Temp:  ");
    Serial.println(ACTUAL_TEMP);
      
    
      if (millis()>= PREVIOUS_SAMPLE + SAMPLE_RATE){           // If the non-blocking code has reached the sample time 
        
          Serial.println("Read LPG_VALUE");                    // First thing we do is check the propane level
          LPG_VALUE = analogRead(LPG_PIN);
            if (LPG_VALUE>=200){                               // If it is too high (i.e. burner has flamed out)
              BURNER_LIT = false;                              // We return to the lighting procedure and wait for user to relight burner
              }
            
          Serial.println("Read ACTUAL_TEMP");                  // We then check the temperature
          sensors.requestTemperatures();
          ACTUAL_TEMP = sensors.getTempFByIndex(0);             
          Serial.println("Compare To DESIRED_TEMP");           // Then we compare it to the DESIRED_TEMP
          TEMP_COMPARE = ACTUAL_TEMP - DESIRED_TEMP;    
      
             // All of these if statements adjust the stepper based on how far the temperature is from the desired temp
                // this probably could be replaced with a more efficient method, possibly some kind of software filter
                
             if ((TEMP_COMPARE >= -150)&&(TEMP_COMPARE<=-75)&&(STEPPER_POS<= 700)){
                Serial.println("open valve 300 steps");
                rotate(300,.01);
                STEPPER_POS = (STEPPER_POS+300);                     
                }
                  
             if ((TEMP_COMPARE >= -75)&&(TEMP_COMPARE<=-10)&&(STEPPER_POS<=950)){
                Serial.println("open valve 50 steps");
                rotate(50,.01);
                STEPPER_POS = (STEPPER_POS+50);
                }
                  
             if ((TEMP_COMPARE >= 10)&&(TEMP_COMPARE<=75)&&(STEPPER_POS>=50)){
                Serial.println("close valve 50 steps");
                rotate(-50,.01);
                STEPPER_POS = (STEPPER_POS-50);
                }
   
             if ((TEMP_COMPARE >= 75)&&(TEMP_COMPARE<=150)&&(STEPPER_POS>=300)){
                Serial.println("close valve 300 steps");
                rotate(-300,.01);
                STEPPER_POS = (STEPPER_POS-300);
                }
      
      
      PREVIOUS_SAMPLE= millis();
  }
  // This is the end of the temperature, stepper position and LPG checks
  
  
  
   // This block is the first aspect of the recipe. In which the recipe will not continue until the temperature is within 10 degrees of the actual temperature    
   if (STEEP_CHECK && (ACTUAL_TEMP <=DESIRED_TEMP+10) && (ACTUAL_TEMP>=DESIRED_TEMP-10)){
       Serial.println("Signal User to Add Grains");    // Asks User to insert specialty grains
       Serial.println("Has Grain been added?");  
          analogWrite(ALARM_PIN, 150);
          delay(500);
          analogWrite(ALARM_PIN, LOW);
          delay(4500);
          char input3 = Serial.read();                // Waits until user has claimed they added the grains           
          
            if (input3 == 'y'){                       // When the user has added the grain
               Serial.println("Grain Added!");
               STEEP_CHECK= false;                    // Moves on to the next step of the brew
               GRAIN_CHECK = true;                      
               TIME_BEGIN_BREW = millis();
               PREVIOUS_SAMPLE= millis();                                                        
               }    
        }
    
    // This block is very similar to the previous block
    if (GRAIN_CHECK && (millis()-TIME_BEGIN_BREW >= STEEP_TIME)) {                                                        
      Serial.println("Signal User to Remove Grain"); 
      Serial.println("Has Grain been removed?");
          analogWrite(ALARM_PIN, 150);
          delay(500);
          analogWrite(ALARM_PIN, LOW);
          delay(4500);
          char input4 = Serial.read();          
            if (input4 == 'y'){
               Serial.println("Grain Removed!");      
               DESIRED_TEMP = HOP_1_TEMP;                                                                
               BOIL_CHECK = true;
               TIME_BEGIN_BREW = millis();
               PREVIOUS_SAMPLE=millis();                                                           
               GRAIN_CHECK = false;
               }    
      }
                                                                    
      
    // Once again this is very similar to the previous block except the servo rotates the hopper to the first hop addition    
    if (BOIL_CHECK && (ACTUAL_TEMP <= DESIRED_TEMP+10) && (ACTUAL_TEMP>= DESIRED_TEMP-10)){
     Serial.println("Hop Addition #1");
     myservo.write(90);  
     EXTRACT_CHECK= true;
     TIME_BEGIN_BREW = millis();
     PREVIOUS_SAMPLE= millis();                                                                  
     BOIL_CHECK= false;     
     }
   
   
   // This block is very similar to the specialty grain step. Except it turns the burner back down to make sure the extract does not burn to the kettle.
   if (EXTRACT_CHECK&& (millis()-TIME_BEGIN_BREW>= EXTRACT_TIME)){
     DESIRED_TEMP = EXTRACT_TEMP;
     Serial.println("Signal User to Add Extract");
     Serial.println("Has Extract been added?"); 
          analogWrite(ALARM_PIN, 150);
          delay(500);
          analogWrite(ALARM_PIN, LOW);
          delay(4500);
          char input2 = Serial.read();          
            if (input2 == 'y'){
               Serial.println("Extract Added!");
               DESIRED_TEMP = HOP_2_TEMP;
               BOIL_CHECK_2=true;
               TIME_BEGIN_BREW = millis();
               PREVIOUS_SAMPLE= millis();                                                     
               EXTRACT_CHECK = false;
                  }
     }
   // This is very similar to the first hop addition step
   if (BOIL_CHECK_2 && (ACTUAL_TEMP <= DESIRED_TEMP+10) && (ACTUAL_TEMP>= DESIRED_TEMP-10)){
     Serial.println("Hop Addition #2");
     
     myservo.write(177);  
    
     TIME_BEGIN_BREW= millis();
     PREVIOUS_SAMPLE=millis();
     BOIL_CHECK_3= true;   
     BOIL_CHECK_2= false;
   }
   
   
   // This is the final step in the brew. 
   if (BOIL_CHECK_3 && (millis()-TIME_BEGIN_BREW>= FINAL_BOIL_TIME)){
     DESIRED_TEMP = 0;
     Serial.println("Signal User Brewing Is Complete");      // Tells the user that everything has been completed
     analogWrite(ALARM_PIN, 150);
     delay(100);
     analogWrite(ALARM_PIN, 255);
     delay(100);
     analogWrite(ALARM_PIN, LOW);
      TIME_TO_BREW = false;                                 // It is no longer time to brew. So we exit the brewing function
     } 
   
  
  // If the brewing is completed
  if(!TIME_TO_BREW){
    if(STEPPER_POS>=10){       // And the stepper is open 
    rotate(-1000,.01);         // Shut the stepper valve
    STEPPER_POS = (STEPPER_POS-1000);
    Serial.println("Fully closing valve for end of brew");
    }
    Serial.println("NO LONGER TIME TO BREW. ENJOY YOUR BEER!"); // The farewell message 
    }
}
}


