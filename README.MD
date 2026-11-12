# Display Mirror
This is a project combining a pair STM32-U575ZI-Qs and ILI9341 based displays and mirroring their touch input over WiFi. One STM32 acts as server creating a Soft-AP and waiting for connections,and the other acts as a simple client and connects to the established Soft-AP. Besides that, the logic is the same across both STM32s.

As is, this proof of concept can be used to play any game that would be played using a pen and paper without having to share the same physical pen and paper.

This implementation uses the basic display driver provided [here](https://github.com/martnak/STM32-ILI9341) with a few minor adjustments and bug fixes to port it to the STM32-U5 series. The touch functionality is provided by my own bare metal bit-bang driver that takes inspiration from implementations found [here](https://github.com/afiskon/stm32-ili9341) and [here](https://community.st.com/stm32-mcus-60/how-to-make-an-efficient-bit-bang-driver-on-stm32-125568).


## Why Wifi?
While a wireless technology like Bluetooth would work for this proof of concept, opting for WiFi increases the possibilities for future improvements, such as setting up a centralized server and mirroring display input across displays connected to different networks. 