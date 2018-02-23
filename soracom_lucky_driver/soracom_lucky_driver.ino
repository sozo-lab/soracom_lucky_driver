#include <Luckyshield_light.h>
#include <MsTimer2.h>
#include <avr/wdt.h>
#include <lorawan_client.h>

#define ANALOGPIN A0
#define PIR_PIN 4
#define MAX_COUNT_OF_RETRY 5
#define MAX_COUNT_OF_RECONNECTION 3

void update_data();
bool try_send_data(float value, char prefix, int prec);
void send_data(float value, char prefix, int prec);
void rebood_program();
void delay_with_blink(unsigned long delay_time, unsigned long blink_interval);

// grobal variables
LoRaWANClient client;
volatile bool human_detection;

void setup() {
  human_detection = false;
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIR_PIN, OUTPUT);
  lucky.begin();
  Serial.begin(9600);
  if (!client.connect()) {
    Serial.println("failed to connect. Halt...");
    while (true);
  }
  MsTimer2::set(3ul * 60 * 1000, &update_data);
  MsTimer2::start();
}

void loop() {
  if (digitalRead(PIR_PIN)
    human_detection = true;
}

void update_data()
{
  static int sensor_index = 0;

  switch (sensor_index++) {
  case 0:
    send_data(lucky.environment().temperature(), 't', 2);
    break;
  case 1:
    send_data(lucky.environment().humidity(), 'h', 2);
    break;
  case 2:
    send_data(human_detection, 'p', 1);
    human_detection = false;
    break;
  case 3:
    send_data(analogRead(ANALOGPIN), 'l', 1);
  }

  sensor_index %= 4;
}

bool try_send_data(float value, char prefix, int prec)
{
  char buf[6];
  dtostrf(value, -1, prec, buf);
  char json[12];
  sprintf(json, "{\"%c\":%s}", prefix, buf);
  return client.sendData(json);
}

void send_data(float value, char prefix, int prec)
{
  int count_of_error = 0;
  int count_of_reconnection = 0;
  for (bool is_good_state = try_send_data(value, prefix, prec);
       !is_good_state;
       is_good_state = try_send_data(value, prefix, prec)) {
    if (++count_of_error > MAX_COUNT_OF_RETRY) {
      Serial.println("Error: many fail the resend function");
      client.connect(false);
      if (++count_of_reconnection > MAX_COUNT_OF_RECONNECTION) {
        Serial.println("Reboot: cannot recconect and resend data");
        reboot_program();
      }
    }
    delay_with_blink(random(500, 3000), 200);
  }
  delay_with_blink(3ul * 60 * 1000, 1000);
}

void reboot_program()
{
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (true);  
}

void delay_with_blink(unsigned long delay_time, unsigned long blink_interval)
{
  static int now_state = LOW;
  const unsigned long count_of_blink = delay_time / blink_interval;

  for (unsigned long i = count_of_blink; i != 0; --i) {
    now_state = now_state == HIGH ? LOW : HIGH;
    digitalWrite(LED_BUILTIN, now_state);
    delay(blink_interval);
  }
  delay(delay_time % blink_interval);
}
