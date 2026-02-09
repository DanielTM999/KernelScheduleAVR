/*
  Exemplo 02: Basic Blink
  Demonstra como criar uma thread simples que roda em paralelo ao loop().
*/
#include <KernelSchedule.h>

// Pilha para a Thread (tamanho pequeno é suficiente para piscar LED)
uint8_t stackBlink[STACK_SIZE_SMALL];
void threadLed();

void setup() {
  Serial.begin(9600);
  
  // Inicializa o Kernel
  OS::init();
  
  // Cria a thread passando a função e o buffer da pilha
  OS::newThread(threadLed, stackBlink);
}

void loop() {
  // O loop principal pode fazer outras coisas ou ficar vazio nesse caso irá ser uma idle thread
}

// Função da Thread
void threadLed() {
  pinMode(LED_BUILTIN, OUTPUT);
  while (true) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // Dorme por 500ms (libera CPU para o loop principal)
    Thread::sleep(500);
  }
}