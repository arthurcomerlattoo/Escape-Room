//Bibliotecas
#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Macros de bit
#define set_bit(reg, bit) ((reg) |= (1 << (bit)))
#define clr_bit(reg, bit) ((reg) &= ~(1 << (bit)))
#define cpl_bit(reg, bit) ((reg) ^= (1 << (bit)))
#define tst_bit(reg, bit) ((reg) & (1 << (bit)))

//Máximo e mínimo de alguma função
volatile int16_t LIMIAR_ALTO = 850; // acima disso: liga
volatile int16_t  LIMIAR_BAIXO = 750; // abaixo disso: desliga

//Variáveis de tempo
uint32_t tempo = 0;
volatile uint32_t ms = 0;
volatile uint32_t t = 0;
uint32_t anterior = 0;
uint32_t variavel_tempo_monitor = 0;


//Constantes para debouncing
volatile unsigned long atraso_debounce = 50;
volatile unsigned long ultimo_tempo = 0;

bool primeira_passagem = true; //Uma flag pra não ficar repetindo coisas no loop

uint8_t penalidade = 0; //Variável para a penalidade de tempo por erro
int16_t quantidade_de_tempo_desafio = 300; //Para definir a cada entrada no modo de desafio, antes tava começando já com menos tempo
uint8_t erros = 0; //Pra resetar os erros
uint8_t desafios_feitos = 0; //Pra resetar os desafios
uint8_t alavanca_ligada = 0; //Constante do potenciômetro, perdão prof, não alteramos o nome
volatile bool botao_confirmado = false; //Parâmetro para verificar se o botão de confirmação foi pressionado, muito importante

//Estados
uint8_t estado = 0; // Têm que começar do 0, tá em 2 só pra teste (não está mais em teste :D)
// Estado 0: Tela de início
// Estado 1: Escolha de modo
// Estado 2: Fácil
// Estado 3: Difícil
// Estado 4: Tela de derrota
// Estado 5: Tela de vitória

uint16_t resposta_dificil = 800; //Para o limiar do modo difícil


//Jogo do botao
volatile bool botao_certo = false;

//Da biblioteca pra usar o led, tem que inicializar meio como se fosse uma malha quadriculada, algo assim
LiquidCrystal_I2C lcd(0x27, 16, 2);

int8_t bit_leitura = 0; //Para a leitura no monitor serial

//Interrupções (parte muito importante do trabalho)
ISR(TIMER1_COMPA_vect) { //Roda a cada 1 ms
  ms++;
}

ISR(INT1_vect) { //Interrupção para verificar se o botão de confirmação foi pressionado
  unsigned long tempo_agora = meu_primeiro_millis();
  if ((tempo_agora - ultimo_tempo) > atraso_debounce ) {
    botao_confirmado = true;
    ultimo_tempo = tempo_agora;
  }
}


//Funções de tempo feitas para usar no resto do código
uint32_t meu_primeiro_millis() { // millis()
  return ms;
}

uint32_t tempo_atual() {
  return ms;
}



/* Essa função serve para calcular quanto tempo de
jogo ainda resta, o parâmetro dado é quanto tempo
inicialmente irá ter, em segundos. */
int32_t tempo_restante(uint32_t tempo_inicial){
  return (tempo_inicial - (meu_primeiro_millis() / 1000));
} //Tirei o u dessa função, para quando usar quando for negativo


//Função nova: agora tá certo, NÃO mexam, como o sanduíche.png do tf2
uint16_t meu_analogRead(uint8_t canal) { //canal seria a porta de entrada (0, 1, 2 ,3... Claro que nao usamos todas (as portas analogicas))
  ADMUX = (1 << REFS0) | canal ; // referência 5 V + canal
  set_bit(ADCSRA, ADSC);         // começa a converter os valores
  while (tst_bit(ADCSRA, ADSC)); //fica aqui até terminar o serviço
  return ADC;
}

//detalhe, ocorre overflow se passar do valor maximo (255) e o led (azul) pisca (do jogo do potenciometro), detalhe bem legal
void Pwm_Timer0(int16_t valor) { // analogWrite na porta 6 (timer 0)
  if (valor >= 0 ){
    OCR0A = valor; //(Output Compare Register 0 A) nome do reg, vai de 0 a 255
  }
  else {
    OCR0A = 0;
  }
} 

// Verifica se está dentro dos limiares definidos
bool verifica_limiar(uint16_t valor) {
  if ((valor > LIMIAR_BAIXO) && (valor < LIMIAR_ALTO)) {
    alavanca_ligada = 1;
    return true;
  } else {
    alavanca_ligada = 0;
    return false;
  }
}

void setup() {
 
  // Configurações pro LCD
  lcd.init();
  lcd.backlight();
 
  set_bit(DDRB, PB0); // Define o buzzer como saída
  set_bit(DDRD, PD6); // Define o led do pwm (azul) como saída

  clr_bit(DDRC, PC0); // Define o potenciômetro como entrada
  clr_bit(DDRC, PC3); // Define o fotoresistor como entrada
  
  // D6 (OC0A) no PWM,
  set_bit(TCCR0A, COM0A1);
  set_bit(TCCR0A, WGM01);
  set_bit(TCCR0A, WGM00);

  set_bit(TCCR0B, CS01);
  set_bit(TCCR0B, CS00);

  cli(); // Inicia as definições das interrupções

  TCCR1A = 0; // apaga o que a IDE configurou
  // modo CTC, topo = OCR1A, N = 64: passo de 4 us
  set_bit(TCCR1B, WGM12);
  set_bit(TCCR1B, CS10);
  set_bit(TCCR1B, CS11);
  OCR1A = 249; // 250 passos = 1 ms
  set_bit(TIMSK1, OCIE1A); // Liga a interupção do timer
  set_bit(ADCSRA, ADEN);
  set_bit(ADCSRA, ADPS2);
  set_bit(ADCSRA, ADPS1);
  set_bit(ADCSRA, ADPS0);  // Liga o ADC, clock: 16 / 128
  clr_bit(DDRD, PD3); // Botão de confirmação 
  set_bit(EICRA, ISC11); // Define a interrupção do botão como borda de descida
  set_bit(EIMSK, INT1); // Ativa a interrupção 1 (pino 3)
 
  clr_bit(DDRD, PD7);  // Define o pino 7 (PD7) como entrada
  set_bit(PORTD, PD7); // Ativa o pull-up interno (fica em 1 solto, vai para 0 quando apertado)
 
  sei(); // Termina as definições das interrupções
  
  Serial.begin(9600);
}

void loop() {
  lcd.clear();
  while (estado == 0) { // Modo de início, pressionar botão para passar
    if (primeira_passagem == true) { // Na primeira passagem, irá redefinir o tempo, e definir os parâmetros necessários
      clr_bit(PORTB, PB0);
      Serial.println("Você chegou a casa de Escape Room, 'seja bem-vindo'");
      primeira_passagem = false;
    } 
    erros = 0;
    desafios_feitos = 0;
    lcd.setCursor(4, 0);
    lcd.print("Escape  Room ");
    lcd.setCursor(3,1);
    lcd.print("aperte o botao");
    lcd.setCursor(5,2);
    lcd.print("para jogar");
    if (botao_confirmado) { // Se pressionar o botão de confirmação, para o estado de escolha de dificuldade
      estado = 1;
      botao_confirmado = false;
      primeira_passagem = true;
      lcd.clear();
    }
  }

  while (estado == 1) { // Modo de escolha de modo, escrever no serial monitor D ou F para escolher a dificuldade
    if (primeira_passagem == true) {
      Serial.println("");
      Serial.println("Você vê duas portas na sua frente, escolha uma delas para começar");
      Serial.println("A porta da esquerda têm uma placa que contém a palavra 'Fácil' e a da direita contém 'Difícil'");
      Serial.println("Em qual delas você deseja entrar? Escreva 'F' para a da direita e 'D' para a da esquerda");
      Serial.println("");
      primeira_passagem = false;
    }

    lcd.setCursor(3, 0);
    lcd.print("Leia o Monitor");
    lcd.setCursor(1,1);
    lcd.print("Serial, e escolha");
    lcd.setCursor(2,2);
    lcd.print("sua dificuldade");

    if (Serial.available() > 0) { // Se tiver algo escrito, ele vai ler, transformar para ASCII e escolher o modo em base nisso
      bit_leitura = Serial.read();
      if ((bit_leitura == 70) || (bit_leitura == 102)){
        estado = 2; // Se ler F, vai pro modo Fácil
        primeira_passagem = true;
        lcd.clear();
      }
      if ((bit_leitura == 68) || (bit_leitura == 100)){
        estado = 3; // Se ler D, vai pro modo Difícil
        primeira_passagem = true;
        lcd.clear();
      }
    }
  }
  
  while (estado == 2) { // Modo fácil, 5 minutos de partida
    if (primeira_passagem == true) {
      ms = 0;
      quantidade_de_tempo_desafio = 300;
      erros = 0;
      primeira_passagem = false;
      penalidade = 40; // -40 segundos por erro
      botao_confirmado = false;
      LIMIAR_ALTO = 600;
      LIMIAR_BAIXO = 400;
      // Coloquei aqui pra nao ter q criar mais flags e caso contrario, ficaria repetindo, mesmo esquema de antes, nao vou repetir essa explicação dnv
      Serial.println("");
      Serial.println("Ao entrar na sala, você lê um aviso:");
      Serial.println("'A luz é o caminho'");
      Serial.println("Gire o potenciômetro para encontrar a luz mais azul e confirme com o botão.");
    }

    if ((erros >= 3) || (tempo_restante(quantidade_de_tempo_desafio) <= 0)) { // 3 erros ou acabou o tempo
      lcd.clear();
      estado = 4;
      primeira_passagem = true;
      break;
    }

    // PWM da alavanca (só enquanto o desafio 1 não foi feito)
    if (desafios_feitos == 0) {
      if (verifica_limiar(meu_analogRead(0)) == true) {
        Pwm_Timer0(255);
      } else if (meu_analogRead(0) < LIMIAR_BAIXO) {
        Pwm_Timer0(255 - (LIMIAR_BAIXO - meu_analogRead(0)));
      } else {
        Pwm_Timer0(255 - (meu_analogRead(0) - LIMIAR_ALTO));
      }
    }

    // Desafio 1: confirmar com o botão se a alavanca está na faixa
    if (botao_confirmado && (desafios_feitos == 0)) {
      botao_confirmado = false;
      if (verifica_limiar(meu_analogRead(0)) == true) {
        desafios_feitos += 1;
        Pwm_Timer0(255); // LED fixo, desafio concluído
        Serial.println("");
        Serial.println("Uma porta secreta se abre e você avança para a próxima sala.");
        Serial.println("Existe outro aviso nesta sala:");
        Serial.println("'A escuridão é o caminho'");
        Serial.println("Faça o sensor de luz ler o menor valor possível.");
      }
      else {
        erros += 1;
        quantidade_de_tempo_desafio -= penalidade;
      }
    }

    // Desafio 2: ficar no escuro
    if (desafios_feitos == 1) {
      set_bit(PORTB, PB4); // PRIMEIRO LED VERMELHO: 1 desafio feito
      botao_confirmado = false; // descarta cliques durante este desafio
      if (meu_analogRead(3) <= 420) {
        set_bit(PORTD, PD2); // SEGUNDO LED VERMELHO: 2 desafios feitos
        desafios_feitos += 1;
      }
    }

    if ((desafios_feitos == 2) && (erros < 3)) { // Vitória
      lcd.clear();
      estado = 5;
      primeira_passagem = true;
      break;
    }

    // Tempo restante no formato M:SS
    tempo = tempo_restante(quantidade_de_tempo_desafio);
    if (tempo < 0) {
      tempo = 0;
    }
    lcd.setCursor(0, 0);
    lcd.print("Tempo restante: ");
    lcd.print(int(tempo / 60));
    lcd.print(":");
    if ((tempo % 60) < 10) {
      lcd.print("0");
    }
    lcd.print(int(tempo % 60));
    lcd.setCursor(6, 1);
    lcd.print("Erros: ");
    lcd.print(erros);
  }

  while (estado == 3) { // modo difícil, 6 minutos
    if (primeira_passagem == true) {
      ms = 0;
      quantidade_de_tempo_desafio = 360;
      erros = 0;
      primeira_passagem = false;
      penalidade = 90;
      botao_confirmado = false;
      botao_certo = false;
      LIMIAR_BAIXO = resposta_dificil - 50; // 750
      LIMIAR_ALTO  = resposta_dificil + 50; // 850
      Pwm_Timer0(0);
      Serial.println("Ao entrar na sala, você lê um aviso:");
      Serial.println("'A luz é o caminho'");
      Serial.println("Gire o potenciômetro para encontrar a luz mais azul.");
    }

    if ((erros >= 3) || (tempo_restante(quantidade_de_tempo_desafio) <= 0)) {
      lcd.clear();
      estado = 4;
      primeira_passagem = true;
      break;
    }

    // PWM da alavanca (só enquanto o desafio 1 não foi feito)
    if (desafios_feitos == 0) {
      if (verifica_limiar(meu_analogRead(0)) == true) {
        Pwm_Timer0(255);
      } else if (meu_analogRead(0) < LIMIAR_BAIXO) {
        Pwm_Timer0(255 - (LIMIAR_BAIXO - meu_analogRead(0)));
      } else {
        Pwm_Timer0(255 - (meu_analogRead(0) - LIMIAR_ALTO));
      }
    }

    // Desafio 1: potenciômetro + botão
    if (botao_confirmado && (desafios_feitos == 0)) {
      botao_confirmado = false;
      if (verifica_limiar(meu_analogRead(0)) == true) {
        desafios_feitos += 1;
        Pwm_Timer0(255);
        Serial.println("");
        Serial.println("A sala se acende e uma porta secreta se abre");
        Serial.println("Você avança para a próxima sala");
        Serial.println("Existe outro aviso nesta sala:");
        Serial.println("'A escuridão é o caminho'");
        Serial.println("Faça o sensor de luz ler o menor valor possível.");
      }
      else {
        erros += 1;
        quantidade_de_tempo_desafio -= penalidade;
      }
    }

    // Desafio 2: ficar no escuro
    if (desafios_feitos == 1) {
      set_bit(PORTB, PB4);
      botao_confirmado = false; // Descarta cliques durante este desafio
      if (meu_analogRead(3) <= 420) {
        desafios_feitos += 1;
        botao_certo = false;
        Serial.println("");
        Serial.println("As luzes da sala se apagam e, mais uma vez, outra porta secreta se abre");
        Serial.println("Você novamente avança para a próxima sala");
        Serial.println("Nessa sala existe mais um aviso:");
        Serial.println("'O caminho óbvio muitas vezes é o errôneo.'");
        Serial.println("SEGURE o botão correto e pressione o CONFIRMAR AO MESMO TEMPO para escapar");
        Serial.println("DICA: existe um botão que claramente não está conectado a nada");
      }
    }

    // Desafio 3: segurar o botão certo (D7) e confirmar
    if (desafios_feitos == 2) {
      set_bit(PORTD, PD2);
      if (botao_confirmado) {
        botao_confirmado = false;
        if (!tst_bit(PIND, PD7)) { // botão certo segurado (pull-up: apertado = 0)
          desafios_feitos += 1;
        }
        else {
          erros += 1;
          quantidade_de_tempo_desafio -= penalidade;
        }
      }
    }

    if ((desafios_feitos == 3) && (erros <= 2)) { // Vitória
      set_bit(PORTD, PD4);
      lcd.clear();
      estado = 5;
      primeira_passagem = true;
      break;
    }

    // Tempo restante no formato M:SS
    tempo = tempo_restante(quantidade_de_tempo_desafio);
    if (tempo < 0) {
      tempo = 0;
    }
    lcd.setCursor(0, 0);
    lcd.print("Tempo restante: ");
    lcd.print(int(tempo / 60));
    lcd.print(":");
    if ((tempo % 60) < 10) {
      lcd.print("0");
    }
    lcd.print(int(tempo % 60));
    lcd.setCursor(6, 1);
    lcd.print("Erros: ");
    lcd.print(erros);
  }
 
  while (estado == 4) { // Tela de derrota
    if (primeira_passagem == true) {
      ms = 0;
      set_bit(PORTB, PB0);
      primeira_passagem = false;
    }
    if (tempo_restante(3) <= 0) {
      clr_bit(PORTB, PB0);
    }

    lcd.setCursor(5, 0);
    lcd.print(" Derrota ");
    lcd.setCursor(0, 1);
    lcd.print("Aperte o botao para");
    lcd.setCursor(0, 2);
    lcd.print("  ir para o inicio ");
    char comando = Serial.read();
    if (botao_confirmado || (comando == 'R') || (comando == 'r')) { // Volta para o modo de início apertando o botão ou digitando r no monitor serial
      clr_bit(PORTB, PB4);
      clr_bit(PORTD, PD2);
      Pwm_Timer0(0);
      clr_bit(PORTB, PB0);
      Pwm_Timer0(0);
      Serial.println("Jogo resetado!");
      estado = 0;
      botao_confirmado = false;
      primeira_passagem = true;
      lcd.clear();
    }
  }

  while (estado == 5) { // Tela de vitória
    if (primeira_passagem == true) {
        ms = 0;
        set_bit(PORTB, PB0);
        primeira_passagem = false;
      }
    if (tempo_restante(3) <= 0) {
      clr_bit(PORTB, PB0);
    }

    lcd.setCursor(5, 0);
    lcd.print(" Vitoria ");
    lcd.setCursor(0, 1);
    lcd.print("Aperte o botao para");
    lcd.setCursor(0, 2);
    lcd.print("  ir para o inicio ");
    char comando = Serial.read();
    if (botao_confirmado || (comando == 'R') || (comando == 'r')) { // Volta para o modo de início apertando o botão ou digitando r no monitor serial
      clr_bit(PORTB, PB4);
      clr_bit(PORTD, PD2);
      Pwm_Timer0(0);
      clr_bit(PORTB, PB0);
      Serial.println("Jogo resetado!");
      estado = 0;
      botao_confirmado = false;
      primeira_passagem = true;
      lcd.clear();
    }
  }
}