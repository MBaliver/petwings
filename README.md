# PetWings — source port

Port do **PetWings 1.3** (Saito Games, 1999–2009, freeware) para Linux e navegador
(desktop e celular), usando SDL2.

A lógica do jogo foi reconstruída a partir do executável original com o auxílio dos
símbolos de depuração (`PetWings.pdb`) que acompanham o pacote: classes, métodos e
constantes seguem o código-fonte original (`JGame`, `JEnemyChara`, `JBossChara01..05`,
`JScript`, ...). Os scripts de movimento, mapas das fases, gráficos e sons são lidos dos
recursos do `.exe` original; nenhum asset é incluído neste repositório.

## Requisitos

- Os arquivos originais: `PetWings.exe`, `Music01.mid`, `Music02.mid`, `Music03.mid`
- Python 3, CMake, um compilador C++17 e SDL2
- Para as músicas: `fluidsynth`, `ffmpeg` e um soundfont GM (ex.: `soundfont-fluid`)
- Para a versão web: Emscripten (com um `node` funcional)

## Compilar e jogar

```sh
./build.sh data ~/Downloads/petwings_package   # extrai gráficos/sons/scripts e renderiza as músicas
./build.sh native                               # gera build/petwings
./build/petwings
```

Versão web (funciona em celular):

```sh
./build.sh web      # gera build-web/index.html + .js + .wasm + .data
./build.sh serve    # http://localhost:8080
```

Para publicar, copie os arquivos de `build-web/` (index.*, ícones, `manifest.webmanifest`,
`og-image.png`) para qualquer hospedagem estática. A página tem tags Open Graph para a prévia de
links (Discord, WhatsApp, Telegram); como `og:image` precisa de endereço absoluto, informe o seu
com `emcmake cmake -S . -B build-web -DPETWINGS_PUBLIC_URL=https://seu.dominio` (padrão:
`https://petwings.mojavelabs.cloud`).

## Hospedagem no ZimaOS (servidor mojave)

```sh
./build.sh web && ./build.sh deploy        # envia para mojave:/DATA/AppData/petwings
ssh mojave 'bash /DATA/AppData/petwings/install.sh'   # só na 1ª vez ou se o compose mudar
```

O app aparece no painel do ZimaOS como **PetWings** e abre em **https://petwings.mojavelabs.cloud**
(rede local, via Nginx Proxy Manager; direto: http://192.168.68.151:8099). O container é um
nginx:alpine na porta 8099, definido em `deploy/zimaos/`. Para atualizar o jogo basta `./build.sh web && ./build.sh deploy`; o nginx
manda revalidar os arquivos, então o navegador pega a versão nova no próximo carregamento.

## Controles

| Ação | Teclado | Gamepad | Toque |
|---|---|---|---|
| Mover | ← → | D-pad / analógico | direcional (esquerda) |
| Atirar / confirmar | Espaço, Ctrl, Enter | A/B/X/Y | botão laranja (direita) |
| Menus | ↑ ↓ ← → | D-pad | direcional |
| Pausar | P | Start | botão ‖ |
| Volume da música / dos efeitos | `-` `=` / Shift+`-` `=` | — | menu OPTIONS |
| Voltar ao título / sair | Esc | Back | — |
| Tela cheia | F11 / Alt+Enter | — | botão ⛶ (web) |
| Escala / filtro / aspecto / tipo de widescreen | F5 / F6 / F7 / F8 | — | menu OPTIONS |
| Modo turbo (original, desligado por padrão) | S, só com `--turbo` / `?turbo` | — | — |

Como no original, o tiro é por toque: cada pressionamento dispara uma vez.

**Nome no ranking pelo controle:** ↑/↓ escolhe a letra, A (ou →) adiciona, B (ou ←) apaga e
Start confirma. O teclado continua funcionando normalmente; no celular aparece a caixa de texto do
navegador.

## Opções (adição do port)

O menu do título ganhou o item **OPTIONS** (entre RANKING e EXIT; os 5 itens originais não
mudam). O texto "OPTIONS" do menu é montado com as letras do próprio bitmap do menu original.

| Opção | Valores | Padrão |
|---|---|---|
| SCALE | `integer` (pixels exatos), `fit` (ajusta mantendo proporção), `stretch` (preenche a janela) | desktop: `integer`; web/celular: `fit` |
| FILTER | `nearest`, `linear` | `nearest` |
| ASPECT RATIO | `4:3` (original), `16:9` (854×480), `21:9` (1120×480) | `4:3` |
| WIDESCREEN | `expanded`: a área de jogo fica mais larga (a formação balança mais, o jogador anda mais; muda o equilíbrio do jogo). `borders`: jogo original 4:3 intacto, com o cenário estendido e escurecido nas laterais | `expanded` |
| FULLSCREEN | `on` / `off` (só no desktop; na web use o botão ⛶) | `off` |
| MUSIC VOLUME / SOUND VOLUME | 0 (desligado) a 10 | 10 |
| LANGUAGE | idiomas encontrados em `data/lang/` | `en` |

As escolhas são salvas em `~/.local/share/SaitoGames/PetWings/config.txt` (Linux) ou no
`localStorage` (web). Mudanças de aspecto/widescreen que alteram a largura da área de jogo
entram em vigor na próxima tela (por exemplo, ao voltar para o título); escala e filtro mudam
na hora.

Linha de comando (sobrepõe a configuração salva, sem gravá-la):

```sh
./build/petwings --scale=fit --filter=linear --aspect=21:9 --widescreen=borders --fullscreen --lang=en --music=6 --sfx=8
./build/petwings --help
```

Na web, os mesmos parâmetros vão na URL: `index.html?aspect=16:9&widescreen=borders`.

`--turbo` (ou `?turbo` na URL) liga a tecla **S** escondida do jogo original, que alterna a
velocidade entre normal e 4×. Sem esse parâmetro a tecla não faz nada; ele não é salvo.

## Ranking compartilhado (versão web)

No servidor, o app tem um segundo container (`petwings-api`, código em `server/server.py`, só
biblioteca padrão do Python + SQLite) e o nginx repassa `/api/` para ele. O banco fica em
`/DATA/AppData/petwings/data/ranking.sqlite3`; começa com a tabela original (SAITO GAMES).

- O jogo busca o top 8 ao abrir, ao entrar no RANKING e ao começar uma partida; ao gravar um
  recorde, envia nome, poções, episódio e fase. A tela RANKING mostra `SHARED RANKING` ou
  `OFFLINE - LOCAL SCORES`.
- Sem conexão, o recorde fica numa fila no navegador e é enviado na próxima vez que o servidor
  responder.
- O servidor valida os dados (nome A–Z/0–9/espaço/`-`/`.`, até 15 letras; poções 0–9999; fase
  válida) e limita a 8 envios por IP a cada 10 minutos (usa o IP real vindo da Cloudflare).
  Como o jogo roda no navegador, não há como impedir de vez pontuações forjadas.
- Remover uma pontuação: `ssh mojave` e
  `docker exec petwings-api python -c "import sqlite3; c=sqlite3.connect('/data/ranking.sqlite3'); c.execute(\"DELETE FROM scores WHERE name='NOME'\"); c.commit()"`
- A versão Linux mantém o ranking local.
- Testar localmente com a API: `./build.sh serve` (banco de teste em `build-web/`).

Acesso pela internet (Cloudflare Tunnel): no painel Zero Trust, crie o *public hostname*
`petwings.mojavelabs.cloud` com serviço `HTTP` → `192.168.68.151:8099`.

## Idiomas

Todos os textos novos (e os 3 textos originais do jogo) ficam em `data/lang/en.txt`, no
formato `CHAVE=texto`. Para traduzir, copie para `data/lang/<código>.txt` (ex.: `pt.txt`) e
traduza os valores; chaves ausentes caem para o inglês. Imagens podem ser substituídas por
idioma em `data/lang/<código>/gfx/<id>.bmp` (BMP 8 bits com a paleta do jogo, mesmo nome do
arquivo em `data/gfx/`), e `menu_options.bmp` substitui o item OPTIONS do menu.
As fontes do jogo só têm A–Z, 0–9, `-`, `.`, `:` e espaço.

## Fidelidade

- 640×480 com paleta de 256 cores, 25 quadros por segundo de lógica (40 ms), como o original.
- Gerador aleatório idêntico ao `rand()` do MSVC.
- Colisão por pixel, padrões de tiro, scripts dos inimigos, chefes, ranking (8 posições) e
  continue (custa 50 poções) reproduzidos a partir do binário.
- O ranking fica em `~/.local/share/SaitoGames/PetWings/ranking.txt` (Linux) ou no
  `localStorage` do navegador.
- As músicas MIDI são pré-renderizadas em OGG (o original usava o sintetizador MIDI do Windows),
  então o timbre depende do soundfont usado.

## Estrutura

```
src/engine.cpp       standard.cpp, JDib, JSprite256, JScript, JMap, JAscii/JNumber
src/game.cpp         JGame e listas (tiros, balas, inimigos, poções, explosões, eventos)
src/boss.cpp         os 5 chefes
src/title.cpp        logo, título, ranking, continue, final, entrada de nome
src/mainapp.cpp      máquina de estados principal e ranking
src/platform_sdl.cpp vídeo, áudio, entrada (teclado/gamepad/toque), armazenamento
tools/               extração de dados e scripts de engenharia reversa (Ghidra)
web/shell.html       página da versão web
```

## Testes automatizados

O binário tem um modo sem janela que joga sozinho e salva capturas:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy PETWINGS_TEST=3000 \
PETWINGS_SHOTS=/tmp/shots PETWINGS_START=1:8:250 PETWINGS_GOD=1 ./build/petwings
```

`PETWINGS_START` também aceita `title` e `options`; `PETWINGS_SHOT_COMPOSED=1` salva o quadro
final (com as laterais widescreen). Para a versão web há um teste no Firefox headless via
WebDriver BiDi (`tools/webtest.mjs`), que abre a página com `?autostart=1`, navega pelos menus e
tira capturas.
