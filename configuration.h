#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define CONFIG_OBJECT(...) \
  (std::unique_ptr<ConfigObject>(new ConfigObject({__VA_ARGS__})))

#define CONFIG_OBJECT_ELEM(name, value) \
  { (name), (value) }

#define CONFIG_LIST(...) \
  (std::unique_ptr<ConfigList>(new ConfigList({__VA_ARGS__})))

#define CONFIG_PAIR(first, second) \
  (std::unique_ptr<ConfigPair>(new ConfigPair((first), (second))))

#define CONFIG_INT(value, min, max) \
  (std::unique_ptr<ConfigInt>(new ConfigInt((value), (min), (max))))

#define CONFIG_FLOAT(value, min, max) \
  (std::unique_ptr<ConfigFloat>(new ConfigFloat((value), (min), (max))))

class Config {
 public:
  enum Type {
    OBJECT = 0,
    LIST,
    STRING,
    INTEGER,
    FLOAT,
  };
  virtual Type GetType() const = 0;
};

class ConfigObject : public Config {
 public:
  Type GetType() const override final { return OBJECT; }
  ConfigObject() = default;
  ConfigObject(const std::map<std::string, std::unique_ptr<Config>>& members)
      : members_(members) {}
  ConfigObject(std::initializer_list<
               std::pair<const std::string, std::unique_ptr<Config>>>
                   l)
      : members_(l) {}

  std::map<std::string, std::unique_ptr<Config>>* GetMembers() {
    return &members_;
  }
  const std::map<std::string, std::unique_ptr<Config>>* GetMembers() const {
    return &members_;
  }

 private:
  std::map<std::string, std::unique_ptr<Config>> members_;
};

class ConfigList : public Config {
 public:
  Type GetType() const override final { return LIST; }

  ConfigList() = default;
  ConfigList(const std::vector<std::unique_ptr<Config>>& list) : list_(list) {}
  ConfigList(std::initializer_list<std::unique_ptr<Config>> l) : list_(l) {}

  std::vector<std::unique_ptr<Config>>* GetList() { return &list_; }
  const std::vector<std::unique_ptr<Config>>* GetList() const { return &list_; }

 private:
  std::vector<std::unique_ptr<Config>> list_;
};

class ConfigPair : public ConfigList {
 public:
  ConfigPair(std::unique_ptr<Config> first, std::unique_ptr<Config> second)
      : ConfigList({std::move(first), std::move(second)}) {}
};

class ConfigInt : public Config {
 public:
  Type GetType() const override final { return INTEGER; }

  ConfigInt(int32_t value, int32_t min, int32_t max)
      : value_(value), min_(min), max_(max) {}

  std::pair<int32_t, int32_t> GetMinMax() const { return {min_, max_}; }
  int32_t GetValue() const { return value_; }
  void SetValue(int32_t value) { value_ = value; }

 private:
  int32_t value_;
  const int32_t min_;
  const int32_t max_;
};

class ConfigFloat : public Config {
 public:
  Type GetType() const override final { return FLOAT; }

  ConfigFloat(float value, float min, float max, float resolution)
      : value_(value), min_(min), max_(max), resolution_(resolution) {}

  std::pair<float, float> GetMinMax() const { return {min_, max_}; }
  float GetResolution() const { return resolution_; }
  float GetValue() const { return value_; }
  void SetValue(float value) { value_ = value; }

 private:
  float value_;
  const float min_;
  const float max_;
  const float resolution_;
};

class Configuration {
 public:
  static Configuration* GetConfig();

  // Joystick X, Y accelaration profile
  inline const std::vector<std::pair<uint16_t, uint8_t>>* GetJoystickXProfile()
      const {
    return &joystick_x_profile_;
  }
  inline const std::vector<std::pair<uint16_t, uint8_t>>* GetJoystickYProfile()
      const {
    return &joystick_y_profile_;
  }
  inline uint8_t GetJoystickScanDivider() const {
    return joystick_scan_frequency_divider_;
  }
  inline uint32_t GetJoystickCalibrationSamples() const {
    return joystick_calibration_samples_;
  }
  inline uint32_t GetJoystickCalibrationThreshold() const {
    return joystick_calibration_threshold_;
  }
  void SetJoystickXProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);
  void SetJoystickYProfile(
      const std::vector<std::pair<uint16_t, uint8_t>>& profile);
  void SetJoystickScanDivider(uint8_t divider) {
    joystick_scan_frequency_divider_ = divider;
  }
  void SetJoystickCalibrationSamples(uint32_t calibration_samples) {
    joystick_calibration_samples_ = calibration_samples;
  }
  void SetJoystickCalibrationThreshold(uint32_t calibration_threshold) {
    joystick_calibration_threshold_ = calibration_threshold;
  }

 private:
  Configuration() = default;

  std::vector<std::pair<uint16_t, uint8_t>> joystick_x_profile_;
  std::vector<std::pair<uint16_t, uint8_t>> joystick_y_profile_;
  uint8_t joystick_scan_frequency_divider_;
  uint32_t joystick_calibration_samples_;
  uint32_t joystick_calibration_threshold_;

  static Configuration* singleton_;
};

#endif /* CONFIGURATION_H_ */