# Catraca eletrônica escolar com ESP32

Protótipo de TCC para identificar alunos por RFID, controlar a entrada e a saída e registrar passagens confirmadas em um cartão microSD.

Este repositório reúne o código de um protótipo acadêmico de controle de acesso. Os cadastros publicados são fictícios e os arquivos de rede não contêm credenciais.

## Comece por aqui

- [Montagem e conexões](docs/conexoes.md)
- [Instalação e configuração](docs/instalacao.md)
- [Explicação da programação](docs/guia_tecnico.md)
- [Exemplo de registro no SD](examples/registro_ficticio.md)
- [Cuidados com dados privados](docs/privacidade.md)

## Funcionamento

O aluno aproxima um cartão cadastrado. O sistema identifica se ele está entrando ou saindo, abre a passagem e acompanha dois sensores infravermelhos.

- **Entrada:** o sensor externo detecta primeiro e o interno confirma.
- **Saída:** a sequência dos sensores se inverte.
- **Confirmação:** os dois sensores devem ficar livres antes de terminar o prazo total de 10 segundos, contado desde a autorização do cartão.
- **Cancelamento:** uma tentativa incompleta, uma ordem inválida ou um recuo detectado não gera registro de acesso nem altera a presença.

Se uma tentativa expirar com algum sensor ocupado, o programa aguarda os sensores liberarem para fechar a passagem. A tentativa continua cancelada durante essa espera.

O Wi-Fi sincroniza a data e a hora por NTP, no fuso de Brasília. Sem horário disponível, o sistema continua funcionando e registra `SEM_HORARIO`.

## Componentes e conexões

| Componente | Conexões ao ESP32 |
| --- | --- |
| RFID RC522 | SS/CS: 5, RST: 27, SCK: 18, MISO: 19, MOSI: 23 |
| LCD I2C 16 × 2 | SDA: 21, SCL: 22, endereço: `0x27` |
| Servomotor SG90 | Sinal: 25 |
| Sensor infravermelho externo | Saída digital: 32 |
| Sensor infravermelho interno | Saída digital: 33 |
| Módulo adaptador microSD | CS: 13, SCK: 14, MOSI: 26, MISO: 34 |

O RC522 utiliza alimentação de **3,3 V**. O adaptador microSD com regulador AMS1117-3.3 usado na montagem recebe **5 V no VCC**. Os sinais do ESP32 trabalham em 3,3 V, e os componentes devem compartilhar o GND.

O SD usa um barramento HSPI separado, a 400 kHz. As conexões acima correspondem à montagem do projeto.

## Como usar

1. Baixe o repositório em **Code > Download ZIP** ou clone-o.
2. Abra `catraca_sd_hspi/catraca_sd_hspi.ino` na IDE Arduino, mantendo os arquivos `.h` nessa mesma pasta.
3. Instale o pacote de placas ESP32 da Espressif e as bibliotecas **MFRC522**, **ESP32Servo** e **LiquidCrystal I2C**, com suporte a `lcd.init()`.
4. Copie `rede_exemplo.h` para `rede_local.h` e preencha sua rede Wi-Fi de 2,4 GHz.
5. Copie `alunos_exemplo.h` para `alunos_local.h` e cadastre os UIDs, nomes e RMs reais.
6. Selecione a placa e a porta corretas, envie o programa e abra o Monitor Serial em **115200 baud**.

Os arquivos de exemplo permitem compilar o projeto sem dados privados. Seus alunos são fictícios e o Wi-Fi fica desativado enquanto o SSID estiver vazio.

Para descobrir os UIDs, altere `MODO_LER_UIDS` para `true`, envie o programa e aproxime os cartões. Depois de preencher o cadastro, volte a opção para `false` e envie novamente.

## Registros no cartão SD

As passagens confirmadas são acrescentadas ao arquivo `acessos_escolares.csv`, com as colunas:

```text
TIPO;RM;NOME;TURMA;ANO_LETIVO;DATA;HORA;UID;DENTRO
```

O arquivo usa UTF-8 e ponto e vírgula como separador. É possível consultá-lo no Excel ou no LibreOffice Calc. Importe UID e RM como texto para preservar zeros iniciais.

## Organização

- [`catraca_sd_hspi/`](catraca_sd_hspi/): programa Arduino e arquivos de exemplo.
- [`docs/guia_tecnico.md`](docs/guia_tecnico.md): explicação completa da programação, estados e limitações.
- [`tests/`](tests/): testes de lógica, CSV e horário executados no computador.
- [`examples/`](examples/): dados fictícios para entender os registros.
- [`tools/`](tools/): verificação local antes de publicar arquivos.
- `.gitignore`: exclusão dos arquivos privados e dos registros de alunos.

## Compatibilidade e limites

O projeto foi desenvolvido para o **ESP32 clássico**. Não foi validado em Arduino Uno, Mega ou outras famílias de placas.

Dois sensores permitem verificar uma sequência de passagem, mas não garantem que apenas uma pessoa atravessou ou que o portador é o dono do cartão. A montagem é um protótipo educacional.

Se o SD falhar, a presença muda na memória RAM após a passagem física e o LCD informa o erro. Não há fila para gravar o evento posteriormente. Consulte o guia técnico antes de alterar a recuperação da presença.

## Testes

Os testes no computador verificam a lógica com periféricos simulados. Eles não substituem a compilação na IDE Arduino e os testes da montagem física.

Veja os comandos e os requisitos em [tests/README.md](tests/README.md).

## Dados privados

Mantenha `rede_local.h`, `alunos_local.h` e os registros CSV fora do repositório público. O `.gitignore` já inclui esses arquivos. Ao enviar arquivos manualmente pelo site, confira a seleção antes de publicar.

Antes de publicar, execute na raiz do projeto. Em um clone Git, selecione primeiro os arquivos do commit com `git add`:

```bash
python3 tools/verificar_publicacao.py
```

O verificador procura alguns padrões de credenciais e arquivos privados. Revise também o conteúdo, pois essa verificação não identifica todos os tipos de dados pessoais.

Para propor uma melhoria, consulte [CONTRIBUTING.md](CONTRIBUTING.md).
