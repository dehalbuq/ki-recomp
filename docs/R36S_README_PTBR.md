# Killer Instinct clássico no R36S — Guia em português

Port independente ARM64 do arcade **Killer Instinct (1994)** para o R36S
(RK3326/Cortex-A35), baseado na recompilação estática desenvolvida por
[`kilnan`](https://github.com/kilnan/ki-recomp) e adaptada por
[`dehalbuq`](https://github.com/dehalbuq).

**Branch de desenvolvimento:**
[`port/r36s-portmaster`](https://github.com/dehalbuq/ki-recomp/tree/port/r36s-portmaster).

## O que funciona e o que foi otimizado

- Executável nativo AArch64, sem MAME nem DOSBox para executar o
  processador MIPS do jogo. O processador de áudio ADSP-2105 **ainda
  é emulado** para reproduzir o hardware original.
- OpenGL ES 2.0 no Mali-450 para apresentar a imagem final com cores
  RGBA, conversão NEON e escala *perfect pixel* 4:3.
- Compilador otimizado para Cortex-A35, caminhos rápidos de memória,
  saltos indiretos MIPS e ciclos ociosos.
- Áudio original do DSP com reprodução estabilizada por WSOLA
  (experimental; preserva a tonalidade, embora possa haver estalos
  em cenas muito exigentes).
- Controle por direcional/analógico, seis golpes, inserção de moeda e
  Start+Select para abrir o menu Sair/Continuar.
- Tela azul da inicialização acelerada temporariamente, mantendo a
  cadência correta nas partidas.
- **Duas telas novas antes do jogo:** créditos aos autores e aviso legal,
  ambas com fonte embutida e sem impacto durante as lutas.
- Compilação **Release sem debug**, sem amostragem de CPU, medições de
  FPS nem scripts de diagnóstico incluídos no pacote final.

**Desempenho medido no Teste 21 (dArkOSRE):** cerca de 53,8 FPS em
média após a abertura, variando de 48,3 a 58,3 FPS em janelas de
cinco segundos. A meta do arcade original é aproximadamente 58,98
FPS. A nova compilação limpa com splash ainda precisa ser validada
fisicamente antes da homologação.

## Como instalar (teste pessoal)

1. Faça backup do conteúdo de `roms/ports/killerinstinct/` e dos saves.
2. Compile de forma privada com seus arquivos adquiridos legalmente,
   seguindo o guia de build; não compartilhe o binário recompilado
   sem verificar os direitos de redistribuição.
3. Extraia o ZIP PortMaster na pasta `roms/ports/`.
4. Coloque seus próprios arquivos **`kinst.zip` (BIOS L1.5D) e
   `kinst.chd`** em `roms/ports/killerinstinct/gamedata/`.
5. Abra **Killer Instinct Arcade** no menu Ports.
   Aparecerão os créditos e o aviso legal antes da inicialização.
   Pressione **A/Start** para avançar após um breve intervalo.

Os arquivos originais, ROMs, músicas, imagens e CHD **não são
disponibilizados pelo projeto**. Não substitua a `glibc`, o driver
Mali, o SDL ou EGL do firmware para forçar compatibilidade.

## Créditos

- **Jogo original:** Rare / Midway e seus respectivos titulares.
- **Recompilação do programa MIPS:** kilnan/ki-recomp.
- **Port para R36S, otimizações e integração PortMaster:** dehalbuq.
- **Emulação ADSP-2105:** código derivado do MAME/Aaron Giles.
- **Bibliotecas e ferramentas:** SDL2, libchdr, miniz e rabbitizer,
  sob suas licenças individuais.

O projeto não tem ligação, patrocínio ou autorização declarada dos
titulares do jogo, nem representa uma publicação oficial do PortMaster.

**Atenção legal:** o executável recompilado pode incorporar código
derivado do jogo original. Portanto, mesmo que não inclua
`kinst.zip` nem `kinst.chd`, a distribuição pública do binário pode
exigir autorização. Publicar o código-fonte independente e testes
sintéticos é diferente de distribuir o jogo recompilado. Consulte
[`R36S_LEGAL.md`](R36S_LEGAL.md).

## Como contribuir

Faça fork do projeto, crie uma branch a partir de
`port/r36s-portmaster` e envie um pull request com uma alteração
pequena e testável. São bem-vindos testes de outros firmwares,
correções de controles, áudio, otimização da CPU, documentação,
regressões de renderização e relatórios de problemas.

Informe modelo, firmware, commit, local da falha, passos para
reproduzir, resultado esperado/observado e, se possível, logs
sem dados pessoais. **Nunca envie ROMs, CHDs, executáveis privados,
C++ gerado do jogo ou conteúdo protegido** para issues e PRs.

Leia o [guia completo de contribuição](../CONTRIBUTING.md) e
o [histórico técnico de testes](R36S_PROGRESS.md).
