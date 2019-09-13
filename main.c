#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "uart_functions_m48.h"
#include "nrf24l01.h"
#include "nrf24l01registers.h"
#define ON PORTD = 0x24
#define OFF PORTD = 0x28
#define  DOWN PORTD = 0x00
uint8_t status;                 //transmission status
uint8_t pipe = 0;               //receiving pipe
uint8_t sec = 0;                //seconds
uint8_t timee = 0;              //cycle time held here
uint8_t i = 0;                  //loop variable for adc read
uint8_t vs = 0;                 //determine the valve status. 0 -> closed, 1 -> open
uint8_t vth = 0;                //determine if in threshold voltage zone
uint16_t adc_result = 0;        //holds adc result
int size_array;
int charge = 0;
char cycle[2];
char message[50];                  //cycle time is saved here
char total_percent[42] = "The percent of the battery is currently: \0";
char number_of_ticks[33] = "the current number of ticks is: \0";


#define FIFO_SIZE 128
#define UNITID 1
#define HUBORNODE 0
#define FOSC 1000000 // Clock Speed
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1



struct fifo_struct{
    uint8_t size;           /* size of buffer in bytes */
    uint8_t read;           /* read pointer */
    uint8_t write;          /* write pointer */
    char buffer[FIFO_SIZE]; /* fifo ring buffer */
}; 

typedef struct fifo_struct fifos;

uint8_t FifoDataLength (fifos *fifo){
  if(fifo->write > fifo->read)
    return((fifo->write - fifo->read - 1));
  else if(fifo->write < fifo->read)
    return ((FIFO_SIZE + fifo->write - fifo->read - 1));
  else
    return (FIFO_SIZE);
  return ((fifo->write - fifo->read) & (fifo->size - 1));
}

uint8_t FifoWrite (fifos *fifo, unsigned char data){
    // fifo full : error
    if (FifoDataLength(fifo) == FIFO_SIZE){
        return 1;
    }
    // write data & increment write pointer
    fifo->buffer[fifo->write] = data;
    fifo->write = (fifo->write + 1);
    if(fifo->write == FIFO_SIZE){
      fifo->write = 0;
    }
    return 0;
}


uint8_t FifoRead (fifos *fifo, unsigned char *data){
    // fifo empty : error
    if (FifoDataLength(fifo) == FIFO_SIZE){
        return 1;
    }
    // read data & increment read pointer
    *data = fifo->buffer[fifo->read];
    fifo->read = (fifo->read + 1);
    if(fifo->read == FIFO_SIZE){
      fifo->read = 0;
    }
    return 0;
}

void fill(fifos *fifo, char message[50]){
 int j = 0;
  while(j<size_array){
    fifo->buffer[fifo->write] = message[j];
    fifo->write = fifo->write + 1;
    j++;
  }
} 

void USART_Transmit(unsigned int data ){
/* Wait for empty transmit buffer */
while (!(UCSR0A&(1<<UDRE0)));

/* Put data into buffer, sends the data */
UDR0 = data;
}

void USART_Init( unsigned int ubrr){
/*Set baud rate */
  UBRR0H = (unsigned char)(ubrr>>8);
  UBRR0L = (unsigned char)ubrr;
  /*Enable receiver and transmitter */
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);
  /* Set frame format: 8data, 2stop bit */
  UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}
//*******************************************************//
uint8_t debug = 1; // 1 enable usart for debugging, 0 disable usart
//*******************************************************//

//********************************************************//
//                
//********************************************************//
void setLEDs(int value){
    PORTD |= (0b11100000);
    PORTB |= (0b00000001);
    if (value == 0){
      PORTD &= ~(0b11100000);
      PORTB &= ~(0b00000001);
    } else if (value < 10) {
      PORTD &= ~(0b11000000);
      PORTB &= ~(0b00000001);
    } else if (value < 50) {
      PORTD &= ~(0b10000000);
      PORTB &= ~(0b00000001);
    } else if (value < 90) {
      PORTD &= ~(0b00000000);
      PORTB &= ~(0b00000001);
    } else {
      PORTD &= ~(0b00000000);
      PORTB &= ~(0b00000000);
    }
}

ISR(USART_RX_vect){//RX complete

}
ISR(USART_TX_vect){//TX complete

}
ISR(USART_UDRE_vect){//Data register empty

}
ISR(PCINT2_vect){
  uint8_t i;
  for (i = 0; i<101; i++){
			setLEDs(i);
			_delay_ms(5);
		  }

  if ((PIND & (1<<3)) == (1<<3)){
    fifos fifo = {.read = 0, .write = 1};
    unsigned char data;
    char to_user[50];
    memset(to_user, '\0', sizeof(to_user)); 
    sprintf(to_user, "%s%d\n", number_of_ticks,charge);
    size_array = sizeof(to_user);
    fill(&fifo,to_user);
    while(1){
    if (FifoDataLength(&fifo) > 0){
        FifoRead(&fifo, &data);
        USART_Transmit(data);
      }
    }
  }

  if ((PIND & (1<<2)) == (1<<2)){
    charge++;
  }
}


//********************************************************//
//                Main function
//********************************************************//
int main(){
	//uint8_t i;
  sei();
  EIMSK |= (1 << INT0) | (1 << INT1);
  EIFR |= (1 << INTF0) | (1 << INTF1);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  PCICR |= (1 << PCIE2);
  PCIFR |= (1 << PCIF2);
  unsigned char data;
  fifos tmit_fifo = {.read = 0, .write = 1};
  fifos recv_fifo = {.read = 0, .write = 1};
	
  int percentage = 99;
  memset(message, '\0', sizeof(message)); 
  sprintf(message, "%s%d\n", total_percent,percentage);
  size_array = sizeof(message);
  fill(&tmit_fifo,message); 

	DDRB |= 0b00000001; //LED
	DDRD |= 0b11100000;

  PORTD |= 0b11100000;

  USART_Init(MYUBRR);

    while (1) {
      if (FifoDataLength(&tmit_fifo) > 0){
        FifoRead(&tmit_fifo, &data);
        USART_Transmit(data);
      }
      /*
      if ((PIND & (1<<3)) == (1<<3)){
        PORTD |= (0b11100000);
        PORTB |= (0b00000001);
      }
      else{
        PORTD &= ~(0b11100000);
        PORTB &= ~(0b00000001);
      }*/
      //sprintf();
      /*	for (i = 0; i<101; i++){
			setLEDs(i);
			_delay_ms(5);
		  }*/
    }//while
}//main