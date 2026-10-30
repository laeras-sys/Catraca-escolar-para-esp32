int verificacoes=0, falhas=0;
void verificar(bool condicao,const char* nome){++verificacoes;if(!condicao){++falhas;std::cerr<<"FALHOU: "<<nome<<"\n";}}

void reiniciarTeste(){
  tempoMs=0;tempoEpoch=0;atrasoRedeMs=0;
  entradas.clear();entradas[PINO_SENSOR_1]=HIGH;entradas[PINO_SENSOR_2]=HIGH;
  disco=DiscoFake();eventos.clear();Serial.texto.clear();
  sensor1.iniciar();sensor2.iniciar();tempoMs=50;
  sensor1.atualizar(tempoMs);sensor2.atualizar(tempoMs);
  for(int i=0;i<TOTAL_PESSOAS;i++)alunoDentro[i]=false;
  recontarPessoas();estado=ESPERANDO;pessoaPendente=-1;
  operacaoEntrada=true;primeiroJaLiberou=false;inicioOperacao=0;inicioMensagem=0;
  motivoCancelamento="";sdDisponivel=true;ntpConfigurado=false;
  notificacaoNtpPendente.store(false);horarioDisponivelAnterior=false;
  ultimoRegistroComHorario=false;wifiEstavaConectado=false;ultimaTentativaWifi=0;
  WiFi.conectado=false;rfid.cartoes.clear();servoPortinhola.angulo=ANGULO_FECHADO;
  WiFi.chamadasBegin=0;WIFI_SSID="REDE_DE_TESTE";WIFI_SENHA="";
  setenv("TZ","BRT3",1);tzset();
}
void tick(unsigned long ms,bool s1,bool s2){tempoMs+=ms;entradas[PINO_SENSOR_1]=s1?LOW:HIGH;entradas[PINO_SENSOR_2]=s2?LOW:HIGH;loop();}
void passo(bool primeiro,bool segundo){const bool s1=operacaoEntrada?primeiro:segundo;const bool s2=operacaoEntrada?segundo:primeiro;tick(1,s1,s2);tick(41,s1,s2);}
void autorizar(bool entrada){alunoDentro[0]=!entrada;recontarPessoas();rfid.cartoes.push_back(banco[0].uid);tick(1,false,false);verificar(estado==AGUARDANDO_PRIMEIRO,"RFID autoriza aluno e aguarda primeiro sensor");verificar(operacaoEntrada==entrada,"presenca determina entrada ou saida");}
void atravessar(bool sobrepostos){passo(true,false);if(sobrepostos){passo(true,true);passo(false,true);}else{passo(false,false);passo(false,true);}passo(false,false);}
size_t linhasSalvas(){auto it=disco.arquivos.find(ARQUIVO_ACESSOS);if(it==disco.arquivos.end())return 0;size_t n=std::count(it->second.begin(),it->second.end(),'\n');return n?n-1:0;}
bool mensagemSd(){return lcd.linhas[1]=="ERRO NO SD";}

int main(){
  for(bool entrada:{true,false})for(bool sobrepostos:{true,false}){
    reiniciarTeste();autorizar(entrada);atravessar(sobrepostos);
    verificar(linhasSalvas()==1,"sequencia completa gera uma linha");
    verificar(alunoDentro[0]==entrada,"sequencia correta atualiza presenca");
    verificar(pessoasDentro==(entrada?1U:0U),"contagem reflete entrada ou saida");
    verificar(servoPortinhola.angulo==ANGULO_FECHADO,"fecha apos ambos livres");
    verificar(disco.arquivos[ARQUIVO_ACESSOS].find("SEM_HORARIO;SEM_HORARIO")!=std::string::npos,"sem internet preserva passagem com SEM_HORARIO");
    for(int i=0;i<10;i++)tick(10,false,false);
    verificar(linhasSalvas()==1,"nao duplica registro apos liberar sensores");
  }
  for(bool entrada:{true,false}){
    reiniciarTeste();autorizar(entrada);passo(false,true);
    verificar(linhasSalvas()==0&&alunoDentro[0]!=entrada,"sensor invertido nao grava nem muda presenca");
    verificar(estado==CANCELADO_AGUARDANDO_LIVRE,"ocupacao apos cancelar aguarda liberacao");
    verificar(servoPortinhola.angulo==ANGULO_ABERTO,"nao fecha ao cancelar com sensor ocupado");
    passo(false,false);verificar(servoPortinhola.angulo==ANGULO_FECHADO,"fecha cancelamento quando sensores liberam");
    reiniciarTeste();autorizar(entrada);passo(true,true);
    verificar(linhasSalvas()==0&&alunoDentro[0]!=entrada,"ambos iniciais sem ordem nao confirmam");
    reiniciarTeste();autorizar(entrada);passo(true,false);passo(true,true);passo(true,false);passo(false,false);
    verificar(linhasSalvas()==0&&alunoDentro[0]!=entrada,"recuo com sobreposicao nao confirma");
    reiniciarTeste();autorizar(entrada);passo(true,false);passo(false,false);passo(true,false);
    verificar(linhasSalvas()==0&&alunoDentro[0]!=entrada,"recuo em sensores separados nao confirma");
  }
  reiniciarTeste();autorizar(true);tick(TEMPO_LIMITE_PASSAGEM_MS,false,false);
  verificar(linhasSalvas()==0&&!alunoDentro[0]&&pessoaPendente==-1,"cartao sem passagem expira e descarta identidade");
  passo(true,false);passo(false,false);passo(false,true);passo(false,false);
  verificar(linhasSalvas()==0,"deteccao apos cancelar nao recupera tentativa expirada");
  reiniciarTeste();autorizar(true);passo(true,false);tick(TEMPO_LIMITE_PASSAGEM_MS,true,false);
  verificar(linhasSalvas()==0&&estado==CANCELADO_AGUARDANDO_LIVRE,"primeiro sem segundo expira sem fechar ocupado");
  passo(false,false);verificar(servoPortinhola.angulo==ANGULO_FECHADO,"timeout fecha apenas apos sensores livres");
  reiniciarTeste();autorizar(true);passo(true,false);passo(true,true);tick(TEMPO_LIMITE_PASSAGEM_MS,true,true);
  verificar(linhasSalvas()==0,"acionar ambos sem libera-los nao conclui");
  reiniciarTeste();autorizar(true);for(int i=0;i<6;i++){tick(10,true,false);tick(10,false,false);}tick(TEMPO_LIMITE_PASSAGEM_MS,false,false);
  verificar(linhasSalvas()==0,"ruido menor que 40 ms nao gera passagem");
  reiniciarTeste();rfid.cartoes.push_back("DEADBEEF");tick(1,false,false);
  verificar(servoPortinhola.angulo==ANGULO_FECHADO&&pessoaPendente==-1&&linhasSalvas()==0,"cartao desconhecido nao abre");
  reiniciarTeste();passo(true,false);rfid.cartoes.push_back(banco[0].uid);tick(1,true,false);
  verificar(servoPortinhola.angulo==ANGULO_FECHADO&&pessoaPendente==-1,"sensor ocupado impede nova autorizacao");

  // Reproduz atraso em WiFi.status() depois da amostragem: 9990 -> 10090 ms.
  reiniciarTeste();autorizar(true);estado=AGUARDANDO_LIBERAR_SEGUNDO;
  tempoMs=inicioOperacao+9990;atrasoRedeMs=100;tick(0,false,false);
  verificar(linhasSalvas()==0&&!alunoDentro[0],"regressao: rede lenta nao aceita confirmacao depois de 10 s");
  reiniciarTeste();autorizar(true);estado=AGUARDANDO_LIBERAR_SEGUNDO;
  tempoMs=inicioOperacao+9999;tick(0,false,false);
  verificar(linhasSalvas()==1,"ambos livres em 9999 ms confirmam");
  reiniciarTeste();autorizar(true);estado=AGUARDANDO_LIBERAR_SEGUNDO;
  tempoMs=inicioOperacao+10000;tick(0,false,false);
  verificar(linhasSalvas()==0,"em 10000 ms o prazo expirou");
  reiniciarTeste();tempoMs=ULONG_MAX-5000;inicioOperacao=tempoMs;estado=AGUARDANDO_PRIMEIRO;pessoaPendente=0;
  tick(10001,false,false);verificar(pessoaPendente==-1,"timeout funciona quando millis da a volta");

  reiniciarTeste();autorizar(true);disco.perderFlush=true;atravessar(false);
  verificar(mensagemSd()&&sdDisponivel==false,"regressao: escrita aceita mas flush perdido deve avisar erro no SD");
  verificar(alunoDentro[0],"falha no SD nao desfaz passagem fisica");
  reiniciarTeste();autorizar(true);disco.corromperFlush=true;atravessar(false);
  verificar(mensagemSd(),"regressao: bytes divergentes na releitura geram erro no SD");
  reiniciarTeste();autorizar(true);disco.limiteEscrita=2;atravessar(false);
  verificar(mensagemSd(),"escrita parcial gera erro no SD");
  reiniciarTeste();autorizar(true);disco.falharAbertura=true;atravessar(false);
  verificar(mensagemSd(),"falha de abertura gera erro no SD");
  reiniciarTeste();sdDisponivel=false;autorizar(true);atravessar(false);
  verificar(alunoDentro[0]&&linhasSalvas()==0&&mensagemSd(),"sem SD permite passagem e avisa falta de registro");
  reiniciarTeste();tempoEpoch=1793306401;autorizar(true);atravessar(false);
  verificar(disco.arquivos[ARQUIVO_ACESSOS].find("29/10/2026;17:40:01")!=std::string::npos,"registro usa Brasilia quando relogio e valido");

  reiniciarTeste();autorizar(true);atravessar(false);const auto historico=disco.arquivos;
  reiniciarTeste();disco.arquivos=historico;restaurarPresencas();
  verificar(alunoDentro[0]&&pessoasDentro==1,"reinicio recupera entrada confirmada");
  autorizar(false);atravessar(true);for(int i=0;i<TOTAL_PESSOAS;i++)alunoDentro[i]=false;restaurarPresencas();
  verificar(!alunoDentro[0]&&pessoasDentro==0&&linhasSalvas()==2,"ultima linha valida recupera saida");
  reiniciarTeste();disco.arquivos=historico;
  const auto linha=std::string("SAIDA;")+banco[0].RM+";Exemplo;Turma;2026;D;H;"+banco[0].uid+";0";
  disco.arquivos[ARQUIVO_ACESSOS]+=linha;restaurarPresencas();
  verificar(alunoDentro[0],"cauda sem quebra de linha nao recupera saida");
  autorizar(false);atravessar(false);
  verificar(disco.arquivos[ARQUIVO_ACESSOS].find(";REGISTRO_INCOMPLETO\r\n")!=std::string::npos,"proxima escrita invalida cauda interrompida");
  verificar(linhasSalvas()==3,"cauda interrompida preservada e nova linha separada");
  reiniciarTeste();disco.arquivos[ARQUIVO_ACESSOS]="CABECALHO INCOMPATIVEL\r\n";autorizar(true);atravessar(false);
  verificar(mensagemSd()&&disco.arquivos[ARQUIVO_ACESSOS]=="CABECALHO INCOMPATIVEL\r\n","nao mistura formato desconhecido no CSV");

  reiniciarTeste();servoPortinhola.angulo=-1;setup();
  const auto fechado=std::find(eventos.begin(),eventos.end(),"servo_fechado");
  const auto sd=std::find(eventos.begin(),eventos.end(),"sd_begin");
  verificar(fechado<sd,"regressao: manda fechar antes de montar SD e recuperar historico");
  reiniciarTeste();WIFI_SSID="";setup();atualizarRede(millis());
  verificar(WiFi.chamadasBegin==0,"exemplo sem SSID nao tenta conectar a uma rede vazia");
  std::cout<<verificacoes<<" verificacoes; "<<falhas<<" falhas.\n";
  return falhas?1:0;
}
