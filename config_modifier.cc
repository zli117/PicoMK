#include "config_modifier.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base.h"
#include "runner.h"

////////////////////////////////////////////////////////////////////////////////

void ListUI::ListDrawImpl(const std::vector<std::string>& content) {
  ScreenOutputDevice::Font font = ScreenOutputDevice::F8X8;
  screen_->Clear();
  for (size_t i = 0; i < content.size() - draw_start_ && i < GetScreenNumRows();
       ++i) {
    screen_->DrawText(i * 8 + screen_top_margin_, 8, content[i + draw_start_],
                      font, ScreenOutputDevice::ADD);
  }
  const uint32_t highlight_idx = current_highlight_ - draw_start_;
  if (highlight_idx < GetScreenNumRows()) {
    screen_->DrawText(highlight_idx * 8 + screen_top_margin_, 0, ">", font,
                      ScreenOutputDevice::ADD);
  }
}

void ListUI::OnUp() {
  if (current_highlight_ == 0) {
    return;
  }
  current_highlight_ -= 1;
  if (current_highlight_ < draw_start_) {
    draw_start_ = current_highlight_;
  }
  redraw_ = true;
}
void ListUI::OnDown() {
  if (current_highlight_ == GetListLength() - 1) {
    return;
  }
  current_highlight_ += 1;
  if (current_highlight_ >= draw_start_ + GetScreenNumRows()) {
    draw_start_ += 1;
  }
  redraw_ = true;
}

////////////////////////////////////////////////////////////////////////////////

void HomeScreen::Draw() {
  if (!redraw_) {
    return;
  }
  ListDrawImpl(menu_items_);
  redraw_ = false;
}

void HomeScreen::OnSelect() {
  if (current_highlight_ == 0) {
    config_modifier_->PushUI(std::make_shared<ConfigObjectScreen>(
        config_modifier_, screen_, global_config_object_, screen_top_margin_));
    redraw_ = true;
  }
  if (current_highlight_ == 1) {
    DeviceRegistry::SaveConfig();
    LOG_INFO("Saved");
  }
  if (current_highlight_ == 2) {
    DeviceRegistry::CreateDefaultConfig();
  }
  if (current_highlight_ == 3) {
    config_modifier_->EndConfig();
  }
}

uint32_t HomeScreen::GetListLength() { return menu_items_.size(); }

////////////////////////////////////////////////////////////////////////////////

static void DispatchChild(Config* child, ConfigModifiersImpl* config_modifier,
                          ScreenOutputDevice* screen,
                          uint8_t screen_top_margin) {
  switch (child->GetType()) {
    case Config::OBJECT: {
      config_modifier->PushUI(std::make_shared<ConfigObjectScreen>(
          config_modifier, screen, reinterpret_cast<ConfigObject*>(child),
          screen_top_margin));
      break;
    }
    case Config::LIST: {
      config_modifier->PushUI(std::make_shared<ConfigListScreen>(
          config_modifier, screen, reinterpret_cast<ConfigList*>(child),
          screen_top_margin));
      break;
    }
    case Config::INTEGER: {
      config_modifier->PushUI(std::make_shared<ConfigIntScreen>(
          config_modifier, screen, reinterpret_cast<ConfigInt*>(child),
          screen_top_margin));
      break;
    }
    case Config::FLOAT: {
      config_modifier->PushUI(std::make_shared<ConfigFloatScreen>(
          config_modifier, screen, reinterpret_cast<ConfigFloat*>(child),
          screen_top_margin));
      break;
    }
  }
}

ConfigObjectScreen::ConfigObjectScreen(ConfigModifiersImpl* config_modifier,
                                       ScreenOutputDevice* screen,
                                       ConfigObject* config_object,
                                       uint8_t screen_top_margin)
    : ListUI(config_modifier, screen, screen_top_margin),
      config_object_(config_object) {
  keys_.clear();
  keys_.push_back("^ Back");
  const auto& map = *config_object->GetMembers();
  for (const auto& [k, v] : map) {
    keys_.push_back(k);
  }
  std::sort(keys_.begin() + 1, keys_.end());
}

void ConfigObjectScreen::Draw() {
  if (!redraw_) {
    return;
  }
  ListDrawImpl(keys_);
  redraw_ = false;
}

void ConfigObjectScreen::OnSelect() {
  if (current_highlight_ == 0) {
    config_modifier_->PopUI();
    return;
  }
  const std::string& key = keys_[current_highlight_];
  Config* child = config_object_->GetMembers()->at(key).get();
  DispatchChild(child, config_modifier_, screen_, screen_top_margin_);
  redraw_ = true;  // This is for when we are back at this screen
}

uint32_t ConfigObjectScreen::GetListLength() { return keys_.size(); }

////////////////////////////////////////////////////////////////////////////////

ConfigListScreen::ConfigListScreen(ConfigModifiersImpl* config_modifier,
                                   ScreenOutputDevice* screen,
                                   ConfigList* config_list,
                                   uint8_t screen_top_margin)
    : ListUI(config_modifier, screen, screen_top_margin),
      config_list_(config_list) {
  indices_.clear();
  indices_.push_back("^ Back");
  const uint32_t list_size = config_list->GetList()->size();
  for (uint32_t i = 0; i < list_size; ++i) {
    indices_.push_back("[" + std::to_string(i) + "]");
  }
}

void ConfigListScreen::Draw() {
  if (!redraw_) {
    return;
  }
  ListDrawImpl(indices_);
  redraw_ = false;
}

void ConfigListScreen::OnSelect() {
  if (current_highlight_ == 0) {
    config_modifier_->PopUI();
    return;
  }
  Config* child = config_list_->GetList()->at(current_highlight_ - 1).get();
  DispatchChild(child, config_modifier_, screen_, screen_top_margin_);
  redraw_ = true;  // This is for when we are back at this screen
}

uint32_t ConfigListScreen::GetListLength() { return indices_.size(); }

////////////////////////////////////////////////////////////////////////////////

void ConfigIntScreen::Draw() {
  if (!redraw_) {
    return;
  }
  screen_->Clear();
  screen_->DrawText(
      (screen_->GetNumRows() - screen_top_margin_) / 2 + screen_top_margin_, 0,
      std::to_string(config_int_->GetValue()), ScreenOutputDevice::F8X8,
      ScreenOutputDevice::ADD);
  redraw_ = false;
}

void ConfigIntScreen::OnUp() {
  int32_t value = config_int_->GetValue();
  if (value == config_int_->GetMinMax().second) {
    return;
  }
  config_int_->SetValue(value + 1);
  redraw_ = true;
}

void ConfigIntScreen::OnDown() {
  int32_t value = config_int_->GetValue();
  if (value == config_int_->GetMinMax().first) {
    return;
  }
  config_int_->SetValue(value - 1);
  redraw_ = true;
}

void ConfigIntScreen::OnSelect() { config_modifier_->PopUI(); }

////////////////////////////////////////////////////////////////////////////////

void ConfigFloatScreen::Draw() {
  if (!redraw_) {
    return;
  }
  screen_->Clear();
  screen_->DrawText(
      (screen_->GetNumRows() - screen_top_margin_) / 2 + screen_top_margin_, 0,
      std::to_string(config_float_->GetValue()), ScreenOutputDevice::F8X8,
      ScreenOutputDevice::ADD);
  redraw_ = false;
}

void ConfigFloatScreen::OnUp() {
  float value = config_float_->GetValue();
  if (value >= config_float_->GetMinMax().second) {
    return;
  }
  config_float_->SetValue(value + config_float_->GetResolution());
  redraw_ = true;
}

void ConfigFloatScreen::OnDown() {
  float value = config_float_->GetValue();
  if (value <= config_float_->GetMinMax().first) {
    return;
  }
  config_float_->SetValue(value - config_float_->GetResolution());
  redraw_ = true;
}

void ConfigFloatScreen::OnSelect() { config_modifier_->PopUI(); }

////////////////////////////////////////////////////////////////////////////////

void ConfigModifiersImpl::SetScreenOutputs(
    const std::vector<std::shared_ptr<ScreenOutputDevice>>* device) {
  screen_output_ = device;
  for (auto screen : *screen_output_) {
    if (screen->GetTag() == screen_tag_) {
      screen_ = screen;
    }
  }
}

void ConfigModifiersImpl::SetConfigMode(bool is_config_mode) {
  if (is_config_mode && screen_ != NULL) {
    ui_stack_.clear();
    ui_stack_.push_back(std::make_shared<HomeScreen>(
        this, screen_.get(), global_config_, screen_top_margin_));
    pop_ui_ = false;
    is_config_ = is_config_mode;
  } else if (is_config_mode) {
    runner::SetConfigMode(false);
  }
}

void ConfigModifiersImpl::Up() {
  if (!is_config_ || ui_stack_.empty()) {
    return;
  }
  ui_stack_.back()->OnUp();
}

void ConfigModifiersImpl::Down() {
  if (!is_config_ || ui_stack_.empty()) {
    return;
  }
  ui_stack_.back()->OnDown();
}

void ConfigModifiersImpl::Select() {
  if (!is_config_ || ui_stack_.empty()) {
    return;
  }
  ui_stack_.back()->OnSelect();
}

void ConfigModifiersImpl::PushUI(std::shared_ptr<ConfigUIBase> ui) {
  ui_stack_.push_back(ui);
}

void ConfigModifiersImpl::PopUI() { pop_ui_ = true; }

void ConfigModifiersImpl::EndConfig() {
  ui_stack_.clear();
  ui_stack_.shrink_to_fit();
  screen_->Clear();
  runner::SetConfigMode(false);
  runner::NotifyConfigChange();
}

void ConfigModifiersImpl::FinalizeInputTickOutput() {
  if (screen_ == NULL || !is_config_ || ui_stack_.empty()) {
    return;
  }
  if (pop_ui_) {
    ui_stack_.pop_back();
    pop_ui_ = false;
  }
  ui_stack_.back()->Draw();
}

static Status registered =
    DeviceRegistry::RegisterConfigModifier([](ConfigObject* global_config) {
      return std::shared_ptr<ConfigModifiersImpl>(
          new ConfigModifiersImpl(global_config, 1, 0));
    });
