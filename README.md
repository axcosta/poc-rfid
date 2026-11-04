# poc-rfid

Firmware de teste para validar um leitor UHF RFID (vendido como Fonkan FM-504) ligado a um ESP32-S3 por UART TTL. Serve para sondar o protocolo do leitor, ler tags e medir a taxa de leitura em função de distância, potência e região. Feito com PlatformIO e o framework Arduino.

Este repositório é uma prova de conceito: o código é voltado a experimentar e medir, não a ser uma biblioteca reutilizável.

## Hardware testado

| Item | Detalhe |
|---|---|
| Microcontrolador | ESP32-S3-N16R8 (16 MB de flash, 8 MB de PSRAM octal), placa de desenvolvimento com duas portas USB-C (`COM` e `USB`) |
| Leitor | Placa "RFID_READER" com módulo UHF, vendida como Fonkan FM-504, com conector de 5 pinos (GND, EN, RX, TX, VCC) e antena conectada |
| Tag | Tag UHF EPC Gen2 comum |

Tags de 13,56 MHz (NFC, Mifare) não funcionam com este leitor.

## Ligação

| Pino do módulo | ESP32-S3 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| EN | 3V3 |
| TX | GPIO16 (RX do ESP) |
| RX | GPIO17 (TX do ESP) |

- TX e RX são cruzados.
- O módulo funcionou alimentado em 3,3 V. Alimentação em 5 V não foi testada, e a tensão aceita pode variar conforme o lote: confira a etiqueta ou o manual do seu módulo.
- Os pinos de dados são TTL 3,3 V, então não é preciso conversor de nível.
- Conecte a antena antes de energizar o módulo.
- No ESP32-S3-N16R8 evite os GPIO 26 a 37 (flash e PSRAM octal), 19 e 20 (USB nativa), 43 e 44 (UART0 da porta `COM`) e os pinos de boot (0, 3, 45 e 46). Os GPIO16 e 17 estão livres.

## Como usar

Pré-requisitos: Visual Studio Code com a extensão PlatformIO IDE. A primeira compilação baixa o toolchain do ESP32 e pode demorar vários minutos.

1. Abra esta pasta como projeto no VS Code (a pasta que contém o `platformio.ini`).
2. Conecte o ESP32-S3 pela porta `USB` (nativa) e grave com o botão Upload do PlatformIO.
3. Abra o monitor serial a 115200 baud.

O `platformio.ini` já configura a placa (flash de 16 MB, PSRAM octal) e envia o `Serial` pela USB nativa (`ARDUINO_USB_CDC_ON_BOOT=1`).

Problemas comuns:

- `Failed to connect`: segure o botão BOOT, aperte e solte RST, solte o BOOT e grave de novo.
- `the port is busy`: feche qualquer monitor serial aberto antes de gravar.
- Na unidade usada nos testes, a porta `COM` não foi reconhecida pelo computador, e a gravação funcionou pela porta `USB`. Se a sua porta `COM` funcionar, ajuste ou remova os `build_flags` para o monitor sair por ela.

## Comandos do monitor serial

Digite o comando e aperte Enter.

| Comando | O que faz |
|---|---|
| `help` | Lista os comandos |
| `info` | Mostra versão, potência e região atuais do leitor |
| `power XX` | Define a potência RF, XX em hexadecimal (`00` a `FF`) |
| `region N` | Define a região, N decimal (1 = US, 5 = EU) |
| `test [N] [gapMs] [delaySec]` | Faz N leituras isoladas (padrão 50), com `gapMs` de pausa (padrão 250) e `delaySec` segundos de contagem antes de começar, e mostra a taxa de acerto |
| `scan [runs] [delaySec]` | Simula um scan com repetição: cada tentativa lê por até 3 s e para na primeira tag, repetido `runs` vezes (padrão 20) |
| `watch [N]` | Lê N vezes com janela fixa de 300 ms e mostra a resposta crua de cada leitura |
| `raw CMD` | Envia um comando qualquer ao leitor (por exemplo `raw U`) e mostra a resposta crua |

O argumento `delaySec` serve para dar tempo de posicionar a tag depois de digitar o comando.

## Protocolo do leitor

UART a 38400 baud, 8N1. Cada comando é enviado como `\n<COMANDO>\r`, e a resposta vem prefixada com a mesma letra do comando, com um `\n` na frente e terminada em `\r\n`.

Comandos usados e verificados neste repositório:

| Comando | Resposta | O que faz |
|---|---|---|
| `V` | `\nV<versão>\r\n` | Versão do firmware do leitor |
| `N0,00` | `\nN<hex>\r\n` | Lê a potência atual (0 a 255) |
| `N1,XX` | `\nN<hex>\r\n` | Define a potência (`00` a `FF`) |
| `N4,00` | `\nN<hex>\r\n` | Lê a região atual |
| `N5,XX` | `\nN<hex>\r\n` | Define a região (01 = US, 05 = EU, ...) |
| `Q` | `\nQ<PC><EPC><CRC16>\r\n` | Lê uma tag. Sem tag no campo, a resposta é só `\nQ\r\n` |

Uma resposta de tag tem o formato `[PC 4 hex][EPC N hex][CRC16 4 hex]`. Os comandos `S`, `U`, `R` e `W` (ID do leitor, inventário múltiplo, leitura e escrita de memória da tag) existem no protocolo, mas não foram verificados aqui; dá para experimentá-los com `raw`.

Detalhes que importam:

- A resposta `N<hex>` é a mesma para leitura de potência e de região, então só dá para interpretá-la sabendo qual comando foi enviado.
- Logo depois de mudar a região, o módulo precisa de um tempo (centenas de milissegundos) antes de aceitar o próximo comando.
- A potência e a região configuradas permaneceram aplicadas depois de reiniciar o ESP (o módulo seguiu energizado). Se persistem após cortar a alimentação do módulo não foi testado.

## Resultados das medições

Resultados com um único módulo, uma única tag e posicionamento manual, com 30 tentativas por ponto (margem de erro em torno de ±9 pontos percentuais). São indicativos, não uma especificação.

- Só o protocolo de texto a 38400 baud respondeu. Tentativas de protocolo binário a 115200 e 9600 baud devolveram apenas lixo.
- O alcance depende muito da orientação da antena. Com a tag ao lado da antena, no plano dela, a leitura foi confiável só até cerca de 1 cm e nula a 10 cm. Com a face da antena virada para a tag houve leitura até 30 cm, mas com taxa baixa e irregular por tentativa (da ordem de 20% a 60%, sem queda regular com a distância).
- A potência (`80`, `C0` e `FF`) não fez diferença consistente.
- A região US (1, 902 a 928 MHz) leu normalmente; a região EU (5) não leu nada com este módulo e antena.
- Repetir a leitura por alguns segundos resolve boa parte da baixa taxa por tentativa: no teste `scan 20 15` (tag a 15 cm, face da antena virada para ela), 20 de 20 scans tiveram sucesso, com média de cerca de 430 ms até a primeira leitura.

## Limitações

- Sem testes com a tag colada em objetos de vidro, metal ou líquidos, que costumam reduzir bastante o alcance de tags UHF.
- Uma única tag, uma única unidade de leitor e um único ambiente.
- Os avisos de `include` que o editor mostra (`Arduino.h` não encontrado) vêm do IntelliSense ainda sem indexar o projeto e não afetam a compilação.
