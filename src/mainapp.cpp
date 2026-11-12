// mainapp.cpp - global state machine (GameProcess) and high-score table.
#include <cstdlib>
#include <cstring>
#include <sstream>
#include "game.h"

static APPDATA appData;
static RANKDATA rankList[8];
static JJsoftLogo* logo = nullptr;
static JTitle* title = nullptr;
static JRanking* rank = nullptr;
static JContinue* cont = nullptr;
static JEnding* ending = nullptr;
static JGame* game = nullptr;
static JOptions* options = nullptr;

APPDATA* GetAppData() { return &appData; }
JGame* GetCurrentGame() { return game; }
RANKDATA* GetRankData(int i) { return &rankList[i]; }

static void SetDefaultRanking() {
    for (int i = 0; i < 8; i++) {
        rankList[i].potions = 100 - i * 10;
        memset(rankList[i].name, 0, sizeof rankList[i].name);
        strncpy(rankList[i].name, "SAITO GAMES", 15);
    }
}

// "potions,name" per line (local cache file / localStorage, or the shared ranking from the server)
void ApplyRankingText(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    for (int i = 0; i < 8 && std::getline(in, line); i++) {
        size_t comma = line.find(',');
        if (comma == std::string::npos || comma == 0) continue;
        long p = atol(line.substr(0, comma).c_str());
        rankList[i].potions = p > 9999 ? 0 : p;
        std::string name = line.substr(comma + 1);
        size_t a = name.find_first_not_of(' '), b = name.find_last_not_of(" \r");
        name = a == std::string::npos ? "" : name.substr(a, b - a + 1);
        memset(rankList[i].name, 0, sizeof rankList[i].name);
        strncpy(rankList[i].name, name.c_str(), 15);
    }
}

std::string RankingText() {
    std::string out;
    for (auto& r : rankList) out += std::to_string(r.potions) + "," + r.name + "\n";
    return out;
}

void LoadRankList() {
    SetDefaultRanking();
    ApplyRankingText(StorageLoad("ranking"));
    PlatformRankingRefresh();  // port: shared ranking on the web build
}

void SaveRankList() { StorageSave("ranking", RankingText()); }

static void ClearScreen() { FillBufferRect(DDGetBackBuffer(), 0); }

void GameProcess() {
    ResetFrameBackdrop();
    switch (appData.gameState) {
    case START_LOGO: case START_TITLE: case START_RANKING: case START_CONTINUE:
    case START_ENDING: case START_GAME: case START_OPTIONS:
        ApplyPendingVideo();  // port: logical screen width may only change between screens
        break;
    default: break;
    }
    switch (appData.gameState) {
    case START_LOGO:
        ClearScreen();
        logo = new JJsoftLogo();
        logo->SetupLogo();
        appData.gameState = IN_LOGO;
        break;
    case IN_LOGO: logo->InLogo(); break;
    case START_TITLE:
        SafeDelete(logo); SafeDelete(game); SafeDelete(title);
        SafeDelete(rank); SafeDelete(cont); SafeDelete(ending); SafeDelete(options);
        ClearScreen();
        title = new JTitle();
        title->SetupTitle();
        appData.gameState = IN_TITLE;
        break;
    case IN_TITLE: title->InTitle(); break;
    case START_RANKING:
        PlatformRankingRefresh();
        SafeDelete(title);
        ClearScreen();
        rank = new JRanking();
        rank->SetupRanking();
        appData.gameState = IN_RANKING;
        break;
    case IN_RANKING: rank->InRanking(); break;
    case START_OPTIONS:
        SafeDelete(title);
        ClearScreen();
        options = new JOptions();
        options->SetupOptions();
        appData.gameState = IN_OPTIONS;
        break;
    case IN_OPTIONS: options->InOptions(); break;
    case START_CONTINUE:
        SafeDelete(game);
        ClearScreen();
        cont = new JContinue();
        cont->SetupContinue();
        appData.gameState = IN_CONTINUE;
        break;
    case IN_CONTINUE: cont->InContinue(); break;
    case START_ENDING:
        SafeDelete(title); SafeDelete(game);
        ClearScreen();
        ending = new JEnding();
        ending->SetupEnding();
        appData.gameState = IN_ENDING;
        break;
    case IN_ENDING: ending->InEnding(); break;
    case START_GAME:
        PlatformRankingRefresh();  // so "is this a top-8 score?" uses the current shared table
        SafeDelete(title); SafeDelete(cont);
        ClearScreen();
        game = new JGame();
        game->SetupEpisode(appData.startingEpisode);
        game->SetupLevel(appData.startingGameLevel);
        game->SetupPotions(appData.startingPotions);
        appData.gameState = IN_GAME;
        break;
    case IN_GAME: game->InGame(); break;
    case EXIT:
        if (PlatformIsWeb()) {  // a web page cannot close itself: go back to the logo
            SafeDelete(title);
            StopMidi();
            appData.gameState = START_LOGO;
        } else {
            PlatformQuit();
        }
        return;
    default: break;
    }
    appData.frameCount++;
}

void AppShutdown() {
    StopMidi();
    SafeDelete(logo); SafeDelete(title); SafeDelete(rank);
    SafeDelete(cont); SafeDelete(ending); SafeDelete(game); SafeDelete(options);
}
