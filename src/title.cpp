// title.cpp - logo, title menu, ranking, continue, ending and name entry screens,
// plus the port's OPTIONS screen. Positions are relative to the original 4:3 area
// (ScreenOX()), so everything stays centred on expanded widescreen play fields.
#include <algorithm>
#include "game.h"

static bool ConfirmPressed() {
    return DIGetKeyboardPressed(DIK_SPACE) || DIGetKeyboardPressed(DIK_RETURN) ||
           DIGetKeyboardPressed(DIK_NUMPADENTER) || DIGetJoystickButtonPressed(0);
}

// Grid of 80x80 tiles scrolling diagonally (title / ranking / continue / options screens).
// The original drew 9x7 tiles; wider screens simply get more columns.
void JScrollingBackground::DrawTiles(JSprite256* tile, unsigned ticks, Surface* s) {
    Backdrop& b = GetBackdrop();
    b.type = Backdrop::TILES; b.img = GetImage(0xbf); b.ticks = ticks;
    int cols = s->w / tile->Width() + 2;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < cols; col++) {
            int t = (int)(ticks % 80);
            tile->SetPosition(tile->Width() * col - t, tile->Height() * row - t);
            tile->Draw(s);
        }
    }
}
static JScrollingBackground g_scroller;

static void DrawFade(Surface* s, bool fadeIn, bool fadeOut, int ticks) {
    if (fadeIn) FadeOut(s, ticks << 2);
    if (fadeOut) FadeOut(s, (25 - ticks) * 4);
}

// ============================================================== JJsoftLogo
JJsoftLogo::JJsoftLogo() { logo = new JSprite256(0x135, 1); lpBackBuffer = DDGetBackBuffer(); }
JJsoftLogo::~JJsoftLogo() { delete logo; }
void JJsoftLogo::SetupLogo() {
    logo->SetTransparent(0);
    logo->SetCenterPosition(DDGetScreenWidth() / 2, 240);
    timeTicks = 0;
}
void JJsoftLogo::InLogo() {
    if (timeTicks == 5) SoundPlay(17);
    if (timeTicks > 224) GetAppData()->gameState = START_TITLE;
    timeTicks++;
    uint8_t white = PaletteMatch(0xffffff);
    FillBufferRect(lpBackBuffer, white);
    GetBackdrop().type = Backdrop::FILL;
    GetBackdrop().color = white;
    logo->Draw(lpBackBuffer);
}

// ============================================================== JTitle
// Menu (port): EPISODE 1, EPISODE 2, EPISODE 3, RANKING, OPTIONS, EXIT
static const int MENU_COUNT = 6;
static const int MENU_OPTIONS = 4, MENU_EXIT = 5;

JTitle::JTitle() {
    scriptBmpChara = new JScript(0xbe);
    bmpChara = new JPotionChara(0xbb, scriptBmpChara, JPotionChara::OTHER);
    background = new JSprite256(0xbf, 1);
    titlePetWings = new JSprite256(0xc3, 1);
    bigWing1 = new JSprite256(0xc5, 1);
    bigWing2 = new JSprite256(0xc4, 1);
    cursorLeft = new JSprite256(0xd7, 2);
    cursorRight = new JSprite256(0xd8, 2);
    menuItems = new JSprite256(0xff, 5);
    optionsItem = new JSprite256(GetOptionsMenuImage(), 1);
    ascii = new JAscii();
    lpBackBuffer = DDGetBackBuffer();
    ascii->SetSurface(lpBackBuffer);
    ascii->SetFontType(0);
}
JTitle::~JTitle() {
    delete bmpChara; delete scriptBmpChara; delete background; delete titlePetWings;
    delete bigWing1; delete bigWing2; delete cursorLeft; delete cursorRight; delete menuItems;
    delete optionsItem; delete ascii;
}

void JTitle::SetupTitle() {
    int ox = ScreenOX();
    background->SetTransparent(0);
    titlePetWings->SetCenterPosition(320 + ox, 136);
    bigWing1->SetCenterPosition(120 + ox, 116);
    bigWing2->SetCenterPosition(520 + ox, 116);
    bmpChara->SetPosition(-10 - bmpChara->Width(), 220);
    cursorLeft->SetAnimationDelay(8);
    cursorRight->SetAnimationDelay(8);
    PlayMidi(3);
    procType = FADEIN;
    procTicks = 25;
}

void JTitle::InTitle() {
    if (procType == TITLE) {
        DIUpdateInputState();
        cursorLeft->IncrementAnimation();
        cursorRight->IncrementAnimation();
        if (DIGetKeyboardPressed(DIK_UP) || DIGetJoystickDirectionPressed(8)) {
            SoundPlay(11);
            if (--selMenu < 0) selMenu = MENU_COUNT - 1;
        } else if (DIGetKeyboardPressed(DIK_DOWN) || DIGetJoystickDirectionPressed(24)) {
            SoundPlay(11);
            if (++selMenu > MENU_COUNT - 1) selMenu = 0;
        }
        if (ConfirmPressed()) {
            cursorLeft->SetFrameIndex(0);
            cursorRight->SetFrameIndex(0);
            SoundPlay(3);
            procType = FADEOUT;
            procTicks = 25;
        }
    } else if (procType == FADEIN) {
        if (procTicks < 1) procType = TITLE;
    } else if (procType == FADEOUT && procTicks < 1) {
        APPDATA* app = GetAppData();
        procType = NON;
        switch (selMenu) {
        case 0: app->startingEpisode = 1; app->startingGameLevel = 1; app->startingPotions = 0; app->gameState = START_GAME; break;
        case 1: app->startingEpisode = 2; app->startingGameLevel = 1; app->startingPotions = 50; app->gameState = START_GAME; break;
        case 2: app->startingEpisode = 3; app->startingGameLevel = 1; app->startingPotions = 100; app->gameState = START_GAME; break;
        case 3: app->gameState = START_RANKING; break;
        case MENU_OPTIONS: app->gameState = START_OPTIONS; break;
        case MENU_EXIT: app->gameState = EXIT; break;
        default: app->gameState = START_TITLE; break;
        }
    }
    timeTicks++;
    if (procTicks > 0) procTicks--;
    bmpChara->Move();
    if (procType == NON) { FadeOut(lpBackBuffer, 100); return; }
    int ox = ScreenOX();
    g_scroller.DrawTiles(background, timeTicks, lpBackBuffer);
    bigWing1->Draw(lpBackBuffer);
    bigWing2->Draw(lpBackBuffer);
    titlePetWings->Draw(lpBackBuffer);
    bmpChara->Draw(lpBackBuffer);
    int step = menuItems->Height() + 8;
    for (int i = 0; i < MENU_COUNT; i++) {
        JSprite256* item = menuItems;
        if (i == MENU_OPTIONS) item = optionsItem;
        else menuItems->SetFrameIndex(i < MENU_OPTIONS ? i : 4);
        item->SetPosition(425 + ox, step * i + 260);
        item->Draw(lpBackBuffer);
    }
    int cy = selMenu * step + menuItems->Height() / 2 + 260;
    cursorLeft->SetCenterPosition(411 + ox, cy);
    cursorRight->SetCenterPosition(menuItems->Width() + 437 + ox, cy);
    cursorLeft->Draw(lpBackBuffer);
    cursorRight->Draw(lpBackBuffer);
    ascii->DrawText(8 + ox, 460, Tr("TITLE_COPYRIGHT").c_str());
    DrawFade(lpBackBuffer, procType == FADEIN, procType == FADEOUT, procTicks);
}

// ============================================================== JRanking
JRanking::JRanking() {
    background = new JSprite256(0xbf, 1);
    textRanking = new JSprite256(0x123, 1);
    rankBack = new JSprite256(0x125, 1);
    ascii = new JAscii();
    number = new JNumber();
    lpBackBuffer = DDGetBackBuffer();
    ascii->SetSurface(lpBackBuffer);
    ascii->SetFontType(1);
    number->SetSurface(lpBackBuffer);
    number->SetFontType(0);
}
JRanking::~JRanking() { delete background; delete textRanking; delete rankBack; delete ascii; delete number; }
void JRanking::SetupRanking() {
    background->SetTransparent(0);
    textRanking->SetCenterPosition(DDGetScreenWidth() / 2, 82);
    procType = FADEIN;
    procTicks = 25;
}
void JRanking::InRanking() {
    if (procType == RANKING) {
        DIUpdateInputState();
        if (ConfirmPressed()) {
            SoundPlay(3);
            procType = FADEOUT;
            procTicks = 25;
        }
    } else if (procType == FADEIN) {
        if (procTicks < 1) procType = RANKING;
    } else if (procType == FADEOUT && procTicks < 1) {
        procType = NON;
        GetAppData()->gameState = START_TITLE;
    }
    timeTicks++;
    if (procTicks > 0) procTicks--;
    if (procType == NON) { FadeOut(lpBackBuffer, 100); return; }
    int ox = ScreenOX();
    g_scroller.DrawTiles(background, timeTicks, lpBackBuffer);
    textRanking->Draw(lpBackBuffer);
    for (int i = 0, y = 157; y < 445; i++, y += 36) {
        number->DrawNumbers(171 + ox, y + 4, i + 1, 1);
        rankBack->SetPosition(196 + ox, y);
        rankBack->Draw(lpBackBuffer);
        RANKDATA* r = GetRankData(i);
        ascii->DrawText(213 + ox, y + 7, r->name);
        number->DrawNumbers(394 + ox, y + 5, (int)r->potions, 4);
    }
    // port: where the table comes from (web build: shared server ranking)
    RankingStatus rs = PlatformRankingStatus();
    if (rs != RANKING_LOCAL) {
        std::string t = Tr(rs == RANKING_ONLINE ? "RANKING_SHARED" : "RANKING_OFFLINE");
        ascii->DrawText((DDGetScreenWidth() - ascii->TextWidth(t.c_str())) / 2, 452, t.c_str());
    }
    DrawFade(lpBackBuffer, procType == FADEIN, procType == FADEOUT, procTicks);
}

// ============================================================== JContinue
JContinue::JContinue() {
    background = new JSprite256(0xbf, 1);
    bmpChara = new JSprite256(0xd3, 1);
    textContinue = new JSprite256(0xd4, 1);
    textYes = new JSprite256(0xd1, 1);
    textNo = new JSprite256(0xd2, 1);
    cursorLeft = new JSprite256(0xd7, 2);
    cursorRight = new JSprite256(0xd8, 2);
    ascii = new JAscii();
    lpBackBuffer = DDGetBackBuffer();
    ascii->SetSurface(lpBackBuffer);
    ascii->SetFontType(1);
}
JContinue::~JContinue() {
    delete background; delete bmpChara; delete textContinue; delete textYes; delete textNo;
    delete cursorLeft; delete cursorRight; delete ascii;
}
void JContinue::SetupContinue() {
    int ox = ScreenOX();
    background->SetTransparent(0);
    bmpChara->SetTransparent(0);
    bmpChara->SetCenterPosition(320 + ox, 254);
    textContinue->SetCenterPosition(320 + ox, 53);
    textYes->SetCenterPosition(240 + ox, 425);
    textNo->SetCenterPosition(400 + ox, 425);
    cursorLeft->SetAnimationDelay(8);
    cursorRight->SetAnimationDelay(8);
    procType = FADEIN;
    procTicks = 25;
}
void JContinue::InContinue() {
    if (procType == CONTINUE) {
        DIUpdateInputState();
        cursorLeft->IncrementAnimation();
        cursorRight->IncrementAnimation();
        if (DIGetKeyboardPressed(DIK_LEFT) || DIGetJoystickDirectionPressed(16)) {
            if (selMenu != 0) { SoundPlay(11); selMenu = 0; }
        } else if (DIGetKeyboardPressed(DIK_RIGHT) || DIGetJoystickDirectionPressed(0)) {
            if (selMenu != 1) { SoundPlay(11); selMenu = 1; }
        } else if (ConfirmPressed()) {
            cursorLeft->SetFrameIndex(0);
            cursorRight->SetFrameIndex(0);
            SoundPlay(3);
            procType = FADEOUT;
            procTicks = 25;
        }
    } else if (procType == FADEIN) {
        if (procTicks < 1) { SoundPlay(12); procType = CONTINUE; }
    } else if (procType == FADEOUT && procTicks < 1) {
        procType = NON;
        GetAppData()->gameState = selMenu == 0 ? START_GAME : START_TITLE;
    }
    timeTicks++;
    if (procTicks > 0) procTicks--;
    if (procType == NON) { FadeOut(lpBackBuffer, 100); return; }
    g_scroller.DrawTiles(background, timeTicks, lpBackBuffer);
    bmpChara->Draw(lpBackBuffer);
    textContinue->Draw(lpBackBuffer);
    textYes->Draw(lpBackBuffer);
    textNo->Draw(lpBackBuffer);
    JSprite256* sel = selMenu == 0 ? textYes : textNo;
    int cy = sel->GetCenterY();
    cursorLeft->SetCenterPosition(sel->GetIntX() - 14, cy);
    cursorRight->SetCenterPosition(sel->Width() + sel->GetIntX() + 13, cy);
    cursorLeft->Draw(lpBackBuffer);
    cursorRight->Draw(lpBackBuffer);
    ascii->DrawText(148 + ScreenOX(), 450, Tr("CONTINUE_COST").c_str());
    DrawFade(lpBackBuffer, procType == FADEIN, procType == FADEOUT, procTicks);
}

// ============================================================== JEnding
JEnding::JEnding() {
    scriptChara = new JScript(299);
    chara = new JPotionChara(0x129, scriptChara, JPotionChara::OTHER);
    background = new JSprite256(0x127, 1);
    textTheEnd = new JSprite256(0x12a, 1);
    textStuff = new JSprite256(0x134, 1);
    lpBackBuffer = DDGetBackBuffer();
}
JEnding::~JEnding() { delete chara; delete scriptChara; delete background; delete textTheEnd; delete textStuff; }
void JEnding::SetupEnding() {
    int cx = DDGetScreenWidth() / 2;
    background->SetTransparent(0);
    background->SetCenterPosition(cx, 240);
    textTheEnd->SetCenterPosition(cx, 60);
    textStuff->SetCenterPosition(cx, 435);
    chara->SetCenterPosition(cx, 240);
    PlayMidi(1);
    procType = FADEIN;
    procTicks = 25;
}
void JEnding::InEnding() {
    if (procType == ENDING) {
        DIUpdateInputState();
        if (ConfirmPressed()) {
            SoundPlay(3);
            procType = FADEOUT;
            procTicks = 25;
        }
    } else if (procType == FADEIN) {
        if (procTicks < 1) procType = ENDING;
    } else if (procType == FADEOUT && procTicks < 1) {
        procType = NON;
        GetAppData()->gameState = START_TITLE;
    }
    if (procTicks > 0) procTicks--;
    chara->Move();
    if (procType == NON) { FadeOut(lpBackBuffer, 100); return; }
    FillBufferRect(lpBackBuffer, 0);
    DrawWideImage(GetImage(0x127), lpBackBuffer, background->GetIntY());
    textTheEnd->Draw(lpBackBuffer);
    textStuff->Draw(lpBackBuffer);
    chara->Draw(lpBackBuffer);
    DrawFade(lpBackBuffer, procType == FADEIN, procType == FADEOUT, procTicks);
}

// ============================================================== JInputText
JInputText::JInputText() {
    cursor = new JSprite256(0x113, 2);
    inputBox1 = new JSprite256(0x11e, 1);
    inputBox2 = new JSprite256(0x11d, 1);
    inputBox3 = new JSprite256(0x11c, 1);
    winFrame1 = new JSprite256(0x121, 1);
    winFrame2 = new JSprite256(0x120, 1);
    winFrame3 = new JSprite256(0x11f, 1);
    ascii = new JAscii();
    ascii->SetFontType(0);
    cursor->SetAnimationDelay(8);
    for (JSprite256* s : {inputBox1, inputBox2, inputBox3, winFrame1, winFrame2, winFrame3}) s->SetTransparent(0);
}
JInputText::~JInputText() {
    delete cursor; delete inputBox1; delete inputBox2; delete inputBox3;
    delete winFrame1; delete winFrame2; delete winFrame3; delete ascii;
}
void JInputText::SetCenterOfScreen() {
    int len = (int)msg.size();
    x = (DDGetScreenWidth() - (len * winFrame2->Width() + winFrame3->Width() + winFrame1->Width())) / 2;
    y = (SCREEN_H - winFrame1->Height()) / 2;
}
void JInputText::SetText(const std::string& t, int lim) {
    text = (int)t.size() > lim ? t.substr(0, lim) : t;
    limit = lim;
}
static const char PICKER[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.";
static const int PICKER_N = (int)sizeof(PICKER) - 1;

void JInputText::StartInput() {
    procType = INPUT;
    padActive = false;
    padChar = 1;
    DIClearKeyboardBuffer();
    GetAppData()->keyCommandEnabled = 0;
    PlatformRequestTextEntry(limit);
}
void JInputText::InInput() {
    if (procType == INPUT) {
        cursor->IncrementAnimation();
        std::string buf;
        DIGetKeyboardBuffer(buf);
        // gamepad letter picker
        DIUpdateInputState();
        blink++;
        if (DIGetJoystickDirectionPressed(8)) { padActive = true; padChar = (padChar + 1) % PICKER_N; SoundPlay(11); }
        else if (DIGetJoystickDirectionPressed(24)) { padActive = true; padChar = (padChar + PICKER_N - 1) % PICKER_N; SoundPlay(11); }
        if (DIGetJoystickButtonPressed(2)) {
            buf += '\n';
        } else if (DIGetJoystickButtonPressed(1) || DIGetJoystickDirectionPressed(16)) {
            padActive = true;
            buf += '\b';
        } else if (DIGetJoystickButtonPressed(0) || DIGetJoystickDirectionPressed(0)) {
            padActive = true;
            buf += PICKER[padChar];
        }
        for (char c : buf) {
            if ((int)text.size() < limit &&
                ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == ' '))
                text += c;
            if (c == '\b') {
                if (text.size() < 2) text.clear();
                else text.pop_back();
            } else if (c == '\n') {
                SoundPlay(3);
                procType = DONE;
                GetAppData()->keyCommandEnabled = 1;
                break;
            }
        }
    }
    DrawScreen();
}
void JInputText::DrawScreen() {
    if (!lpSurface) return;
    FadeOut(lpSurface, 100);
    if (procType != INPUT) return;
    int px = x;
    winFrame1->SetPosition(px, y); winFrame1->Draw(lpSurface); px += winFrame1->Width();
    for (size_t i = 0; i < msg.size(); i++) {
        winFrame2->SetPosition(px, y); winFrame2->Draw(lpSurface); px += winFrame2->Width();
    }
    winFrame3->SetPosition(px, y); winFrame3->Draw(lpSurface);
    px = winFrame1->Width() + x;
    inputBox1->SetPosition(px, y + 37); inputBox1->Draw(lpSurface); px += inputBox1->Width();
    for (int i = 0; i < limit + 1; i++) {
        inputBox2->SetPosition(px, y + 37); inputBox2->Draw(lpSurface); px += inputBox2->Width();
    }
    inputBox3->SetPosition(px, y + 37); inputBox3->Draw(lpSurface);
    ascii->DrawText(winFrame1->Width() + x, y + 14, msg.c_str());
    int cx = ascii->DrawText(x + winFrame1->Width() + inputBox1->Width(), y + 41, text.c_str());
    if (padActive && (int)text.size() < limit) {  // letter being picked, blinking
        char cand[2] = {PICKER[padChar], 0};
        if ((blink / 8) % 2 == 0) ascii->DrawText(cx, y + 41, cand);
        cx += ascii->TextWidth(cand);
    }
    cursor->SetPosition(cx, y + 41);
    cursor->Draw(lpSurface);
    if (padActive || PlatformHasGamepad()) {
        std::string hint = Tr("NAME_PAD_HINT");
        ascii->DrawText((DDGetScreenWidth() - ascii->TextWidth(hint.c_str())) / 2, y + winFrame1->Height() + 24, hint.c_str());
    }
}

// ============================================================== JOptions (port)
JOptions::JOptions() {
    background = new JSprite256(0xbf, 1);
    heading = new JSprite256(GetOptionsMenuImage(), 1);
    cursorLeft = new JSprite256(0xd7, 2);
    cursorRight = new JSprite256(0xd8, 2);
    fontRed = new JAscii();
    fontBlue = new JAscii();
    lpBackBuffer = DDGetBackBuffer();
    fontRed->SetSurface(lpBackBuffer);
    fontRed->SetFontType(1);
    fontBlue->SetSurface(lpBackBuffer);
    fontBlue->SetFontType(0);
    rows = {ROW_SCALE, ROW_FILTER, ROW_ASPECT, ROW_WIDE};
    if (PlatformHasFullscreenOption()) rows.push_back(ROW_FULLSCREEN);
    rows.push_back(ROW_MUSIC);
    rows.push_back(ROW_SFX);
    rows.push_back(ROW_LANGUAGE);
    rows.push_back(ROW_BACK);
}
JOptions::~JOptions() {
    delete background; delete heading; delete cursorLeft; delete cursorRight; delete fontRed; delete fontBlue;
}
void JOptions::SetupOptions() {
    background->SetTransparent(0);
    heading->SetCenterPosition(DDGetScreenWidth() / 2, 72);
    cursorLeft->SetAnimationDelay(8);
    cursorRight->SetAnimationDelay(8);
    procType = FADEIN;
    procTicks = 25;
}

static int Cycle(int v, int delta, int count) { return ((v + delta) % count + count) % count; }

void JOptions::Change(int row, int delta) {
    VideoConfig& c = GetConfig();
    switch (row) {
    case ROW_SCALE: c.scale = Cycle(c.scale, delta, SCALE_COUNT); break;
    case ROW_FILTER: c.filter = Cycle(c.filter, delta, FILTER_COUNT); break;
    case ROW_ASPECT: c.aspect = Cycle(c.aspect, delta, ASPECT_COUNT); break;
    case ROW_WIDE: c.wide = Cycle(c.wide, delta, WIDE_COUNT); break;
    case ROW_FULLSCREEN: c.fullscreen = !c.fullscreen; break;
    case ROW_MUSIC: c.musicVolume = delta > 1 ? (c.musicVolume + 1) % 11 : std::max(0, std::min(10, c.musicVolume + delta)); break;
    case ROW_SFX: c.sfxVolume = delta > 1 ? (c.sfxVolume + 1) % 11 : std::max(0, std::min(10, c.sfxVolume + delta)); break;
    case ROW_LANGUAGE: {
        std::vector<std::string> langs = ListLanguages();
        if (langs.empty()) return;
        int i = 0;
        for (int k = 0; k < (int)langs.size(); k++) if (langs[k] == c.language) i = k;
        c.language = langs[Cycle(i, delta, (int)langs.size())];
        LangLoad(c.language);
        break;
    }
    default: return;
    }
    SoundPlay(11);
    ConfigChanged();
}

std::string JOptions::ValueText(int row) {
    const VideoConfig& c = GetConfig();
    static const char* scale[] = {"VAL_SCALE_INTEGER", "VAL_SCALE_FIT", "VAL_SCALE_STRETCH"};
    static const char* filter[] = {"VAL_FILTER_NEAREST", "VAL_FILTER_LINEAR"};
    static const char* aspect[] = {"VAL_ASPECT_4_3", "VAL_ASPECT_16_9", "VAL_ASPECT_21_9"};
    static const char* wide[] = {"VAL_WIDE_EXPANDED", "VAL_WIDE_BORDERS"};
    switch (row) {
    case ROW_SCALE: return Tr(scale[c.scale]);
    case ROW_FILTER: return Tr(filter[c.filter]);
    case ROW_ASPECT: return Tr(aspect[c.aspect]);
    case ROW_WIDE: return Tr(wide[c.wide]);
    case ROW_FULLSCREEN: return Tr(c.fullscreen ? "VAL_ON" : "VAL_OFF");
    case ROW_MUSIC: return c.musicVolume ? std::to_string(c.musicVolume) : Tr("VAL_OFF");
    case ROW_SFX: return c.sfxVolume ? std::to_string(c.sfxVolume) : Tr("VAL_OFF");
    case ROW_LANGUAGE: return LangDisplayName(c.language);
    default: return "";
    }
}

void JOptions::InOptions() {
    if (procType == OPTIONS) {
        DIUpdateInputState();
        cursorLeft->IncrementAnimation();
        cursorRight->IncrementAnimation();
        int n = (int)rows.size();
        if (DIGetKeyboardPressed(DIK_UP) || DIGetJoystickDirectionPressed(8)) {
            SoundPlay(11);
            sel = Cycle(sel, -1, n);
        } else if (DIGetKeyboardPressed(DIK_DOWN) || DIGetJoystickDirectionPressed(24)) {
            SoundPlay(11);
            sel = Cycle(sel, 1, n);
        } else if (DIGetKeyboardPressed(DIK_LEFT) || DIGetJoystickDirectionPressed(16)) {
            Change(rows[sel], -1);
        } else if (DIGetKeyboardPressed(DIK_RIGHT) || DIGetJoystickDirectionPressed(0)) {
            Change(rows[sel], 1);
        } else if (ConfirmPressed()) {
            if (rows[sel] == ROW_BACK) {
                SoundPlay(3);
                procType = FADEOUT;
                procTicks = 25;
            } else {
                Change(rows[sel], rows[sel] == ROW_MUSIC || rows[sel] == ROW_SFX ? 2 : 1);
            }
        }
    } else if (procType == FADEIN) {
        if (procTicks < 1) procType = OPTIONS;
    } else if (procType == FADEOUT && procTicks < 1) {
        procType = NON;
        GetAppData()->gameState = START_TITLE;  // new video/language settings take effect here
    }
    timeTicks++;
    if (procTicks > 0) procTicks--;
    if (procType == NON) { FadeOut(lpBackBuffer, 100); return; }

    static const char* labels[] = {"OPT_SCALE", "OPT_FILTER", "OPT_ASPECT", "OPT_WIDESCREEN",
                                   "OPT_FULLSCREEN", "OPT_MUSIC", "OPT_SFX", "OPT_LANGUAGE", "OPT_BACK"};
    const int ox = ScreenOX(), cx = DDGetScreenWidth() / 2;
    g_scroller.DrawTiles(background, timeTicks, lpBackBuffer);
    heading->Draw(lpBackBuffer);
    int y0 = 128, step = 32;
    for (int i = 0; i < (int)rows.size(); i++) {
        int y = y0 + i * step;
        int row = rows[i];
        JAscii* labelFont = i == sel ? fontRed : fontBlue;
        JAscii* valueFont = labelFont;
        std::string label = Tr(labels[row]);
        int vx0, vx1;
        if (row == ROW_BACK) {
            vx0 = cx - labelFont->TextWidth(label.c_str()) / 2;
            vx1 = labelFont->DrawText(vx0, y + step / 2, label.c_str());
        } else {
            labelFont->DrawText(110 + ox, y, label.c_str());
            std::string v = ValueText(row);
            vx0 = 450 + ox - valueFont->TextWidth(v.c_str()) / 2;
            vx1 = valueFont->DrawText(vx0, y, v.c_str());
        }
        if (i == sel) {
            int cy = (row == ROW_BACK ? y + step / 2 : y) + fontRed->GlyphHeight() / 2;
            cursorLeft->SetCenterPosition(vx0 - 14, cy);
            cursorRight->SetCenterPosition(vx1 + 13, cy);
            cursorLeft->Draw(lpBackBuffer);
            cursorRight->Draw(lpBackBuffer);
        }
    }
    std::string hint = Tr("OPTIONS_HINT");
    fontBlue->DrawText(cx - fontBlue->TextWidth(hint.c_str()) / 2, 448, hint.c_str());
    DrawFade(lpBackBuffer, procType == FADEIN, procType == FADEOUT, procTicks);
}
