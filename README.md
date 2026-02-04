# KernelSchedule RTOS

O KernelSchedule RTOS é um sistema operacional de tempo real preemptivo desenvolvido para microcontroladores AVR, com foco em baixo consumo de recursos e execução determinística. Ele implementa multitarefa baseada em fatias de tempo utilizando o Timer1 como fonte de interrupção periódica.

## Arquitetura Geral

O núcleo do sistema é composto por três estruturas principais:

- OS: responsável pelo gerenciamento global, escalonamento e controle do tempo.
- Thread: representa uma tarefa executável com sua própria pilha e estado.
- Mutex: fornece mecanismos de sincronização para acesso seguro a recursos compartilhados.

O sistema utiliza um array fixo de threads, permitindo controle direto da memória e previsibilidade no uso de recursos.

## Modelo de Threads

Cada thread possui:

- Área de memória dedicada para pilha.
- Ponteiro atual da pilha.
- Estado de execução.
- Tempo de despertar para operações de espera.
- Mecanismo de verificação de integridade da stack através de um valor sentinela.

Estados possíveis:

- UNUSED: posição livre.
- READY: pronta para execução.
- RUNNING: atualmente em execução.
- BLOCKED: aguardando liberação de recurso.
- SLEEP: suspensa até determinado tempo.

## Inicialização da Thread

Durante a criação, a pilha é preparada manualmente com o endereço da função inicial e registradores zerados, permitindo que o contexto seja restaurado diretamente pelo mecanismo de troca de contexto.

Um byte canário é inserido no início da pilha para detectar corrupção ou overflow.

## Escalonador

O escalonador é baseado em round-robin com preempção por tempo.

Funcionamento:

1. O contador global de ticks é incrementado a cada interrupção.
2. Threads em estado de espera são verificadas e podem voltar ao estado READY.
3. Caso não exista bloqueio atômico ativo, a próxima thread pronta é selecionada.
4. A thread atual retorna para READY e a nova passa para RUNNING.

Esse processo garante alternância contínua entre tarefas disponíveis.

## Controle de Tempo

O Timer1 é configurado em modo de comparação para gerar interrupções periódicas. Cada interrupção representa um time slice do sistema, utilizado para:

- Atualizar o contador global de ticks.
- Despertar threads em espera.
- Executar o escalonador.

## Troca de Contexto

O kernel salva e restaura o ponteiro de pilha da thread ativa durante a troca de contexto. Essa operação permite alternar entre múltiplas execuções sem perda de estado.

## Sleep e Yield

Uma thread pode suspender sua execução por um período definido, alterando seu estado para SLEEP e definindo o tempo de despertar.

O método yield força a execução do escalonador sinalizando uma interrupção pendente.

## Mutex

O sistema possui um mecanismo simples de exclusão mútua:

- Quando livre, o mutex é adquirido pela thread atual.
- Caso esteja ocupado, a thread é bloqueada e marcada como aguardando.
- Ao liberar, a primeira thread em espera retorna ao estado READY.

Esse modelo evita acesso concorrente a recursos críticos.

## Seções Críticas

# Deadlocks e Race Condition com Serial (UART)

O uso da Serial (UART) exige atenção especial dentro do KernelSchedule, pois a comunicação serial depende de interrupções do hardware e pode afetar diretamente o escalonamento das threads.

Evite utilizar chamadas de Serial dentro de regiões protegidas por AtomicGuard ou seções críticas longas. Como AtomicGuard impede a troca de contexto entre threads, qualquer operação mais demorada dentro dessa região fará com que as demais threads fiquem impedidas de executar, causando travamentos aparentes e perda de responsividade do sistema.

Boas práticas recomendadas:

- Utilize AtomicGuard apenas para alterações rápidas de estado ou variáveis compartilhadas.
- Não execute Serial.print ou qualquer IO dentro de regiões atômicas.
- Mantenha seções críticas extremamente curtas.
- Caso múltiplas threads utilizem Serial, proteja apenas o acesso imediato com Mutex e libere rapidamente.
- Prefira arquiteturas com escritor único e múltiplos leitores para reduzir necessidade de sincronização.


O uso incorreto pode gerar starvation das demais tarefas, já que a thread atual continuará executando enquanto o escalonador estiver bloqueado.

Esse modelo mantém o sistema simples, previsível e eficiente dentro das limitações de microcontroladores AVR.


## Verificação de Stack

Cada thread possui verificação de integridade baseada em um valor sentinela inserido no início da pilha. Caso o valor seja alterado, indica possível overflow ou corrupção de memória.

## Inicialização do Sistema

Na inicialização:

- O contador de ticks é zerado.
- A thread principal é marcada como RUNNING.
- As demais posições são definidas como UNUSED.
- O Timer1 é configurado e as interrupções são habilitadas.

## Objetivos de Design

- Determinismo em tempo real.
- Baixo uso de memória RAM.
- Troca de contexto eficiente.
- Integração direta com Arduino AVR.
- Simplicidade estrutural sem dependências externas.

