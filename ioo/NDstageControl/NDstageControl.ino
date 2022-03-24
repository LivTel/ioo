/* 

NDstageControl for IO:O

Description:

Telnet server for commands
Drives IOO ND filter stages

cjm's Dichroic code hacked around by rmb.
- rmb: Had to rename wProgram.h include in Messenger.cpp to Arduino.h
- rmb: Had to rename Server/Client objects with "Ethernet" prefix

Notes:

1. Uses an Arduino Ethernet POE
2. Check IP address is free before connection to the network
3. Ensure you have the right MAC address specified (see sticker on back of Arduino)
4. Uses modified Messenger library:
   - case 10 (LF) drops through to case 13 (CR) and terminates the message
5. **IMPORTANT: INPUT PINS ARE FLOATED HIGH AS LOGIC REVERSED (i.e. a digitalRead() of LOW signifies switch is activated)**

Board set up:

Pin 5 - Input - Filter 2 position sensor (stowed)
Pin 6 - Input - Filter 2 position sensor (deployed)
Pin 7 - Input - Filter 3 position sensor (stowed)
Pin 8 - Input - Filter 3 position sensor (deployed)
Pin 2 - Output - Filter 2 drive pin
Pin 3 - Output - Filter 3 drive pin

*/

#include <SPI.h>
#include <Ethernet.h>
#include <Messenger.h>
  
// io
// digi pin 5 - Filter 2 input position sensor (stowed)   - pulled HIGH if in stowed position, LOW if not
#define FILTER_2_STOWED_PIN            (5)
// digi pin 6 - Filter 2 input position sensor (deployed) - pulled HIGH if in deployed position, LOW if not
#define FILTER_2_DEPLOYED_PIN          (6)
// digi pin 7 - Filter 3 input position sensor (stowed)   - pulled HIGH if in stowed position, LOW if not
#define FILTER_3_STOWED_PIN            (7)
// digi pin 8 - Filter 3 input position sensor (deployed) - pulled HIGH if in deployed position, LOW if not
#define FILTER_3_DEPLOYED_PIN          (8)
// digi pin 2 - Filter 2 drive - pull HIGH to deploy
#define FILTER_2_DRIVE_PIN             (2)
// digi pin 3 - Filter 3 drive - pull HIGH to deploy
#define FILTER_3_DRIVE_PIN             (3)

// initial slide positions
#define FILTER_2_DRIVE_INIT            LOW
#define FILTER_3_DRIVE_INIT            LOW

// length of string used for command parsing
#define STRING_LENGTH                 (16)

// slide positions
#define SLIDE_POSITION_STOWED         (0)
#define SLIDE_POSITION_DEPLOYED       (1)
#define SLIDE_POSITION_UNKNOWN        (-1)

// ethernet server port
#define ETHERNET_SERVER_PORT          (23)

// default movement timeouts, all in milliseconds
#define DEFAULT_DEPLOY_TIMEOUT        (10000)
#define DEFAULT_STOW_TIMEOUT          (10000)

// ethernet shield mac address / IP
// REMOTE

byte mac[] =     {0x90, 0xA2, 0xDA, 0x00, 0x61, 0x97};
byte ip[]  =     {192, 168, 1, 72};
byte gateway[] = {192, 168, 1, 254};
byte subnet[]  = {255, 255, 255, 0};

EthernetServer server(ETHERNET_SERVER_PORT);
EthernetClient client = 0;

// instantiate Messenger object with the default separator (the space character)
Messenger message = Messenger(); 

// we use a flag separate from client.connected so we can recognise when a new connection has been created
boolean connectFlag = 0; 

// string used for command parsing
char string[STRING_LENGTH];

// error number
int errorNumber = 0;

// movement timeouts, all in milliseconds
int deployTimeout = DEFAULT_DEPLOY_TIMEOUT;
int stowTimeout   = DEFAULT_STOW_TIMEOUT;

void setup()
{
  // configure pins
  pinMode(FILTER_2_STOWED_PIN, INPUT);
  pinMode(FILTER_2_DEPLOYED_PIN, INPUT);
  pinMode(FILTER_3_STOWED_PIN, INPUT);
  pinMode(FILTER_3_DEPLOYED_PIN, INPUT);
  pinMode(FILTER_2_DRIVE_PIN, OUTPUT);
  pinMode(FILTER_3_DRIVE_PIN, OUTPUT);
  
  // set initial slide positions
  digitalWrite(FILTER_2_DRIVE_PIN, FILTER_2_DRIVE_INIT);
  digitalWrite(FILTER_3_DRIVE_PIN, FILTER_3_DRIVE_INIT);

  // configure inputs to float HIGH
  digitalWrite(FILTER_2_STOWED_PIN, HIGH);
  digitalWrite(FILTER_2_DEPLOYED_PIN, HIGH);
  digitalWrite(FILTER_3_STOWED_PIN, HIGH);
  digitalWrite(FILTER_3_DEPLOYED_PIN, HIGH);
  
  // configure serial
  Serial.begin(9600);
  
  // configure ethernet and callback routine
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  
  // messenger callback function
  message.attach(messageReady);
}

void loop()
{
  // check if new connection available
  if(server.available() && !connectFlag)
  {
      connectFlag = 1;
      client = server.available();
      Serial.println("loop:New client connected.");
  }
  // check for input from client
  if(client.connected() && client.available())
  {
      Serial.println("loop:Reading characters from TCP connection:");
      while(client.available())
      {
        char ch;
        ch = client.read();
        Serial.print(ch);
        message.process(ch);
      }
      Serial.println();
  }
  // wait a bit to stop the arduino locking up
  delay(10);
}

/* 
   Messenger callback function.
   
   handles commands of the form:
   
   - get 2|3 position|error|sensor {sensor ? stow|deploy : null}
   - move 2|3 stow|deploy
   - help
   
*/
void messageReady()
{
  int position, value, filter;
  Serial.println("");
  if(message.available())
  {
    if(message.checkString("get"))
    {
      // ascertain which filter to pass into the getPosition() method
      if(message.checkString("2"))
      {
        filter = 2;
      } else if (message.checkString("3"))
      {
        filter = 3;
      } else
      {
        message.copyString(string, STRING_LENGTH);
        client.print("1 Unknown filter in get command:");
        client.println(string);        
      } 
      
      if(message.checkString("position"))
      {
        Serial.println("Get position of filter " + (String)filter + ".");
        position = getPosition(filter);
        client.println(position);
      }
      else if(message.checkString("error"))
      {
        Serial.println("Get error number.");
        client.println(errorNumber);
      }
      else if(message.checkString("sensor"))
      {
        int thisFilterStowedPin, thisFilterDeployedPin;
  	switch(filter)
  	{
    	case 2:
      		thisFilterStowedPin    = FILTER_2_STOWED_PIN;
      		thisFilterDeployedPin  = FILTER_2_DEPLOYED_PIN;
      		break;
    	case 3:
     		thisFilterStowedPin    = FILTER_3_STOWED_PIN;
     		thisFilterDeployedPin  = FILTER_3_DEPLOYED_PIN;
      		break;
  	}
        if(message.checkString("deploy"))
        {
          Serial.print("Get sensor deploy: ");
          value = digitalRead(thisFilterDeployedPin);
          client.println(value);
          Serial.println(value);
        }
        else if(message.checkString("stow"))
        {
          Serial.print("Get sensor stow: ");
          value = digitalRead(thisFilterStowedPin);
          client.println(value);
          Serial.println(value);
        }
        else
        {
          message.copyString(string,STRING_LENGTH);
          client.print("12 Unknown get sensor command:");
          client.println(string);
        }
      }
      else
      {
        message.copyString(string, STRING_LENGTH);
        client.print("2 Unknown get command:");
        client.println(string);
      }
    }  
    
    else if(message.checkString("move"))
    {
       // ascertain which filter to pass into moveStow() and moveDeploy() methods
      if(message.checkString("2"))
      {
        filter = 2;
      } else if (message.checkString("3"))
      {
        filter = 3;
      } else
      {
        message.copyString(string, STRING_LENGTH);
        client.print("3 Unknown filter in move command:");
        client.println(string);        
      } 
      
      if(message.checkString("0") || message.checkString("stow"))
      {
        Serial.println("Moving to stowed position.");
        moveStow(filter);
        client.println(errorNumber);
      }
      else if(message.checkString("1") || message.checkString("deploy"))
      {
        Serial.println("Moving to deployed position.");
        moveDeploy(filter);
        client.println(errorNumber);
      }
      else
      {
        message.copyString(string, STRING_LENGTH);
        client.print("4 Unknown move command:");
        client.println(string);
      }
    }
    else if(message.checkString("help"))
    {
      client.println("ND filter stage control commands:");
      client.println("- get 2|3 position|error|sensor {sensor ? stow|deploy : null}");
      client.println("- move 2|3 stow|deploy");
      client.println("- help");
    } 
    else
    {
      message.copyString(string, STRING_LENGTH);
      client.print("5 Unknown command:");
      client.println(string);
    }
  }
  // close connection to client
  client.stop();
  connectFlag = 0;
  client = 0;
}

/*
   Get the current position of the stage
   @return The position
*/
int getPosition(int filter)
{
  
  int thisFilterStowedPin, thisFilterDeployedPin;
  
  switch(filter)
  {
    case 2:
      thisFilterStowedPin    = FILTER_2_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_2_DEPLOYED_PIN;
      break;
    case 3:
      thisFilterStowedPin    = FILTER_3_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_3_DEPLOYED_PIN;
      break;
  }
  
  int stowed, deployed;
  
  stowed = digitalRead(thisFilterStowedPin);
  Serial.print("getPosition:filter" + (String)filter + ":stowed:");
  Serial.println(stowed);
  
  deployed = digitalRead(thisFilterDeployedPin);
  Serial.print("getPosition:filter" + (String)filter + ":deployed:");
  Serial.println(deployed);
  
  if((stowed == LOW) && (deployed == HIGH))
    return SLIDE_POSITION_STOWED;
  else if((stowed == HIGH) && (deployed == LOW))
    return SLIDE_POSITION_DEPLOYED;
  else  
    return SLIDE_POSITION_UNKNOWN;    
}

/*
   Move the stage to the stowed|SLIDE_POSITION_STOWED position
   @return 0 on success and the errorNumber on failure
*/
int moveStow(int filter)
{  
  int thisFilterStowedPin, thisFilterDeployedPin, thisFilterDrivePin;
  
  switch(filter)
  {
    case 2:
      thisFilterStowedPin    = FILTER_2_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_2_DEPLOYED_PIN;
      thisFilterDrivePin     = FILTER_2_DRIVE_PIN;
      break;
    case 3:
      thisFilterStowedPin    = FILTER_3_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_3_DEPLOYED_PIN;
      thisFilterDrivePin     = FILTER_3_DRIVE_PIN;      
      break;
  }
  
  unsigned long startTime;
  unsigned long currentTime;
    
  errorNumber = 0;
  Serial.println("moveStow:filter" + (String)filter + ":Started.");
  startTime = millis();
  currentTime = millis();
  digitalWrite(thisFilterDrivePin, LOW);
  while(true)
  {
    delay(10);
    // see if we are in position stowed|SLIDE_POSITION_STOWED
    if(digitalRead(thisFilterStowedPin)==LOW)
    {
      break;
    }
    currentTime = millis();
    Serial.println("moveStow:filter" + (String)filter + ":loop:Checking Timeout.");
    if((currentTime-startTime) > stowTimeout)
    {
      Serial.println("moveStow:filter" + (String)filter + ":ERROR 6:Stow timeout.");
      errorNumber = 6;
      return errorNumber;
    }
  }
  
  // check all position sensors are correct
  Serial.println("moveStow:filter" + (String)filter + ":Checking final position.");
  if(digitalRead(thisFilterStowedPin)==HIGH)
  {
    Serial.println("moveStow:filter" + (String)filter + ":ERROR 7:Filter NOT in stowed position");
    errorNumber = 7;
    return errorNumber;
  }
  if(digitalRead(thisFilterDeployedPin)==LOW)
  {
    Serial.println("moveStow:filter" + (String)filter + ":ERROR 8:Filter in deployed position.");
    errorNumber = 8;
    return errorNumber;
  }
  Serial.println("moveStow:filter" + (String)filter + ":Finished.");
  return errorNumber;
}

/*
   Move the stage to the deployed|SLIDE_POSITION_DEPLOYED position
   @return 0 on success and the errorNumber on failure
*/
int moveDeploy(int filter)
{
  int thisFilterStowedPin, thisFilterDeployedPin, thisFilterDrivePin;
  
  switch(filter)
  {
    case 2:
      thisFilterStowedPin    = FILTER_2_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_2_DEPLOYED_PIN;
      thisFilterDrivePin     = FILTER_2_DRIVE_PIN;
      break;
    case 3:
      thisFilterStowedPin    = FILTER_3_STOWED_PIN;
      thisFilterDeployedPin  = FILTER_3_DEPLOYED_PIN;
      thisFilterDrivePin     = FILTER_3_DRIVE_PIN;      
      break;
  }
  
  unsigned long startTime;
  unsigned long currentTime;
    
  errorNumber = 0;
  Serial.println("moveDeploy:filter" + (String)filter + ":Started.");
  startTime = millis();
  currentTime = millis();
  digitalWrite(thisFilterDrivePin, HIGH);
  while(true)
  {
    delay(10);
    // see if we are in position deployed|SLIDE_POSITION_DEPLOYED
    if(digitalRead(thisFilterDeployedPin)==LOW)
    {
      break;
    }
    currentTime = millis();
    Serial.println("moveDeploy:filter" + (String)filter + ":loop:Checking Timeout.");
    if((currentTime-startTime) > deployTimeout)
    {
      Serial.println("moveDeploy:filter" + (String)filter + ":ERROR 9:Deploy timeout.");
      errorNumber = 9;
      return errorNumber;
    }
  }
  
  // check all position sensors are correct
  Serial.println("moveDeploy:filter" + (String)filter + ":Checking final position.");
  if(digitalRead(thisFilterDeployedPin)==HIGH)
  {
    Serial.println("moveDeploy:filter" + (String)filter + ":ERROR 10:Filter is NOT in deployed position");
    errorNumber = 10;
    return errorNumber;
  }
  if(digitalRead(thisFilterStowedPin)==LOW)
  {
    Serial.println("moveDeploy:filter" + (String)filter + ":ERROR 11:Filter in stowed position.");
    errorNumber = 11;
    return errorNumber;
  }
  Serial.println("moveDeploy:filter" + (String)filter + ":Finished.");
  return errorNumber;
}


