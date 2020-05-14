## Wireless Outdoor Weather Station + PM10 PM2.5 Air Quality Monitor
Reads sensors and sends data to ThingsSpeak every 5 minutes.

#### Hardware:
- NodeMCU v3 (esp8266 12e)
- Shinyei Optical Dust sensor PPD42NS (modified with 3.3v voltage divider on pin2 and pin4, 100uF capacitor as C8)
- BMP180 Barometric BMP/Temperature/Altitude Sensor
- DHT21/AM2301 Temperature and humidity module

#### Wire Connections:
NodeMCU     Sensors 
3.3V        VCC(BMP180, DHT21)
GND         GND(BMP180, DHT21)
D1          SCL(BMP180)          
D2          SDA(BMP180)
D3          DHT DATA
D5          PPD42 P2(pin 2)
D6          PPD42 P1(pin 4)    

#### More about the PPD42 sensor:
- https://files.seeedstudio.com/wiki/Grove_Dust_Sensor/resource/Grove_-_Dust_sensor.pdf
- https://www.shinyei.co.jp/stc/eng/products/optical/ppd42nj.html
- https://www.researchgate.net/publication/324171671_Understanding_the_Shinyei_PPD42NS_low-cost_dust_sensor

#### PM10, PM25:
For now, it calculates the average based on the last three readings (data from the last aprox. 20min),
since it is powered during short intervals and not all readings are valid.

#### Conclusions:
It detects successfully household dust, cigarette smoke, pollen, gypsum plaster powder, etc.
If compared with data from other sources, there is a larger difference between PM10 and PM25, probably because:
- most of the dust particles at the place of measurement are in the 10um channel
- the minimum detected particle for PPD42 is 1um
- for the same number of particles, the mass in PM10 channel is about 200 times higher than in 2.5 channel
Daily and monthly average calculations may be more precise.