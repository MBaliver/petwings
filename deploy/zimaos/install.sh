#!/bin/bash
# Registra o PetWings como app do ZimaOS (funciona como usuário mojave; sudo não é necessário).
# Rodar no servidor com: bash /DATA/AppData/petwings/install.sh
set -e
cd /DATA/AppData/petwings
export DOCKER_CONFIG=/DATA/AppData/ai-cli/.docker
if casaos-cli app-management list apps 2>/dev/null | grep -q '^petwings '; then
    casaos-cli app-management apply petwings -f docker-compose.yml
else
    casaos-cli app-management install -f docker-compose.yml
fi
echo "PetWings instalado: https://petwings.mojavelabs.cloud/ (direto: http://192.168.68.151:8099/)"
