# Pacman-Emu

Emulador educacional do hardware arcade original de Pac-Man, escrito em C17 e usando SDL2 para vídeo e entrada. Esta versão implementa CPU, memória, barramento, vídeo, sprites, paleta, temporização, interrupções e controles. **Áudio ainda não está implementado.**

## Estado do projeto

### Implementado

- núcleo Z80 usado pelo jogo;
- ROM de programa de 16 KiB;
- RAM de vídeo, RAM de cores e RAM de trabalho;
- interrupções em modo IM 2;
- tilemap do Pac-Man;
- sprites, transparência, prioridade e recorte;
- paleta e tabela de cores por PROM;
- tela vertical de 224 × 288 pixels;
- sincronização por quadro;
- entradas ativas em nível baixo;
- moeda, início de partida e movimento;
- apresentação por SDL2.

### Não implementado

- áudio Namco WSG;
- gravação de estado;
- interface de depuração gráfica;
- configuração externa de DIP switches;
- suporte garantido a outros jogos ou ROM sets.

## Requisitos

Em distribuições baseadas em Debian ou Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev
```

O projeto usa:

- compilador compatível com C17;
- GNU Make;
- SDL2;
- ROMs obtidas legalmente pelo usuário.

## ROMs necessárias

Crie a pasta `roms/` na raiz do projeto e coloque nela os arquivos esperados pela implementação.

### Programa

```text
pacman.6e
pacman.6f
pacman.6h
pacman.6j
```

### Gráficos e cores

A versão atual pode usar os arquivos gráficos configurados em `video.c`, além das PROMs:

```text
82s123.7f
82s126.4a
```

Confira os nomes realmente usados em `video_load_roms()` antes de executar. As ROMs comerciais não devem ser incluídas no repositório.

## Compilação

```bash
make clean
make
```

O executável gerado é:

```text
pacman-emu
```

## Execução

Na raiz do projeto:

```bash
./pacman-emu
```

## Controles

```text
Setas ou W/A/S/D  Movimento
5                 Inserir moeda 1
6                 Inserir moeda 2
1                 Iniciar 1 jogador
2                 Iniciar 2 jogadores
C                 Crédito de serviço, se habilitado
Esc               Encerrar
```

As entradas do arcade são ativas em nível baixo: um bit em `1` representa botão solto e um bit em `0` representa botão pressionado.

## Estrutura do projeto

```text
src/
├── Main.c
├── bus/
│   ├── bus.c
│   └── bus.h
├── core/
│   └── types.h
├── cpu/z80/
│   ├── z80.c
│   └── z80.h
├── emulator/
│   ├── emulator.c
│   └── emulator.h
├── memory/
│   ├── memory.c
│   └── memory.h
├── plataform/
│   ├── plataform.h
│   └── sdl_video.c
├── rom/
│   ├── rom_db.c
│   ├── rom_db.h
│   ├── rom_loader.c
│   └── rom_loader.h
└── video/
    ├── palette.c
    ├── palette.h
    ├── sprites.c
    ├── sprites.h
    ├── tilemap.c
    ├── tilemap.h
    ├── video.c
    └── video.h
```

## Resumo técnico

O Z80 é executado em blocos equivalentes a um quadro. A implementação usa aproximadamente 50.688 ciclos por quadro, atende a interrupção de vídeo, renderiza o tilemap e os sprites e apresenta o framebuffer pelo SDL2.

O mapa principal inclui:

```text
0x0000–0x3FFF  ROM de programa
0x4000–0x43FF  Video RAM
0x4400–0x47FF  Color RAM
0x4800–0x4FFF  RAM e atributos de sprite
0x5000–0x50FF  Entradas e registradores de hardware
```

Para detalhes de arquitetura e manutenção, consulte `DOCUMENTACAO.md`.

## Diagnóstico de CPU

Quando uma instrução ainda não está implementada, o programa pode imprimir:

```text
Opcode não implementado
Endereço da instrução
Bytes em memória
Registradores da CPU
```

Essas informações permitem identificar a sequência completa e implementar a instrução no espaço correto: base, CB, ED, DD, FD, DDCB ou FDCB.

## Limitações e responsabilidade

Este projeto é educacional. Pac-Man e seus recursos pertencem aos respectivos titulares. O repositório não deve distribuir ROMs comerciais.

## Próxima etapa

A próxima etapa prevista é implementar o áudio de três vozes do Namco WSG e a saída SDL em arquivos separados, sem misturar síntese de som com o núcleo da CPU.
