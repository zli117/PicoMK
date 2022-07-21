#ifndef CONFIG_MODIFIER_H_
#define CONFIG_MODIFIER_H_

#include <memory>
#include <vector>

#include "base.h"
#include "configuration.h"

class ConfigModifiersImpl;

class ConfigUIBase {
 public:
  ConfigUIBase(ConfigModifiersImpl* config_modifier, ScreenOutputDevice* screen,
               uint8_t screen_top_margin)
      : config_modifier_(config_modifier),
        screen_(screen),
        redraw_(true),
        screen_top_margin_(screen_top_margin) {}

  virtual void Draw() = 0;
  virtual void OnUp() = 0;
  virtual void OnDown() = 0;
  virtual void OnSelect() = 0;

 protected:
  inline uint8_t GetScreenNumRows() {
    return (screen_->GetNumRows() - screen_top_margin_) / 8;
  }

  ScreenOutputDevice::Font GetFont() { return ScreenOutputDevice::F8X8; }

  ConfigModifiersImpl* config_modifier_;
  ScreenOutputDevice* screen_;
  bool redraw_;
  const uint8_t screen_top_margin_;
};

class ListUI : public ConfigUIBase {
 public:
  ListUI(ConfigModifiersImpl* config_modifier, ScreenOutputDevice* screen,
         uint8_t screen_top_margin)
      : ConfigUIBase(config_modifier, screen, screen_top_margin),
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
             ConfigObject* global_config_object, uint8_t screen_top_margin)
      : ListUI(config_modifier, screen, screen_top_margin),
        global_config_object_(global_config_object),
        menu_items_({"Edit Config", "Save Config", "Load Default", "Exit"}) {}

  void Draw() override;
  void OnSelect() override;

 protected:
  uint32_t GetListLength() override;

  ConfigObject* global_config_object_;
  std::vector<std::string> menu_items_;
};

class ConfigObjectScreen : public ListUI {
 public:
  ConfigObjectScreen(ConfigModifiersImpl* config_modifier,
                     ScreenOutputDevice* screen, ConfigObject* config_object,
                     uint8_t screen_top_margin);

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
                   ScreenOutputDevice* screen, ConfigList* config_list,
                   uint8_t screen_top_margin);

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
                  ScreenOutputDevice* screen, ConfigInt* config_int,
                  uint8_t screen_top_margin)
      : ConfigUIBase(config_modifier, screen, screen_top_margin),
        config_int_(config_int) {}

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
                    ScreenOutputDevice* screen, ConfigFloat* config_float,
                    uint8_t screen_top_margin)
      : ConfigUIBase(config_modifier, screen, screen_top_margin),
        config_float_(config_float) {}

  void Draw() override;
  void OnUp() override;
  void OnDown() override;
  void OnSelect() override;

 protected:
  ConfigFloat* config_float_;
};

class ConfigModifiersImpl : virtual public ConfigModifier {
 public:
  ConfigModifiersImpl(ConfigObject* global_config, uint8_t screen_tag,
                      uint8_t screen_top_margin = 0)
      : global_config_(global_config),
        screen_tag_(screen_tag),
        screen_top_margin_(screen_top_margin),
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
  const uint8_t screen_top_margin_;
  std::shared_ptr<ScreenOutputDevice> screen_;
  std::vector<std::shared_ptr<ConfigUIBase>> ui_stack_;
  bool is_config_;  // No need to lock since there's no OutputTick
  bool pop_ui_;
};

Status RegisterConfigModifier(uint8_t screen_tag);

#endif /* CONFIG_MODIFIER_H_ */