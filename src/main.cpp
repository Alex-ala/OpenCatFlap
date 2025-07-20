#include <Arduino.h>
#include <definitions.h>
#include <OCF_RFID.h>
bool inited = false;

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println("Starting up...");
  OCF_RFID::init();
  inited = true;
  Serial.println("OCF_RFID initialized.");
}
int count = 0;
void loop() {
  if (!inited) {
    delay(100);
    return;
  }
  count++;
  if (count > 100000000){
    count = 0;
    OCF_RFID::swap_direction();
  }
}

/*
Expected bits:
Chip id dec: 202300001841
Chip id binary:101111 00011010 00000101 00001110 00110001
Chip country dec: 900
Chip country binary: 1110000100
Chip idLSB: 10001100 01110000 10100000 01011000 111101
Chip id RF: 10001100101110000110100000101011000111110100110000111
00000000001100011001011100001101000001010110001111101 <- Calculated
00000000001100011001011100001101000001010110001111101001100001111100000001000000011011111101101011111000000001000000001000000001 <- Scanned
            |byte 0|||byte 1|||byte 3|||byte 4|||byte 5|||byte 6|
                    |        |        |        |        | 
               Separator 1
                                                      2 bit + 8 bit country
---------------------------------------------------------------------------
Chip 2: 756 095310133740
Chip 2 binary: 01011000110000111011010011100111101100
chip 2 lsb: 00110111 10011100 10110111 00001100 011010
Chip2 RF: 
00000000001001101111100111001101101111000011001011010 <- Calculated
00000000001001101111100111001101101111000011001011010 <- Scanned
----------------------------------------------------------------------------
Chip3: 158002010656
Binary: 10010011001001101001110101101000100000
LSB: 00000100 01011010 11100101 10010011 001001
RF:
00000000001000001001010110101111001011100100111001001 <- Calculated
00000000001000001001010110101111001011100100111001001 <- Scanned
*/



/*
Measure chip frequency:
  unsigned long cycles[250];
  void storeTime(){
    if (cycle_count >= 250){
      for (int i = 249; i > 0; i--){
        cycles[i] = cycles[i] - cycles[i-1];
      }
      cycles[0] = 0;
      double average = 0;
      for (int i = 1; i < 250; i++){
        average += cycles[i];
      }
      average = average / 250;
      Serial.printf("Average: %f\n", average);
      cycle_count = 0;
      detachInterrupt(OCF_RFID_INSIDE_CLK);
      return;
    }
    cycles[cycle_count] = micros();
    cycle_count++;
  }
  void setup() {
    attachInterrupt(OCF_RFID_INSIDE_CLK, storeTime, RISING);
  }

*/
