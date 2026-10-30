import tempfile
from pathlib import Path
from datetime import datetime,timezone
import subprocess,os

root=Path(__file__).resolve().parents[1]
code=(root/'catraca_sd_hspi/catraca_sd_hspi.ino').read_text()
start=code.index('bool obterDataHora(')
end=code.index('void atualizarRede(',start)
function=code[start:end]
epoch=int(datetime(2026,10,29,20,40,1,tzinfo=timezone.utc).timestamp())
virada=int(datetime(2027,1,1,1,0,0,tzinfo=timezone.utc).timestamp())
tests=r'''
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
time_t instanteControlado=0;
time_t horaControlada(time_t* destino) {
  if(destino)*destino=instanteControlado;
  return instanteControlado;
}
#define time horaControlada
'''+function+r'''
#undef time
int main() {
  setenv("TZ","BRT3",1);
  tzset();
  char data[16],hora[16];
  assert(!obterDataHora(data,sizeof(data),hora,sizeof(hora)));
  assert(strcmp(data,"SEM_HORARIO")==0 && strcmp(hora,"SEM_HORARIO")==0);
  instanteControlado=EPOCH_VALIDO;
  assert(obterDataHora(data,sizeof(data),hora,sizeof(hora)));
  assert(strcmp(data,"29/10/2026")==0 && strcmp(hora,"17:40:01")==0);
  instanteControlado+=10;
  assert(obterDataHora(data,sizeof(data),hora,sizeof(hora)));
  assert(strcmp(hora,"17:40:11")==0);
  instanteControlado=EPOCH_VIRADA;
  assert(obterDataHora(data,sizeof(data),hora,sizeof(hora)));
  assert(strcmp(data,"31/12/2026")==0 && strcmp(hora,"22:00:00")==0);
  std::cout<<"OK: horario ausente, Brasilia UTC-3, passagem do tempo e virada de data.\n";
}
'''
tests=tests.replace('EPOCH_VALIDO',str(epoch)).replace('EPOCH_VIRADA',str(virada))
pasta_temporaria=tempfile.TemporaryDirectory(prefix='teste_catraca_')
saida=Path(pasta_temporaria.name)
cpp=saida/'teste_horario.cpp'
binary=saida/'teste_horario'
cpp.write_text(tests)
subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(cpp),'-o',str(binary)],check=True)
env=dict(os.environ)
env['ASAN_OPTIONS']=(env.get('ASAN_OPTIONS','')+':detect_leaks=0').strip(':')
subprocess.run([str(binary)],check=True,env=env)
