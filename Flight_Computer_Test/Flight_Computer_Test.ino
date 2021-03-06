#include <SPI.h>
#include <RH_RF95.h>
#include <SoftwareSerial.h>

// miso 12
// mosi 11
// sck 13
#define RFM95_CS  4
#define RFM95_RST 2
#define RFM95_INT 3

#define TLED 8

#define GPS_TX 7
#define GPS_RX 8
 
#define RF95_FREQ 433.0

// radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

#define GREEN 24
#define RED 25

#define PACKET_SIZE 86
char packet[PACKET_SIZE];
char VEHICLE_HEADER[4] = {'H','E','Y','Y'};
String GS_HEADER = "U_UP";
String gpsBuffer;
char byteIn;
bool transmitReady;

// gps serial
SoftwareSerial GPS(GPS_TX, GPS_RX);

void blinkLed(int pin, int blinks, int duration)
{
  if (blinks <= 0) return;
  if (duration <= 0) return;

  for (int i=0; i<blinks; i++)
  {
    digitalWrite(pin, HIGH);
    delay(duration);
    digitalWrite(pin, LOW);
    delay(duration);
  }
}
 
void setup() 
{
  // setup led pins
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);

  // status leds on
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, HIGH);
  
  // initialize pins
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  
  // start serial
  Serial.begin(9600);
  delay(100);

  // start gps
  GPS.begin(9600); // start gps
  delay(100);
 
  // manual reset LoRa
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  // init and check radio systems
  while (!rf95.init()) 
  {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");
 
  if (!rf95.setFrequency(RF95_FREQ)) 
  {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
 
  // Set transmit power to 23 dBm
  rf95.setTxPower(23, false);

  transmitReady = false;
  gpsBuffer.reserve(PACKET_SIZE - sizeof(VEHICLE_HEADER)); // allocate memory
  GPS.listen(); // start listening

  // status leds off
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
}

void loop()
{
  
  // GPS Reading
  while (GPS.available()) 
  {
    byteIn = GPS.read(); // read 1 byte
    gpsBuffer += char(byteIn); // add byte to buffer
    if (byteIn == '\n') 
    { // end of line
      transmitReady = true; // ready to transmit
    }
  }

  // Transmitting
  if (transmitReady) 
  {
    if (gpsBuffer.startsWith("$GPGGA")) // only transmit what's needed
    {
      char payload[PACKET_SIZE - sizeof(VEHICLE_HEADER)];
      gpsBuffer.toCharArray(payload, PACKET_SIZE - sizeof(VEHICLE_HEADER)); // copy gps data to packet
      packet[PACKET_SIZE - 1] = 0; // null terminate the packet
      packet[0] = VEHICLE_HEADER[0];
      packet[1] = VEHICLE_HEADER[1];
      packet[2] = VEHICLE_HEADER[2];
      packet[3] = VEHICLE_HEADER[3];
      for (int i = sizeof(VEHICLE_HEADER); i < PACKET_SIZE; i++)
        packet[i] = payload[i - sizeof(VEHICLE_HEADER)];
      

      Serial.print("PACKET: ");
      Serial.println(packet);
      rf95.send((uint8_t*)packet, PACKET_SIZE); // cast pointer to unsigned character and send
      rf95.waitPacketSent(); // wait for send to finish
      
      // check for a response
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      Serial.println("Waiting for reply..."); delay(10);
      if (rf95.waitAvailableTimeout(1000))
      { 
        // Should be a reply message for us now   
        if (rf95.recv(buf, &len))
        {
          Serial.println("Received something, checking header...");
          String str((char*)buf);
          Serial.println(str.substring(0,4));
          if (str.substring(0,4).equals(GS_HEADER)) {
            // Message has valid header 
            Serial.println("Valid header.");
            Serial.print("Got reply: ");
            Serial.println((char*)buf);
            blinkLed(GREEN, 2, 200);
          }
          Serial.print("RSSI: ");
          Serial.println(rf95.lastRssi(), DEC);    
        }
        else
        {
          Serial.println("Receive failed");
          blinkLed(RED, 2, 1000);
        }
      }
      else
      {
        Serial.println("No reply, is there a listener around?");
        blinkLed(RED, 2, 200); 
      }
    } 
    else 
    {
      Serial.println("none");
      blinkLed(RED, 2, 100);
    }

    delay(1000); // breathing room
    gpsBuffer = ""; // clear gps buffer
    transmitReady = false; // read more gps data
  }
}
