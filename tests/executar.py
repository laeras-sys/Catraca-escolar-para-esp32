from pathlib import Path
import argparse
import os
import subprocess
import tempfile

parser=argparse.ArgumentParser(description='Verifica o sketch real com perifericos simulados, sem alterar seus arquivos.')
parser.add_argument('sketch',type=Path)
args=parser.parse_args()
base=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='teste_catraca_') as pasta:
    pasta=Path(pasta)
    (pasta/'simulador.h').write_bytes((base/'simulador.h').read_bytes())
    for biblioteca in ('Arduino.h','SPI.h','MFRC522.h','Wire.h','LiquidCrystal_I2C.h','ESP32Servo.h','WiFi.h','FS.h','SD.h','esp_sntp.h'):
        (pasta/biblioteca).write_text('#pragma once\n#include "simulador.h"\n')
    fonte='#include "'+str(args.sketch.resolve())+'"\n'+(base/'cenarios.cpp').read_text()
    (pasta/'testes.cpp').write_text(fonte)
    comando=['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I',str(pasta),str(pasta/'testes.cpp'),'-o',str(pasta/'testes')]
    subprocess.run(comando,check=True)
    env=dict(os.environ)
    env['ASAN_OPTIONS']='detect_leaks=0'
    resultado=subprocess.run([str(pasta/'testes')],env=env)
    raise SystemExit(resultado.returncode)
