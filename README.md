# KernelSchedule

KernelSchedule é uma biblioteca simples de escalonamento cooperativo
para microcontroladores AVR (ex: Arduino UNO), permitindo criação de
múltiplas threads leves com controle manual de stack e sincronização
crítica.

## Recursos

-   Threads preemptivas
-   Sleep não bloqueante
-   Controle de região crítica
-   Atomic Guard
-   Mutex
-   Contagem de threads ativas
-   Baixo consumo de memória
-   Foco em sistemas embarcados

------------------------------------------------------------------------

## Instalação

1.  Copie os arquivos da biblioteca para a pasta `libraries` do Arduino.
2.  Inclua no projeto:

``` cpp
#include <KernelSchedule.h>
```

------------------------------------------------------------------------

## Exemplo Básico

``` cpp
#include <KernelSchedule.h>

uint8_t stackThread1[STACK_SIZE_SMALL];

volatile int tempoSorteado = 0;

void threadLimitedBlink();

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(analogRead(0));

  OS::init();
  OS::newThread(threadLimitedBlink, stackThread1);
}

void loop() {
  Serial.print("Threads Ativas: ");
  Serial.print(OS::getActiveThreads());

  Serial.print(" | Ultimo Tempo Sorteado: ");

  OS::enterCritical();
  int t = tempoSorteado;
  OS::exitCritical();

  Serial.print(t);
  Serial.println(" ms");
}

void threadLimitedBlink(){
  for(int i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    tempoSorteado = random(150, 3001);
    Thread::sleep(tempoSorteado);
  }
}
```

------------------------------------------------------------------------

## Criando Threads

``` cpp
uint8_t stack[STACK_SIZE_SMALL];
OS::newThread(minhaFuncao, stack);
```

Cada thread precisa de um buffer de stack próprio.

------------------------------------------------------------------------

## Sleep da Thread

``` cpp
Thread::sleep(1000);
```

Suspende apenas a thread atual.

------------------------------------------------------------------------

## Região Crítica

``` cpp
OS::enterCritical();
// acesso protegido
OS::exitCritical();
```

Bloqueia interrupções ou troca de contexto durante uma seção crítica.
Use apenas para trechos curtos.

------------------------------------------------------------------------

## Atomic Guard

O Atomic Guard é uma forma segura e automática de proteger uma região
crítica usando escopo.

``` cpp
{
  AtomicGuard guard;
  tempoSorteado++;
}
```

Ao sair do escopo, a proteção é liberada automaticamente, evitando erros
por esquecimento de `exitCritical`.

------------------------------------------------------------------------

## Mutex

Mutex permite sincronização entre threads sem bloquear todo o sistema.

``` cpp
Mutex mutex;

void thread1() {
  mutex.lock();
  // recurso compartilhado
  mutex.unlock();
}

void thread2() {
  mutex.lock();
  // recurso compartilhado
  mutex.unlock();
}
```

Use mutex quando múltiplas threads acessam o mesmo recurso por mais
tempo.

------------------------------------------------------------------------

## Threads Ativas

``` cpp
int total = OS::getActiveThreads();
```

------------------------------------------------------------------------

## Observações

-   O sistema é preemptivo, porém evite loops infinitos sem `sleep` para
    facilitar a troca de contexto.
-   Atomic Guard é indicado para trechos muito curtos.
-   Mutex é indicado para recursos compartilhados mais complexos.
-   Sempre forneça stacks suficientes para cada thread.
-   Ideal para sistemas com pouca memória.

------------------------------------------------------------------------

## Licença

Uso livre para estudos e projetos embarcados.
