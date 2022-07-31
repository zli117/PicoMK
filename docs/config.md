# Configuration Menu

One of the unique features of PicoMK is the configuration menu. Each device can export a set of configs and the firmware will automatically present on the configuration menu. With such menu, it's possible to customize the keyboard on the go with any host OS and no need to memorize the key bindings you set several months ago.

## Basic Use

To enable the config mode, you need to register a `ScreenOutputDevice` in `layout.cc`, such as SSD1306 or other types of screens. Then in the `layout.cc` file, register a `ConfigModifier` device. For the built-in implementation, you can use the following line:

```cpp
static Status register_config_modifier = RegisterConfigModifier(ssd1306_tag);
```

## Define the Config for a Device 

You need to override two methods from the `GenericDevice` in your custom device:

```cpp
// Called when the config has been updated.
virtual void OnUpdateConfig(const Config* config);

// Called during initialization to create the default config. Note that it'll
// be called even when there's a valid config json file in the filesystem. The
// default config specifies the structure of the config sub-tree to later
// parse the json file.
virtual std::pair<std::string, std::shared_ptr<Config>> CreateDefaultConfig();
```

Further, if the device behaves different in config mode or in normal mode, you also need to override this method:

```cpp
// Override this if the device needs to behave differently when in and out of
// config mode. One example would be USB output devices, where during config
// mode it doesn't report any key code or mouse movement to the host.
virtual void SetConfigMode(bool is_config_mode){};
```

The config the device is represented by a tree of `Config` objects. Each object can be an "object", "list", "integer", or a "float". They are organized in a similar way as in json format, so that later on the config can be serialized to and deserialized from json. Further more, for numerical values, we define a constraint on their value range for later use by the config modifier. There are a list of helper macros in `configuration.h` to build the config tree: 

```cpp
// Create an object from a list of key value pairs
#define CONFIG_OBJECT(...)

// Create one key value pair from a string name to a subconfig.
#define CONFIG_OBJECT_ELEM(name, value)

// Create a list of subconfigs
#define CONFIG_LIST(...)

// Convenient macro for creating a special list with length of two
#define CONFIG_PAIR(first, second)

// Value can only be in range [min, max]. value is the compile time default.
#define CONFIG_INT(value, min, max)

// Value can only be in range [min, max]. Each up/down on the config modifier
// increases/decreases the value by resolution amount. value is the compile time
// default.
#define CONFIG_FLOAT(value, min, max, resolution)
```

For example, to create a config tree whose default values correspond to the following json:

```json
{
   "int1":10,
   "float1":10.5,
   "list1":[
      {
         "int":5
      },
      {
         "float":11.1
      },
      30,
   ]
}
```

```cpp
auto config = CONFIG_OBJECT(
    // Valid value in range [0, 100], default is 10
    CONFIG_OBJECT_ELEM("int1", CONFIG_INT(10, 0, 100)),

    // Valid value in range [5, 20.0], default is 10.5, and each time it's
    // increased/decreased by 0.2
    CONFIG_OBJECT_ELEM("float1", CONFIG_FLOAT(10.5, 5, 20.0, 0.2)),

    // A list of 3 elements
    CONFIG_OBJECT_ELEM(
        "list1", CONFIG_LIST(CONFIG_OBJECT(CONFIG_OBJECT_ELEM(
                                 "int", CONFIG_INT(5, 0, 100))),
                             CONFIG_OBJECT(CONFIG_OBJECT_ELEM(
                                 "float", CONFIG_FLOAT(11.1, 5, 20.0, 0.1))),
                             CONFIG_INT(30, 0, 2048))));
```

Finally, in `CreateDefaultConfig` return the device name as well as the config tree:

```cpp
return {"Device Name", config};
```

To parse the config, you need to check the type tag of the current config by calling `GetType()` and dynamically cast the pointer. 

For more examples, please take a look at the implementation for joystick (`joystick.cc`) and WS2812 (`ws2912.cc`).
