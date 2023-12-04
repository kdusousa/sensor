/******************************************************
 ****             MONTAGEM DO CIRCUITO             ****
 ****                                              ****
 **                                                  **
 **    P2.0         - Trigger do Sensor.             **
 **    P1.2 e P1.3  - Captura do Sensor (echo).      **
 **    P2.5         - Buzzer.                        **
 **                                                  **
 ******************************************************/

#include <msp430.h> 

#define LED_RED_ON P1OUT |= BIT0
#define LED_RED_OFF P1OUT &= ~BIT0
#define LED_GRE_ON  P4OUT |= BIT7
#define LED_GRE_OFF P4OUT &= ~BIT7
#define T17hz  61680// Limite para 17 medidas por segundo
#define T20us 20 //período de 20 us, (1048576 x 0,00002) - 1
#define T12ms 12582// (1048576 x 0,012) - 1

void io_config(void);
void config_ta01(void); //TA0.1 - configura Entrada e captura P1.2 do Echo e o canal para ler subida do Echo
void config_ta02(void); //TA0.2 - configura captura P1.3 para ler descida do Echo
void config_ta11(void); //TA1.1 (P2.0 Saída - Trigger)
void config_ta22(void); //TA2.2 (P2.5 Saída - Buzzer)
void r2d2(int);
void leds(float);
int calc_freq(float);

volatile unsigned int tStart = 0, tEnd = 0, eTime;
volatile unsigned int freq = 0;
volatile float dist;


/**
 * main.c
 */
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	
	io_config();
	config_ta11(); //P2.0
	config_ta01(); //P1.2
	config_ta02(); //P1.3
	config_ta22(); //P2.5

	__enable_interrupt();

	while(1);


	return 0;
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void func_eco(){
    switch(_even_in_range(TA0IV,0x0E)){
    case 0x00:
        break;
    case 0x02:
        tStart = TA0CCR1;
        break;
    case 0x04:
        tEnd = TA0CCR2;
        eTime = (tEnd > tStart)? (tEnd - tStart):(T12ms - (tStart - tEnd));
        dist = 170 * 0.012 * eTime/12582; //340/2
        dist = dist * 100;// centimetros
        leds(dist);
        freq = calc_freq(dist);
        r2d2(freq); //P2.5 saída Buzzer
        break;
    default:
        break;
    }
}

//Saída em P2.0 para o trigger
void config_ta11(void){
    TA1CTL = TASSEL__SMCLK | MC__UP;
    TA1CCTL1 = OUTMOD_6; //TOGGLE - Reset
    TA1CCR0 = T17hz;//T17hz; // Limite para 17 medidas por segundo (61680)
    TA1CCR1 = T20us; //Período 20 us

    P2DIR |= BIT0; //P2.0 como saída
    P2SEL |= BIT0; //P2.0 dedicado ao TA1.1
}

//******************************************************************************
//configura Entrada e captura P1.2 do Echo e o canal para ler subida do Echo
void config_ta01(void){
    TA0CTL = TASSEL__SMCLK | MC__UP;
    TA0CCR0 = T12ms;
    TA0CCTL1 = CM_1 |
               CCIS_0 |
               SCS |
               CAP |
               CCIE;
    P1SEL |= BIT2; //P1.2 captura
    P1DIR &= ~BIT2; //P1.2 como entrada

}

//Captura descida do echo no pino P1.3
void config_ta02(void){
    //TA0CTL = TASSEL__SMCLK | MC__UP;
    //TA0CCR0 = T12ms;
    TA0CCTL2 = CM_2 | //configura o canal 1 para ler descida do Echo
               CCIS_0 |
               SCS |
               CAP |
               CCIE;
    P1SEL |= BIT3; //P1.3 captura
    P1DIR &= ~BIT3; //P1.3 como entrada

}

//****************************************************************************************
//Buzzer na porta 2.5
void config_ta22(void){
    TA2CTL = TASSEL__SMCLK | MC__UP;
    TA2EX0 = TAIDEX_0; //???
    TA2CCTL2 = OUTMOD_6; //TA2.2, saída no Modo 6

    P2DIR |= BIT5; //P2.5 como saída
    P2OUT &= ~BIT5;
    P2SEL |= BIT5; //P2.5 dedicado ao TA2.2
}

void io_config(void){

    //Configurar Led Vermelho
    P1DIR |= BIT0;              // Configura pino como saída - P1.0 = 1
    P1OUT &= ~BIT0;             // Led 1 apagado             - P1.0 = 0

    //Configura Led Verde
    P4DIR |= BIT7;              // Led 2 = saída
    P4OUT &= ~BIT7;             // Led 2 apagado

    //Configura chave S1
    P2DIR &= ~BIT1;              // Configura chave como entrada - P2.1 = 0
    P2REN |= BIT1;               // Habilita resistor
    P2OUT |= BIT1;               // Habilita pullup

    //Configura chave S2
    P1DIR &= ~BIT1;              // Configura chave como entrada - P1.1 = 0
    P1REN |= BIT1;               // Habilita resistor
    P1OUT |= BIT1;               // Habilita pullup
}

void debounce(int a){
    volatile int x; //volatile evita optimizador do compilador
    for (x=0; x<a; x++); //Apenas gasta
}


void leds(float dist){
    if (dist > 50.0){
        LED_RED_OFF;
        LED_GRE_OFF;
    }
    else if ((dist >= 30.0) && (dist <= 50.0) ){
        LED_RED_OFF;
        LED_GRE_ON;
    }
    else if ((dist >= 10.0) && (dist < 30.0)){
        LED_RED_ON;
        LED_GRE_OFF;
    }
    else if ((dist < 10.0)){
        LED_RED_ON;
        LED_GRE_ON;
    }
}

int calc_freq(float dist){
    if (dist >= 50) freq = 0;
    else if ((dist > 0) && (dist < 50)){
        freq = 5000 - 100 * dist;
        freq = 1048576 / freq;
    }
    return freq;
}

void r2d2(int freq){
    if (freq != 0){ // Freq != zero
     TA2CCR0 = freq; //Programar o período
     TA2CCR2 = TA2CCR0/2; //Carga de 50%
     }
     else{ //Se Freq = zero
     TA2CCR0 = 0;
     TA2CCR2 = 0;
     }
}
