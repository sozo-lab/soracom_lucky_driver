#include <limits.h>
#include <stdint.h>

#include <Luckyshield_light.h>
#include <avr/wdt.h>
#include <lorawan_client.h>

// define constants
#define COUNT_INTERVAL_FOR_MAIN_LOOP 30000u
#define LIGHT_PIN                    A0

/**
 * The loop counter for detect overflow as timer.
 */
class LoopCounter
{
public:
  LoopCounter(unsigned int interval);
  explicit operator bool();
  LoopCounter& operator ++();
  LoopCounter& next();

private:
  const unsigned int interval_;
  unsigned int now_count_;
};

struct FormatedData
{
  uint16_t humidity    : 10;
  uint16_t light       : 10;
  int16_t  temperature : 11;
  bool     pir         :  1;
  uint16_t accelerometer_x;
  uint16_t accelerometer_y;
  uint16_t accelerometer_z;
};

// Faward declaration
void reboot();

// Grobal variables
LoRaWANClient lora_client;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  lucky.begin();
  if (!lora_client.connect()) {
    Serial.println("Failed to first connect. Reboot...");
    reboot();
  }
  if (sizeof(FormatedData) != 11) {
    Serial.print("Wrong data size! : ");
    Serial.println(sizeof(FormatedData));
  }
}

void loop()
{
  static LoopCounter rate_manager(COUNT_INTERVAL_FOR_MAIN_LOOP);

  if (++rate_manager) {
    lucky.accelerometer().read();
    FormatedData fd = {lucky.environment().humidity() * 10,
                       analogRead(LIGHT_PIN),
                       lucky.environment().temperature() * 10,
                       lucky.gpio().digitalRead(PIR) == LOW,
                       lucky.accelerometer().x(),
                       lucky.accelerometer().y(),
                       lucky.accelerometer().z()};
    lora_client.sendBinary(reinterpret_cast<byte*>(&fd), sizeof(fd));
  }
}

LoopCounter::LoopCounter(unsigned int interval)
  : interval_ (interval),
    now_count_ (interval)
{
}

LoopCounter::operator bool()
{
  return now_count_ == 0;
}

LoopCounter& LoopCounter::next()
{
  if (--now_count_ < 0)
    now_count_ = interval_;
  return *this;
}

LoopCounter& LoopCounter::operator++() {
  return next();
}

void reboot()
{
  wdt_disable();
  wdt_enable(WDTO_15MS);
  for (unsigned int i = 0; i != UINT_MAX; ++i);
}
