#ifndef KEYSCAN_H_
#define KEYSCAN_H_

#include <functional>
#include <memory>
#include <string>

#include "layout.h"
#include "utils.h"

// Each registration creates at most one instance of the handler
#define REGISTER_CUSTOM_KEYCODE_HANDLER(KEYCODE, CLS)            \
  static status register_##(CLS) = RegisterCustomKeycodeHandler( \
      (KEYCODE), []() -> std::unique_ptr<CustomKeycodeHandler> { \
        return std::make_unique<(CLS)>();                        \
      });

class CustomKeycodeHandler {
 public:
  virtual void ProcessKeyState(Keycode kc, bool is_pressed) = 0;
  virtual std::string GetName() const = 0;
};

using CustomKeycodeHandlerCreator =
    std::function<std::unique_ptr<CustomKeycodeHandler>()>;

// Not threadsafe. Should only be called at C++ initialization time.
status RegisterCustomKeycodeHandler(uint8_t keycode,
                                    CustomKeycodeHandlerCreator creator);

status KeyScanInit();

status StartKeyScanTask();

// Keyscan APIs. Threadsafe.

status SetLayerStatus(uint8_t layer, bool active);
status ToggleLayerStatus(uint8_t layer);

// From high layers to low layers
std::vector<uint8_t> GetActiveLayers();

status PressThenReleaseKey(Keycode kc);
status SendStandardKeycode(uint8_t keycode);

#endif /* KEYSCAN_H_ */