Please read and explain this M5Stack documentation page. Summarize the key points and help answer follow-up questions.

Title: Core2 For AWS
Page URL: https://docs.m5stack.com/en/core/Core2_For_AWS
Markdown source: https://docs.m5stack.com/en/core/Core2_For_AWS.md

I have also copied the full Markdown to my clipboard. If you cannot read the links directly, please wait for me to paste the Markdown content.

---

# Core2 For AWS

<span class="product-sku">SKU:K010-AWS</span>

<PictureViewer>
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_01.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_02.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/646/K010-AWS-package-01.jpg">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/646/K010-AWS-package.jpg">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_03.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_04.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_05.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_06.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_07.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_08.webp">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_09.webp">
</PictureViewer>

## Description

**Core2 for AWS** is a dedicated kit for AWS IoT learning projects. It consists of the **M5Stack Core2** main control unit and the **M5GO-Bottom For AWS** expansion base, with an additional custom integration of the ATECC608 [Trust\&GO](https://www.microchip.com/design-centers/security-ics/trust-platform/trust-go) hardware encryption, making it an ideal kit for IoT learning and secure project development.

The main control unit **Core2** is equipped with the ESP32-D0WDQ6-V3, featuring two independently controllable Xtensa® 32-bit LX6 processors with a clock speed of up to 240MHz, supporting WiFi functionality. It comes with 16MB Flash and 8MB PSRAM, and programs can be downloaded via the TYPE-C interface. The robust configuration meets the resource demands of complex applications. The front features a 2.0-inch integrated capacitive touchscreen, providing a smoother human-machine interaction experience. The device includes a built-in vibration motor for tactile feedback and vibration alerts. The built-in RTC module offers precise timing functions. The power section is equipped with the AXP192 power management chip, effectively controlling power consumption, and includes a green power indicator. The device also features a TF-card (microSD) slot and a speaker. To ensure higher-quality sound, an I2S digital audio interface amplifier chip is used, effectively preventing signal distortion. The left side and bottom of the device have independent power and reset (RST) buttons. The three dots on the front of the screen are part of the touchscreen and can be programmed as three virtual buttons.

**M5GO-Bottom For AWS** is an expansion base specifically designed for this custom model. The base integrates an MPU6886 six-axis motion sensor, a digital microphone (SPM1423), and a 500mAh lithium battery. It provides two HY2.0-4P expansion interfaces, exposing commonly used ADC/DAC/UART pins for connecting various sensors. The sides of the base feature 10 programmable RGB LEDs (SK6812), paired with frosted light-transmitting material, providing a soft and comfortable lighting effect. The bottom uses a pogo pin magnetic charging interface. When attached to the charging base, current flows safely into the internal battery via the built-in TP4057 charging chip. In addition to charging, the pogo pin interface exposes the main control I2C bus, allowing for magnetic attachment of external expansions. The base includes built-in magnets and a LEGO-compatible hole design on the back, enabling seamless integration with other LEGO structures. The PCB of Core2 for AWS reserves interfaces for the CP2104 chip and the lithium battery.
**The AWS custom model embeds the ATECC608 hardware encryption chip, enhancing the security of IoT communication at the hardware level.**

## Tutorial

learn>| ![AWS IoT Core](https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_08.webp) | [AWS IoT Core](https://core2-for-aws-docs.m5stack.com) | This tutorial shows how to configure the Core2 for AWS device to quickly connect to AWS IoT Core. |

learn>| ![UIFlow](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/assets/img/uiflow/uiflow1.0_banner_01.png) | [UIFlow](/en/uiflow/m5core2/program) | This tutorial shows how to control the Core2 for AWS device through the UIFlow graphical programming platform. |

learn>| ![UiFlow2](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/assets/img/uiflow2/uiflow2.0_banner_01.png) | [UiFlow2](/en/uiflow2/m5core2/program) | This tutorial shows how to control the Core2 for AWS device through the UiFlow2 graphical programming platform. |

learn>| ![Arduino IDE](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/assets/img/arduino/arduino_banner_01.png) | [Arduino IDE](/en/arduino/m5core2/program) | This tutorial shows how to program and control the Core2 for AWS device using the Arduino IDE. |

## Notes

- The built-in vibration motor of M5Core2 interferes structurally with the M5 Base series bases. To prevent equipment damage, do not stack M5Core2 with any M5 Base series functional bases.

- Some screens may exhibit non-linear touch response at the edges. You can try upgrading the screen firmware using [M5Tool](https://github.com/m5stack/M5Tools) to resolve this issue.

## Features

- Based on ESP32, supports WiFi
- Built-in ATECC608 hardware encryption chip
- 16MB Flash, 8MB PSRAM
- Built-in speaker, power indicator, vibration motor, RTC, I2S amplifier, capacitive touchscreen, power button, reset button
- TF card slot (supports up to 16GB)
- Built-in lithium battery with power management chip
- Built-in 6-axis IMU, PDM microphone
- M5-Bus socket
- Development Platform
  - UiFlow1
  - UiFlow2
  - Arduino IDE
  - ESP-IDF
  - PlatformIO

## Includes

- 1 x M5Stack Core2
- 1 x M5GO Bottom2 for AWS
- 1 x USB Type-C cable (50cm)
- 1 x Hex wrench

## Applications

- IoT controller
- STEM education
- DIY projects

## Specifications

| Specification            | Parameter                                                                        |
| ------------------------ | -------------------------------------------------------------------------------- |
| SoC                      | ESP32-D0WDQ6-V3 @ Xtensa® 32-bit LX6 dual-core processor, clock frequency 240MHz |
| DMIPS                    | 600                                                                              |
| SRAM                     | 520KB                                                                            |
| Flash                    | 16MB                                                                             |
| PSRAM                    | 8MB                                                                              |
| Wi-Fi                    | 2.4 GHz Wi-Fi                                                                    |
| Hardware Encryption Chip | ATECC608B-TNGTLSU-G (addr 0x35)                                                  |
| Input Voltage            | 5V @ 500mA                                                                       |
| Host Interface           | Type-C x1, POGO PIN x1, I2C x1, GPIO x1, UART x1                                 |
| Programmable LED         | SK6812\*10                                                                       |
| Button                   | Power button, RST button, screen virtual buttons \* 3                            |
| Vibration Alert          | Vibration motor                                                                  |
| IPS LCD Screen           | 2.0"@320 x 240 ILI9342C / ILI9342E                                               |
| Capacitive Touch IC      | FT6336U                                                                          |
| Speaker                  | 1W-0928                                                                          |
| Microphone               | SPM1423                                                                          |
| I2S Amplifier            | NS4168                                                                           |
| IMU                      | MPU6886                                                                          |
| RTC                      | BM8563                                                                           |
| PMU                      | AXP192                                                                           |
| USB Chip                 | CP2104                                                                           |
| DC-DC Boost              | SY7088                                                                           |
| TF Card Slot             | Supports up to 16G                                                               |
| Lithium Battery          | 500mAh @ 3.7V                                                                    |
| Antenna                  | 2.4G 3D antenna                                                                  |
| Operating Temperature    | 0 ~ 40°C                                                                         |
| Case Material            | Plastic ( PC )                                                                   |
| Product Size             | 54.0 x 54.0 x 23.5mm                                                             |
| Product Weight           | 69.5g                                                                            |
| Package Size             | 90.0 x 60.0 x 30.0mm                                                             |
| Gross Weight             | 110.0g                                                                           |

## Learn

### Power Management

- Power On: Click the power button on the left side
- Power Off: Long press the power button on the left side for 6 seconds
- Reset: Click the RST button on the bottom

### IMU Triaxial Direction Schematic Diagram

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/646/IMU-Core2-For-AWS.jpg" width="70%">

## Schematics

- [Core2 Schematic PDF](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/schematic/Core/CORE2_V1.0_SCH.pdf)

<SchViewer>
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/646/CORE2_V1.0_SCH_page_01.png" width="100%">
<img src="https://static-cdn.m5stack.com/resource/docs/products/core/core2_for_aws/core2_for_aws_sch_01.webp" width="100%">
</SchViewer>

## PinMap

### LCD Screen & TF Card

LCD Pixels: 320x240
TF Card supports up to 16GB

| ESP32-D0WDQ6-V3     | G38  | G23  | G18 | G5  | G15 |
| ------------------- | ---- | ---- | --- | --- | --- |
| ILI9342C / ILI9342E | MISO | MOSI | SCK | CS  | DC  |

| AXP192 Chip         | AXP_IO4 | AXP_DC3 | AXP_LDO2 |
| ------------------- | ------- | ------- | -------- |
| ILI9342C / ILI9342E | RST     | BL      | PWR      |

| ESP32-D0WDQ6-V3 | G38  | G23  | G18 | G4  |
| --------------- | ---- | ---- | --- | --- |
| TF Card         | MISO | MOSI | SCK | CS  |

### CAP.TOUCH (I2C Addr: 0x38)

| ESP32-D0WDQ6-V3 | G21 | G22 | G39 |
| --------------- | --- | --- | --- |
| FT6336U (0x38)  | SDA | SCL | INT |

| AXP192  | AXP_IO4 |
| ------- | ------- |
| FT6336U | RST     |

### Microphone & NS4168 Amplifier

| ESP32-D0WDQ6-V3 | G12  | G0   | G2   | G34  |
| --------------- | ---- | ---- | ---- | ---- |
| NS4168          | BCLK | LRCK | DATA |      |
| SPM1423         |      | CLK  |      | DATA |

| AXP192 | AXP_IO2 |
| ------ | ------- |
| NS4168 | SPK_EN  |

### AXP Power Indicator LED & Vibration Motor

| AXP192          | AXP_IO1 | AXP_LDO3 |
| --------------- | ------- | -------- |
| Green LED       | Vcc     | /        |
| Vibration motor |         | Vcc      |

### RTC

| ESP32-D0WDQ6-V3 | G21 | G22 |
| --------------- | --- | --- |
| BM8563 (0x51)   | SDA | SCL |

| AXP192 | AXP_PWR |
| ------ | ------- |
| BM8563 | INT     |

### IMU (3-axis Gyroscope + 3-axis Accelerometer) & Pogo Pin

| ESP32-D0WDQ6-V3 | G21 | G22 |
| --------------- | --- | --- |
| MPU6886 (0x68)  | SDA | SCL |
| Pogo Pin        | SDA | SCL |

### USB to Serial Download

| ESP32-D0WDQ6-V3 | G1  | G3  |
| --------------- | --- | --- |
| CP2104          | RXD | TXD |

### SK6812-LED

| ESP32-D0WDQ6-V3 | G25  |
| --------------- | ---- |
| SK6812-LED      | DATA |

### Internal I2C Connection

| ESP32-D0WDQ6-V3 | G21 | G22 |
| --------------- | --- | --- |
| MPU6886         | SDA | SCL |
| AXP192 (0x34)   | SDA | SCL |
| BM8563          | SDA | SCL |
| FT6336U         | SDA | SCL |
| ATECC608 (0x35) | SDA | SCL |

### HY2.0-4P

::grove-table
| HY2.0-4P | Black | Red | Yellow | White |
| -------- | ----- | --- | ------ | ----- |
| PORT.A   | GND   | 5V  | G32    | G33   |
| PORT.B   | GND   | 5V  | G26    | G36   |
| PORT.C   | GND   | 5V  | G14    | G13   |
::

### M5-Bus

::m5-bus-table
| FUNC       | PIN | LEFT | RIGHT | PIN | FUNC       |
| ---------- | --- | ---- | ----- | --- | ---------- |
|            | GND | 1    | 2     | G35 | ADC        |
|            | GND | 3    | 4     | G36 | ADC        |
|            | GND | 5    | 6     | RST | EN         |
| MOSI       | G23 | 7    | 8     | G25 | DAC        |
| MISO       | G38 | 9    | 10    | G26 | DAC        |
| SCK        | G18 | 11   | 12    | 3V3 |            |
| RXD0       | G3  | 13   | 14    | G1  | TXD0       |
| RXD2       | G13 | 15   | 16    | G14 | TXD2       |
| Int SDA    | G21 | 17   | 18    | G22 | Int SCL    |
| PORT.A SDA | G32 | 19   | 20    | G33 | PORT.A SCL |
| GPIO       | G27 | 21   | 22    | G19 | GPIO       |
| I2S_DOUT   | G2  | 23   | 24    | G0  | I2S_LRCK   |
|            | NC  | 25   | 26    | G34 | I2S_DATA   |
|            | NC  | 27   | 28    | 5V  |            |
|            | NC  | 29   | 30    | BAT |            |
::

### M5Core2 Port Description

**HY2.0-4P-PortA(Red)**

| ESP32 Chip | G32      | G33      |
| ---------- | -------- | -------- |
| PortA      | G32(SDA) | G33(SCL) |

### M5GO-Bottom For AWS Port Description

**HY2.0-4P-PortB(black)**

| ESP32-D0WDQ6-V3 | G26      | G36      |
| --------------- | -------- | -------- |
| PortB           | G26(DAC) | G36(ADC) |

**HY2.0-4P-PortC(blue)**

| ESP32-D0WDQ6-V3 | G13       | G14       |
| --------------- | --------- | --------- |
| PortC           | G13(RXD2) | G14(TXD2) |

**Charging Current Measurements**

| Charging Current | Current After Full Charge (Off) | Current After Full Charge (On) |
| ---------------- | ------------------------------- | ------------------------------ |
| 0.219A           | 0.055A                          | 0.147A                         |

## Model Size

<img alt="module size" src="https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/core2_for_aws/img-dea808fa-615c-41c0-9d1c-b1c4d56f3900.jpg" width="100%" />

## Structure

- [Core2 For AWS Structure Files](https://github.com/m5stack/M5_Hardware/tree/master/Products/K010-AWS_Core2_For_AWS/Structures)

## Datasheets

- [ESP32](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/644/esp32_datasheet_en.pdf)
- [FT6336U](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/Ft6336GU_Firmware%20%E5%A4%96%E9%83%A8%E5%AF%84%E5%AD%98%E5%99%A8_20151112.xlsx)
- [NS4168](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/NS4168_CN_datasheet.pdf)
- [MPU6886](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/MPU-6886-000193%2Bv1.1_GHIC_en.pdf)
- [ILI9342C](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/ILI9342C-ILITEK.pdf)
- [SPM1423](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/SPM1423HM4H-B_datasheet_en.pdf)
- [BM8563](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/BM8563_V1.1_cn.pdf)
- [SY7088](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/SY7088-Silergy.pdf)
- [AXP192 datasheet](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/AXP192_datasheet_en.pdf)
- [AXP192 register](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/AXP192_datasheet_en.pdf)
- [ATECC608](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/ATECC608A-TNGTLS-CryptoAuthentication-Data-Sheet-DS40002112B.pdf)
- [1027DC Motor](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/1027RFN01-33d.pdf)
- [ILI9342E](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/644/ILI9342E.pdf)

## Softwares

### Quick Start

- [Connect to AWS IoT Core](https://core2-for-aws-docs.m5stack.com)

### Arduino

- [Core2 for AWS Arduino Quick Start](/en/arduino/m5core2/program)
- [Core2 for AWS Arduino Driver Library](https://github.com/m5stack/M5Core2)
- [Core2 for AWS Arduino API](/en/arduino/m5core2/button)

### UiFlow1

- [Core2 For AWS UiFlow1 Quick Start](/en/uiflow/m5core2/program)

### UiFlow2

- [Core2 For AWS UiFlow2 Quick Start](/en/uiflow2/m5core2/program)

### PlatformIO

```
[env:m5stack-core2-for-aws]
platform = espressif32@6.12.0
board = m5stack-core2
framework = arduino
upload_speed = 921600
monitor_speed = 115200
board_build.partitions = default_16MB.csv
build_type = debug
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
lib_deps =
    M5Unified=https://github.com/m5stack/M5Unified
```

### ESP-IDF

[Core2 For AWS ESP-IDF Example Program](https://github.com/m5stack/Core2-for-AWS-IoT-Kit)
[Core2 For AWS ESP-IDF Factory Firmware](https://github.com/m5stack/Core2-for-AWS-IoT-Kit/tree/master/Factory-Firmware)

### USB Driver

\#>Click the link below to download the driver that matches the operating system. There are currently two driver chip versions, CP210X (for `CP2104` version)/CP34X (for `CH9102` version) driver compressed package. After decompressing the compressed package, select the installation package corresponding to the number of operating systems to install. (If you are not sure of the USB chip used by your device, you can install both drivers at the same time. During the installation process of `CH9102_VCP_SER_MacOS v1.7`, an error may occur, but the installation is actually completed, just ignore it.) When using it, if If the program cannot be downloaded normally (the prompt is overtime or Failed to write to target RAM), you can try to reinstall the device driver.

| Driver name               | Applicable driver chip | Download link                                                                                        |
| ------------------------- | ---------------------- | ---------------------------------------------------------------------------------------------------- |
| CP210x_VCP_Windows        | CP2104                 | [Download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CP210x_VCP_Windows.zip)     |
| CP210x_VCP_MacOS          | CP2104                 | [Download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CP210x_VCP_MacOS.zip)       |
| CP210x_VCP_Linux          | CP2104                 | [Download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CP210x_VCP_Linux.zip)       |
| CH9102_VCP_SER_Windows    | CH9102                 | [Download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CH9102_VCP_SER_Windows.exe) |
| CH9102_VCP_SER_MacOS v1.7 | CH9102                 | [Download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/drivers/CH9102_VCP_MacOS_v1.7.zip)  |

\\

### Easyloader

| Easyloader                         | Download Link                                                                                                         | Note |
| ---------------------------------- | --------------------------------------------------------------------------------------------------------------------- | ---- |
| Core2 For AWS User Demo Easyloader | [download](https://m5stack.oss-cn-shenzhen.aliyuncs.com/EasyLoader/Windows/CORE/EasyLoader_Core2_for_AWS_Default.exe) | /    |

## Video

<VideoGallery>
  <VideoItem title="Core2 For AWS Video 1" url="https://m5stack.oss-cn-shenzhen.aliyuncs.com/video/Product_example_video/Core/CORE2_FOR_AWS.mp4" />
  <VideoItem title="Core2 For AWS Video 2" url="https://m5stack.oss-cn-shenzhen.aliyuncs.com/video/Product_example_video/Core/CORE2_FOR_AWS_FACTORY2.mp4" />
</VideoGallery>

## Product Comparison

To compare information on the controller series products, you can visit the [Product Selection Table](/en/products_selector/m5core_compare?select=K010-AWS), check the target products, and get the comparison results. The selection table covers key information such as core parameters and functional features, and supports comparison of multiple products simultaneously.

## Version Change

| Release Date | Product Change                 | Note:                               |
| ------------ | ------------------------------ | ----------------------------------- |
| 2026.8.7 | LCD screen model changed from ILI9342C to ILI9342E | When compiling programs with the M5GFX library, M5GFX library version >= 0.2.27 is required |
| 2023.27      | Cancel RTC coin cell batteries | Does not affect the timing function |
