/*
  Exemplo 01: Random Blink
  Demonstra uma thread que roda em paralelo ao loop(), alterando
  o tempo de piscar aleatoriamente. 
  
  Conceitos abordados:
  - Criação de Thread
  - Thread::sleep()
  - Proteção de variável compartilhada (Critical Section)
*/

#include <KernelSchedule.h>

// 1. Alocação da Pilha (Stack)
// O tamanho SMALL (128 bytes) é suficiente para piscar LED e usar random()
uint8_t stackThread1[STACK_SIZE_SMALL];

// 2. Variável Compartilhada
// 'volatile' avisa ao compilador que este valor pode mudar a qualquer momento
volatile int tempoSorteado = 0;

// Protótipo da função da thread
void threadRandomBlink();

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  // Inicializa o gerador de números aleatórios com ruído analógico
  randomSeed(analogRead(0));

  // Inicializa o Kernel
  OS::init();

  // Cria a thread passando a função e o buffer da pilha
  OS::newThread(threadRandomBlink, stackThread1);
}

void loop() {
  // O Loop principal vai monitorar o que a thread está decidindo
  Serial.print("Tempo atual de espera: ");
  
  // --- INÍCIO DA SEÇÃO CRÍTICA ---
  // Precisamos parar o escalonador momentaneamente para ler a variável 'int' (2 bytes).
  // Sem isso, a thread poderia alterar o valor no meio da leitura do byte alto/baixo.
  OS::enterCritical();
  int t = tempoSorteado;
  OS::exitCritical();
  // --- FIM DA SEÇÃO CRÍTICA ---
  
  Serial.print(t);
  Serial.println(" ms");
}

void threadRandomBlink(){
  // Loop enquanto a stack estiver saudável (não corrompida/Stack overflow)
  while(!Thread::isCorrupted()){

    // Inverte o estado do LED
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    
    // Sorteia um novo tempo entre 50ms e 3000ms
    tempoSorteado = random(50, 3001);

    // Entrega o processador para outras tarefas pelo tempo sorteado
    Thread::sleep(tempoSorteado);
  }
}
