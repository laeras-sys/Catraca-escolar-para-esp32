# Dados privados e publicação

Este repositório publica exemplos de alunos, UIDs e RMs fictícios. O arquivo de rede de exemplo mantém SSID e senha vazios.

## Arquivos que ficam no computador da montagem

- `catraca_sd_hspi/rede_local.h`: sua rede e sua senha.
- `catraca_sd_hspi/alunos_local.h`: cadastro real de alunos e cartões.
- Arquivos CSV retirados do SD: registros reais de entrada e saída.
- Cópias de segurança que incluam esses dados.

O `.gitignore` evita incluir esses arquivos com comandos comuns do Git. Ele não impede enviar arquivos pelo navegador nem retira um arquivo que já tenha sido versionado.

Não publique ZIPs da sua pasta de uso local. O botão **Code > Download ZIP** do GitHub já fornece uma cópia da versão pública.

## Antes de publicar

Na raiz do repositório, execute:

```bash
python3 tools/verificar_publicacao.py
```

Em um clone Git, selecione primeiro os arquivos do commit, por exemplo com `git add`, e execute a verificação. Ela lê os bytes do índice Git que entrarão no commit, mesmo se você alterar a cópia de trabalho depois. Arquivos novos ainda não selecionados não entram nessa verificação. Em uma cópia sem `.git`, verifica os arquivos presentes na pasta.

Ele detecta nomes de arquivos privados, alguns padrões de tokens e chaves, endereços de e-mail e valores de rede preenchidos fora dos exemplos de teste. Também confere o padrão fictício do cadastro de exemplo. As mensagens indicam arquivo e linha sem reproduzir o possível segredo.

A verificação não consegue reconhecer todos os nomes reais ou todos os tipos de segredo. Revise o diff antes do commit e os arquivos selecionados antes de enviar pelo navegador.

Para pedir ajuda, substitua dados de alunos e credenciais antes de compartilhar CSVs, capturas de tela ou mensagens do Monitor Serial.

## Arquivos atuais e histórico Git

Remover um dado dos arquivos atuais não o remove de commits anteriores, branches, forks ou cópias já baixadas. O histórico também guarda autoria e e-mail dos commits, independentemente do conteúdo dos arquivos.

A organização atual preserva o histórico existente. Para reduzir a exposição em novos commits, configure na sua conta e no seu cliente Git o endereço privado de commits fornecido pelo GitHub.

Se uma credencial tiver sido publicada, revogue ou troque a credencial e planeje a limpeza do histórico. Essa limpeza precisa considerar outras branches e cópias do repositório.

[Voltar ao README](../README.md)
