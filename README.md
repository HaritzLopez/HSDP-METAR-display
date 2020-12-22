# HSDP-METAR-display
This is the repo for the code of the ESP32 in the HDSP-Metar Display board

The code depends on:
  1. HTTPClient
  2. ArduinoJson
  3. MCP23008
  4. WiFi
  
You need to install them first. Use the Arduino IDE's library manager for the purpose. It will ease your life.
The code was written for the ESP32 board, it may work for other WiFi capable Arduinos, but it may requiere some adaptations.

Steps for building and uploading the code to the ESP32 board:
  1. Download the .ino file and open it in the Arduino IDE.
  2. Select DOIT ESP32 DEVKIT V1 board under Tools > Boards > ESP32 Arduino
  3. Select boardspeed 80 Mhz under Tools > Boards > Flash Frequncy
  4. Select baudrate 921600 Tools > Boards > Upload Speed
  
If this options are not shown in your IDE, you may need to instal the ESP32 board. Check: https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
  
If you still have trouble, please send me an e-mail.
