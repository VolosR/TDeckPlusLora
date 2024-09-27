#include <RadioLib.h>
#include "pins.h"
#include "Noto.h"   // big header font
#include "Noto2.h"  // smaller font
#include <Wire.h>
#include <TFT_eSPI.h>


TFT_eSPI tft = TFT_eSPI();  // display
TFT_eSprite sprite = TFT_eSprite(&tft);  // sprite

#define color1 TFT_BLACK
#define color2 0x0249
#define red 0xB061
#define green 0x1B08
unsigned short grays[13];


// Create an instance of the SX1262 driver
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
int transmissionState = RADIOLIB_ERR_NONE; // save transmission states between loops
bool transmitFlag = false; // flag to indicate transmission or reception state
volatile bool operationDone = false; // flag to indicate that a packet was sent or received

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif


int n=0;
String notice="";
String msg[10]={"","","","","","","","","",""};
String msgT[10]={"","","","","","","","","",""};

bool writer[10]={0};  // who is author of msg 1 is me 0 is someone else
bool writerT[10]={0};
int nMsg=-1;  //number of messages
char keyValue = 0;  // keyboard input
int recN=0;  //number of recieved messages
int sndN=0; //number of sent messages

String name= "";
char nameC[6]="00000";
char avaliabe[17]="0123456789ABCDEF";

int brightness[6]={70,90,120,160,200,250};
int bri=4; // selected brightness level
//button debouncing
int deb=0;
int deb2=0;

void setFlag(void) {
  // when something happnds , when message is recieved or send this function will be caled
  operationDone = true;
  Serial.println("something happened");
  }


void setup() {
  
     //! The board peripheral power control pin needs to be set to HIGH when using the peripheral
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);

    Serial.begin(115200);
    Serial.println("T-DECK started");

    pinMode(0, INPUT_PULLUP); //middle button
    pinMode(2,INPUT_PULLUP); // button right
    pinMode(3,INPUT_PULLUP); // button down ?
    pinMode(1,INPUT_PULLUP); // button left
    pinMode(15,INPUT_PULLUP); // button down ?

    pinMode(BOARD_SDCARD_CS, OUTPUT);
    pinMode(RADIO_CS_PIN, OUTPUT);
    pinMode(BOARD_TFT_CS, OUTPUT);

    digitalWrite(BOARD_SDCARD_CS, HIGH);
    digitalWrite(RADIO_CS_PIN, HIGH);
    digitalWrite(BOARD_TFT_CS, HIGH);

    analogWrite(BOARD_BL_PIN,0);

    delay(500);

    //init keyboard
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    // Check keyboard
    Wire.requestFrom(0x55, 1);
    if (Wire.read() == -1) {
        while (1) {
            Serial.println("LILYGO Keyboad not online .");
            delay(1000);
        }
    }

    tft.begin();
    tft.setRotation( 1 );
    analogWrite(BOARD_BL_PIN,brightness[bri]);
    sprite.createSprite(320,240);

     int co = 210;
     for (int i = 0; i < 13; i++) {
        grays[i] = tft.color565(co, co, co);
        co = co - 20;
     }

     // get random name
     for(int i=0;i<5;i++)
     nameC[i]=avaliabe[random(0,16)];

     name=String(nameC);

     // Initialize SX1262 
     setupRadio();
     delay(3000);
     draw();
     draw();
}

bool setupRadio()
{
    digitalWrite(BOARD_SDCARD_CS, HIGH);
    digitalWrite(RADIO_CS_PIN, HIGH);
    digitalWrite(BOARD_TFT_CS, HIGH);
    SPI.end();
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI); //SD

    int state = radio.begin(RADIO_FREQ);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("Start Radio success!");
    } else {
        Serial.print("Start Radio failed,code:");
        Serial.println(state);
        return false;
    }

    bool hasRadio = true;

    // set carrier frequency to 868.0 MHz
    if (radio.setFrequency(RADIO_FREQ) == RADIOLIB_ERR_INVALID_FREQUENCY) {
        Serial.println(F("Selected frequency is invalid for this module!"));
        return false;
    }

    // set bandwidth to 125 kHz
    if (radio.setBandwidth(125.0) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
        Serial.println(F("Selected bandwidth is invalid for this module!"));
        return false;
    }

    // set spreading factor to 10
    if (radio.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
        Serial.println(F("Selected spreading factor is invalid for this module!"));
        return false;
    }

    // set coding rate to 6
    if (radio.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
        Serial.println(F("Selected coding rate is invalid for this module!"));
        return false;
    }

    // set LoRa sync word to 0xAB
    if (radio.setSyncWord(0xAB) != RADIOLIB_ERR_NONE) {
        Serial.println(F("Unable to set sync word!"));
        return false;
    }

    // set output power to 10 dBm (accepted range is -17 - 22 dBm)
    if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        return false;
    }

    // set over current protection limit to 140 mA (accepted range is 45 - 140 mA)
    // NOTE: set value to 0 to disable overcurrent protection
    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        return false;
    }

    // set LoRa preamble length to 15 symbols (accepted range is 0 - 65535)
    if (radio.setPreambleLength(15) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) {
        Serial.println(F("Selected preamble length is invalid for this module!"));
        return false;
    }

    // disable CRC
    if (radio.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
        Serial.println(F("Selected CRC is invalid for this module!"));
        return false;
    }

    radio.setDio1Action(setFlag);
    // start listening for LoRa packets on this node
    Serial.print(F("[SX1262] Starting to listen ... "));
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true) { delay(10); }
    }
 
    return true;
}

void draw()
  {
  
    sprite.fillSprite(TFT_BLACK); 
    sprite.fillRect(0,0,320,20,grays[9]);
    sprite.fillRect(0,20,320,2,color2);
    sprite.fillRect(0,202,320,2,0xBC81);
    //sprite.fillSmoothRoundRect(0, 20, 320, 182 , 2, color1, TFT_BLACK);
    sprite.fillSmoothRoundRect(0, 218, 320, 22 , 2, color2, TFT_BLACK);
    sprite.fillSmoothRoundRect(4, 2, 56, 14 , 2, grays[6], grays[9]);
    sprite.fillSmoothRoundRect(2, 2, 16, 14 , 2, red, grays[9]);
    sprite.fillSmoothRoundRect(272, 2, 40, 16 , 2, green, grays[9]);
    sprite.fillSmoothRoundRect(308, 6, 8, 8 , 2, green, grays[9]);
    sprite.fillSmoothRoundRect(275, 4, 34, 12 , 2,  TFT_BLACK,green);


    //draw brightness blocks
    for(int i=0;i<5;i++)
    {
      if(i<bri)
      sprite.fillRect(282+(i*8),207,5,8,grays[3]);
      else
      sprite.fillRect(282+(i*8),207,5,8,grays[7]);
    }

    // draw horizonatl lines
    for(int i=0;i<9;i++)
    sprite.drawFastHLine(4, 38+(i*18), 312, grays[8]);
    
    
    sprite.loadFont(NotoSansBold15);
    sprite.setTextColor(0xBC81,grays[9]);
    sprite.drawString("LORA Messenger",64,2,2);
   
   
    sprite.unloadFont(); 
    sprite.loadFont(NotoSans13);

    
    sprite.setTextColor(TFT_WHITE,red);
    sprite.drawString("T",6,4);
    sprite.setTextColor(TFT_BLACK,grays[6]);
    sprite.drawString("DECK",21,4);

    sprite.setTextColor(grays[1],color2);
    sprite.drawString(notice,6,223,2);

      for(int i=0;i<nMsg+1;i++)
      if(msg[i].length()>0)
      {
        if(writer[i]==1) 
          sprite.setTextColor(grays[1],color1); else sprite.setTextColor(0x663C,color1);
          sprite.drawString(msg[i],6,25+(i*18),2);
      }

    sprite.setTextColor(grays[5],TFT_BLACK);
    sprite.unloadFont(); 
    sprite.drawString("Your ID: "+name,2,208);
    sprite.setTextColor(grays[4],grays[9]);
    sprite.drawString("VOLOS",210,2);
    sprite.setTextColor(grays[5],grays[9]);
    sprite.drawString("projects",210,9);
    sprite.setTextColor(grays[2],TFT_BLACK);
    sprite.drawString(String(analogRead(4)),280,7);

    sprite.setTextColor(grays[8],color2);
    sprite.drawString("ENTER YOUR MESSAGE",200,226);

    sprite.setTextColor(grays[7],TFT_BLACK);
    sprite.drawString("SND:",120,208);
    sprite.drawString(String(sndN),145,208);

    sprite.drawString("REC:",190,208);
    sprite.drawString(String(recN),215,208);

  
    
    


    sprite.pushSprite(0,0);
   
  }

void recieveMessages()
{
      String str;
      int state = radio.readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        Serial.println(F("[SX1262] Received packet!"));

        // print data of the packet
        Serial.print(F("[SX1262] Data:\t\t"));
        Serial.println(str);

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1262] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1262] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));
      }

       if(str.length()>0) {
         nMsg++;
         recN++;
        if(nMsg>9)
        {
          nMsg=9;
          msg[nMsg] =str;
          writer[nMsg]=0;
          for(int i=9;i>0;i--){
            msg[i-1]=msgT[i];
            writer[i-1]=writerT[i];
          }
        }else{msg[nMsg] = str; writer[nMsg]=0;}

        for(int i=0;i<10;i++)
          {msgT[i]=msg[i];
          writerT[i]=writer[i];}
      }
}

void sendMessage()
{
  transmitFlag=true;
  transmissionState = radio.startTransmit(name+" : "+notice);
}

void backToListening()
{
    int state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("i am listening again"));
    } else {
      Serial.print(F("failed to listen again "));
      Serial.println(state);
      while (true) { delay(10); }
    }
}

void readButtons()
{

 

    if(digitalRead(2)==0){
    if(deb==0){
      deb=1;
      bri++; if(bri>5) bri=0;
      analogWrite(BOARD_BL_PIN,brightness[bri]);
      draw();
    }
  }else deb=0;

    if(digitalRead(1)==0){
    if(deb2==0){
      deb2=1;
      bri--; if(bri<0) bri=5;
      analogWrite(BOARD_BL_PIN,brightness[bri]);
      draw();
      
    }
  }else deb2=0;
}



void loop() {
readButtons();
  if(operationDone)
  { 
    n++;
    if(!transmitFlag)
    recieveMessages();
    operationDone = false;

    if(transmitFlag) {
      if (transmissionState == RADIOLIB_ERR_NONE) {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));
        sndN++;
      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }
    transmitFlag=false;
    backToListening();
    
    }
     draw(); draw();
  }

  Wire.requestFrom(0x55, 1);  //request keyboard data
  while (Wire.available() > 0) {
    keyValue = Wire.read();
    if (keyValue != (char)0x00) {
      if ((int)keyValue == 13) {
        if(notice.length()>0)
        {
        nMsg++;
        if(nMsg>9)
        {
          nMsg=9;
          msg[nMsg] =name+" : "+notice;
          writer[nMsg]=1;
          for(int i=9;i>0;i--){
            msg[i-1]=msgT[i];
            writer[i-1]=writerT[i];
          }
        }else{msg[nMsg] = name+" : "+notice; writer[nMsg]=1;}

        for(int i=0;i<10;i++)
          {msgT[i]=msg[i];
          writerT[i]=writer[i];}

        
        draw();  // Update the screen before transmission
        sendMessage();
        notice = "";
        }
      }
      else if ((int)keyValue == 8) {
        notice = notice.substring(0,notice.length()-1);
        draw();
      } else {
        if(notice.length()<42)
        notice += String(keyValue);
        draw();
      }
    }
  }

}




