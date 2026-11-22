#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>

// Macros de bit
#define set_bit(reg, bit) ((reg) |= (1 << (bit)))
#define clr_bit(reg, bit) ((reg) &= ~(1 << (bit)))
#define cpl_bit(reg, bit) ((reg) ^= (1 << (bit)))
#define tst_bit(reg, bit) ((reg) & (1 << (bit)))

#define LIMIAR_ALTO 850 // acima disso: liga
#define LIMIAR_BAIXO 750 // abaixo disso: desliga

// Variáveis de tempo
volatile uint32_t ms = 0;
volatile uint32_t t = 0;
uint32_t anterior = 0;
uint32_t variavel_tempo_monitor = 0;

uint8_t penalidade = 60;
int16_t quantidade_de_tempo_facil = 300;
int16_t quantidade_de_tempo_dificil = 360;
uint8_t erros = 0;
uint8_t desafios_feitos = 0;
uint8_t alavanca_ligada = 0; // Constante do potenciômetro
volatile bool botao_confirmado = false; // Parâmetro para verificar se o botão de confirmação foi pressionado 

//Estados
uint8_t estado = 3; // Têm que começar do 0, tá em 2 só pra teste
// Estado 0: Tela de início
// Estado 1: Escolha de modo
// Estado 2: Fácil
// Estado 3: Difícil
// Estado 4: Tela de derrota
// Estado 5: Tela de vitória

int resposta_dificil = 800; 
//integral [0,2] raiz((4x17 + 2x(raiz(256)) x (93/3 + 729^1/3)))
//ou 400 + 400

//jogo do botao
volatile bool botao_certo = false;

// Da biblioteca
Adafruit_LiquidCrystal lcd(0);


// Interrupções de timers e externas
ISR(TIMER1_COMPA_vect) { // roda a cada 1 ms
	ms++;
}

ISR(INT1_vect) { // Interrupção para verificar se o botão de confirmação foi pressionado
	botao_confirmado = true;
}

ISR(INT0_vect) { // Interrupção externa para o que precisar
	// colocar o que faz aqui
}

// Funções feitas para usar no resto do código
uint32_t meu_primeiro_millis() { // millis()
	return ms;
}

uint32_t tempo_atual() {
	return ms;
}

/* Essa função serve para calcular quanto tempo de 
jogo ainda vai ter, o parâmetro dado é quanto tempo
inicialmente irá ter, em segundos. */
int32_t tempo_restante(uint32_t tempo_inicial){
  return (tempo_inicial - (meu_primeiro_millis() / 1000));
} // Tirei o u dessa função, para quando usar quando for negativo R:perfeito, Morgan.


uint16_t meu_analogRead(uint8_t canal) {
	ADMUX = (1 << REFS0) | canal; // referência 5 V + canal
	set_bit(ADCSRA, ADSC); // começa a conversão
	while (tst_bit(ADCSRA, ADSC)); // espera terminar
	return ADC; // resultado: 0 a 1023
}

void meu_analogWrite(uint8_t valor) { // analogWrite(6, valor)
OCR0A = valor;
}

bool atualiza_alavanca(uint16_t valor) {
	if ((valor > LIMIAR_BAIXO) && (valor < LIMIAR_ALTO)) {
		alavanca_ligada = 1;
		set_bit(PORTB, PB5); // acende o LED (D13)
    	return true;
    } else {
		alavanca_ligada = 0;
		clr_bit(PORTB, PB5); // apaga o LED
    	return false;
	}
}

void setup() {
  
	lcd.begin(16, 2);
	lcd.setBacklight(1);

  	//Display text
  	//lcd.setCursor(0, 0);
	//lcd.print("Hello, World!");

  	//lcd.setCursor(0, 1);
	//lcd.print("I2C LCD Ready");
  	
	set_bit(DDRB, PB1); // Define o led verde como saída
  	set_bit(DDRB, PB2); // Define o led amarelo como saída
  	set_bit(DDRB, PB3); // Define o led vermelho como saída
  	set_bit(DDRD, PD4); // Define o led azul como saída
  	set_bit(DDRB, PB0); // Define o buzzer como saída
  	
  	//set_bit(DDRB, PB2); //trig (saída)
  	//clr_bit(DDRB, PB3); //echo (etrada)
  	
  	clr_bit(DDRC, PC0); // Define o potenciômetro como entrada (não precisa, no padrão já é 0)
	clr_bit(DDRC, PC3); // Define o fotoresistor como entrada (não precisa, no padrão já é 0)
  	
  	cli(); // Inicia as definições das interrupções
  	
	TCCR1A = 0; // apaga o que a IDE configurou
	// modo CTC, topo = OCR1A, N = 64: passo de 4 us
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 249; // 250 passos = 1 ms
	TIMSK1 = (1 << OCIE1A); // Liga a interrupção do timer
  	ADCSRA = (1 << ADEN) // Liga o ADC
	| (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // clock: 16 / 128
    clr_bit(DDRD, PD3);
  	set_bit(EICRA, ISC11); // Define a interrupção do botão como borda de descida
    set_bit(EIMSK, INT1); // Ativa a interrupção 1 (pino 3)
  	set_bit(EIMSK, INT0); // Ativa a interrupção 0 (ainda não usada)
  	
  	clr_bit(DDRD, PD6);  // Define o pino 6 (PD6) como entrada
	set_bit(PORTD, PD6); // Ativa o pull-up interno (fica em 1 solto, vai para 0 quando apertado)
  	
  	
  	sei(); // Termina as definições das interrupções
  
  Serial.begin(9600);
}

void loop() {
  	while (estado == 0) { // Modo de início, pressionar botão para passar
      
    }
  	while (estado == 1) { // Modo de escolha de modo, pressione o botão de escolha, ou escreva no monitor

    }
  	while (estado == 2) { // Modo fácil, 5 minutos
      	penalidade = 40; //R: pra que definir penalidade =60 no começo então?
    	if ((erros >= 4) || (tempo_restante(quantidade_de_tempo_facil) <= 0)) { // Se errar a quarta vez, ou acabar o tempo, vai pra tela de derrota
        	lcd.clear();
          	estado = 4;
          	break;
      	}
    	Serial.println(meu_analogRead(0)); // Printa o valor do potênciometro, para saber se está correto
    	if(botao_confirmado && (desafios_feitos == 0)) { // Confirma o valor com o botão, daí verifica se o potênciometro está no valor correto
  			botao_confirmado = false; 
      		if (atualiza_alavanca(meu_analogRead(0)) == true) { // se estiver dentro do limiar 750 - 850 passou no desafio
     	 		desafios_feitos += 1;
   			}
    		else {
      			erros += 1; // Aumenta a quantidade de erro
        		Serial.println(erros);
      			quantidade_de_tempo_facil -= penalidade; // Tira o tempo de penalidade a cada erro
      		}
    	}
      	if (desafios_feitos == 1) {
          	set_bit(PORTB, PB4); // liga o led laranja para demonstrar que já fez 1
          	if (meu_analogRead(3) <= 512) { // Quando a luz for ligada, ele vai detectar
              	desafios_feitos += 1;
            }
       	}
        if ((desafios_feitos == 2) && (erros <=3)) {
          	set_bit(PORTD, PD4); // Liga o led azul para demonstrar que completou o segundo desafio
          	lcd.clear();
      		estado = 5; // Vai para a tela de vitória
          	break;
        }
    	lcd.setCursor(0, 0);
    	lcd.print("Tempo restante: ");
  		lcd.setCursor(0, 1);
    	lcd.print(tempo_restante(quantidade_de_tempo_facil)); // 600000 inicial, mas pra outro modo pode ser menos
  	  	lcd.print("  "); // Para quando diminuir a quantidade de tempo, não bugar, e não precisar de clear
      	lcd.setCursor(7, 1);
      	lcd.print("Erros: ");
      	lcd.print(erros);
    }
      while (estado == 3) { // modo difícil, 6 minutos
      penalidade = 90; //pra ser bem difícil
      if ((erros >= 3) || (tempo_restante(quantidade_de_tempo_dificil) <= 0)) { //pra mim 3 vidas significa no max 2 erros
        lcd.clear();
        estado = 4;
        break;
      }
		//igual o modo facil:
      Serial.println(meu_analogRead(0));

      if (botao_confirmado && (desafios_feitos == 0)) {
        botao_confirmado = false;
        if (atualiza_alavanca(meu_analogRead(0)) == true) {
          desafios_feitos += 1;
        } 
        else {
          erros += 1;
          Serial.println(erros);
          quantidade_de_tempo_dificil -= penalidade;
        }
      }

      if (desafios_feitos == 1) {
        set_bit(PORTB, PB4); 
        if (meu_analogRead(3) <= 512) {
          desafios_feitos += 1;
        }
      }
	  //desafio novo:
      if (desafios_feitos == 2) {
        set_bit(PORTD, PD2);
        if (!tst_bit(PIND, PD6)){
        	botao_certo = true;
        }
        if (botao_confirmado && botao_certo == true) {
            desafios_feitos += 1;
            botao_confirmado = false;
        } 
        else if (botao_confirmado == true) {
        	erros += 1;
        	quantidade_de_tempo_dificil -= penalidade;
        	botao_confirmado = false;
        }
      }
      

      if ((desafios_feitos == 3) && (erros <= 2)) {
        set_bit(PORTD, PD4);
        lcd.clear();
        estado = 5;
        break;
      }

      lcd.setCursor(0, 0);
      lcd.print("Tempo restante: ");
      lcd.setCursor(0, 1);
      lcd.print(tempo_restante(quantidade_de_tempo_dificil));
      lcd.print("  ");
      lcd.setCursor(7, 1);
      lcd.print("Erros: ");
      lcd.print(erros);
    }
    
  	while (estado == 4) { // Tela de derrota
      	lcd.setCursor(0, 0);
   		lcd.print(" Derrota ");
    }
  	while (estado == 5) { // Tela de vitória
     	lcd.setCursor(0, 0);
  		lcd.print(" Vitoria ");
    }
  	if (tempo_atual() <= 300000){
		if (meu_primeiro_millis() - anterior >= 1000) {
      		Serial.println("entrou");
			anterior = meu_primeiro_millis();
			cpl_bit(PORTB, PB1); // pisca o led VERDE
      		cpl_bit(PORTB, PB0);
		}
  	}
 	 else if (tempo_atual() >= 300000 && tempo_atual() <= 540000){
	    if (meu_primeiro_millis() - anterior >= 700) {
			anterior = meu_primeiro_millis();
 	       cpl_bit(PORTB, PB2); //pisca o led AMARELO
	        cpl_bit(PORTB, PB0);
	    }
	  }
	  else{
	  	if (meu_primeiro_millis() - anterior >= 350) {
			anterior = meu_primeiro_millis();
	        cpl_bit(PORTB, PB3); //pisca o led VERMELHO
	        cpl_bit(PORTB, PB0);
 	   }
	  }
  
  // Sensor LDR: quando ele ler um determinado valor de luminosidade, acende o led cinza.
  const int sensor_ldr = 3;
  int leitura_ldr = meu_analogRead(sensor_ldr);
  float tensao_ldr_adc = leitura_ldr * (5 / 1024);
  
  if (leitura_ldr < 512){
    clr_bit(PORTB, PB5); // O led cinza fica apagado.
  } else {
    set_bit(PORTB, PB5); // O led ciza acende.
  }
  delay(100);
  
  t = tempo_restante(600000);
  
  static unsigned long lastUpdate = 0;
    
  	variavel_tempo_monitor = meu_primeiro_millis();
    lcd.setCursor(0, 0);
    lcd.print("Tempo restante: ");
  	lcd.setCursor(0, 1);
    lcd.print(t);
// o resto do loop roda sem esperar
}
