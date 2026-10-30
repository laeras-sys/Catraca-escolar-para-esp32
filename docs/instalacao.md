# Instalação e configuração

## 1. Abrir o programa

Baixe o repositório pelo botão **Code > Download ZIP** e extraia os arquivos, ou faça um clone. Abra `catraca_sd_hspi/catraca_sd_hspi.ino` na IDE Arduino. O nome da pasta deve continuar igual ao nome do sketch.

Use uma placa ESP32 clássica, conforme o [mapa de conexões](conexoes.md). Instale o pacote de placas ESP32 da Espressif e estas bibliotecas:

- MFRC522
- ESP32Servo
- LiquidCrystal I2C com suporte a `lcd.init()`

WiFi, SPI, Wire, FS, SD e as funções de horário fazem parte do ambiente Arduino-ESP32.

## 2. Configurar a rede

Na pasta `catraca_sd_hspi`, copie `rede_exemplo.h` para `rede_local.h`. Preencha o SSID e a senha na cópia local. Utilize uma rede Wi-Fi de 2,4 GHz.

O sketch procura primeiro `rede_local.h`. Se o arquivo não existir, utiliza o exemplo, cujo SSID vazio desativa as tentativas de conexão. A catraca pode funcionar offline.

Não preencha suas credenciais no arquivo `rede_exemplo.h` que será publicado.

## 3. Cadastrar cartões

Copie `alunos_exemplo.h` para `alunos_local.h`, na mesma pasta do sketch. Cada aluno possui:

```cpp
{"00000001", "Aluno Exemplo 1", "000001", TURMA_PADRAO, ANO_LETIVO_PADRAO}
```

Os valores acima são fictícios. Use o UID completo do seu cartão, em letras maiúsculas, sem espaços e mantendo os zeros iniciais. O RM também é texto. Não repita o mesmo UID em dois cadastros.

Para descobrir um UID:

1. Altere `MODO_LER_UIDS` para `true` no sketch.
2. Envie o programa e abra o Monitor Serial em 115200 baud.
3. Aproxime um cartão por vez e preencha sua cópia `alunos_local.h`.
4. Volte `MODO_LER_UIDS` para `false` e envie novamente.

No modo de leitura de UID, o programa não inicializa o SD nem comanda o servo.

Você pode trocar as referências `TURMA_PADRAO` e `ANO_LETIVO_PADRAO` por textos específicos em cada linha de `alunos_local.h`. Assim os dados reais ficam somente no arquivo privado.

## 4. Enviar e testar

Selecione a placa e a porta corretas na IDE. Envie o sketch e confira as mensagens no Monitor Serial.

Teste uma entrada completa e uma saída completa. Depois aproxime o cartão sem concluir a passagem e aguarde o prazo de 10 segundos. A tentativa incompleta não deve criar uma linha de acesso no SD.

Confira também o funcionamento sem Wi-Fi e o arquivo `acessos_escolares.csv`, conforme o [exemplo de registro](../examples/registro_ficticio.md).

Desligue a placa antes de retirar o SD. Os testes automatizados no computador estão descritos em [tests/README.md](../tests/README.md).

[Voltar ao README](../README.md)
