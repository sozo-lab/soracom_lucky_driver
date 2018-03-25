#include <limits.h>
#include <stdint.h>

#include <Luckyshield_light.h>
#include <avr/wdt.h>
#include <lorawan_client.h>

// define constants by user.
#define COUNT_INTERVAL_FOR_MAIN_LOOP 30000u
#define LIGHT_PIN                    A0

// define constants for system.
#define FORMATED_DATA_OBJECT_SIZE 4

/**
 * The loop counter for detect overflow as like timer.
 */
class LoopCounter
{
public:
  /**
   * The initialization of a counter with the limit value for active.
   *
   * @param interval The limit value for overflow detection.
   */
  LoopCounter(unsigned int interval);

  /**
   * The cast operator for as bool.
   */
  explicit operator bool();

  /**
   * The decrement operator with underflow detector.
   * When underflow, this function reset count to the interval.
   *
   * @return This instance.
   */
  LoopCounter& operator --();

  /**
   * The decrement function with underflow detector.
   * When underflow, this function reset count to the interval.
   *
   * @return This instance.
   */
  LoopCounter& next();

private:
  unsigned int interval_; //!< The limit value for overflow detection.
  unsigned int now_count_; //!< The now value.
};

/**
 * The blinker for HIGH and LOW.
 */
class Blinker
{
public:
  /**
   * Setup first value.
   */
  Blinker(bool state);

  /**
   * The cast operator for as HIGH or LOW.
   */
  operator bool();

  /**
   * Change to another state.
   */
  void blink();

private:
  bool state_; //!< The now state.
};

/**
 * The PIN Blinker.
 */
class PinBlinker : private Blinker
{
public:
  /**
   * Setup first value at the pin.
   *
   * @param pin The target pin number.
   * @param state The first state.
   */
  PinBlinker(int pin, bool state);

  /**
   * Change PIN to anthor state.
   */
  void blink();

private:
  int pin_; //!< The pin number.
};

/**
 * The value of it is reset on reading.
 *
 * @tparam T The value type.
 * @tparam init The init value for reset.
 */
template<typename T>
class VolatilityValue
{
public:
  /**
   * Set the init value for reading.
   */
  VolatilityValue(T init);

  /**
   * Set the value.
   *
   * @param value A new value.
   */
  void set(T value);

  /**
   * Get the value with reset.
   *
   * @return The value before reset.
   */
  T get();

private:
  T init_; //!< The init value for reset.
  T value_; //!< The now value.
};

/**
 * The binary layout for Soracom binary parser.
 */
struct FormatedData
{
  uint16_t light       : 10; //!< 0xffc00000 is light value.
  uint16_t humidity    : 10; //!< 0x003ff000 is humidity value.
  uint16_t temperature : 11; //!< 0x00000ffe is temperature value.
  bool     pir         :  1; //!< 0x00000001 is PIR value.
};

// Faward declarations

/**
 * The reboot function.
 *
 * With hardware reset.
 */
void reboot();

/**
 * Set pin state by bool value.
 *
 * @param value It means: true as HIGH, false as LOW.
 */
void set_pin(int pin, bool value);

// Grobal variables
LoRaWANClient lora_client; //!< The handle instance for LoRa device.

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  lucky.begin();
  if (!lora_client.connect()) {
    Serial.println("Failed to first connect. Reboot...");
    reboot();
  }
  if (sizeof(FormatedData) != FORMATED_DATA_OBJECT_SIZE) {
    Serial.print("Wrong formated data size! : ");
    Serial.println(sizeof(FormatedData));
  }
}

void loop()
{
  static LoopCounter rate_manager(COUNT_INTERVAL_FOR_MAIN_LOOP);
  static PinBlinker led_blinker(LED_BUILTIN, LOW);
  static VolatilityValue<bool> human_detection(false);

  if (!--rate_manager) {
    led_blinker.blink();
    FormatedData fd = 
        {lucky.environment().humidity() * 10,
         analogRead(LIGHT_PIN),
         lucky.environment().temperature() * 10,
         human_detection.get()};
    lora_client.sendBinary(reinterpret_cast<byte*>(&fd), sizeof(fd));
  } else {
    if (lucky.gpio().digitalRead(PIR) == LOW)
      human_detection.set(true);
  }
}

inline LoopCounter::LoopCounter(unsigned int interval)
  : interval_(interval),
    now_count_()
{
}

inline LoopCounter::operator bool()
{
  return now_count_;
}

inline LoopCounter& LoopCounter::next()
{
  now_count_ = now_count_ != 0 ? now_count_ - 1 : interval_;
  return *this;
}

inline LoopCounter& LoopCounter::operator--() {
  return next();
}

inline Blinker::Blinker(bool state)
  : state_(state)
{
}

inline Blinker::operator bool()
{
  return state_;
}

inline void Blinker::blink()
{
  state_ = !state_;
}

PinBlinker::PinBlinker(int pin, bool state)
  : Blinker(state),
    pin_(pin)
{
  pinMode(pin, OUTPUT);
  set_pin(pin, state);
}

void PinBlinker::blink()
{
  Blinker::blink();
  set_pin(pin_, *this);
}

template<typename T>
inline VolatilityValue<T>::VolatilityValue(T init)
  : init_(init),
    value_(init)
{
}

template<typename T>
inline void VolatilityValue<T>::set(T value)
{
  value_ = value;
}

template<typename T>
inline T VolatilityValue<T>::get()
{
  T last_value = value_;
  value_ = init_;
  return last_value;
}

void reboot()
{
  wdt_disable();
  wdt_enable(WDTO_15MS);
  for (unsigned int i = 0; i != UINT_MAX; ++i);
}

inline void set_pin(int pin, bool value)
{
  digitalWrite(pin, value ? HIGH : LOW);
}
