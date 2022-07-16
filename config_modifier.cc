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
  for (size_t i = 0;
       i < content.size() - draw_start_ && i < screen_->GetNumRows(); ++i) {
    screen_->DrawText(i * 8, 8, content[i + draw_start_], font,
                      ScreenOutputDevice::ADD);
  }
  const uint32_t highlight_idx = current_highlight_ - draw_start_;
  if (highlight_idx < screen_->GetNumRows()) {
    screen_->DrawText(highlight_idx * 8, 0, "-", font, ScreenOutputDevice::ADD);
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
  if (current_highlight_ >= draw_start_ + screen_->GetNumRows()) {
    draw_start_ += 1;
  }
  redraw_ = true;
}

////////////////////////////////////////////////////////////////////////////////

void HomeScreen::Draw() {
  if (!redraw_) {
    return;
  }
  ListDrawImpl(
      {{"Edit Config", "Save Config", "Load Config", "Load Default", "Exit"}});
  redraw_ = false;
}

void HomeScreen::OnSelect() {
  if (current_highlight_ == 0) {
    config_modifier_->PushUI(std::make_shared<ConfigObjectScreen>(
        config_modifier_, screen_, global_config_object_));
  }
  if (current_highlight_ == 1) {
  }
  if (current_highlight_ == 2) {
  }
  if (current_highlight_ == 3) {
  }
  if (current_highlight_ == 4) {
    config_modifier_->EndConfig();
  }
}

uint32_t HomeScreen::GetListLength() { return 5; }

////////////////////////////////////////////////////////////////////////////////

static void DispatchChild(Config* child, ConfigModifiersImpl* config_modifier,
                          ScreenOutputDevice* screen) {
  switch (child->GetType()) {
    case Config::OBJECT: {
      config_modifier->PushUI(std::make_shared<ConfigObjectScreen>(
          config_modifier, screen, reinterpret_cast<ConfigObject*>(child)));
      break;
    }
    case Config::LIST: {
      config_modifier->PushUI(std::make_shared<ConfigListScreen>(
          config_modifier, screen, reinterpret_cast<ConfigList*>(child)));
      break;
    }
    case Config::INTEGER: {
      config_modifier->PushUI(std::make_shared<ConfigIntScreen>(
          config_modifier, screen, reinterpret_cast<ConfigInt*>(child)));
      break;
    }
    case Config::FLOAT: {
      config_modifier->PushUI(std::make_shared<ConfigFloatScreen>(
          config_modifier, screen, reinterpret_cast<ConfigFloat*>(child)));
      break;
    }
  }
}

ConfigObjectScreen::ConfigObjectScreen(ConfigModifiersImpl* config_modifier,
                                       ScreenOutputDevice* screen,
                                       ConfigObject* config_object)
    : ListUI(config_modifier, screen), config_object_(config_object) {
  keys_.clear();
  keys_.push_back("<- Back");
  const auto& map = *config_object->GetMembers();
  for (const auto& [k, v] : map) {
    keys_.push_back(k);
  }
  std::sort(keys_.begin(), keys_.end());
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
  const std::string& key = keys_[current_highlight_ - 1];
  Config* child = config_object_->GetMembers()->at(key).get();
  DispatchChild(child, config_modifier_, screen_);
}

uint32_t ConfigObjectScreen::GetListLength() { return keys_.size(); }

////////////////////////////////////////////////////////////////////////////////

ConfigListScreen::ConfigListScreen(ConfigModifiersImpl* config_modifier,
                                   ScreenOutputDevice* screen,
                                   ConfigList* config_list)
    : ListUI(config_modifier, screen), config_list_(config_list) {
  indices_.clear();
  indices_.push_back("<- Back");
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
  DispatchChild(child, config_modifier_, screen_);
}

uint32_t ConfigListScreen::GetListLength() { return indices_.size(); }

////////////////////////////////////////////////////////////////////////////////

void ConfigIntScreen::Draw() {
  if (!redraw_) {
    return;
  }
  screen_->Clear();
  screen_->DrawText(screen_->GetNumRows() / 2, 0,
                    std::to_string(config_int_->GetValue()),
                    ScreenOutputDevice::F8X8, ScreenOutputDevice::ADD);
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
  screen_->DrawText(screen_->GetNumRows() / 2, 0,
                    std::to_string(config_float_->GetValue()),
                    ScreenOutputDevice::F8X8, ScreenOutputDevice::ADD);
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
  is_config_ = is_config_mode;
  if (is_config_ && screen_ != NULL) {
    ui_stack_.clear();
    ui_stack_.push_back(
        std::make_shared<HomeScreen>(this, screen_.get(), global_config_));
    pop_ui_ = false;
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
  SetConfigMode(false);
  NotifyConfigChange();
}

void ConfigModifiersImpl::FinalizeInputTickOutput() {
  if (screen_ == NULL) {
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
          new ConfigModifiersImpl(global_config, 1));
    });
