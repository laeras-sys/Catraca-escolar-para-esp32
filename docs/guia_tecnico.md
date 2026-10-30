# Guia da programação

O sketch reúne o controle da passagem, o cadastro, o LCD, o SD e o relógio. Seus blocos têm comentários numerados para facilitar a leitura. O cadastro e a rede reais ficam em arquivos locais separados dos exemplos públicos.

## Por onde começar a ler

| Parte do código | Responsabilidade |
| --- | --- |
| Definições dos pinos e constantes | Conexões, ângulos, polaridade dos sensores e prazos |
| `Pessoa` e `banco[]` | UID, nome, RM, turma e ano letivo |
| `EntradaFiltrada` | Estabilização das leituras digitais |
| `Estado` e variáveis da operação | Etapa atual, aluno pendente e direção |
| `CSVEscolar` | Montagem e leitura das linhas de registros |
| Funções de SD | Inicialização, recuperação de presença, escrita e conferência |
| Funções de RFID | Leitura do UID e encerramento da comunicação com o cartão |
| Funções de rede e horário | Wi-Fi, NTP e conversão para data e hora |
| `solicitarPassagem()` | Identificação e autorização de uma tentativa |
| `cancelarPassagem()` | Descarte da tentativa sem registrar acesso |
| `concluirPassagem()` | Atualização da presença após confirmar a passagem |
| `setup()` | Inicialização dos componentes |
| `loop()` | Acompanhamento contínuo dos sensores e dos estados |

## Identificação e direção

O RFID lê o UID completo e o compara com `banco[]`. Um UID desconhecido não abre a catraca. O programa também exige sensores livres antes de autorizar uma tentativa.

`alunoDentro[]` guarda uma posição por aluno. Quando o aluno está fora, seu cartão solicita entrada. Quando está dentro, solicita saída. `pessoasDentro` é a contagem desses estados, recalculada após cada confirmação.

Uma tentativa guarda uma única identidade em `pessoaPendente`. Outros cartões não são aceitos enquanto a tentativa ou a mensagem de resultado estiver em andamento. Afaste o cartão do leitor antes de aproximá-lo novamente.

## Sensores e confirmação

Na entrada, o primeiro sensor é o externo S1 e o segundo é o interno S2. Na saída, os papéis se invertem.

O programa aceita áreas sobrepostas, em que os dois sensores podem ficar ocupados juntos, e áreas separadas, em que existe um intervalo livre entre eles. O intervalo livre após detectar apenas o primeiro sensor não conclui a passagem.

Só ocorre confirmação depois de detectar a ordem esperada e os dois sensores ficarem livres. O programa rejeita padrões reconhecidos de recuo e ativações em ordem contrária. Se os dois forem detectados inicialmente na mesma amostragem, a ordem é ambígua e a tentativa é cancelada.

### Filtro de 40 ms

`EntradaFiltrada` acompanha a leitura bruta e o estado estável:

- `estaAtiva()` informa o nível filtrado.
- `ativou()` informa uma nova ativação naquela volta do `loop()`.
- `estaLivre()` exige estado filtrado e leitura bruta livres, com estabilidade por 40 ms.

A montagem usa sensores ativos em `LOW`. Pulsos muito curtos podem ser descartados pelo filtro ou perdidos durante uma operação bloqueante de biblioteca.

## Prazo total de 10 segundos

`inicioOperacao` recebe `millis()` quando um cartão válido é autorizado. Nenhum sensor reinicia esse prazo. A sequência e a liberação dos sensores precisam terminar antes de 10000 ms. A estabilização de 40 ms também faz parte desse tempo.

O código não usa `delay(10000)`. Cada volta do `loop()` compara o tempo decorrido. A subtração entre valores sem sinal mantém a comparação válida na volta do contador `millis()` do ESP32.

A atualização da rede vem antes da amostragem dos sensores e do relógio. A função de conclusão verifica novamente o tempo atual para impedir que um valor antigo autorize uma confirmação tardia.

Se o prazo vencer, o programa descarta a identidade e não grava uma linha de acesso. Se houver um sensor ocupado, mantém a passagem aberta até os dois ficarem livres. Uma detecção tardia não recupera a tentativa cancelada.

## Estados

| Estado | O que o programa aguarda |
| --- | --- |
| `ESPERANDO` | Novo cartão |
| `AGUARDANDO_PRIMEIRO` | Sensor do lado de origem |
| `AGUARDANDO_SEGUNDO` | Segundo sensor, sem recuo reconhecido |
| `AGUARDANDO_LIBERAR_PRIMEIRO` | Liberação do primeiro após sobreposição |
| `AGUARDANDO_LIBERAR_SEGUNDO` | Liberação do último sensor |
| `CANCELADO_AGUARDANDO_LIVRE` | Sensores livres para fechar após cancelar |
| `MOSTRANDO_MENSAGEM` | Fim dos dois segundos de exibição do resultado |

O comando inicial para fechar o servo vem antes de montar o SD e percorrer o histórico. Após uma passagem confirmada, o comando para fechar também vem antes da gravação no SD. O comando não mede a posição real do motor.

## Dados e cartão SD

`registrarPassagem()` é a função que acrescenta registros. Ela só é chamada na conclusão de uma passagem confirmada. O arquivo atual é `acessos_escolares.csv`, com UTF-8, BOM e ponto e vírgula como separador.

O gravador confere o cabeçalho, escreve os dados, solicita `flush()`, fecha e reabre o arquivo. Compara o tamanho e cada byte da linha nova antes de anunciar que salvou. Isso detecta falhas de escrita ou leitura, mas não garante que um desligamento durante uma operação não corrompa o cartão.

Linhas têm limite de 1024 bytes, sem CR/LF. Campos com aspas e ponto e vírgula recebem escape. Quebras de linha do cadastro são substituídas por espaços para manter uma passagem em cada linha.

### Recuperação da presença

No arranque, o programa lê o arquivo antigo `acessos.csv`, se existir, e depois o atual. O arquivo antigo tem sete campos, é preservado e não recebe novas linhas. O histórico atual tem prioridade.

Para cada aluno, a última linha válida com UID e RM correspondentes define sua presença. Cabeçalhos, linhas inválidas, linhas longas demais e caudas sem quebra de linha final não geram presença.

Se uma escrita anterior ficou sem terminação, a próxima gravação acrescenta o marcador `REGISTRO_INCOMPLETO` antes da linha nova. A cauda anterior é invalidada para não se juntar ao próximo evento.

### Falhas do SD

Uma passagem física confirmada atualiza a presença na RAM mesmo quando a gravação falha. O LCD informa `PASSAGEM OK` e `ERRO NO SD`. Não existe fila de reenvio.

O SD é inicializado uma vez no `setup()`. Depois de corrigir um erro de cartão, reinicie a placa. O programa não formata o SD automaticamente.

Sem histórico utilizável, todos começam como fora. Uma passagem não salva pode ser perdida da presença após um reinício. Não existe reinicialização automática da presença a cada dia letivo.

## Wi-Fi e horário

A conexão Wi-Fi funciona em modo estação. Quando a conexão aparece, o programa configura NTP com `pool.ntp.org` e `time.nist.gov`. O fuso usado é Brasília, UTC-3, sem horário de verão nessa configuração.

O controle da passagem não espera a internet. Sem relógio disponível, `DATA` e `HORA` recebem `SEM_HORARIO`. Depois da sincronização, o relógio continua enquanto a placa está alimentada, mesmo que o Wi-Fi caia. Após desligar a alimentação, pode ser necessária outra sincronização.

O horário de um registro corresponde à confirmação da passagem. Registros antigos com `SEM_HORARIO` não são corrigidos automaticamente. `millis()` mede prazos e `time()` fornece o calendário, portanto a sincronização NTP não renova a tentativa.

O callback NTP apenas sinaliza a sincronização com uma variável atômica. O `loop()` exibe as mensagens. O código não envia os cadastros nem os registros a um servidor.

## Personalização

Configure alunos reais somente em `alunos_local.h` e a rede somente em `rede_local.h`. O cadastro publicado utiliza `Aluno Exemplo`, turma fictícia e UIDs e RMs de exemplo. O ano letivo é um campo do cadastro, separado da data do relógio.

Consulte [instalação](instalacao.md), [conexões](conexoes.md), [exemplo de registro](../examples/registro_ficticio.md) e [testes](../tests/README.md) para adaptar sua cópia.

## Limites da montagem

Os sensores verificam uma sequência de ocupação. Não identificam pessoas, não impedem acompanhantes e não comprovam que o portador é o dono do cartão. Um padrão fisicamente ambíguo pode parecer uma passagem normal para apenas dois sensores.

O firmware usa leitura por polling e algumas bibliotecas podem bloquear brevemente. Não há medição de posição do servo nem um sistema certificado de proteção mecânica. A montagem é um protótipo educacional.

## Referências técnicas

- [SPI e barramentos no Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/spi.html)
- [Wi-Fi no Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [Biblioteca SD](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD)
- [Interface de arquivos](https://github.com/espressif/arduino-esp32/blob/master/libraries/FS/src/FS.h)
- [Implementação de escrita e flush](https://github.com/espressif/arduino-esp32/blob/master/libraries/FS/src/vfs_api.cpp)
- [Exemplo SimpleTime](https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Time/SimpleTime/SimpleTime.ino)
- [Relógio do ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)
- [Biblioteca MFRC522](https://github.com/miguelbalboa/rfid)

[Voltar ao README](../README.md)
