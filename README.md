## HID device data Dynamic modification
With this project, You can cheat in FPS games.

## Environment
1. Hardware
    - stm32-f4discovery
    - CH375 (USB Bus interface Module) [you need two, one for mouse, other for keyboard]

2. Develop Environment
    - Ubunutu
    - STM32CubeMX
    - openocd (For stlink debug progame)
    - gcc-arm-none-eabi
    - vscode (with Arm extensions)

## Reference
1. USB docs from USB offical website (www.usb.org).
> usb_20.pdf

2. HID report descriptors from this website.
> https://eleccelerator.com/tutorial-about-usb-hid-report-descriptors/

> You can all so use the HID Descriptor Tool from usb.org
> https://www.usb.org/document-library/hid-descriptor-tool

3. libusb (API design, constant definition referencing)
> offical website: https://libusb.info/
> GitHub Repository: https://github.com/libusb/libusb

4. CH375 offical documents (offical website is http://www.wch.cn/products/CH375.html, you can find more docs from here)
> CH375DS1.PDF, CH375DS2.PDF