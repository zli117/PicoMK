# Design and Tradeoffs

This doc documents the design decisions and tradeoffs involved. At the core of the firmware, are two design patterns Observer and Registry. The observed can produce events to observer. Each class can be both an observer and an observed. This decouples the obligation of each device, and allows many to many communication between classes. For example, there can be multiple devices sending key codes via usb. We can centrolize the usb report construction and sending in one observer class `USBKeyboardOutput`. To achieve conditional compilation, we use the Registry pattern which allows us to have zero, one, or even multiple instances of a device, such as rotary encoder, as long as you assign a different tag to each instance at registration time.

Now the tradeoffs:

 * C++ vs C vs Python:
   * Pros of C++: 
     * There are many features that's desirable: compile time evaluated `constexpr` functions, class to encapsulate both data and code, inheritance, template metaprogramming, STL (especially smart pointers) etc.  
     * Still allows fairly low level controls.
   * Cons of C++: 
     * Inheritance usually involves virtual functions and vtable, which incurs extra pointer jump. Could be extra slow if the code (and vtable) is on flash.
     * A lot of common embedded libraries (FreeRTOS, tinyusb etc.) are designed for C (takes function pointer) etc. Extra work to make it play well with C code (i.e. `extern "C"`).
     * STL (and others) can bloat the binary size, which means we can't fit in SRAM. Extra work for flash writes.
   * Pros of C:
     * Simpler language constructs. Less time spent on debugging weird language behaviors (like virtual inheritance). More predictable.
     * Plays well with FreeRTOS and tinyusb.
     * Requires less run-time resources than C++ (i.e. no STL, no vtable stuffs)
   * Cons of C:
     * Might need extensive use of macros for metaprogramming or conditional compilation. Could be a maintaince nightmare.
     * Behavior customization if implemented with weak linage can be quite hard to manage. However this might be mitigated with function pointers. 
     * No concept of destructor. Need to manually handle scoping for things like memory deallocation and lock release.
   * Pros of (Micro)Python:
     * Generally requires less code than C/C++ for the same feature
     * USB HID and filesystem are already handled 
     * Easier to get started and can just upload text python code instead of compilation
     * Interactive REPL for faster iterations
     * Easier to learn for newcomers
   * Cons of (Micro)Python:
     * Dynamic typing can be hard to maintain once project gets big and complex
     * Hard to have direct low level control
     * Even more bloated than C++
     * Not sure how to integrate with RTOS
   * Decision: C++. Reasons:
     * Even though C++ has more startup cost than Python or even C, in the long run it can be easier to maintain than C, since it has more options to modularize things, and template can replace a lot of use of macros.
     * Binary size is generally not a huge issue for RP2040 as it usually has over 2MB of flash storage. Can't reside in SRAM can be a bit annoying, but this can still be an issue for C and especially Python
     * The destructor based scope management is a huge plus compared to C. Never need to remember to release a lock when returning inside an `if`.

 * [FreeRTOS](https://www.freertos.org/) vs [Zephyr](https://zephyrproject.org/) 
   * Pros of FreeRTOS:
     * Relatively simpler. The code is only in a few files
     * More familiar to me, as I came from ESP-IDF ecosystem
     * Has descent documentation and active community
   * Cons of FreeRTOS:
     * SMP (multicore) support seems to be an after thought
   * Pros of Zephyr:
     * A more modern RTOS, with a more Linux like setup, such as device tree hardware description.
     * A lot of utilities.
     * Really nice documentation.
   * Cons of Zephyr:
     * Seems like a massive overkill for this project. 
     * Too much extra things to learn.
   * Decision: FreeRTOS. Reasons:
     * Easier to get started. I spent two hours reading [Zephyr docs](https://docs.zephyrproject.org/latest/), and still don't feel comfortable to start coding.
     * We need to do our own device configurations, like the `layout.cc` file, rather than using device tree, since this is a very specific case.
