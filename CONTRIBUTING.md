# Como contribuir

Descreva o problema, a mudança proposta e como você a verificou. Para alterações da lógica, execute os testes documentados em [tests/README.md](tests/README.md).

Mantenha o nome da pasta e do sketch `catraca_sd_hspi`. Alterações dos pinos precisam vir acompanhadas da atualização do mapa de conexões e de validação na montagem correspondente.

Use somente dados fictícios nos exemplos. Preserve os arquivos `*_exemplo.h` sem credenciais ou cadastros reais. Coloque suas configurações somente nas cópias locais ignoradas pelo Git.

Depois de selecionar os arquivos com `git add`, execute `python3 tools/verificar_publicacao.py` e revise o diff do commit. Não inclua registros reais do SD, fotos de cartões com identificação ou ZIPs da pasta de uso local.

[Voltar ao README](README.md)
