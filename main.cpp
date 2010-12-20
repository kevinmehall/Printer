#include "wiring_minimal.h"
#include "Ansiterm.h"
#include <avr/delay.h>

#define minpin 0
#define maxpin 7

short pinstates = 0;
char selected = 0;
Ansiterm ansi;

class Encoder{
	public:
	Encoder(volatile uint8_t &_port, char _pin1, char _pin2):
		PORT(_port), pin1(_pin1), pin2(_pin2), pos(0), speed(0)
		{}
	
	volatile uint8_t &PORT;
	const char pin1;
	const char pin2;
	volatile short pos;
	volatile short speed;
	volatile uint16_t last_pos;
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
	
	inline void timerOvf(){
		speed = pos - last_pos;
		last_pos = pos;
	}
	
	inline void zero(){
		pos = 0;
		last_pos = 0;
	}
	
};

#define STEP_TIME 5

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
	
	inline void stop(){
		PORT |= (1 << pin1) | (1 << pin2) | (1 << pin_en);
	}
	
};

void stepMotor(Encoder &enc, Motor &mot, short step){
	short startpos = enc.pos;
	short dest  = startpos + step;
	
	uint8_t i=0;
	uint16_t error = abs(enc.pos - dest);
	
	while(abs(enc.pos - startpos) < abs(step)){
		error = abs(enc.pos - dest);
		int16_t force = max(error, 50) - abs(enc.speed)/2;
		uint8_t power = constrain(force, 0, 255);
		
		if (force < 0){
			mot.stop();
		}else{
			if (i < power){
				if (step > 0)
					mot.right();
				else
					mot.left();
			}else{
				mot.disable();
			}
		}
		
		i++;
	}
	
	mot.stop();
	
	_delay_ms(100);
	
	mot.disable();
}





Motor carriage(PORTC, DDRC, 2, 0, 1);
Motor feed(PORTC, DDRC, 5, 4, 3);

Encoder carriageEnc(PORTB, 1, 2);
Encoder feedEnc(PORTB, 6, 7);

ISR(PCINT0_vect){
	static uint8_t old_input_vals = 0;
	uint8_t input = PINB;
	uint8_t changed = input ^ old_input_vals;
	old_input_vals = input;
	
	carriageEnc.pinChanged(input, changed);
	feedEnc.pinChanged(input, changed);
}

ISR(TIMER1_OVF_vect){
	DDRB |= (1 << 5);
	PORTB ^= (1 << 5);

	carriageEnc.timerOvf();
	feedEnc.timerOvf();
}

#define SERVO_PORT PORTB
#define SERVO_DDR DDRB
#define SERVO_PIN 3

uint8_t servo_state = 0;
volatile uint16_t servoPos = 1500;

ISR(TIMER1_COMPA_vect){
	// Variable time interrupt - Handle pulsing of PWM servo outputs
	uint16_t time = TCNT1; // take start time ASAP so time of this code doesn't matter
	if (servo_state){
		SERVO_PORT |= (1 << SERVO_PIN);
		OCR1A = time + servoPos;
		servo_state = 0;
	}else{
		SERVO_PORT &= ~ (1 << SERVO_PIN);
		OCR1A = time + 16000;
		servo_state = 1;
	}
}

#define PAPER_SENSOR_PORT PORTB
#define PAPER_SENSOR_INPUT PINB
#define PAPER_SENSOR_PIN 0

inline uint8_t paperSense(){
	return PAPER_SENSOR_INPUT & (1 << PAPER_SENSOR_PIN);
}

void feedPaper(){
	while (!paperSense()){
		feed.right();
	}
	stepMotor(feedEnc, feed, 5500);
	feedEnc.zero();
}

void printStatus(){
	Serial.print("F: ");
	Serial.print(feedEnc.pos);
	Serial.print(" C: ");
	Serial.print(carriageEnc.pos);
	Serial.print(" P: ");
	Serial.print(paperSense(), 2);
	Serial.print(" S: ");
	Serial.println(servoPos);
}


int main() { 
	UCSR0B = 0;
	
	Serial.begin(115200);

	PCMSK0 = (1 << 1) | (1 << 6) | (1 << 2) | (1 << 7);
	PCICR |= (1 << PCIE0);
	PAPER_SENSOR_PORT |= (1 << PAPER_SENSOR_PIN);
	
	OCR1A = 100;
	TCCR1A = 0;
	TCCR1B = ((1 << CS11)); // Enable timer, 1mhz 
	TIMSK1 = (1 << TOIE1) | (1 << OCIE1A);
	TCNT1 = 0;
	
	SERVO_DDR |= (1 << SERVO_PIN);
	
	sei();
	
	uint16_t val = 0;

	for(;;){
		if (carriageEnc.changeFlag || feedEnc.changeFlag){
			carriageEnc.changeFlag = feedEnc.changeFlag = 0;
			//Serial.println(carriageEnc.speed);
		}
		
		if (Serial.available() > 0) {
    		int incomingByte = Serial.read();
    		switch (incomingByte){
    			case 'p':
    				feedPaper();
    				break;
    			case 't':
    				Serial.println(val);
					break;
    			case 'f':
    				stepMotor(feedEnc, feed, val);
    				break;
    			case 'c':
    				stepMotor(carriageEnc, carriage, val-carriageEnc.pos);
    				break;
    			case 's':
    				servoPos = val;
    				_delay_ms(100);
    				break;
    			case 'x':
    				feed.right();
    				_delay_ms(1500);
    				feed.disable();
    			case 'n':
    				servoPos += 10;
    				break;
    			case 'm':
    				servoPos -= 10;
    				break;
    			default:
    				feed.disable();
    				carriage.disable();
    			}
    				
				if (incomingByte >='1' && incomingByte <='9'){
					val = val*10 + (1 + incomingByte - '1');
				}else if (incomingByte == '0'){
					val = val*10;
				}else{
					val = 0;
					printStatus();
				}
    	}
	}

	return 0;
}

