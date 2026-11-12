#!/bin/sh
# PetWings source port - build helper
#   ./build.sh data <dir-with-PetWings.exe>   extract game data into ./data
#   ./build.sh native                          Linux binary in build/petwings
#   ./build.sh web                             browser build in build-web/ (index.html + .js/.wasm/.data)
#   ./build.sh serve                           serve build-web (+ ranking API) on http://localhost:8080
set -e
cd "$(dirname "$0")"

case "$1" in
  data)
    [ -n "$2" ] || { echo "usage: $0 data <dir containing PetWings.exe and Music0N.mid>"; exit 1; }
    python3 -I tools/prepare_data.py "$2" data
    ;;
  native)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    echo "Run: ./build/petwings"
    ;;
  web)
    [ -f data/gfx/106.bmp ] || { echo "Run '$0 data <dir>' first."; exit 1; }
    EMCMAKE=$(command -v emcmake || echo /usr/lib/emscripten/emcmake)
    "$EMCMAKE" cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
    cmake --build build-web -j
    echo "Output in build-web/ (index.html, index.js, index.wasm, index.data)"
    ;;
  deploy)
    # copia a build web e a definição do app para o ZimaOS (sem root); depois rode lá:
    #   bash /DATA/AppData/petwings/install.sh   (só na primeira vez ou se o compose mudar)
    HOST=${2:-mojave}
    [ -f build-web/index.wasm ] || { echo "Run '$0 web' first."; exit 1; }
    ssh "$HOST" 'mkdir -p /DATA/AppData/petwings/www.new'
    tar -C build-web -cf - index.html index.js index.wasm index.data manifest.webmanifest \
        icon-32.png icon-180.png icon-192.png icon-512.png og-image.png | ssh "$HOST" 'tar -C /DATA/AppData/petwings/www.new -xf -'
    tar -C deploy/zimaos -cf - docker-compose.yml nginx.conf install.sh | ssh "$HOST" 'mkdir -p /DATA/AppData/petwings/conf.new && tar -C /DATA/AppData/petwings/conf.new -xf -'
    # ranking server code (read-only mount) and its database folder
    ssh "$HOST" 'mkdir -p /DATA/AppData/petwings/api /DATA/AppData/petwings/data'
    ssh "$HOST" 'cat > /DATA/AppData/petwings/api/server.py' < server/server.py
    # www/ e nginx.conf são montados no container: a pasta e o arquivo têm de continuar sendo
    # os mesmos (trocar a pasta inteira deixa o container apontando para a antiga, apagada).
    # Os arquivos dentro de www/ são trocados um a um com mv (atômico), index.html por último.
    ssh "$HOST" 'set -e; cd /DATA/AppData/petwings; mkdir -p www
        for f in icon-32.png icon-180.png icon-192.png icon-512.png og-image.png manifest.webmanifest index.data index.wasm index.js index.html; do
            mv -f "www.new/$f" "www/$f"; done
        rmdir www.new
        reload=0; cmp -s conf.new/nginx.conf nginx.conf 2>/dev/null || reload=1
        for f in docker-compose.yml nginx.conf install.sh; do cat "conf.new/$f" > "$f"; done
        chmod +x install.sh; rm -rf conf.new
        if [ $reload = 1 ] && docker ps -q --filter name=^petwings$ 2>/dev/null | grep -q .; then
            docker exec petwings nginx -s reload 2>/dev/null && echo "nginx recarregado"; fi
        docker restart petwings-api >/dev/null 2>&1 || true'
    echo "Enviado para $HOST:/DATA/AppData/petwings"
    ;;
  serve)
    # páginas + API do ranking num só processo (o banco de teste fica em build-web/ranking-dev.sqlite3)
    PETWINGS_STATIC=build-web PETWINGS_PORT=8080 PETWINGS_DB=build-web/ranking-dev.sqlite3 python3 -I server/server.py
    ;;
  *)
    sed -n '2,6p' "$0"
    exit 1
    ;;
esac
