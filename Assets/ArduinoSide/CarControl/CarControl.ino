#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

//MyVersion
//Car control variables
int incomingByte = 0;
int speedCar = 400;//0 to 1023
int speed_Coeff = 3; //aux reduce turn speed

#define ENA   14          // Enable/speed motors Right        GPIO14(D5)
#define ENB   12          // Enable/speed motors Left         GPIO12(D6)
#define IN_1  15          // L298N in1 motors Right           GPIO15(D8)
#define IN_2  13          // L298N in2 motors Right           GPIO13(D7)
#define IN_3  2           // L298N in3 motors Left            GPIO2(D4)
#define IN_4  0           // L298N in4 motors Left            GPIO0(D3)

const char* ssid = "Vivo";
const char* password = "d4c3b21a";

WiFiUDP Udp;
unsigned int localUdpPort = 1987;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets
//char  replyPacket[] = "Menssagem de volta, funcionou!";  // a reply string to send back

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

 pinMode(ENA, OUTPUT);
 pinMode(ENB, OUTPUT);  
 pinMode(IN_1, OUTPUT);
 pinMode(IN_2, OUTPUT);
 pinMode(IN_3, OUTPUT);
 pinMode(IN_4, OUTPUT); 
}


void loop()
{
  ListenPacketRoutine();
  ListenKeyboardRoutine();
}

void forward()
{
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void backward()
{
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void right()
{
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void left()
{
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void forwardRight()
{      
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar/speed_Coeff);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar);
}

void forwardLeft()
{     
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, HIGH);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, HIGH);
  analogWrite(ENB, speedCar/speed_Coeff);
}

void backwardRight()
{
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar/speed_Coeff);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar);
}

void backwardLeft()
{
  digitalWrite(IN_1, HIGH);
  digitalWrite(IN_2, LOW);
  analogWrite(ENA, speedCar);

  digitalWrite(IN_3, HIGH);
  digitalWrite(IN_4, LOW);
  analogWrite(ENB, speedCar/speed_Coeff);
}

void stop()
{
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, LOW);
  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, LOW);
}

void ListenPacketRoutine(){
  //listen for packets
  int packetSize = Udp.parsePacket();
  if (packetSize){
    int len = Udp.read(incomingPacket, 255);
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
    if (incomingPacket[0] == 'W'){
        forward();
    } else if (incomingPacket[0] == 'X'){
        backward();
    } else if (incomingPacket[0] == 'A'){
        left();
    } else if (incomingPacket[0] == 'D'){
        right();
    } else if (incomingPacket[0] == 'E'){
        forwardRight();
    } else if (incomingPacket[0] == 'Q'){
        forwardLeft();
    } else if (incomingPacket[0] == 'C'){
        backwardRight();
    }  else if (incomingPacket[0] == 'Z'){
        backwardLeft();
    }  else if (incomingPacket[0] == 'S'){
        stop();
    }
  }
}

void ListenKeyboardRoutine(){

 if (Serial.available() > 0) {
    incomingByte = Serial.read();
    }
  
 switch(incomingByte)
  {
     case 'S': 
      { stop();
       Serial.println("Stop\n"); 
       incomingByte='*';}
      
     break;
     
     case 'W':
     {  forward(); 
       
       Serial.println("Forward\n");
       incomingByte='*';}
     break;
    
      case 'X':  
    {   backward();
       
       Serial.println("Backward\n");
       incomingByte='*';}
     break;
     
     case 'D':
     {  
       right(); 
       Serial.println("Rotate Right\n");
       incomingByte='*';}
     break;

       case 'A':
      { 
       left();    
       Serial.println("Rotate Left\n");
       incomingByte='*';}
     break;
     
      case 'E':
      { 
       forwardRight();    
       Serial.println("Rotate Left\n");
       incomingByte='*';}
     break;

      case 'Q':
      { 
       forwardLeft();    
       Serial.println("Rotate Left\n");
       incomingByte='*';}
     break;
     
      case 'C':
      { 
       backwardRight();    
       Serial.println("Rotate Left\n");
       incomingByte='*';}
     break;

       case 'Z':
      { 
       backwardLeft();    
       Serial.println("Rotate Left\n");
       incomingByte='*';}
     break;
  }
}
