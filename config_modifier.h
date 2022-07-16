#ifndef CONFIG_MODIFIER_H_
#define CONFIG_MODIFIER_H_

#include <memory>
#include <vector>

#include "base.h"
#include "configuration.h"

class ConfigModifiersImpl;

class ConfigUIBase {
 public:
  ConfigUIBase(ConfigModifiersImpl* config_modifier, ScreenOutputDevice* screen)
      : config_modifier_(config_modifier), screen_(screen), redraw_(true) {}

  virtual void Draw() = 0;
  virtual void OnUp() = 0;
  virtual void OnDown() = 0;
  virtual void OnSelect() = 0;

 protected:
  ConfigModifiersImpl* config_modifier_;
  ScreenOutputDevice* screen_;
  bool redraw_;
};

class ListUI : public ConfigUIBase {
 public:
  ListUI(ConfigModifiersImpl* config_modifier, ScreenOutputDevice* screen)
      : ConfigUIBase(config_modifier, screen),
        current_highlight_(0),
        draw_start_(0) {}

  void OnUp() override;
  void OnDown() override;

 protected:
  virtual uint32_t GetListLength() = 0;

  virtual void ListDrawImpl(const std::vector<std::string>& content);

  uint32_t current_highlight_;
  uint32_t draw_start_;
};

class HomeScreen : public ListUI {
 public:
  HomeScreen(ConfigModifiersImpl* config_modifier, ScreenOutputDevice* screen,
             ConfigObject* global_config_object)
      : ListUI(config_modifier, screen),
        global_config_object_(global_config_object) {}

  void Draw() override;
  void OnSelect() override;

 protected:
  uint32_t GetListLength() override;

  ConfigObject* global_config_object_;
};

class ConfigObjectScreen : public ListUI {
 public:
  ConfigObjectScreen(ConfigModifiersImpl* config_modifier,
                     ScreenOutputDevice* screen, ConfigObject* config_object);

  void Draw() override;
  void OnSelect() override;

 protected:
  uint32_t GetListLength() override;

  ConfigObject* config_object_;
  std::vector<std::string> keys_;
};

class ConfigListScreen : public ListUI {
 public:
  ConfigListScreen(ConfigModifiersImpl* config_modifier,
                   ScreenOutputDevice* screen, ConfigList* config_list);

  void Draw() override;
  void OnSelect() override;

 protected:
  uint32_t GetListLength() override;

  ConfigList* config_list_;
  std::vector<std::string> indices_;
};

class ConfigIntScreen : public ConfigUIBase {
 public:
  ConfigIntScreen(ConfigModifiersImpl* config_modifier,
                  ScreenOutputDevice* screen, ConfigInt* config_int)
      : ConfigUIBase(config_modifier, screen), config_int_(config_int) {}

  void Draw() override;
  void OnUp() override;
  void OnDown() override;
  void OnSelect() override;

 protected:
  ConfigInt* config_int_;
};

class ConfigFloatScreen : public ConfigUIBase {
 public:
  ConfigFloatScreen(ConfigModifiersImpl* config_modifier,
                    ScreenOutputDevice* screen, ConfigFloat* config_float)
      : ConfigUIBase(config_modifier, screen), config_float_(config_float) {}

  void Draw() override;
  void OnUp() override;
  void OnDown() override;
  void OnSelect() override;

 protected:
  ConfigFloat* config_float_;
};

class ConfigModifiersImpl : virtual public ConfigModifier {
 public:
  ConfigModifiersImpl(ConfigObject* global_config, uint8_t screen_tag)
      : global_config_(global_config),
        screen_tag_(screen_tag),
        screen_(NULL),
        is_config_(false) {}

  void SetScreenOutputs(
      const std::vector<std::shared_ptr<ScreenOutputDevice>>* device) override;

  void SetConfigMode(bool is_config_mode) override;

  void InputLoopStart() override {}
  void InputTick() override {}
  void OutputTick() override {}

  void StartOfInputTick() override {}
  void FinalizeInputTickOutput() override;

  void Up() override;
  void Down() override;
  void Select() override;

  virtual void PushUI(std::shared_ptr<ConfigUIBase> ui);
  virtual void PopUI();
  virtual void EndConfig();

 protected:
  ConfigObject* const global_config_;
  const uint8_t screen_tag_;
  std::shared_ptr<ScreenOutputDevice> screen_;
  std::vector<std::shared_ptr<ConfigUIBase>> ui_stack_;
  bool is_config_;  // No need to lock since there's no OutputTick
  bool pop_ui_;
};

#endif /* CONFIG_MODIFIER_H_ */