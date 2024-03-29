#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cJSON/cJSON.h"
#include "utils.h"

// Create an object from a list of key value pairs
#define CONFIG_OBJECT(...) \
  (std::shared_ptr<ConfigObject>(new ConfigObject({__VA_ARGS__})))

// Create one key value pair from a string name to a subconfig.
#define CONFIG_OBJECT_ELEM(name, value) \
  { (name), (value) }

// Create a list of subconfigs
#define CONFIG_LIST(...) \
  (std::shared_ptr<ConfigList>(new ConfigList({__VA_ARGS__})))

// Convenient macro for creating a special list with length of two
#define CONFIG_PAIR(first, second) \
  (std::shared_ptr<ConfigPair>(new ConfigPair((first), (second))))

// Value can only be in range [min, max]. value is the compile time default.
#define CONFIG_INT(value, min, max) \
  (std::shared_ptr<ConfigInt>(new ConfigInt((value), (min), (max))))

// Value can only be in range [min, max]. Each up/down on the config modifier
// increases/decreases the value by resolution amount. value is the compile time
// default.
#define CONFIG_FLOAT(value, min, max, resolution) \
  (std::shared_ptr<ConfigFloat>(                  \
      new ConfigFloat((value), (min), (max), (resolution))))

class Config {
 public:
  enum Type {
    INVALID = 0,
    OBJECT,
    LIST,
    INTEGER,
    FLOAT,
  };
  virtual Type GetType() const { return INVALID; }
  virtual cJSON* ToCJSON() const { return NULL; }
  virtual Status FromCJSON(const cJSON* json) { return ERROR; }
};

class ConfigObject : public Config {
 public:
  Type GetType() const override final { return OBJECT; }
  ConfigObject() = default;
  ConfigObject(const std::map<std::string, std::shared_ptr<Config>>& members)
      : members_(members) {}
  ConfigObject(std::initializer_list<
               std::pair<const std::string, std::shared_ptr<Config>>>
                   l)
      : members_(l) {}

  std::map<std::string, std::shared_ptr<Config>>* GetMembers() {
    return &members_;
  }
  const std::map<std::string, std::shared_ptr<Config>>* GetMembers() const {
    return &members_;
  }

  std::string ToJSON() const;
  cJSON* ToCJSON() const override;
  Status FromCJSON(const cJSON* json) override;

 private:
  std::map<std::string, std::shared_ptr<Config>> members_;
};

class ConfigList : public Config {
 public:
  Type GetType() const override final { return LIST; }

  ConfigList() = default;
  ConfigList(const std::vector<std::shared_ptr<Config>>& list) : list_(list) {}
  ConfigList(std::initializer_list<std::shared_ptr<Config>> l) : list_(l) {}

  std::vector<std::shared_ptr<Config>>* GetList() { return &list_; }
  const std::vector<std::shared_ptr<Config>>* GetList() const { return &list_; }

  cJSON* ToCJSON() const override;
  Status FromCJSON(const cJSON* json) override;

 private:
  std::vector<std::shared_ptr<Config>> list_;
};

class ConfigPair : public ConfigList {
 public:
  ConfigPair(std::shared_ptr<Config> first, std::shared_ptr<Config> second)
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

  cJSON* ToCJSON() const override;
  Status FromCJSON(const cJSON* json) override;

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

  cJSON* ToCJSON() const override;
  Status FromCJSON(const cJSON* json) override;

 private:
  float value_;
  const float min_;
  const float max_;
  const float resolution_;
};

// If return is ERROR, default_config will be in an invalid state.
// default_config is modified in place.
Status ParseJsonConfig(const std::string& json, Config* default_config);

#endif /* CONFIGURATION_H_ */
