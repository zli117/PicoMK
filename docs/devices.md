# Devices and Registration Functions

## Config Modifier

Registration function:

```cpp
Status RegisterConfigModifier(uint8_t screen_tag);
```

Unlike others, config modifier doesn't need a tag to identify it, since there can be at most one. It takes the tag of the screen to display the config menu as input.


## USB I/O

Registration functions:

```cpp
Status RegisterUSBKeyboardOutput(uint8_t tag);
Status RegisterUSBMouseOutput(uint8_t tag);
Status RegisterUSBInput(uint8_t tag);
```

`USBKeyboardOutput` is responsible for sending keyboard HID reports to the host, and `USBMouseOutput` is for sending mouse HID reports to the host. `USBInput` is for handling various requests from host, such as suspend, capslock LED etc.

## Key Scan

Registration function:

```cpp
Status RegisterKeyscan(uint8_t tag);
```

For scanning the key matrix.

## Joystick

Registration function:

```cpp
Status RegisterJoystick(uint8_t tag, uint8_t x_adc_pin, uint8_t y_adc_pin,
                        size_t buffer_size, bool flip_x_dir, bool flip_y_dir,
                        bool flip_vertical_scroll, uint8_t alt_layer);
```

The default implementation of a joystick. You can register multiple instances. Normally, it sends joystick input as mouse movement. `alt_layer` is the layer number that if activated, will send joystick input as horizontal and vertical scrolling.

## Rotary Encoder

Registration function:

```cpp
Status RegisterEncoder(uint8_t tag, uint8_t pin_a, uint8_t pin_b,
                       uint8_t resolution);
```

The default implementation of a rotary encoder. You can register multiple instances. `resolution` is how many readings to count as one movement.

## WS2812

Registration function:

```cpp
Status RegisterWS2812(uint8_t tag, uint8_t pin, uint8_t num_pixels,
                      float max_brightness = 0.5, PIO pio = pio0,
                      uint8_t state_machine = 0);
```

`pin` is for the pin connecting to DIN. `max_brightness` is a scaler in range of [0, 1] that controls how bright **each** RGB LED can get. If `max_brightness` is 1, then each RGB LED can get max brightness of `255`, and if `max_brightness` is 0.2, then each RGB LED can get max brightness of `51`. Don't modify `pio` and `state_machine` unless you know what they do (i.e. you know how pio works on RP2040).

## Temperature Sensor

Read the RP2040 onboard temperature sensor.

Registration function:

```cpp
Status RegisterTemperatureInput(uint8_t tag);
```
