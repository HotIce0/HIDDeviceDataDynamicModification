## HID device data dynamic modification
With this project, You can cheat in FPS games.

## TODO
- Y] USB host basic api
- Y] HID Host
- Y] HID Host Mouse
- Y] HID Host Keyboard
- Y] Optimize HID Host code
- X] HID Device(composite device[mouse, keyboard])
- T] Finish HID data transmission
- T] HID data dynamic modification
- T] HID Report Descriptor Clone
- T] Optimize HID Device Code

> Y: done
> X: doing
> T: todo

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


## Remaining Problems
1. 0951:16D2 HyperX Alloy FPS Pro Mechanical Gaming Keyboard, control transfer error(token send failed) after set configuration.


## Project Structure
- Drivers
    - ch375: (include libraries,  usbhost, usbhid, hidkeyboard, hidmouse)
    - ...
- Middlewares/ST/STM32_USB_Device_Library
    - Class/CustomHID

### CH375 Use
- CH375A
    - USART2
    - PE14     (Int Pin)
- CH375B
    - USART3
    - PE15     (Int Pin)
- Log Out
    - UART4
