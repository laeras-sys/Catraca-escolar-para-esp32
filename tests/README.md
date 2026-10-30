# Testes no computador

Os testes usam o código do sketch com periféricos simulados. Não conectam a um ESP32 nem usam cartões reais.

## Requisitos

- Python 3.
- g++ com suporte a C++17 e aos sanitizadores de endereço e comportamento indefinido.
- Ambiente Linux ou WSL para os comandos e cabeçalhos utilizados.

Não é necessário instalar pacotes Python adicionais.

## Executar

Abra um terminal na raiz do repositório:

```bash
python3 tests/executar.py catraca_sd_hspi/catraca_sd_hspi.ino
python3 tests/testar_csv.py
python3 tests/testar_horario.py
python3 tools/verificar_publicacao.py
```

| Arquivo | O que verifica |
| --- | --- |
| `executar.py` e `cenarios.cpp` | Entrada, saída, ordem dos sensores, recuo, ruído, prazo, falhas de SD e presença |
| `simulador.h` | Implementações simuladas de RFID, sensores, servo, LCD, rede e SD |
| `testar_csv.py` | Separação dos campos, aspas, acentos, linhas interrompidas e recuperação de presença |
| `testar_horario.py` | Relógio ausente, UTC-3 e mudança de data usando instantes controlados |
| `../tools/verificar_publicacao.py` | Alguns padrões de dados que não devem ser publicados |

Os executáveis de teste ficam em pastas temporárias. Os dados e as datas dos cenários são fictícios e não dependem do horário atual do computador.

## Alcance

Um teste aprovado não comprova estabilidade elétrica, alinhamento dos sensores, movimento mecânico ou compatibilidade das bibliotecas instaladas. Compile na IDE Arduino e teste a montagem física depois de modificar o sketch.

[Voltar ao README](../README.md)
