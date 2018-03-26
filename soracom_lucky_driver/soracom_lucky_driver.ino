#include <stdint.h>

#include <Luckyshield_light.h>
#include <lorawan_client.h>

// define constants by user.
#define LIGHT_PIN                   A0
#define RESET_PIN                   7
#define INTERVAL_TIME_FOR_SENDING_S (2u * 60u)
#define RATE_TIME_HZ                2u

// define constants for system.
#define FORMATED_DATA_OBJECT_SIZE    4
#define INTERVAL_COUNT_FOR_MAIN_LOOP (INTERVAL_TIME_FOR_SENDING_S * RATE_TIME_HZ)

/**
 * Destructor makes update the value by now millis().
 */
class TimeUpdateGuard
{
public:
  /**
   * Hold the target reference.
   *
   * @param target The update target.
   */
  TimeUpdateGuard(unsigned long& target);

  /**
   * Update target by now millis().
   */
  ~TimeUpdateGuard();

private:
  unsigned long& target_; //!< The reference of target.
};

/**
 * Class to update variable on destruct time.
 *
 * @code
 * int v = 10;
 * {
 *   DestructUpdateGuard g(v, 50);
 *   assert(v == 10);
 * } // destruct g.
 * assert(v == 50);
 * @endcode
 *
 * @tparam T The variable type.
 */
template<typename T>
class DestructUpdateGuard
{
public:
  /**
   * Hold the target variable.
   */
  DestructUpdateGuard(T& target, T value);

  /**
   * Update target with the preset value.
   */
  ~DestructUpdateGuard();

private:
  T& target_;
  T value_;
};

/**
 * Class to help run loops at a desired frequency.
 *
 * @code
 * Rate r(2); // 2 Hz work
 * while (true) {
 *   // do your work
 *   r.sleep();
 * }
 * @endcode
 */
class Rate
{
public:
  /**
   * Constructor, creates a Rate.
   *
   * @param frequency The desired rate to run at in Hz.
   */
  Rate(double frequency);

  /**
   * Sets the start time for the rate to now.
   */
  void reset();

  /**
   * Sleeps for any leftover time in a cycle.
   *
   * Calculated from the last time sleep, reset, or the constructor was called.
   *
   * @return True if the desired rate was met for the cycle, false otherwise.
   */
  bool sleep();

private:
  unsigned long interval_duration_; //!< interval time for millis function.
  unsigned long last_time_; //!< The time of sleep, seret, or the constructor was called.
};

/**
 * The loop counter for detect overflow as like timer.
 *
 * @code
 * LoopCounter lc(3);
 * assert(!lc);
 * assert(--lc);
 * assert(--lc);
 * assert(!--lc);
 * @endcode
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
 *
 * @code
 * Blinker b(false);
 * assert(!b);
 * b.blink();
 * assert(b);
 * b.blink();
 * assert(!b);
 * @endcode
 */
class Blinker
{
public:
  /**
   * Setup first value.
   *
   * @param state First state.
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
 * @code
 * VolatilityValue<int> v(200);
 * assert(v.get() == 200);
 * v.set(10);
 * assert(v.get() == 10);
 * assert(v.get() == 200);
 * @endcode
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
   *
   * @param init The default value for reset.
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
  pinMode(LIGHT_PIN, INPUT);
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
  static Rate rate_manager(RATE_TIME_HZ);
  static LoopCounter loop_counter(INTERVAL_COUNT_FOR_MAIN_LOOP);
  static PinBlinker led_blinker(LED_BUILTIN, LOW);
  static VolatilityValue<bool> human_detection(false);

  if (!--loop_counter) {
    led_blinker.blink();
    FormatedData fd =
        {analogRead(LIGHT_PIN),
         lucky.environment().humidity() * 10,
         lucky.environment().temperature() * 10,
         human_detection.get()};
    if (!lora_client.sendBinary(reinterpret_cast<byte*>(&fd), sizeof(fd)))
      reboot();
  } else {
    if (lucky.gpio().digitalRead(PIR) == LOW)
      human_detection.set(true);
  }
  rate_manager.sleep();
}

inline TimeUpdateGuard::TimeUpdateGuard(unsigned long& target)
  : target_(target)
{
}

inline TimeUpdateGuard::~TimeUpdateGuard()
{
  target_ = millis();
}

template<typename T>
inline DestructUpdateGuard<T>::DestructUpdateGuard(T& target, T value)
  : target_(target),
    value_(value)
{
}

template<typename T>
inline DestructUpdateGuard<T>::~DestructUpdateGuard()
{
  target_ = value_;
}

inline Rate::Rate(double frequency)
  : interval_duration_(1000. / frequency),
    last_time_(millis())
{
}

inline void Rate::reset()
{
  last_time_ = millis();
}

bool Rate::sleep()
{
  DestructUpdateGuard<decltype(last_time_)> update_guard(last_time_, last_time_ + interval_duration_);
  const unsigned long wait_time = last_time_ + interval_duration_ - millis();
  if (wait_time >= interval_duration_)
    return false;
  delay(wait_time);
  return true;
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
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
}

inline void set_pin(int pin, bool value)
{
  digitalWrite(pin, value ? HIGH : LOW);
}
