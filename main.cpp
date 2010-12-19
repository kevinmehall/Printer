#include "wiring_minimal.h"
#include "Ansiterm.h"

#define minpin 0
#define maxpin 7

short pinstates = 0;
char selected = 0;
Ansiterm ansi;

class Encoder{
	public:
	Encoder(volatile uint8_t &_port, char _pin1, char _pin2):
		PORT(_port), pin1(_pin1), pin2(_pin2)
		{}
	
	volatile uint8_t &PORT;
	const char pin1;
	const char pin2;
	short pos;
	volatile char changeFlag;
	
	inline void pinChanged(uint8_t portval, uint8_t changes){	
		if (changes & (1 << pin1)){ // pin1 changed
			if (portval & (1 << pin1)){ // pin1 low -> high
				if (portval & (1 << pin2)){ 		// pin2 high
					pos += 1;
				}else{				 		// pin2 low
					pos -= 1;
				}
			}else{ 						// pin1 high -> low
				if (portval & (1 << pin2)){ 		// pin2 high
					pos -= 1;
				}else{				 		// pin2 low
					pos += 1;
				}
			}
			changeFlag = 1;
		}
	}
	
	inline void zero(){
		pos = 0;
	}
	
};

class Motor{
	public: 
	Motor(volatile uint8_t &_port, volatile uint8_t &_ddr, char _pin_en, char _pin1, char _pin2):
		PORT(_port), DDR(_ddr), pin_en(_pin_en), pin1(_pin1), pin2(_pin2)
	{
		DDR |= (1 << pin_en) | (1 << pin1) | (1 << pin2);	
	}
	
	volatile uint8_t &PORT;
	volatile uint8_t &DDR;
	const char pin_en;
	const char pin1;
	const char pin2;
	
	inline void enable(){
		PORT |= (1 << pin_en);
	}
	
	inline void disable(){
		PORT &= ~ (1 << pin_en);
	}
	
	inline void left(){
		PORT |= (1 << pin1);
		PORT &= ~(1 << pin2);
		enable();
	}
	
	inline void right(){
		PORT |= (1 << pin2);
		PORT &= ~(1 << pin1);
		enable();
	}
};

Motor feed(PORTC, DDRC, 2, 0, 1);
Motor carriage(PORTC, DDRC, 5, 3, 4);

Encoder carriageEnc(PORTB, 1, 2);
Encoder feedEnc(PORTB, 6, 7);

ISR(PCINT0_vect){
	DDRB |= (1 << 5);
	PORTB |= (1 << 5);
	static uint8_t old_input_vals = 0;
	uint8_t input = PINB;
	uint8_t changed = input ^ old_input_vals;
	old_input_vals = input;
	
	carriageEnc.pinChanged(input, changed);
	feedEnc.pinChanged(input, changed);
}


int main() { 
	UCSR0B = 0;
	
	Serial.begin(115200);

	PCMSK0 = (1 << 1) | (1 << 6) | (1 << 2) | (1 << 7);
	PCICR |= (1 << PCIE0);
	sei();

	for(;;){
		if (carriageEnc.changeFlag || feedEnc.changeFlag){
			carriageEnc.changeFlag = feedEnc.changeFlag = 0;
			Serial.print("F: ");
			Serial.print(feedEnc.pos);
			Serial.print(" C: ");
			Serial.println(carriageEnc.pos);
		}
		
		if (Serial.available() > 0) {
    		int incomingByte = Serial.read();
    		switch (incomingByte){
    			case 'q':
    				feed.left();
    				break;
    			case 'a':
    				feed.right();
    				break;
    			case 'w':
    				carriage.left();
    				break;
    			case 's':
    				carriage.right();
    				break;
    			default:
    				feed.disable();
    				carriage.disable();	
    		}
    		Serial.print(PORTC, 2);
    		Serial.print(" ");
    		Serial.println((PINB&2), 2);
    	}
	}

	return 0;
}

