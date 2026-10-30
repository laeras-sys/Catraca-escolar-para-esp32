# Conexões da montagem

O programa usa o ESP32 clássico e mantém dois barramentos SPI separados. A ordem dos argumentos em `SPI.begin()` é SCK, MISO, MOSI, CS.

| Componente | Sinal | GPIO |
| --- | --- | ---: |
| RC522 | SS/CS, também marcado SDA no módulo | 5 |
| RC522 | RST | 27 |
| RC522 | SCK | 18 |
| RC522 | MISO | 19 |
| RC522 | MOSI | 23 |
| LCD I2C | SDA | 21 |
| LCD I2C | SCL | 22 |
| SG90 | Sinal de controle | 25 |
| Sensor externo S1 | OUT/DO | 32 |
| Sensor interno S2 | OUT/DO | 33 |
| Adaptador microSD | CS | 13 |
| Adaptador microSD | SCK | 14 |
| Adaptador microSD | MOSI/DI | 26 |
| Adaptador microSD | MISO/DO | 34 |

## Alimentação

- **RC522:** alimentação em 3,3 V.
- **Adaptador microSD usado no projeto:** VCC em 5 V, pois a placa possui regulador AMS1117-3.3. Essa indicação não se aplica a um cartão sem adaptador nem a qualquer módulo de outro modelo.
- **Sensores:** suas saídas precisam fornecer níveis compatíveis com 3,3 V.
- **Servo:** GPIO 25 transmite o comando. A alimentação do motor precisa de uma fonte adequada à sua corrente.
- **Terra:** mantenha o GND comum entre os circuitos.

Os GPIOs do ESP32 não devem receber sinais de 5 V. Não conecte a alimentação de 5 V do adaptador SD à alimentação de 3,3 V do RC522.

## Sensores e direção

S1 fica do lado externo e S2 do lado interno. O programa considera detecção em nível `LOW` e exige 40 ms de estabilidade.

Para entrar, a sequência é S1 e depois S2. Para sair, S2 e depois S1. Os dois precisam ficar livres para concluir. A lógica admite sensores separados por um espaço ou com áreas de detecção sobrepostas.

## Configurações usadas

| Item | Configuração |
| --- | --- |
| LCD | 16 × 2, endereço I2C `0x27` |
| Servo | fechado em 0°, aberto em 90° |
| SD | HSPI, 400 kHz |
| Cartão de referência da montagem | microSD de 8 GB, FAT32 |
| Monitor Serial | 115200 baud |

Não é necessário mudar os pinos para usar esta organização dos arquivos. Confira os ângulos do servo na sua montagem mecânica.

[Voltar ao README](../README.md)
