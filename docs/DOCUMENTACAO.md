# Documentação técnica do Pacman-Emu

## 1. Objetivo

O Pacman-Emu reproduz, em C, os componentes necessários para executar o programa do arcade Pac-Man em um computador moderno. O projeto mantém os subsistemas separados para facilitar teste, depuração e evolução.

Esta documentação descreve a versão **sem áudio**.

## 2. Arquitetura geral

O fluxo principal é:

```text
Main.c
  ├─ inicializa Emulator e SDL
  ├─ carrega ROMs
  ├─ coleta entradas
  ├─ executa ciclos do Z80
  ├─ solicita interrupção por quadro
  ├─ renderiza vídeo
  └─ apresenta framebuffer
```

A estrutura `Emulator` reúne:

```text
Z80     estado da CPU
Memory  ROM e RAM
Bus     mapa de memória, entradas e registradores
Video   ROMs gráficas, paleta e framebuffer
```

## 3. Main.c

Responsabilidades:

- criar e inicializar o emulador;
- carregar as quatro ROMs de programa;
- carregar ROMs de vídeo e PROMs de cor;
- inicializar a plataforma SDL;
- processar eventos e teclado;
- controlar ciclos por quadro;
- disparar interrupções;
- renderizar e apresentar a imagem;
- imprimir diagnóstico em caso de opcode ausente.

A temporização deve evitar vincular a velocidade emulada à velocidade do computador hospedeiro. O quadro lógico usa aproximadamente 50.688 ciclos da CPU.

O diagnóstico de opcode registra o endereço inicial antes da busca da instrução. Isso é importante porque prefixos e operandos avançam o PC durante a decodificação.

## 4. emulator/emulator.c e emulator.h

A camada de emulação conecta os subsistemas sem reproduzir suas regras internas.

Funções esperadas:

```c
void emulator_init(Emulator* emulator);
void emulator_reset(Emulator* emulator);
bool emulator_load_rom_chip(Emulator* emulator,
                            const char* path,
                            u32 destination);
int emulator_step(Emulator* emulator);
int emulator_interrupt(Emulator* emulator);
```

`emulator_step()` encaminha a execução para `z80_step()`. `emulator_interrupt()` usa o vetor armazenado no barramento.

## 5. memory/memory.c e memory.h

A memória mantém regiões separadas:

```c
u8 rom[0x4000];
u8 video_ram[0x0400];
u8 color_ram[0x0400];
u8 ram[0x0800];
```

Separar as regiões ajuda a impedir escrita acidental na ROM e torna o mapa explícito.

`memory_reset()` limpa o estado volátil. `memory_load_rom_chip()` copia uma ROM somente quando destino e tamanho cabem na região reservada.

## 6. bus/bus.c e bus.h

O barramento converte endereços do Z80 em acessos aos dispositivos.

### 6.1 Mapa principal

```text
0x0000–0x3FFF  ROM
0x4000–0x43FF  Video RAM
0x4400–0x47FF  Color RAM
0x4800–0x4FFF  RAM de trabalho e sprite RAM
0x5000–0x503F  IN0
0x5040–0x507F  IN1 em leitura
0x5080–0x50BF  DIP switches em leitura
```

### 6.2 Registradores de escrita

A implementação utiliza, entre outros:

```text
0x5000  habilitação de interrupção
0x5001  habilitação de som, reservada para a etapa de áudio
0x5003  inversão da tela
0x5060–0x506F  coordenadas dos sprites
```

Leitura e escrita no mesmo endereço podem representar dispositivos diferentes.

### 6.3 Entradas

As entradas são ativas em nível baixo:

```text
1 = solto
0 = pressionado
```

IN0 contém direções e moedas. IN1 contém Start 1, Start 2 e entradas associadas ao segundo jogador ou gabinete cocktail.

## 7. cpu/z80/z80.h

A estrutura `Z80` armazena:

- registradores principais e alternativos;
- `IX`, `IY`, `SP` e `PC`;
- registradores `I` e `R`;
- flags de interrupção `IFF1` e `IFF2`;
- modo de interrupção;
- estado de HALT;
- atraso de EI;
- último opcode;
- endereço inicial da última instrução;
- total de ciclos.

As flags do registrador F são:

```text
S   0x80  sinal
Z   0x40  zero
Y   0x20  bit não documentado 5
H   0x10  half carry
X   0x08  bit não documentado 3
P/V 0x04  paridade ou overflow
N   0x02  subtração
C   0x01  carry
```

## 8. cpu/z80/z80.c

### 8.1 Busca e decodificação

`z80_step()`:

1. valida CPU e barramento;
2. trata HALT;
3. salva o endereço inicial da instrução;
4. busca o opcode;
5. executa o caso correspondente;
6. soma os ciclos;
7. atualiza o atraso de EI.

### 8.2 Espaços de opcode

A implementação separa:

```text
base      opcodes sem prefixo
CB        rotações, shifts, BIT, RES e SET
ED        operações especiais e de bloco
DD        operações com IX
FD        operações com IY
DD CB     operações de bit em IX + deslocamento
FD CB     operações de bit em IY + deslocamento
```

Prefixos devem ser tratados pelo decodificador correspondente. Um caso `DD CB d op` não deve ser colocado no decodificador ED.

### 8.3 Instruções relevantes implementadas durante a validação

Além das famílias comuns, a execução do jogo exigiu tratamento correto de:

- `DAA` (`0x27`);
- `CCF` (`0x3F`);
- `NEG` (`ED 44`);
- `CPI` (`ED A1`);
- `CPIR` (`ED B1`);
- prefixos DD e FD consecutivos;
- operações DDCB e FDCB;
- `BIT`, `RES`, `SET`, rotações e shifts indexados.

### 8.4 Interrupções

O jogo utiliza interrupção mascarável. O fluxo deve respeitar:

- `IFF1` habilitado;
- atraso de `EI` concluído;
- vetor armazenado pelo barramento;
- saída do estado HALT;
- empilhamento do PC;
- busca do endereço de destino conforme o modo de interrupção usado.

## 9. rom/rom_loader.c e rom_loader.h

O carregador:

- abre o arquivo binário;
- determina o tamanho;
- aloca memória;
- lê o conteúdo;
- devolve dados e tamanho;
- libera a alocação quando não for mais necessária.

Erros de arquivo, tamanho ou leitura devem resultar em falha explícita, sem continuar com dados parciais.

## 10. rom/rom_db.c e rom_db.h

A base de ROMs descreve nome, tamanho e destino esperado para cada chip de programa. Ela pode ser ampliada para incluir CRC32 ou SHA-1 e impedir o uso silencioso de ROMs incompatíveis.

## 11. video/video.h

A estrutura de vídeo contém:

- framebuffer final de 224 × 288;
- ROM de tiles;
- ROM de sprites;
- PROM de paleta;
- PROM da tabela de cores;
- estrutura de paleta decodificada;
- indicador de ROMs carregadas.

Dependendo da versão atual do renderer, pode existir uma superfície intermediária nativa de 288 × 224 ou a rotação pode ser aplicada durante o desenho.

## 12. video/video.c

Responsabilidades:

- carregar ROMs gráficas e PROMs;
- validar tamanhos;
- decodificar a paleta;
- limpar ou preparar o quadro;
- renderizar tilemap;
- renderizar sprites por cima;
- devolver falha quando algum subsistema não estiver pronto.

A transformação de orientação deve ocorrer uma única vez. Duplicar rotações em tilemap, sprites e apresentação causa espelhamento ou transposição.

## 13. video/palette.c e palette.h

A PROM de paleta é convertida para cores ARGB de 32 bits. A PROM de consulta transforma o par:

```text
código de cor + valor de pixel
```

em um índice da paleta final.

A transparência de sprites deve ser determinada pelo resultado da tabela de cores, não apenas pela hipótese de que o valor gráfico zero é sempre transparente.

## 14. video/tilemap.c e tilemap.h

O tilemap usa caracteres de 8 × 8 pixels. A área lógica corresponde a 36 × 28 tiles antes da orientação final.

O mapeamento de VRAM possui linhas e colunas especiais para placar, créditos e informações inferiores. A função de offset deve reproduzir o layout físico da placa. Alterações globais de posição no framebuffer não substituem o mapeamento correto.

O decodificador gráfico lê dois planos de bits e combina ambos para produzir valores de pixel entre 0 e 3.

## 15. video/sprites.c e sprites.h

O hardware oferece oito slots de sprite com imagens de 16 × 16 pixels.

O renderer:

- lê código, cor e bits de inversão;
- obtém coordenadas dos registradores do barramento;
- desenha em ordem de prioridade;
- aplica transparência;
- limita sprites à área visível apropriada;
- trata a cópia de contorno do túnel quando necessária;
- aplica o ajuste específico dos primeiros slots.

A cópia de contorno não deve produzir uma segunda imagem permanente na região do placar.

## 16. plataform/plataform.h e sdl_video.c

A plataforma isola detalhes do SDL da lógica do arcade.

Funções típicas:

```c
bool platform_video_init(Platform* platform,
                         const char* title,
                         int scale);
bool platform_process_events(Platform* platform);
bool platform_video_present(Platform* platform,
                            const Video* video);
void platform_video_shutdown(Platform* platform);
```

A textura deve usar o mesmo formato de pixels do framebuffer. A apresentação deve manter proporção inteira ou lógica para não deformar o quadro.

## 17. Temporização

O loop não deve executar “o mais rápido possível”. A máquina emulada é dividida em quadros, com orçamento de ciclos para cada um.

Uma implementação estável deve:

- usar relógio de alta resolução;
- acumular tempo decorrido;
- executar quadros vencidos;
- limitar recuperação excessiva após pausa;
- preservar excesso de ciclos de uma instrução;
- apresentar no máximo uma imagem por atualização do hospedeiro quando estiver recuperando atraso.

## 18. Controles

Mapeamento recomendado:

```text
Setas/WASD  direção
5           moeda 1
6           moeda 2
1           Start 1
2           Start 2
C           crédito de serviço
Esc         sair
```

O estado do teclado é convertido nos bits de IN0 e IN1. A CPU deve continuar lendo entradas pelo barramento, sem conhecer SDL.

## 19. Compilação

Exemplo de compilação direta:

```bash
gcc -Wall -Wextra -std=c17 -g \
    -I/usr/include/SDL2 -D_REENTRANT \
    src/Main.c \
    src/emulator/emulator.c \
    src/memory/memory.c \
    src/video/video.c \
    src/video/palette.c \
    src/video/tilemap.c \
    src/video/sprites.c \
    src/plataform/sdl_video.c \
    src/bus/bus.c \
    src/cpu/z80/z80.c \
    src/rom/rom_loader.c \
    src/rom/rom_db.c \
    -o pacman-emu -lSDL2
```

Preferencialmente use:

```bash
make clean
make
```

## 20. Testes recomendados

### Inicialização

- ROMs válidas carregam sem erro;
- ROM ausente encerra com mensagem clara;
- a janela abre com 224 × 288 em escala inteira.

### CPU

- o jogo alcança a tela de apresentação;
- HALT é retomado pela interrupção;
- não ocorre opcode desconhecido durante uma partida normal;
- créditos e placar usam BCD corretamente.

### Vídeo

- textos são legíveis;
- labirinto e cores correspondem à referência;
- sprites não ficam duplicados;
- direções de animação são distintas;
- títulos e pontuações ficam alinhados.

### Entrada

- moeda é reconhecida apenas enquanto o bit está ativo;
- Start 1 inicia a partida;
- as quatro direções respondem;
- teclas soltas restauram o bit para 1.

### Temporização

- animação não acelera com CPU hospedeira rápida;
- arrastar a janela não causa recuperação indefinida;
- o ritmo permanece estável durante a demonstração e a partida.

## 21. Limitações atuais

- áudio não implementado;
- sem savestate;
- sem depurador interativo;
- suporte focado no conjunto configurado de Pac-Man;
- a cobertura completa de todos os opcodes Z80 deve ser validada por testes unitários, mesmo que o jogo execute normalmente.

## 22. Extensão futura de áudio

A implementação de áudio deve ser adicionada em etapa separada, usando:

```text
namco_wsg.c
namco_wsg.h
sdl_audio.c
sdl_audio.h
```

O barramento entregará as escritas de registradores ao WSG; o WSG produzirá amostras mono; e a camada SDL consumirá essas amostras. Essa etapa não faz parte da versão documentada aqui.

## 23. Boas práticas de manutenção

- faça commit antes de alterar um subsistema estável;
- não corrija erro de CPU tratando dados como NOP;
- imprima bytes completos de instruções prefixadas;
- valide ROMs por tamanho e checksum;
- mantenha CPU, barramento, vídeo, entrada e plataforma desacoplados;
- compile sempre com `-Wall -Wextra`;
- adicione testes para cada opcode corrigido.
