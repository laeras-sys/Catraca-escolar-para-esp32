/*
  TCC - ESP32 + RC522 + LCD 16x2 + SG90 + DOIS sensores digitais + SD.

  MAPA DE CONEXOES:
  RC522: SS=5, RST=27, SCK=18, MISO=19, MOSI=23, VCC=3V3.
  LCD I2C: SDA=21, SCL=22, endereco=0x27. Servo: sinal=25.

  SENSORES E CARTAO SD:
  Sensor 1, lado de fora: OUT/DO=32.
  Sensor 2, lado de dentro: OUT/DO=33.
  Modulo SD em SPI separada (HSPI): CS=13, SCK=14, MISO/DO=34, MOSI/DI=26.
  Adaptador SD com regulador AMS1117-3.3: VCC=5V e GND comum.
  O RC522 permanece alimentado em 3V3.
  RFID continua usando SPI (VSPI) nos GPIOs 18, 19 e 23.
  Pinos do SD atualizados para as conexoes informadas pelo grupo.
  Sensores com saida compativel com 3,3 V e GND comum ao ESP32.
  Nao usa botao. Sensores digitais ativos em LOW; nao usa HC-SR04.

  LOGICA:
  1. O RFID identifica o aluno, tanto para entrar quanto para sair.
  2. Se o aluno estiver fora, autoriza entrada: S1 -> S2.
     Se estiver dentro, autoriza saida: S2 -> S1.
  3. Exige a ordem correta e a liberacao final dos dois sensores.
     Aceita sensores com areas de deteccao sobrepostas ou separadas.
  4. O prazo TOTAL e de 10 segundos desde a autorizacao; nao e renovado
     quando o primeiro sensor e acionado. Nao usa delay(10000), pois
     precisa continuar lendo os sensores.
  5. Sem confirmacao, com ordem errada, recuo detectado ou prazo vencido:
     descarta a tentativa e NAO grava no SD nem altera a presenca.
     Se algum sensor continuar ocupado, aguarda liberar para fechar.
     Depois do cancelamento, deteccoes tardias nao podem gerar registro.
  6. Uma passagem confirmada altera a presenca e grava /acessos_escolares.csv.
     Cada linha tem tipo, RM, nome, turma, ano letivo, data, hora, UID e contagem.
  7. Ao reiniciar, recupera a presenca dos CSVs antigo e novo, nessa ordem.
     Sem SD, controla pela RAM; ao reiniciar sem historico, todos comecam
     como fora. Erros de gravacao aparecem no LCD e no Monitor Serial.

  CADASTRO: array banco[] em alunos_local.h. Exemplos em alunos_exemplo.h.
  Serial: 115200 baud. Consulte README.md e docs/guia_tecnico.md na raiz.
  BIBLIOTECAS: MFRC522, LiquidCrystal I2C (com lcd.init()), ESP32Servo.
  WiFi, FS, SD e time acompanham o pacote ESP32 da IDE Arduino.
  Ajuste WIFI_SSID e WIFI_SENHA em rede_local.h, usando rede_exemplo.h como modelo.
  Horario: NTP em UTC-3; sem horario valido, libera e salva SEM_HORARIO.
  O horario e o da passagem CONFIRMADA, nao apenas da leitura do cartao.
  Turma e ano letivo de exemplo: Turma Exemplo, 2026.
  Novos registros: /acessos_escolares.csv, UTF-8 e separador ponto e virgula.
  /acessos.csv e preservado; nao recebe novas linhas nem e reformatado.
  O Wi-Fi acerta o relogio; os registros continuam locais, no cartao SD.

  Limite do prototipo: a ordem dos sensores indica a direcao, mas nao
  garante uma unica pessoa por autorizacao nem identifica acompanhantes.
*/

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <atomic>
#include <array>
#include <string>

// 1. CONEXOES FIXAS DO PROJETO: nao e necessario mover nenhum cabo.
#define SS_PIN        5
#define RST_PIN       27
#define SCK_PIN       18
#define MISO_PIN      19
#define MOSI_PIN      23
#define LCD_SDA       21
#define LCD_SCL       22
#define PINO_SERVO    25
#define PINO_SENSOR_1 32
#define PINO_SENSOR_2 33
#define PINO_SD_CS    13
#define PINO_SD_SCK   14
#define PINO_SD_MISO  34
#define PINO_SD_MOSI  26

// 2. AJUSTES. Em modo UID, somente mostra cartoes; nao abre nem grava.
const bool MODO_LER_UIDS = false;
const uint32_t FREQUENCIA_SD_HZ = 400000; // Frequencia conservadora usada na montagem.
const int NIVEL_SENSOR_1_ATIVO = LOW;
const int NIVEL_SENSOR_2_ATIVO = LOW;
const int ANGULO_FECHADO = 0;
const int ANGULO_ABERTO = 90;
const unsigned long TEMPO_LIMITE_PASSAGEM_MS = 10000;
const unsigned long TEMPO_MENSAGEM_MS = 2000;
const unsigned long INTERVALO_WIFI_MS = 30000;

// A configuracao privada fica fora do sketch para facilitar a publicacao.
// Sem o arquivo local, usa o exemplo: Wi-Fi desativado por SSID vazio.
#if __has_include("rede_local.h")
#include "rede_local.h"
#else
#include "rede_exemplo.h"
#endif
// CONFIGURACAO DO HORARIO E DO CADASTRO.
const char* SERVIDOR_NTP = "pool.ntp.org";
const char* SERVIDOR_NTP_RESERVA = "time.nist.gov";
const char* TURMA_PADRAO = "Turma Exemplo"; // Dado ficticio: configure sua turma localmente.
const char* ANO_LETIVO_PADRAO = "2026"; // Atualize ao mudar o ano letivo.
const long FUSO_SEGUNDOS = -3L * 3600L;
// O arquivo antigo e preservado e consultado para recuperar presencas.
// O novo arquivo usa um cabecalho proprio para nao misturar formatos.
const char* ARQUIVO_ACESSOS = "/acessos_escolares.csv";
const char* ARQUIVO_ACESSOS_ANTIGO = "/acessos.csv";
const char* CABECALHO_CSV = "TIPO;RM;NOME;TURMA;ANO_LETIVO;DATA;HORA;UID;DENTRO";
const size_t LIMITE_LINHA_CSV = 1024;

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoPortinhola;
// Barramento exclusivo do SD; o RC522 continua usando a instancia global SPI.
SPIClass spiSD(HSPI);

// 3. CADASTRO. RM e UID sao textos para preservar zeros iniciais.
struct Pessoa {
  const char* uid;
  const char* nome;
  const char* RM;
  const char* turma;
  const char* anoLetivo;
};

// Pessoa precisa estar definida antes de incluir o cadastro.
// O repositorio contem exemplos. Os dados reais ficam somente na copia local.
#if __has_include("alunos_local.h")
#include "alunos_local.h"
#else
#include "alunos_exemplo.h"
#endif
const int TOTAL_PESSOAS = sizeof(banco) / sizeof(banco[0]);
// Uma posicao por aluno: false=fora, true=dentro. O ultimo evento valido
// do SD repoe este estado no arranque. A contagem e recalculada, nao somada.
bool alunoDentro[TOTAL_PESSOAS] = {};
unsigned int pessoasDentro = 0;

// Filtra os sensores para que oscilacoes curtas nao gerem passagens.
class EntradaFiltrada {
 public:
  EntradaFiltrada(int pino, int nivelAtivo, unsigned long filtroMs)
      : pino_(pino), ativo_(nivelAtivo), filtro_(filtroMs) {}

  void iniciar() {
    pinMode(pino_, INPUT_PULLUP);
    bruto_ = estavel_ = digitalRead(pino_);
    mudouEm_ = millis();
    ativou_ = false;
  }

  void atualizar(unsigned long agora) {
    ativou_ = false;
    int leitura = digitalRead(pino_);
    if (leitura != bruto_) {
      bruto_ = leitura;
      mudouEm_ = agora;
    }
    if (agora - mudouEm_ >= filtro_ && estavel_ != bruto_) {
      estavel_ = bruto_;
      ativou_ = (estavel_ == ativo_);
    }
  }

  // ativou() vale por UMA iteracao; estaAtiva() mantem o nivel filtrado.
  bool ativou() const { return ativou_; }
  bool estaAtiva() const { return estavel_ == ativo_; }
  bool brutaAtiva() const { return digitalRead(pino_) == ativo_; }
  // Para fechar, exige tambem a leitura bruta livre por 40 ms.
  bool estaLivre(unsigned long agora) const {
    return !estaAtiva() && !brutaAtiva() && agora - mudouEm_ >= filtro_;
  }

 private:
  int pino_;
  int ativo_;
  unsigned long filtro_;
  int bruto_ = HIGH;
  int estavel_ = HIGH;
  unsigned long mudouEm_ = 0;
  bool ativou_ = false;
};

EntradaFiltrada sensor1(PINO_SENSOR_1, NIVEL_SENSOR_1_ATIVO, 40);
EntradaFiltrada sensor2(PINO_SENSOR_2, NIVEL_SENSOR_2_ATIVO, 40);

// 4. MAQUINA DE ESTADOS. Nao usa uma espera bloqueante de 10 segundos.
// Cada volta do loop verifica sensores e pode avancar, confirmar ou cancelar.
enum Estado {
  ESPERANDO,                       // Le um novo cartao; ainda nao ha identidade pendente.
  AGUARDANDO_PRIMEIRO,              // Autorizado: espera o sensor do lado de origem.
  AGUARDANDO_SEGUNDO,               // Primeiro visto; falta a confirmacao do outro.
  AGUARDANDO_LIBERAR_PRIMEIRO,       // Ambos ocupados: espera deixar o primeiro.
  AGUARDANDO_LIBERAR_SEGUNDO,        // Falta liberar o ultimo sensor.
  CANCELADO_AGUARDANDO_LIVRE,       // Sem identidade; so espera poder fechar.
  MOSTRANDO_MENSAGEM                // Pausa de mensagem sem delay().
};
Estado estado = ESPERANDO;
bool operacaoEntrada = true;
bool primeiroJaLiberou = false;
int pessoaPendente = -1;
unsigned long inicioOperacao = 0;
unsigned long inicioMensagem = 0;
String motivoCancelamento;
bool sdDisponivel = false;
bool ntpConfigurado = false;
std::atomic<bool> notificacaoNtpPendente(false);
bool horarioDisponivelAnterior = false;
bool ultimoRegistroComHorario = false;
bool wifiEstavaConectado = false;
unsigned long ultimaTentativaWifi = 0;

void mostrarTela(const String& linha1, const String& linha2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linha1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(linha2.substring(0, 16));
}

void mostrarTelaInicial() {
  mostrarTela("CATRACA ESCOLAR", "APROXIME CARTAO");
}

void mostrarMensagem(const String& linha1, const String& linha2) {
  mostrarTela(linha1, linha2);
  estado = MOSTRANDO_MENSAGEM;
  inicioMensagem = millis();
}

bool sensoresLivres(unsigned long agora) {
  return sensor1.estaLivre(agora) && sensor2.estaLivre(agora);
}

int procurarPessoaPorUID(const String& uid) {
  for (int i = 0; i < TOTAL_PESSOAS; i++) {
    if (uid == banco[i].uid) return i;
  }
  return -1;
}

void recontarPessoas() {
  pessoasDentro = 0;
  for (int i = 0; i < TOTAL_PESSOAS; i++) {
    if (alunoDentro[i]) pessoasDentro++;
  }
}

// INICIO_CSV_ESCOLAR - rotinas independentes da placa, verificadas em C++.
class CSVEscolar {
 public:
  static const size_t TOTAL_CAMPOS = 9;
  using Campos = std::array<std::string, TOTAL_CAMPOS>;
  struct Presenca {
    std::string uid;
    std::string rm;
    bool entrada = false;
  };

  // Mantem uma passagem por linha; aspas e ponto e virgula sao escapados.
  static std::string escaparCampo(const std::string& texto) {
    std::string limpo;
    for (char c : texto) {
      if (c == '\r' || c == '\n') limpo += ' ';
      else if (c != '\0') limpo += c;
    }
    if (limpo.find_first_of(";\"") == std::string::npos) return limpo;
    std::string resultado = "\"";
    for (char c : limpo) {
      if (c == '"') resultado += '"';
      resultado += c;
    }
    resultado += '"';
    return resultado;
  }

  static std::string montarLinha(const Campos& campos) {
    std::string linha;
    for (size_t i = 0; i < campos.size(); ++i) {
      if (i != 0) linha += ';';
      linha += escaparCampo(campos[i]);
    }
    return linha;
  }

  // Retorna zero para linhas truncadas, com aspas invalidas ou campos demais.
  static size_t separarLinha(const std::string& linha, Campos& campos) {
    campos = Campos();
    size_t coluna = 0;
    size_t inicio = linha.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    bool entreAspas = false;
    bool aspasFechadas = false;
    for (size_t i = inicio; i < linha.size(); ++i) {
      char c = linha[i];
      if (c == '\r' || c == '\n' || c == '\0') return 0;
      if (entreAspas) {
        if (c == '"') {
          if (i + 1 < linha.size() && linha[i + 1] == '"') {
            campos[coluna] += '"';
            ++i;
          } else {
            entreAspas = false;
            aspasFechadas = true;
          }
        } else campos[coluna] += c;
      } else if (c == ';') {
        if (++coluna >= campos.size()) return 0;
        aspasFechadas = false;
      } else if (c == '"') {
        if (!campos[coluna].empty() || aspasFechadas) return 0;
        entreAspas = true;
      } else {
        if (aspasFechadas) return 0;
        campos[coluna] += c;
      }
    }
    return entreAspas ? 0 : coluna + 1;
  }

  static bool lerPresenca(const std::string& linha, bool formatoAntigo,
                         Presenca& registro) {
    Campos campos;
    size_t quantidade = separarLinha(linha, campos);
    if (quantidade != (formatoAntigo ? 7U : TOTAL_CAMPOS)) return false;
    if (campos[0] != "ENTRADA" && campos[0] != "SAIDA") return false;
    const std::string& contagem = campos[formatoAntigo ? 6 : 8];
    if (contagem.empty()) return false;
    for (char c : contagem) if (c < '0' || c > '9') return false;
    registro.uid = campos[formatoAntigo ? 1 : 7];
    registro.rm = campos[formatoAntigo ? 3 : 1];
    if (registro.uid.empty() || registro.rm.empty()) return false;
    registro.entrada = (campos[0] == "ENTRADA");
    return true;
  }
};
// FIM_CSV_ESCOLAR

void aplicarRegistroAnterior(const String& linha, bool formatoAntigo) {
  CSVEscolar::Presenca registro;
  if (!CSVEscolar::lerPresenca(linha.c_str(), formatoAntigo, registro)) return;
  int indice = procurarPessoaPorUID(String(registro.uid.c_str()));
  if (indice < 0 || registro.rm != banco[indice].RM) return;
  alunoDentro[indice] = registro.entrada;
}

void restaurarArquivo(const char* caminho, bool formatoAntigo) {
  if (!SD.exists(caminho)) return;
  File arquivo = SD.open(caminho, FILE_READ);
  if (!arquivo) {
    Serial.print("Nao foi possivel ler o historico: ");
    Serial.println(caminho);
    return;
  }
  String linha;
  bool longaDemais = false;
  while (arquivo.available()) {
    int valor = arquivo.read();
    if (valor < 0) break;
    char c = static_cast<char>(valor);
    if (c == '\n') {
      if (!longaDemais) aplicarRegistroAnterior(linha, formatoAntigo);
      linha = "";
      longaDemais = false;
    } else if (c != '\r' && !longaDemais) {
      if (linha.length() < LIMITE_LINHA_CSV) linha += c;
      else longaDemais = true;
    }
  }
  // Uma linha sem terminacao final nao e usada para recuperar presenca.
  arquivo.close();
}

// 5. SD E RECUPERACAO. Linhas validas sao aplicadas na ordem em que estao
// no arquivo. O novo historico prevalece sobre o legado; nao apaga nenhum.
void restaurarPresencas() {
  restaurarArquivo(ARQUIVO_ACESSOS_ANTIGO, true);
  restaurarArquivo(ARQUIVO_ACESSOS, false);
  recontarPessoas();
  Serial.print("Presenca recuperada do SD. Pessoas dentro: ");
  Serial.println(pessoasDentro);
}

bool inspecionarArquivoCSV(bool& criarCabecalho, bool& completarLinha,
                          bool& invalidarCauda) {
  criarCabecalho = true;
  completarLinha = false;
  invalidarCauda = false;
  if (!SD.exists(ARQUIVO_ACESSOS)) return true;
  File leitura = SD.open(ARQUIVO_ACESSOS, FILE_READ);
  if (!leitura) return false;
  size_t tamanho = leitura.size();
  if (tamanho == 0) {
    leitura.close();
    return true;
  }
  criarCabecalho = false;
  String cabecalho;
  while (leitura.available()) {
    int c = leitura.read();
    if (c < 0) { leitura.close(); return false; }
    if (c == '\n') break;
    if (cabecalho.length() >= LIMITE_LINHA_CSV) {
      leitura.close();
      return false;
    }
    cabecalho += static_cast<char>(c);
  }
  size_t fimCabecalho = leitura.position();
  if (cabecalho.startsWith("\xEF\xBB\xBF")) cabecalho.remove(0, 3);
  cabecalho.trim();
  if (cabecalho != CABECALHO_CSV || !leitura.seek(tamanho - 1)) {
    leitura.close();
    return false;
  }
  int ultimo = leitura.read();
  leitura.close();
  if (ultimo < 0) return false;
  completarLinha = (ultimo != '\n');
  invalidarCauda = completarLinha && tamanho > fimCabecalho;
  return true;
}

void inicializarSD() {
  // Ordem dos argumentos: SCK, MISO, MOSI, CS.
  // Esta chamada configura somente o HSPI do SD, preservando o SPI do RFID.
  spiSD.begin(PINO_SD_SCK, PINO_SD_MISO, PINO_SD_MOSI, PINO_SD_CS);
  Serial.println("SD: HSPI, CS=13, SCK=14, MISO=34, MOSI=26, 400 kHz.");
  sdDisponivel = SD.begin(PINO_SD_CS, spiSD, FREQUENCIA_SD_HZ);
  if (!sdDisponivel || SD.cardType() == CARD_NONE) {
    sdDisponivel = false;
    Serial.println("SD indisponivel. Passagens nao poderao ser salvas.");
    return;
  }
  Serial.println("SD iniciado. Apenas passagens confirmadas serao gravadas.");
  restaurarPresencas();
  Serial.print("Arquivo dos novos registros: ");
  Serial.println(ARQUIVO_ACESSOS);
}

String lerUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// 6. RFID. Encerra a comunicacao com a tag apos obter o UID. Para reutilizar
// o mesmo cartao, afaste-o do leitor e aproxime novamente.
bool lerCartao(String& uid) {
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;
  uid = lerUID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.print("UID lido: ");
  Serial.println(uid);
  return true;
}

void imprimirLinhaParaCadastro(const String& uid) {
  Serial.print("  {\"");
  Serial.print(uid);
  Serial.println("\", \"NOME DO ALUNO\", \"RM DO ALUNO\", TURMA_PADRAO, ANO_LETIVO_PADRAO},");
}

void mostrarUID(const String& uid) {
  if (uid.length() <= 16) mostrarTela("UID DO CARTAO", uid);
  else mostrarTela(uid.substring(0, 16), uid.substring(16));
}

// NTP chama esta funcao em outra tarefa. A mensagem e exibida no loop.
void aoSincronizarHorario(struct timeval* instante) {
  (void)instante;
  notificacaoNtpPendente.store(true);
}

// 7. RELOGIO. millis() mede prazos; time() fornece calendario. Sao funcoes
// diferentes: ajustar NTP nunca renova os 10 segundos da passagem.
bool obterDataHora(char* data, size_t tamanhoData, char* hora, size_t tamanhoHora) {
  snprintf(data, tamanhoData, "%s", "SEM_HORARIO");
  snprintf(hora, tamanhoHora, "%s", "SEM_HORARIO");
  time_t agora = time(nullptr);
  struct tm horario;
  // Sem esperar pela rede: o controle da passagem continua funcionando.
  if (agora < 1704067200 || localtime_r(&agora, &horario) == nullptr) return false;
  bool dataOk = strftime(data, tamanhoData, "%d/%m/%Y", &horario) != 0;
  bool horaOk = strftime(hora, tamanhoHora, "%H:%M:%S", &horario) != 0;
  return dataOk && horaOk;
}

void atualizarRede(unsigned long agora) {
  if (WIFI_SSID[0] == '\0') return; // Exemplo publico: operacao offline.
  bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiEstavaConectado) {
    Serial.print("Wi-Fi conectado. IP: ");
    Serial.println(WiFi.localIP());
  }
  if (!conectado && wifiEstavaConectado) {
    Serial.println("Wi-Fi desconectado. A catraca continua funcionando.");
  }
  wifiEstavaConectado = conectado;
  if (conectado && !ntpConfigurado) {
    configTime(FUSO_SEGUNDOS, 0, SERVIDOR_NTP, SERVIDOR_NTP_RESERVA);
    ntpConfigurado = true;
    Serial.println("Sincronizacao NTP solicitada. Aguardando horario correto...");
  }
  if (!conectado && agora - ultimaTentativaWifi >= INTERVALO_WIFI_MS) {
    ultimaTentativaWifi = agora;
    Serial.println("Tentando reconectar ao Wi-Fi...");
    WiFi.reconnect();
  }

  char data[16];
  char hora[16];
  bool horarioDisponivel = obterDataHora(data, sizeof(data), hora, sizeof(hora));
  bool sincronizouAgora = notificacaoNtpPendente.exchange(false);
  if (horarioDisponivel && (sincronizouAgora || !horarioDisponivelAnterior)) {
    Serial.print(sincronizouAgora ? "Horario sincronizado via NTP: "
                                 : "Horario disponivel: ");
    Serial.print(data);
    Serial.print(" ");
    Serial.print(hora);
    Serial.println(" (Brasilia, UTC-3).");
  }
  horarioDisponivelAnterior = horarioDisponivel;
}

// Confere somente a linha nova, depois de fechar e reabrir o arquivo.
// File::flush() nao devolve um resultado de sucesso. A releitura detecta
// gravacoes perdidas/divergentes que uma contagem de bytes aceitos nao prova.
// Isso nao torna o cartao imune a uma queda de energia durante a gravacao.
bool conferirLinhaGravada(size_t inicio, const std::string& linha) {
  File leitura = SD.open(ARQUIVO_ACESSOS, FILE_READ);
  if (!leitura) return false;
  bool confere = leitura.size() == inicio + linha.size() && leitura.seek(inicio);
  for (size_t i = 0; confere && i < linha.size(); ++i) {
    confere = leitura.read() == static_cast<unsigned char>(linha[i]);
  }
  leitura.close();
  return confere;
}

// Esta e a UNICA funcao que grava passagens; so e chamada na confirmacao.
bool registrarPassagem() {
  char data[16];
  char hora[16];
  ultimoRegistroComHorario = obterDataHora(data, sizeof(data), hora, sizeof(hora));
  CSVEscolar::Campos campos = {{
    operacaoEntrada ? "ENTRADA" : "SAIDA",
    banco[pessoaPendente].RM,
    banco[pessoaPendente].nome,
    banco[pessoaPendente].turma,
    banco[pessoaPendente].anoLetivo,
    data,
    hora,
    banco[pessoaPendente].uid,
    String(pessoasDentro).c_str()
  }};
  std::string linha = CSVEscolar::montarLinha(campos) + "\r\n";
  Serial.print("PASSAGEM CONFIRMADA: ");
  Serial.print(linha.c_str());
  if (!ultimoRegistroComHorario) {
    Serial.println("Passagem com SEM_HORARIO: relogio ainda sem horario valido.");
  }

  if (!sdDisponivel) return false;
  // O restaurador aceita no maximo este tamanho por linha, sem CR/LF.
  if (linha.size() - 2 > LIMITE_LINHA_CSV) {
    Serial.println("ERRO: linha CSV excede o limite; reduza o tamanho do cadastro.");
    return false;
  }
  bool criarCabecalho;
  bool completarLinha;
  bool invalidarCauda;
  if (!inspecionarArquivoCSV(criarCabecalho, completarLinha, invalidarCauda)) {
    Serial.println("ERRO: nao foi possivel conferir o cabecalho do CSV.");
    sdDisponivel = false;
    return false;
  }
  File arquivo = SD.open(ARQUIVO_ACESSOS, FILE_APPEND);
  if (!arquivo) {
    Serial.println("ERRO: nao foi possivel abrir o arquivo do SD.");
    sdDisponivel = false;
    return false;
  }
  bool gravou = true;
  if (criarCabecalho) {
    // BOM UTF-8 permite ao Excel reconhecer os acentos.
    const uint8_t bom[] = {0xEF, 0xBB, 0xBF};
    gravou = (arquivo.write(bom, sizeof(bom)) == sizeof(bom));
    String cabecalho = String(CABECALHO_CSV) + "\r\n";
    if (gravou) gravou = (arquivo.print(cabecalho) == cabecalho.length());
  } else if (completarLinha) {
    // Se uma escrita anterior foi interrompida, nao cola o novo registro nela.
    const char* termino = invalidarCauda ? ";REGISTRO_INCOMPLETO\r\n" : "\n";
    gravou = (arquivo.print(termino) == strlen(termino));
  }
  size_t inicioLinha = arquivo.position();
  if (gravou) gravou = (arquivo.print(linha.c_str()) == linha.length());
  arquivo.flush();
  arquivo.close();
  if (gravou) gravou = conferirLinhaGravada(inicioLinha, linha);
  if (!gravou) {
    sdDisponivel = false;
    Serial.println("ERRO: escrita incompleta ou releitura divergente no SD; confira o cartao.");
  } else {
    Serial.print("Registro salvo em ");
    Serial.println(ARQUIVO_ACESSOS);
  }
  return gravou;
}

// 8. AUTORIZACAO. Le apenas quando ESPERANDO. Uma tentativa usa um unico
// aluno, uma direcao e um inicioOperacao; sensores nao renovam esse inicio.
void solicitarPassagem() {
  String uid;
  if (!lerCartao(uid)) return;
  int indice = procurarPessoaPorUID(uid);
  if (indice < 0) {
    imprimirLinhaParaCadastro(uid);
    mostrarMensagem("ACESSO NEGADO", uid);
    return;
  }
  if (!sensoresLivres(millis())) {
    mostrarMensagem("SENSOR OCUPADO", "LIBERE SENSORES");
    return;
  }
  pessoaPendente = indice;
  operacaoEntrada = !alunoDentro[indice];
  primeiroJaLiberou = false;
  inicioOperacao = millis();
  estado = AGUARDANDO_PRIMEIRO;
  servoPortinhola.write(ANGULO_ABERTO);
  mostrarTela(operacaoEntrada ? "ACESSO LIBERADO" : "SAIDA LIBERADA",
              String("RM ") + banco[indice].RM);
  Serial.println(operacaoEntrada
      ? "Entrada autorizada: aguarda S1 -> S2 e ambos livres em ate 10 s."
      : "Saida autorizada: aguarda S2 -> S1 e ambos livres em ate 10 s.");
}

// Cancelar nao grava linha de acesso e nao muda alunoDentro[]. Se houver
// ocupacao, mantem aberto e exige liberar os sensores antes de fechar.
void cancelarPassagem(const String& motivo) {
  pessoaPendente = -1;  // Descarta a identidade pendente; nao altera presenca.
  motivoCancelamento = motivo;
  Serial.print("NAO REGISTRADO NO SD: ");
  Serial.println(motivo);
  if (sensoresLivres(millis())) {
    servoPortinhola.write(ANGULO_FECHADO);
    mostrarMensagem(motivoCancelamento, "SEM REGISTRO");
  } else {
    estado = CANCELADO_AGUARDANDO_LIVRE;
    mostrarTela("NAO REGISTRADO", "LIBERE SENSORES");
  }
}

void concluirPassagem() {
  // Ultima checagem com o instante atual: nunca aceita uma confirmacao tardia.
  if (millis() - inicioOperacao >= TEMPO_LIMITE_PASSAGEM_MS) {
    cancelarPassagem("TEMPO ESGOTADO");
    return;
  }
  // So chega aqui depois da sequencia correta, dentro do prazo, com
  // os DOIS sensores livres. Fecha antes de iniciar a escrita no SD.
  servoPortinhola.write(ANGULO_FECHADO);
  // A passagem fisica ocorreu: atualiza a RAM mesmo se o SD falhar.
  // Neste caso, o LCD avisa ERRO NO SD; nao existe fila de reenvio.
  alunoDentro[pessoaPendente] = operacaoEntrada;
  recontarPessoas();
  bool salvo = registrarPassagem();
  if (salvo) {
    mostrarMensagem(operacaoEntrada ? "ENTRADA OK" : "SAIDA CONFIRMADA",
                    ultimoRegistroComHorario
                        ? String("DENTRO: ") + String(pessoasDentro)
                        : String("SEM HORARIO"));
  } else {
    mostrarMensagem("PASSAGEM OK", "ERRO NO SD");
  }
  pessoaPendente = -1;
}

// 9. INICIALIZACAO. Executa uma vez ao ligar ou reiniciar.
void setup() {
  Serial.begin(115200);
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  sensor1.iniciar();
  sensor2.iniciar();

  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  pinMode(PINO_SD_CS, OUTPUT);
  digitalWrite(PINO_SD_CS, HIGH);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);

  if (!MODO_LER_UIDS) {
    // Montar o SD e percorrer o historico pode demorar. Manda fechar primeiro.
    servoPortinhola.setPeriodHertz(50);
    servoPortinhola.attach(PINO_SERVO, 500, 2400);
    servoPortinhola.write(ANGULO_FECHADO);
    inicializarSD();
  }
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial();
  if (MODO_LER_UIDS) {
    mostrarTela("LER UID CARTAO", "APROXIME CARTAO");
    return;
  }

  // Configura o fuso mesmo antes de existir conexao com a internet.
  setenv("TZ", "BRT3", 1);
  tzset();
  sntp_set_time_sync_notification_cb(aoSincronizarHorario);
  Serial.print("Turma: ");
  Serial.print(TURMA_PADRAO);
  Serial.print(" | Ano letivo: ");
  Serial.println(ANO_LETIVO_PADRAO);
  Serial.println("Ate o relogio estar pronto, as passagens usam SEM_HORARIO.");
  if (WIFI_SSID[0] != '\0') {
    Serial.print("Wi-Fi: conectando a ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
  } else {
    Serial.println("Wi-Fi sem configuracao: operacao local, sem sincronizar NTP.");
  }
  ultimaTentativaWifi = millis();
  mostrarTelaInicial();
}

// 10. CONTROLE CONTINUO. Entrada: S1->S2. Saida: S2->S1.
// A subtracao de unsigned long continua valida quando millis() da a volta.
void loop() {
  // Rede pode levar tempo. Atualize-a ANTES de amostrar sensores e relogio.
  // Assim o prazo nao usa um instante antigo capturado antes da rede.
  if (!MODO_LER_UIDS) atualizarRede(millis());
  unsigned long agora = millis();
  sensor1.atualizar(agora);
  sensor2.atualizar(agora);
  if (MODO_LER_UIDS) {
    String uid;
    if (lerCartao(uid)) {
      mostrarUID(uid);
      imprimirLinhaParaCadastro(uid);
    }
    return;
  }
  bool emPassagem = (estado == AGUARDANDO_PRIMEIRO
                  || estado == AGUARDANDO_SEGUNDO
                  || estado == AGUARDANDO_LIBERAR_PRIMEIRO
                  || estado == AGUARDANDO_LIBERAR_SEGUNDO);
  // Em 10000 ms, o prazo terminou. Ambos devem ter liberado ANTES disso;
  // o filtro de 40 ms tambem precisa terminar dentro desse prazo total.
  if (emPassagem && agora - inicioOperacao >= TEMPO_LIMITE_PASSAGEM_MS) {
    cancelarPassagem("TEMPO ESGOTADO");
    return;
  }

  // Na saida, os papeis dos sensores se invertem.
  EntradaFiltrada& primeiro = operacaoEntrada ? sensor1 : sensor2;
  EntradaFiltrada& segundo = operacaoEntrada ? sensor2 : sensor1;

  switch (estado) {
    case ESPERANDO:
      solicitarPassagem();
      break;

    case AGUARDANDO_PRIMEIRO:
      if (segundo.estaAtiva()) {
        // Ambos ao mesmo tempo tambem sao ambiguos: nao ha ordem valida.
        cancelarPassagem("ORDEM INVALIDA");
      } else if (primeiro.ativou()) {
        estado = AGUARDANDO_SEGUNDO;
      }
      break;

    case AGUARDANDO_SEGUNDO:
      // Um intervalo com ambos livres e permitido quando ha espaco entre
      // sensores. Reativar o primeiro nesse intervalo caracteriza recuo.
      if (!primeiro.estaAtiva()) primeiroJaLiberou = true;
      if (primeiroJaLiberou && primeiro.ativou()) {
        cancelarPassagem("PASSAGEM RECUOU");
      } else if (segundo.ativou()) {
        estado = primeiro.estaAtiva()
            ? AGUARDANDO_LIBERAR_PRIMEIRO : AGUARDANDO_LIBERAR_SEGUNDO;
      }
      break;

    case AGUARDANDO_LIBERAR_PRIMEIRO:
      if (sensoresLivres(agora)) {
        concluirPassagem();
      } else if (!segundo.estaAtiva() && primeiro.estaAtiva()) {
        // Ex.: S1 -> ambos -> apenas S1 indica retorno ao lado inicial.
        cancelarPassagem("PASSAGEM RECUOU");
      } else if (!primeiro.estaAtiva()) {
        estado = AGUARDANDO_LIBERAR_SEGUNDO;
      }
      break;

    case AGUARDANDO_LIBERAR_SEGUNDO:
      if (primeiro.estaAtiva()) {
        cancelarPassagem("PASSAGEM RECUOU");
      } else if (sensoresLivres(agora)) {
        concluirPassagem();
      }
      break;

    case CANCELADO_AGUARDANDO_LIVRE:
      if (sensoresLivres(agora)) {
        servoPortinhola.write(ANGULO_FECHADO);
        mostrarMensagem(motivoCancelamento, "SEM REGISTRO");
      }
      break;

    case MOSTRANDO_MENSAGEM:
      if (agora - inicioMensagem >= TEMPO_MENSAGEM_MS) {
        estado = ESPERANDO;
        mostrarTelaInicial();
      }
      break;
  }
}
