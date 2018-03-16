#include <FlexiTimer2.h>
#include <Luckyshield_light.h>
#include <TimerOne.h>
#include <avr/wdt.h>
#include <limits.h>
#include <lorawan_client.h>

// constants of settings
#define ANALOGPIN                 A0
#define MAX_COUNT_OF_RETRY        5
#define MAX_COUNT_OF_RECONNECTION 3
#define SENDING_INTERVAL_MS       (2ul * 60ul * 1000ul)
#define REBOOT_INTERVAL_S         (24ul * 60ul * 60ul)

// constants by calculation
#define REBOOT_IGNORE_COUNT REBOOT_INTERVAL_S

void update_data(); //!< integrate sending function for all sensors
bool try_send_data(float value, char prefix, int prec); //!< send once function
void send_data(float value, char prefix, int prec); //!< send with re-try function for one sensor
void reboot_program(); //!< reboot the program with Hardware Reset(WDT)
void reboot_program_periodically(); //!< reboot the program with Hardware Reset(WDT)
void delay_with_blink(unsigned long delay_time, unsigned long blink_interval); //!< delay for miliseconds with blinking

// grobal variables
LoRaWANClient client;
volatile bool human_detection;

void setup() {
  // disable all modules
  FlexiTimer2::stop();
  Timer1.stop();
  Serial.end();
  // setup variables
  human_detection = false;
  pinMode(LED_BUILTIN, OUTPUT);
  // setup periodically reboot timer
  Timer1.initialize();
  Timer1.attachInterrupt(&reboot_program_periodically);
  // setup modules
  lucky.begin();
  Serial.begin(9600);
  if (!client.connect()) {
    Serial.println("failed to connect. Reboot...");
    reboot_program();
  }
  // setup interrupts
  FlexiTimer2::set(SENDING_INTERVAL_MS, &update_data);
  FlexiTimer2::start();
}

void loop() {
  static const unsigned int interval = 3000u;
  static unsigned int loop_count = interval;
  static int led_state = HIGH;
  if (--loop_count == 0) {
    loop_count = interval;
    digitalWrite(LED_BUILTIN, led_state = led_state == HIGH ? LOW : HIGH);
  }
  if (lucky.gpio().digitalRead(PIR) == LOW)
    human_detection = true;
}

void update_data()
{
  static int sensor_index = 0;
  interrupts();
  Serial.print("interrupt ");
  Serial.println(sensor_index);

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
}

void reboot_program()
{
  wdt_disable();
  wdt_enable(WDTO_15MS);
  interrupts();
  for (unsigned int i = 0; i != UINT_MAX; ++i);
}

void reboot_program_periodically()
{
  static unsigned long enter_count = 0;
  interrupts();
  if (++enter_count <= REBOOT_IGNORE_COUNT) {
    digitalWrite(LED_BUILTIN, enter_count % 2 ? HIGH : LOW);
    return;
  }
  wdt_disable();
  wdt_enable(WDTO_15MS);
  
  for (unsigned int i = 0; i != UINT_MAX; ++i);
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
