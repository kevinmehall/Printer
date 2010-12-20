
SOURCES=HardwareSerial.cpp Print.cpp Ansiterm.cpp main.cpp
F_CPU=8000000L
MCU=atmega168
CFLAGS=-g -mmcu=$(MCU) -Wall -Os -mcall-prologues -ffunction-sections -fdata-sections -DF_CPU=$(F_CPU) -DARDUINO=18 

%.o: %.cpp
	avr-g++ $(CFLAGS) -c $< -o $@


OBJECTS = $(SOURCES:.cpp=.o)
prog.hex: $(OBJECTS)
	avr-gcc -Os -Wl,--gc-sections -mmcu=$(MCU) -o prog.elf -Wl,-Map,prog.map $(OBJECTS)
	avr-objcopy -R .eeprom -O ihex prog.elf prog.hex
	
isp: prog.hex
	echo "Reset AVR"
	sleep 1
	sudo avrdude -p m168 -c arduino -P /dev/ttyUSB1 -b 115200 -e -U flash:w:"prog.hex"
	
	

