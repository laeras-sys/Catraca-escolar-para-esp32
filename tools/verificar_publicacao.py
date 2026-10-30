"""Procura alguns dados que nao devem acompanhar a versao publica.

Mensagens mostram caminho, linha e motivo, nunca o valor encontrado.
Esta verificacao nao substitui a revisao humana e nao limpa historico Git.
"""

from pathlib import Path
import argparse
import re
import subprocess


ARQUIVOS_PRIVADOS = {
    "rede_local.h", "alunos_local.h", "segredos.h", "secrets.h", ".env"
}
EXTENSOES_PRIVADAS = {".csv", ".zip", ".7z", ".rar", ".tar", ".gz", ".pem", ".key", ".bak"}
REDE_EXEMPLO = {"", "SUA_REDE", "SUA_SENHA", "SSID_EXEMPLO", "SENHA_EXEMPLO", "REDE_DE_TESTE"}
EMAIL = re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.I)
TOKEN = re.compile(r"(?:ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|AKIA[0-9A-Z]{16}|sk-[A-Za-z0-9_-]{24,})")
CHAVE = re.compile(r"-----BEGIN (?:[A-Z ]+ )?PRIVATE KEY-----")
REDE = re.compile(r'\bWIFI_(?:SSID|SENHA)\s*=\s*"([^"\r\n]*)"')
CADASTRO = re.compile(r'\{\s*"([0-9A-F]+)"\s*,\s*"([^"\r\n]+)"\s*,\s*"(\d+)"')


def arquivos_para_conferir(raiz):
    """Em um clone, confere apenas arquivos que o Git acompanha."""
    try:
        topo = subprocess.run(
            ["git", "-C", str(raiz), "rev-parse", "--show-toplevel"],
            capture_output=True, text=True, check=True,
        )
        if Path(topo.stdout.strip()).resolve() == raiz:
            resultado = subprocess.run(
                ["git", "-C", str(raiz), "ls-files", "--cached", "-z"],
                capture_output=True, check=True,
            )
            return [raiz / nome.decode() for nome in resultado.stdout.split(b"\0") if nome], True
    except (OSError, subprocess.CalledProcessError):
        pass
    return sorted(p for p in raiz.rglob("*") if p.is_file() and not any(
        parte in {".git", "__pycache__", "node_modules"} for parte in p.relative_to(raiz).parts
    )), False


def conferir(raiz):
    problemas = []
    arquivos, usando_indice = arquivos_para_conferir(raiz)
    for arquivo in arquivos:
        relativo = arquivo.relative_to(raiz).as_posix()
        nome = arquivo.name.lower()
        if (nome in ARQUIVOS_PRIVADOS or arquivo.suffix.lower() in EXTENSOES_PRIVADAS
                or (nome.startswith(".env.") and nome != ".env.example")):
            problemas.append(f"{relativo}: arquivo privado ou pacote que exige revisao")
            continue
        if arquivo.is_symlink():
            problemas.append(f"{relativo}: link simbolico exige revisao manual")
            continue
        if not usando_indice and not arquivo.exists():
            continue
        try:
            if usando_indice:
                # Confere os bytes preparados para commit, mesmo se a copia
                # de trabalho foi alterada depois do git add.
                conteudo = subprocess.run(
                    ["git", "-C", str(raiz), "show", ":" + relativo],
                    capture_output=True, check=True,
                ).stdout
                texto = conteudo.decode("utf-8")
            else:
                texto = arquivo.read_text(encoding="utf-8")
        except UnicodeError:
            problemas.append(f"{relativo}: arquivo binario exige revisao manual")
            continue
        except (OSError, subprocess.CalledProcessError):
            problemas.append(f"{relativo}: nao foi possivel conferir o conteudo")
            continue
        for numero, linha in enumerate(texto.splitlines(), 1):
            motivo = None
            if EMAIL.search(linha):
                motivo = "possivel endereco de e-mail"
            elif TOKEN.search(linha) or CHAVE.search(linha):
                motivo = "possivel token ou chave privada"
            elif any(m.group(1) not in REDE_EXEMPLO for m in REDE.finditer(linha)):
                motivo = "configuracao de rede preenchida fora do modelo"
            if motivo:
                problemas.append(f"{relativo}:{numero}: {motivo}")
        if nome == "alunos_exemplo.h":
            registros = list(CADASTRO.finditer(texto))
            if not registros:
                problemas.append(f"{relativo}: cadastro de exemplo ausente ou fora do formato")
            for indice, registro in enumerate(registros, 1):
                esperado = (f"{indice:08X}", f"Aluno Exemplo {indice}", f"{indice:06d}")
                if registro.groups() != esperado:
                    numero = texto.count("\n", 0, registro.start()) + 1
                    problemas.append(f"{relativo}:{numero}: use somente o cadastro ficticio do modelo")
    return problemas


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path(__file__).resolve().parents[1])
    raiz = parser.parse_args().raiz.resolve()
    if not raiz.is_dir():
        parser.error("A raiz precisa ser uma pasta existente.")
    problemas = conferir(raiz)
    if problemas:
        print("Revise os seguintes arquivos antes de publicar:")
        print("\n".join(problemas))
        return 1
    print("OK: nenhum dos padroes verificados foi encontrado nos arquivos atuais.")
    print("Revise o diff: nomes reais e outros segredos podem escapar desta verificacao.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
