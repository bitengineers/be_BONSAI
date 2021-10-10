# FIRMWARE

[Work In Progress]

be BONSAI プロジェクトで作成しているデバイスの firmware のソースコード。

現 version では esp32 を使用した M5Stack, M5Stick 向けに [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html) を使用して書かれています。
センサーを用いて盆栽や観葉植物のデータを取得し AWS IoT Core の Device Shadow サービスへデータを送信する仕様となってます。動作させるには AWS アカウントと各設定が必要です。

## How to build

### aws credential files

Place the files which are used to certificate the thing device. 
Please download them in AWS IoT Core management console, Manage->Things->YOUR THING->Certificates.
And place the files to locations listed below:

```
./main/certs/aws-root-ca.pem
./main/certs/certificate.pem.crt
./main/certs/private.pem.key
```

### configure

```
idf.py menuconfig
```

Please input the values listed below

```
Component config --->
  Amazon Web Services IoT Platform --->
     WS IoT Endpoint Hostname <= YOUR AWS HOST_NAME  (ex. HOGE.iot.ap-northeast-1.amazonaws.com)
     WS IoT MQTT Port         <= 443
     Maximum MQTT Topic Filters <= should be 10
     
  be_BONSAI --->
     AWS IoT Client ID    <= POT NAME by you
     AWS IoT Thing Name   <= YOUR AWS Thing Name defined in AWS IoT Core
     Product type         <= Choice M5 device which you use
     PORT_A configuration <= choice
```

### Build a binary & flash it & show the output log on your console.
```
idf.py build flash monitor
```

## How to setup AWS

... TODO


## PORT_A connection

- 1-0. PORT_A connects to EARTH SENSOR directly
- 2-0. PORT_A is setup as I2C
  - 2-1. if it connects to PAHUB
    - 2-1-1. ENV3 on CH0
    - 2-1-2. SHT30 sensor on CH1 (same IC used in ENV3)
  - 2-2. if it connects to PBHUB
    - 2-2-1. Light Sensor on CH0
  - 2-3. both 2-1 and 2-2


### Tips

In my development environment, "idf.py flash" always fails for M5Stick C+. 
Instead of 'idf.py flash', you can directly type the command output on failure into the CLI to flash it.

ex.
```
cd ${ESP_IDF_HOME}/components/esptool_py && /usr/bin/cmake -D IDF_PATH="${ESP_IDF_HOME}" -D SERIAL_TOOL="/usr/bin/python ${ESP_IDF_HOME}/components/esptool_py/esptool/esptool.py --chip esp32" -D SERIAL_TOOL_ARGS="--before=default_reset --after=hard_reset write_flash @flash_args" -D WORKING_DIRECTORY="${YOUR_PROJECT_DIR}/iot_firmware/build" -P ${ESP_IDF_HOME}/components/esptool_py/run_serial_tool.cmake
```
