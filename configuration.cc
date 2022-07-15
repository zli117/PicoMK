#include "configuration.h"

#include <algorithm>

// Configuration* Configuration::singleton_ = NULL;

// Configuration* Configuration::GetConfig() {
//   if (singleton_ == NULL) {
//     singleton_ = new Configuration();
//     singleton_->SetJoystickXProfile({{20, 1},{700, 2}, {1000, 3}, {2000, 7}});
//     singleton_->SetJoystickYProfile({{20, 1},{700, 2}, {1000, 3}, {2000, 7}});
//     singleton_->SetJoystickScanDivider(5);
//     singleton_->SetJoystickCalibrationSamples(1000);
//     singleton_->SetJoystickCalibrationThreshold(500);
//   }
//   return singleton_;
// }

// static void SetJoystickProfile(
//     const std::vector<std::pair<uint16_t, uint8_t>>& source,
//     std::vector<std::pair<uint16_t, uint8_t>>* target) {
//   target->clear();
//   target->insert(target->end(), source.begin(), source.end());
//   std::sort(target->begin(), target->end(),
//             [](auto a, auto b) { return a.first < b.first; });
// }

// void Configuration::SetJoystickXProfile(
//     const std::vector<std::pair<uint16_t, uint8_t>>& profile) {
//   SetJoystickProfile(profile, &joystick_x_profile_);
// }
// void Configuration::SetJoystickYProfile(
//     const std::vector<std::pair<uint16_t, uint8_t>>& profile) {
//   SetJoystickProfile(profile, &joystick_y_profile_);
// }
