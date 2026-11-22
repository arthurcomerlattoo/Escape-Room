#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
int16_t quantidade_de_tempo_desafio = 300; // Para definir a cada entrada no modo de desafio, antes tava começando já com menos tempo
uint8_t erros = 0;
uint8_t desafios_feitos = 0;
uint8_t alavanca_ligada = 0; // Constante do potenciômetro
volatile bool botao_confirmado = false; // Parâmetro para verificar se o botão de confirmação foi pressionado 

//Estados
uint8_t estado = 0; // Têm que começar do 0, tá em 2 só pra teste
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
LiquidCrystal_I2C lcd(0x27, 16, 2);

int8_t bit_leitura = 0; // Para a leitura no monitor serial

// Interrupções de timers e externas
ISR(TIMER1_COMPA_vect) { // roda a cada 1 ms
	ms++;
}

ISR(INT1_vect) { // Interrupção para verificar se o botão de confirmação foi pressionado
	botao_confirmado = true;
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
	set_bit(ADMUX, REFS0 | canal); //ADMUX = (1 << REFS0) | canal; // referência 5 V + canal
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
  
  // Configurações pro LCD
	lcd.init();
	lcd.backlight();
  	

	set_bit(DDRB, PB1); // Define o led verde como saída
  set_bit(DDRB, PB2); // Define o led amarelo como saída
  set_bit(DDRB, PB3); // Define o led vermelho como saída
  set_bit(DDRD, PD4); // Define o led azul como saída
  set_bit(DDRB, PB0); // Define o buzzer como saída

  clr_bit(DDRC, PC0); // Define o potenciômetro como entrada
	clr_bit(DDRC, PC3); // Define o fotoresistor como entrada
  	
  cli(); // Inicia as definições das interrupções
  	
	TCCR1A = 0; // apaga o que a IDE configurou
	// modo CTC, topo = OCR1A, N = 64: passo de 4 us
  set_bit(TCCR1B, WGM12);
  set_bit(TCCR1B, CS10);
  set_bit(TCCR1B, CS11); //TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 249; // 250 passos = 1 ms
  set_bit(TIMSK1, OCIE1A); //TIMSK1 = (1 << OCIE1A); Liga a interrupção do timer
  set_bit(ADCSRA, ADEN);
  set_bit(ADCSRA, ADPS2);
  set_bit(ADCSRA, ADPS1);
  set_bit(ADCSRA, ADPS0);  // ADCSRA = (1 << ADEN) Liga o ADC
-	//| (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // clock: 16 / 128
  clr_bit(DDRD, PD3);
  set_bit(EICRA, ISC11); // Define a interrupção do botão como borda de descida
  set_bit(EIMSK, INT1); // Ativa a interrupção 1 (pino 3)
  	
  clr_bit(DDRD, PD6);  // Define o pino 6 (PD6) como entrada
	set_bit(PORTD, PD6); // Ativa o pull-up interno (fica em 1 solto, vai para 0 quando apertado)	
  	
  sei(); // Termina as definições das interrupções
  
  Serial.begin(9600);
}

void loop() {

  while (estado == 0) { // Modo de início, pressionar botão para passar
    lcd.setCursor(4, 0);
  	lcd.print("Escape  Room ");
    lcd.setCursor(3,1);
    lcd.print("aperte o botao");
    lcd.setCursor(5,2);
    lcd.print("para jogar");
    if (botao_confirmado) { // Se pressionar o botão de confirmação, para o estado de escolha de dificuldade 
      estado = 1;
      botao_confirmado = false;
      lcd.clear();
    }
  }

  while (estado == 1) { // Modo de escolha de modo, escrever no serial monitor D ou F para escolher a dificuldade
    if (Serial.available() > 0) { // Se tiver algo escrito, ele vai ler, transformar para ASCII e escolher o modo em base nisso
      bit_leitura = Serial.read();
      Serial.println(bit_leitura, DEC);
      if (bit_leitura == 70){
        estado = 2; // Se ler F, vai pro modo Fácil
        quantidade_de_tempo_desafio = 300; // 5 minutos para completar o desafio fácil
      }
      if (bit_leitura == 68){
        estado = 3; // Se ler D, vai pro modo Difícil 
        quantidade_de_tempo_desafio = 360; // 6 minutos para completar o desafio difícil 
      }
    }
  }
  while (estado == 2) { // Modo fácil, 5 minutos de partida
    penalidade = 40; // -40 segundos por erro
    if ((erros >= 3) || (tempo_restante(quantidade_de_tempo_desafio) <= 0)) { // Se errar 3 vezes ou acabar o tempo, vai para o modo de derrota
      lcd.clear();
      estado = 4;
      break;
    }
    
    Serial.println(meu_analogRead(0)); // Printa o valor do potênciometro, para saber se está correto, trocar pelo pwm dps

    if(botao_confirmado && (desafios_feitos == 0)) { // Confirma o valor com o botão, então verifica se o potênciometro está no valor correto
      botao_confirmado = false; 
      if (atualiza_alavanca(meu_analogRead(0)) == true) { // se estiver dentro do limiar 750 - 850 passou no desafio
        desafios_feitos += 1;
      }
      else { // Se errar, vai aumentar o erro
        erros += 1; 
        quantidade_de_tempo_desafio -= penalidade; // Tira o tempo de penalidade a cada erro
      }
    }
    if (desafios_feitos == 1) {
        set_bit(PORTB, PB4); // liga o led laranja para demonstrar que já fez 1 desafio anteriormente
        if (meu_analogRead(3) <= 420) { // Quando ficar escuro, ele vai detectar
            desafios_feitos += 1;
        }
    }
      if ((desafios_feitos == 2) && (erros < 3)) { // Se completar os 2 desafios e for menos que 3 erros, vai para a tela de vitória
        set_bit(PORTD, PD4); // Liga o led azul para demonstrar que completou o segundo desafio
        lcd.clear();
        estado = 5;
        break;
      }

    lcd.setCursor(0, 0);
    lcd.print("Tempo restante: ");
    lcd.setCursor(0, 1);
    lcd.print(tempo_restante(quantidade_de_tempo_desafio)); // 600000 inicial, mas pra outro modo pode ser menos
    lcd.print("  "); // Para quando diminuir a quantidade de tempo, não bugar, e não precisar de clear
    lcd.setCursor(7, 1);
    lcd.print("Erros: ");
    lcd.print(erros);
  }

  while (estado == 3) { // modo difícil, 6 minutos
    penalidade = 90; //pra ser bem difícil
    if ((erros >= 3) || (tempo_restante(quantidade_de_tempo_desafio) <= 0)) { //pra mim 3 vidas significa no max 2 erros
      lcd.clear();
      estado = 4;
      break;
    }
    //igual o modo facil:
    Serial.println(meu_analogRead(0));
    if (botao_confirmado && (desafios_feitos == 0)) { // Igual do desafio fácil
      botao_confirmado = false;
      if (atualiza_alavanca(meu_analogRead(0)) == true) {
        desafios_feitos += 1;
      } 
      else {
        erros += 1;
        quantidade_de_tempo_desafio -= penalidade;
      }
    }

    if (desafios_feitos == 1) { // Mesmo do desafio fácil
      set_bit(PORTB, PB4); 
      Serial.println(meu_analogRead(3));
      if (meu_analogRead(3) <= 420) {
        desafios_feitos += 1;
      }
    }
	  //desafio novo:
    if (desafios_feitos == 2) { // Têm que apertar o bortão correto, e depois confirmar
      set_bit(PORTD, PD2);
      if (tst_bit(PIND, PD6)){
        botao_certo = true;
      }
      if (botao_confirmado && botao_certo == true) {
          desafios_feitos += 1;
          botao_confirmado = false;
      } 
      else if (botao_confirmado == true) {
        erros += 1;
        quantidade_de_tempo_desafio -= penalidade;
        botao_confirmado = false;
      }
    }

    if ((desafios_feitos == 3) && (erros <= 2)) { // Se fizer os 3 desafios e não tiver 3 erros, vai para o estado de vitória
      set_bit(PORTD, PD4);
      lcd.clear();
      estado = 5;
      break;
    }

    lcd.setCursor(0, 0);
    lcd.print("Tempo restante: ");
    lcd.setCursor(0, 1);
    lcd.print(tempo_restante(quantidade_de_tempo_desafio));
    lcd.print("  ");
    lcd.setCursor(7, 1);
    lcd.print("Erros: ");
    lcd.print(erros);
  }
    
  while (estado == 4) { // Tela de derrota
    lcd.setCursor(5, 0);
    lcd.print(" Derrota ");
    lcd.setCursor(0, 1);
    lcd.print("Aperte o botao para");
    lcd.setCursor(0, 2);
    lcd.print("  ir para o inicio ");
    if (botao_confirmado) { // Volta para o modo de início apertando o botão
      estado = 0;
      botao_confirmado = false;
      lcd.clear();
    }
  }

  while (estado == 5) { // Tela de vitória
    lcd.setCursor(5, 0);
    lcd.print(" Vitoria ");
    lcd.setCursor(0, 1);
    lcd.print("Aperte o botao para");
    lcd.setCursor(0, 2);
    lcd.print("  ir para o inicio ");
    if (botao_confirmado) { // Volta para o modo de início apertando o botão
      estado = 0;
      botao_confirmado = false;
      lcd.clear();
    }
  }
}