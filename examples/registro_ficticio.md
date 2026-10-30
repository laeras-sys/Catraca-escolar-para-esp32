# Exemplo fictício de registro

O arquivo `acessos_escolares.csv` usa ponto e vírgula como separador e codificação UTF-8. Cada passagem confirmada acrescenta uma linha.

```csv
TIPO;RM;NOME;TURMA;ANO_LETIVO;DATA;HORA;UID;DENTRO
ENTRADA;000001;Aluno Exemplo 1;Turma Exemplo;2026;29/10/2026;08:00:00;00000001;1
ENTRADA;000002;Aluno Exemplo 2;Turma Exemplo;2026;29/10/2026;08:01:00;00000002;2
SAIDA;000001;Aluno Exemplo 1;Turma Exemplo;2026;29/10/2026;12:00:00;00000001;1
```

Todos os dados acima são inventados. `DENTRO` é a quantidade total de alunos cadastrados considerados dentro depois do evento.

Sem horário disponível, a passagem ainda pode ser registrada:

```csv
ENTRADA;000003;Aluno Exemplo 3;Turma Exemplo;2026;SEM_HORARIO;SEM_HORARIO;00000003;2
```

Abra o CSV no Excel ou no LibreOffice Calc. Se necessário, importe usando UTF-8 e ponto e vírgula. Selecione UID e RM como texto para preservar zeros iniciais.

O SD não recebe uma linha de acesso para cartão desconhecido, passagem incompleta ou tentativa cancelada. Os registros com `SEM_HORARIO` não são corrigidos automaticamente após uma sincronização NTP.

[Voltar ao README](../README.md)
