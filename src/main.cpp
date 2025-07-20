#include <Arduino.h>
#include <definitions.h>

double timing = 7.5;
bool carryover = false;
int cycle_count = 0;
unsigned long cycles[250];
bool demod[128];
byte demod_pos = 0;
unsigned long last_miros = 0;
bool reading = false;
int zero_count = 0;
bool read_code[128];
bool code_received = false;

void findClock(){
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
    detachInterrupt(OCF_RFID_OUTSIDE_CLK);
    return;
  }
  cycles[cycle_count] = micros();
  cycle_count++;
  }
void storeTime(){
  unsigned long current_micros = micros();
  unsigned long diff = current_micros - last_miros;
  last_miros = current_micros;
  if (diff < timing*39){   // x * 37
    if (!reading){
      zero_count++;
    }else{
      demod[demod_pos] = 0;
      demod_pos++;
    }
  }else if (diff > timing*40 && diff < timing*55){ // x*40 - x*54.5
    if (!reading){
      if ((carryover && zero_count >= 9) || (!carryover && zero_count >= 10)){
        zero_count = 0;
        carryover = false;
        reading = true;
        for (int i = 0; i < 10; i++){
          demod[i] = 0;
        }
        demod[10] = 1;
        demod_pos = 11;
      }
      carryover = !carryover;
    }else{
      if (carryover){
        if (demod_pos > 126){
          reading = false;
          demod_pos = 0;
          zero_count = 0;
          carryover = false;
        }else{
          demod[demod_pos] = 0;
          demod[demod_pos + 1] = 1;
          demod_pos += 2;
        }
      }else{
        demod[demod_pos] = 1;
        demod_pos++;
      }
      carryover = !carryover;
    }
  }else if (diff > timing*55 && diff < timing*73){ // x*55.8 - x*73
    if (!reading){
      if (zero_count >= 10){
        zero_count = 0;
        reading = true;

        for (int i = 0; i < 10; i++){
          demod[i] = 0;
        }
        demod[10] = 1;
        demod[11] = 1;
        demod_pos = 12;
      }
      carryover = false;
      zero_count = 0;
    }else{
      if (demod_pos > 126){
        reading = false;
        demod_pos = 0;
        return;
      }
      demod[demod_pos] = 1;
      demod[demod_pos+1] = 1;
      demod_pos += 2;
    }
  }else{
    demod_pos = 0;
    reading = false;
    zero_count = 0;
  }
  if (demod_pos >= 128){
    reading = false;
    demod_pos = 0;
    code_received = true;
    for (int i = 0; i < 128; i++){
      read_code[i] = demod[i];
      demod[i] = 0;
    }
  }
}

void parse_id(){
  long long result = 0;
  int bit_count = 0;
  int skipped_bits = 0;
  // Extract 38bit chip id after header
  for (int i = 11; i < 53; i++) {
    // Turn into long long while removing spacing 1s
    bit_count++;
    if (bit_count > 8) {
      skipped_bits++;
      bit_count = 0;
      continue;
    }
    result |= ((long long)read_code[i] << (i - skipped_bits - 11));
  }
  Serial.printf("Chip id: %lld\n", result);
}

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println("Starting up...");
  pinMode(OCF_RFID_OUTSIDE_EN, OUTPUT);
  pinMode(OCF_RFID_OUTSIDE_CLK, INPUT);
  pinMode(OCF_RFID_OUTSIDE_DMOD, INPUT);
  digitalWrite(OCF_RFID_OUTSIDE_EN, HIGH);
  digitalWrite(OCF_RFID_INSIDE_EN, HIGH);
  sleep(10);
  attachInterrupt(OCF_RFID_OUTSIDE_CLK, findClock, RISING);
}

bool inside = true;
int count = 0;
void loop() {
  count++;
  if (count > 5000000){
    count = 0;
    inside = !inside;
    Serial.printf("Inside: %d\n", inside);
     if (inside){
    digitalWrite(OCF_RFID_INSIDE_EN, LOW);
    digitalWrite(OCF_RFID_OUTSIDE_EN, HIGH);
    detachInterrupt(OCF_RFID_OUTSIDE_DMOD);

  }else{
    digitalWrite(OCF_RFID_INSIDE_EN, HIGH);
    digitalWrite(OCF_RFID_OUTSIDE_EN, LOW);
    attachInterrupt(OCF_RFID_OUTSIDE_DMOD, storeTime, RISING);
  }
  }
 
  if (code_received){
    parse_id();
    code_received = false;
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