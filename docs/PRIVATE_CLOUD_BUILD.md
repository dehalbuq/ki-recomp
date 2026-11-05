# Compilacao privada na nuvem (R36S)

Este roteiro NAO coloca kinst.zip, kinst.chd, arquivos extraidos ou codigo
do jogo no fork publico dehalbuq/ki-recomp. A build ocorre em um
repositorio GitHub separado e privado. Dados sao enviados ao GitHub e
sujeitos aos controles e politicas de retencao dessa plataforma: nao ha
garantia de confidencialidade absoluta.

## Passos

1. Crie um repositorio GitHub PRIVATE, por exemplo,
   dehalbuq/ki-recomp-build-private.
2. Copie tools/r36s/private-ci-workflow.yml.template do fork publico para
   .github/workflows/build-r36s.yml do repositorio PRIVADO.
3. No seu clone do repositorio PRIVADO (nao do fork publico), copie os
   arquivos originais para data/kinst.zip e data/kinst.chd. Ative Git LFS
   para o CHD, pois o GitHub bloqueia blobs comuns maiores que 100 MiB:

~~~sh
git lfs install
git lfs track "data/*.chd"
mkdir -p data
cp /caminho/kinst.zip data/kinst.zip
cp /caminho/kinst.chd data/kinst.chd
git add .gitattributes data .github/workflows/build-r36s.yml
git commit -m "Add private KI build inputs"
git push
~~~

4. No GitHub, em Actions do repositorio PRIVADO, execute:
   Killer Instinct R36S - PRIVATE ARM64 build > Run workflow.
5. Em caso de sucesso, baixe o artifact privado KI-R36S-private-debug,
   que contem o ZIP experimental SEM ROMs/CHD.
6. Copie os seus arquivos originais para killerinstinct/gamedata no R36S
   depois de extrair o ZIP. Envie somente logs do dispositivo para analise.

## Limites

- Apenas quem tem acesso ao repositorio privado pode ver seus assets e
  artifacts pelo GitHub; o provedor ainda processa/guarda os dados conforme
  seus termos. Apagar um arquivo de Git ou Git LFS nao o remove
  automaticamente de todos os objetos ou copias historicas.
- A toolchain ARM64 do GitHub pode gerar executaveis incompatíveis com
  a glibc e SDL2 do DarkOSre/ArkOS. Precisamos do diagnostico real do R36S.
- O codigo atual expande o CHD em RAM, o que pode ultrapassar a memoria
  disponivel no R36S. Otimizacao futura: leitura sob demanda.
- Codigo derivado da ROM e binarios de jogo devem ficar em artifacts PRIVADOS.
  Nunca publique ROM, CHD ou fontes gerados a partir do jogo.
