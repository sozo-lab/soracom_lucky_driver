#include <Luckyshield_light.h>
#include <lorawan_client.h>
#include <avr/wdt.h>

#define ANALOGPIN A0
#define MAX_COUNT_OF_RETRY 5
#define MAX_COUNT_OF_RECONNECTION 3

bool try_send_data(float value, char prefix, int prec);
void send_data(float value, char prefix, int prec);
void rebood_program();

LoRaWANClient client;

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

void setup() {
  lucky.begin();
  Serial.begin(9600);
  if (!client.connect()){
    Serial.println("failed to connect. Halt...");
    while (true);
  }
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  while (!client.connect(false));
  send_data(lucky.environment().temperature(), 't', 2);
  send_data(lucky.environment().humidity(), 'h', 2);
  send_data(lucky.gpio().digitalRead(PIR), 'p', 1);
  send_data(analogRead(ANALOGPIN), 'l', 1);
}

bool try_send_data(float value, char prefix, int prec)
{
  char buf[6];
  dtostrf(value, -1, prec, buf);
  char json[12];
  sprintf(json, "{\"%c\":%s}", prefix, buf);
  bool res = client.sendData(json);
  return res;
}

void send_data(float value, char prefix, int prec)
{
  int count_of_error = 0;
  int count_of_reconnection = 0;
  for (bool res = try_send_data(value, prefix, prec);
       !res;
       res = try_send_data(value, prefix, prec)) {
    if (++count_of_error > MAX_COUNT_OF_RETRY) {
      Serial.print("Error: many fail the resend function");
      client.connect(false);
      if (++count_of_reconnection > MAX_COUNT_OF_RECONNECTION) {
        Serial.print("Reboot: cannot recconect and resend data");
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
