import tempfile
from pathlib import Path
import subprocess
import os

root = Path(__file__).resolve().parents[1]
code = (root/'catraca_sd_hspi/catraca_sd_hspi.ino').read_text()
start = code.index('class CSVEscolar {')
end = code.index('// FIM_CSV_ESCOLAR',start)
csv_class = code[start:end]

tests = r'''
#include <array>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iostream>
'''+csv_class+r'''
int main() {
  using Campos = CSVEscolar::Campos;
  using Presenca = CSVEscolar::Presenca;
  int casos = 0;
  const std::string turma = "Turma Exemplo";
  const Campos entrada = {{"ENTRADA","000001","Aluno Exemplo 1",turma,"2026",
                          "29/10/2026","17:40:01","00000001","1"}};
  const auto serializada = CSVEscolar::montarLinha(entrada);
  Campos recuperados;
  assert(CSVEscolar::separarLinha(serializada,recuperados)==9);
  assert(recuperados==entrada);
  ++casos;
  Presenca p;
  assert(CSVEscolar::lerPresenca(serializada,false,p));
  assert(p.uid=="00000001" && p.rm=="000001" && p.entrada);
  ++casos;

  Campos semHorario = entrada;
  semHorario[5] = semHorario[6] = "SEM_HORARIO";
  assert(CSVEscolar::separarLinha(CSVEscolar::montarLinha(semHorario),recuperados)==9);
  assert(recuperados[5]=="SEM_HORARIO" && recuperados[6]=="SEM_HORARIO");
  assert(CSVEscolar::lerPresenca(CSVEscolar::montarLinha(semHorario),false,p));
  ++casos;

  // Nomes ficticios usados somente para verificar caracteres e escapes.
  const std::vector<std::string> nomes = {
      "Ana; Maria", "Joao \"Jota\"", "Ana;\"Maria\"", "", "Álvaro Çésar", "A ",
      "\"", ";", "\";\"", "A\nB\rC"};
  for (const auto& nome : nomes) {
    Campos dado = entrada;
    dado[2] = nome;
    const auto linha = CSVEscolar::montarLinha(dado);
    assert(linha.find('\n')==std::string::npos && linha.find('\r')==std::string::npos);
    assert(CSVEscolar::separarLinha(linha,recuperados)==9);
    std::string esperado;
    for(char c:nome) esperado += (c=='\r'||c=='\n') ? ' ' : c;
    assert(recuperados[2]==esperado);
    assert(CSVEscolar::lerPresenca(linha,false,p));
    ++casos;
  }

  assert(CSVEscolar::separarLinha(std::string("\xEF\xBB\xBF")+serializada,recuperados)==9);
  assert(recuperados==entrada);
  ++casos;
  const std::vector<std::string> invalidas = {
    "TIPO;RM;NOME;TURMA;ANO_LETIVO;DATA;HORA;UID;DENTRO",
    "ENTRADA;000001;Aluno Exemplo 1", serializada+";EXTRA", serializada+";REGISTRO_INCOMPLETO",
    "ENTRADA;000001;\"Aluno Exemplo 1", "ENTRADA;000001;\"Aluno Exemplo 1\"X;T;2026;D;H;00000001;1",
    "ENTRADA;000001;Alu\"no;T;2026;D;H;00000001;1",
    "ENTRADA;000001;Aluno Exemplo 1;T;2026;D;H;00000001;",
    "ENTRADA;000001;Aluno Exemplo 1;T;2026;D;H;00000001;abc",
    "QUALQUER;000001;Aluno Exemplo 1;T;2026;D;H;00000001;1",
    "ENTRADA;;Aluno Exemplo 1;T;2026;D;H;00000001;1",
    "ENTRADA;000001;Aluno Exemplo 1;T;2026;D;H;;1",
    "ENTRADA;000001;Aluno Exemplo 1;T;2026;D;H;00000001;1\n",
    std::string("ENTRADA;000001;Alu") + std::string(1,'\0') + "no;T;2026;D;H;00000001;1"
  };
  for(const auto& linha:invalidas) {
    assert(!CSVEscolar::lerPresenca(linha,false,p));
    ++casos;
  }

  for(size_t corte=0;corte<serializada.size();++corte) {
    // Nenhuma cauda de uma escrita interrompida pode virar uma presenca valida.
    const auto incompleta=serializada.substr(0,corte)+";REGISTRO_INCOMPLETO";
    assert(!CSVEscolar::lerPresenca(incompleta,false,p));
    ++casos;
  }

  assert(CSVEscolar::lerPresenca("ENTRADA;00000001;Aluno Exemplo 1;000001;29/10/2026;07:00:00;1",true,p));
  assert(p.entrada && p.uid=="00000001" && p.rm=="000001");
  ++casos;
  assert(CSVEscolar::lerPresenca("SAIDA;00000001;Aluno Exemplo 1;000001;SEM_DATA;SEM_HORA;0",true,p));
  assert(!p.entrada);
  ++casos;
  assert(!CSVEscolar::lerPresenca(serializada,true,p));
  assert(!CSVEscolar::lerPresenca("ENTRADA;00000001;Aluno Exemplo 1;000001;D;H;1",false,p));
  ++casos;

  // Simula a mesma ordem de restauracao: historico antigo, depois novo.
  std::map<std::string,std::string> banco = {{"00000001","000001"},{"00000002","000002"}};
  std::map<std::string,bool> dentro;
  auto aplicar = [&](const std::string& linha, bool antigo) {
    Presenca r;
    if(!CSVEscolar::lerPresenca(linha,antigo,r))return;
    auto it=banco.find(r.uid);
    if(it!=banco.end() && it->second==r.rm)dentro[r.uid]=r.entrada;
  };
  aplicar("ENTRADA;00000001;Aluno Exemplo 1;000001;D;H;1",true);
  aplicar("ENTRADA;00000002;Aluno Exemplo 2;000002;D;H;2",true);
  Campos saida = entrada;
  saida[0]="SAIDA"; saida[8]="1";
  aplicar(CSVEscolar::montarLinha(saida),false);
  Campos rmErrado=entrada; rmErrado[1]="000000";
  aplicar(CSVEscolar::montarLinha(rmErrado),false);
  assert(!dentro["00000001"] && dentro["00000002"]);
  ++casos;

  std::cout << "OK: " << casos << " casos de CSV, acentos, SEM_HORARIO e presencas.\n";
}
'''
pasta_temporaria=tempfile.TemporaryDirectory(prefix='teste_catraca_')
saida=Path(pasta_temporaria.name)
cpp=saida/'teste_csv.cpp'
binary=saida/'teste_csv'
cpp.write_text(tests)
subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(cpp),'-o',str(binary)],check=True)
env=dict(os.environ)
# O ambiente de execucao nao permite ao LeakSanitizer consultar /proc/task.
env['ASAN_OPTIONS']=(env.get('ASAN_OPTIONS','')+':detect_leaks=0').strip(':')
subprocess.run([str(binary)],check=True,env=env)
