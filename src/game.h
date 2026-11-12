// PetWings source port - game classes.
// Reconstructed from the original binary (PetWings 1.3, Saito Games) using its debug
// symbols; class, member and method names follow the original sources.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "platform.h"

template <class T> inline void SafeDelete(T*& p) { delete p; p = nullptr; }

// ---------------------------------------------------------------- standard.cpp
void InitSinCosTables32();
float Sin32(int dir);
float Cos32(int dir);
int FindDirection32(int x1, int y1, int x2, int y2);
long Random(long n);
void Randomize();
long Round(double v);
inline long Max(long a, long b) { return a > b ? a : b; }

// ---------------------------------------------------------------- JDib / JSprite
struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px;  // top-down, w*h
};
// Loads gfx/<id>.bmp; data/lang/<language>/gfx/<id>.bmp takes precedence when present.
Image* GetImage(int resId);
// Optional port-specific image by name (lang folder first, then gfx/); nullptr if absent.
Image* GetNamedImage(const std::string& name);
// "OPTIONS" menu entry in the style of the original menu bitmap (letters taken from it).
Image* GetOptionsMenuImage();
// Draws a 640-wide picture centred on the surface, mirroring its edges into wider screens.
void DrawWideImage(Image* img, Surface* s, int y);

// ---------------------------------------------------------------- language table (port)
// data/lang/en.txt is always loaded first; data/lang/<lang>.txt overrides it. Format: KEY=text
void LangLoad(const std::string& lang);
const std::string& Tr(const std::string& key);
std::string LangDisplayName(const std::string& lang);

// ---------------------------------------------------------------- backdrop (port)
// What the current screen uses as background, so the "borders" widescreen mode can extend it.
struct Backdrop {
    enum Type { BLACK, FILL, IMAGE, TILES } type = BLACK;
    uint8_t color = 0;
    Image* img = nullptr;  // IMAGE: 640-wide picture; TILES: the tile
    int y = 0;             // IMAGE: vertical position
    unsigned ticks = 0;    // TILES: scroll position
};
Backdrop& GetBackdrop();
int GetFadeBand();          // rows covered by FadeOut curtains this frame (top and bottom)
void ResetFrameBackdrop();
void NoteFadeBand(int band);

class JSprite256 {
public:
    JSprite256(int resId, int numFrames);
    JSprite256(Image* img, int numFrames);
    virtual ~JSprite256() {}
    virtual int GetIntX();
    virtual int GetIntY();
    virtual void Move();

    void Draw(Surface* s);
    void DrawAtPos(int x, int y, Surface* s);
    int Collision_Full(JSprite256* o);
    int Collision_Rect(JSprite256* o);
    int OutOfScreen_Full();
    int OutOfScreen_Partial();
    void IncrementAnimation();
    void IncrementFrame() { frameIndex = (frameIndex + 1) % numFrames; }
    int AnimationFirstTime() const { return animFirstTime; }
    int GetCenterX() { return width / 2 + GetIntX(); }
    int GetCenterY() { return height / 2 + GetIntY(); }
    void SetCenterPosition(int x, int y) {
        posX = (float)x - (float)(width / 2);
        posY = (float)y - (float)(height / 2);
    }
    void SetPosition(int x, int y) { posX = (float)x; posY = (float)y; }
    void SetPosX(int x) { posX = (float)x; }
    void SetPosY(int y) { posY = (float)y; }
    void ShiftPosition(int dx, int dy) { posX = (float)dx + posX; posY = (float)dy + posY; }
    void SetAnimationDelay(int d) { animDelay = d; animIndex = 0; }
    void SetDirection(int d) { direct = d; }
    int GetDirection() const { return direct; }
    void SetFrameIndex(int f) { frameIndex = f; }
    int GetFrameIndex() const { return frameIndex; }
    void SetSpeed(float s) { speed = s; }
    float GetSpeed() const { return speed; }
    void SetTransparent(int on) { transOn = on; }
    int Width() const { return width; }
    int Height() const { return height; }

protected:
    void SetupSprite(Image* img);
    Image* img = nullptr;
    int numFrames;
    uint8_t transColor = 0;
    float posX = 0, posY = 0, speed = 1.0f;
    int direct = -1;
    int width = 0, height = 0;
    int frameIndex = 0, animDelay = 0, animIndex = 0;
    int transOn = 1, animFirstTime = 1;
};

// ---------------------------------------------------------------- JScript
enum ACTION {
    UNKNOWN = 0, MOVE, STOP, SHIFT_X, SHIFT_Y, SPEED, SPEED_PERCENT, DIRECTION,
    DIRECTION_TO_MYCHARA, DIRECTION_REVERSE, ROTATE_LEFT, ROTATE_RIGHT, LABEL, GOTO_LABEL,
    SET_FRAME, INCR_FRAME, REPEAT, REPEAT_END, SHOOT, SET_BULLET_OFFSET, PLAY_SOUND, GOSUB, RETURN
};
struct JScriptItem { int action = 0; long value1 = 0; long value2 = 0; };

class JScript {
public:
    explicit JScript(int resId);
    unsigned long Count() const { return (unsigned long)items.size(); }
    const JScriptItem* GetScriptItem(unsigned long i) const { return &items[i]; }
private:
    std::vector<JScriptItem> items;
};
std::vector<std::string> GetStringListFromResource(int resId);

// ---------------------------------------------------------------- JMap
class JMap {
public:
    void Reset(int episode);
    void SetGameLevel(int l) { gameLevel = l; }
    char GetMapData(int x, int y);
private:
    int gameLevel = 0;
    std::vector<std::string> mapList;
};

// ---------------------------------------------------------------- JAscii / JNumber
class JAscii {
public:
    JAscii();
    ~JAscii();
    void SetSurface(Surface* s) { lpSurface = s; }
    void SetFontType(int t) { fontType = t; }
    int DrawText(int x, int y, const char* text);
    int TextWidth(const char* text);
    int GlyphHeight() const { return asciiSet1->Height(); }
private:
    int fontType = 0;
    JSprite256* asciiSet1;
    JSprite256* asciiSet2;
    Surface* lpSurface = nullptr;
};

class JNumber {
public:
    JNumber();
    ~JNumber();
    void SetSurface(Surface* s) { lpSurface = s; }
    void SetFontType(int t) { fontType = t; }
    int DrawNumbers(int x, int y, int value, int digits);
    int DrawDash(int x, int y);
private:
    int fontType = 0;
    JSprite256* numberSet1;
    Surface* lpSurface = nullptr;
};

// ---------------------------------------------------------------- characters
class JGame;
class JMyChara;
class JBulletList;
class JExplodeList;

class JBulletChara : public JSprite256 {
public:
    enum DRAWSTATE { FRONT = 0, BACK = 1 };
    JBulletChara(Image* img, int frames) : JSprite256(img, frames) { SetAnimationDelay(1); }
    DRAWSTATE drawState = BACK;
};

class JFireChara : public JSprite256 {
public:
    explicit JFireChara(Image* img) : JSprite256(img, 2) { SetSpeed(24.0f); SetAnimationDelay(1); }
    int firePower = 1;
};

class JEnemyChara : public JSprite256 {
public:
    enum STATE { WALK = 0, ATTACK = 1 };
    JEnemyChara(int resId, JScript* script, int frames, STATE st);
    void MoveScript(JBulletList* bullets, JMyChara* my);  // JEnemyChara::Move
    void DecreaseShield(int v) { shield -= v; if (shield < 0) shield = 0; }
    unsigned short bulletRate = 0;
    unsigned long timeToAttack = 0;
    int shield = 0;
    STATE state;
    JScript* moveScript;
    int currentAction = UNKNOWN;
    unsigned long scriptIndex = 0;
    long currentValue1 = 0, currentValue2 = 0;
    unsigned long repeatIndex = 0;
    long repeatValue = 0;
    long gosubIndex = -1;
    int bulletOffsetX = 0, bulletOffsetY = 0;
};

class JEnemySubChara : public JEnemyChara {
public:
    JEnemySubChara(int resId, JScript* script, int frames) : JEnemyChara(resId, script, frames, ATTACK) {}
    int GetIntX() override { return JSprite256::GetIntX() + offsetX; }
    int GetIntY() override { return JSprite256::GetIntY() + offsetY; }
    void SetOffset(int x, int y) { offsetX = x; offsetY = y; }
    int offsetX = 0, offsetY = 0;
};

class JPotionChara : public JSprite256 {
public:
    enum PotionType { POTION = 0, HEART = 1, OTHER = 2 };
    JPotionChara(int resId, JScript* script, PotionType t) : JSprite256(resId, 1), type(t), moveScript(script) {}
    JPotionChara(Image* img, JScript* script, PotionType t) : JSprite256(img, 1), type(t), moveScript(script) {}
    void Move() override;
    PotionType type;
    JScript* moveScript;
    unsigned long scriptIndex = 0;
    int currentAction = UNKNOWN;
    long currentValue = 0;
    unsigned long repeatIndex = 0;
    long repeatValue = 0;
};

class JMyChara : public JSprite256 {
public:
    enum STATE { NORMAL = 0, FLASH = 1, DEAD = 2 };
    explicit JMyChara(JGame* g);
    ~JMyChara() override;
    void Move() override;
    void DecreaseLife(JExplodeList* expl);
    void Reset();
    JGame* game;
    int fireLevel = 0, potionCount = 0, flashTimeOut = 0, life = 5;
    STATE state = NORMAL;
    JSprite256* myCharaFlash;
};

// ---------------------------------------------------------------- lists
class JFireList {
public:
    JFireList();
    ~JFireList();
    int GetTotalNull();
    int GetNullIndex();
    void CreateFire(int x, int y, int level);
    JFireChara* NewFire(Image* img, int dir);
    Image* fireDib1; Image* fireDib2;
    JFireChara* fireChara[12] = {};
};

class JBulletList {
public:
    JBulletList();
    ~JBulletList();
    int GetTotalNull();
    int GetNullIndex();
    void CreateBullet(int x, int y, int type, JMyChara* my);
    Image *bulletDib1, *bulletDib2, *bulletDib2top, *bulletDib2btm, *bulletDib3, *bulletDib4,
          *bulletDib5up, *bulletDib5down, *bulletDib6;
    JBulletChara* bulletChara[60] = {};
private:
    JBulletChara* Spawn(Image* img, int frames, float speed, int dir, int x, int y, bool front);
};

class JExplodeList {
public:
    JExplodeList();
    ~JExplodeList();
    void CreateExplode(int x, int y, int type, int sound);
    void CreateSmall(int x, int y);  // small hit spark + sound (inlined in CollisionWithFire)
    Image *expDib1, *expDib2;
    JSprite256* expChara[40] = {};
};

class JPotionList {
public:
    JPotionList();
    ~JPotionList();
    void CreatePotion(int x, int y, int kind);
    void CreateHeart(int x, int y, int kind);
    Image *potionDib, *heartDib;
    JScript *moveScript1, *moveScript2, *moveScript3;
    JPotionChara* potionChara[25] = {};
};

class JEnemyList {
public:
    JEnemyList();
    ~JEnemyList();
    char CreateLevelEnemySet(JMap* map);
    void CollisionWithFire(JFireList* fire, JExplodeList* expl, JPotionList* pot);
    int AllDead();
    JScript *moveScript[9];
    JEnemyChara* enemyChara[60] = {};
};

class JEventProcess {
public:
    enum EventType { GETREADY = 0, GAMEOVER = 1, POWERUP = 2 };
    JEventProcess();
    ~JEventProcess();
    void StartProcess(EventType t);
    void Move();
    void Clear() { SafeDelete(sprite1); SafeDelete(sprite2); }
    EventType eventType = GETREADY;
    Image *dibPowerUp, *dibPowerUpBmp, *dibGetReady, *dibGame, *dibOver;
    JScript *scriptPowerUp, *scriptPowerUp2, *scriptGetReady, *scriptGame, *scriptOver;
    JPotionChara* sprite1 = nullptr;
    JPotionChara* sprite2 = nullptr;
};

struct ExplodeRegion { int x, y, w, h, count, bonus; };
class JExplodeRegionList {
public:
    JExplodeRegionList(JExplodeList* e, JPotionList* p) : expList(e), potionList(p) {}
    void CreateExplodeRegion(JSprite256* s, int count, int bonus);
    void Process();
    void Reset() { regionList.clear(); }
    bool IsEmpty() const { return regionList.empty(); }
    JExplodeList* expList;
    JPotionList* potionList;
    std::vector<ExplodeRegion> regionList;
};

// ---------------------------------------------------------------- bosses (boss.cpp)
class JBossBase {
public:
    virtual ~JBossBase();
    virtual void CreateBoss() = 0;
    virtual void MoveBoss(JBulletList* b, JMyChara* my) = 0;
    virtual void DrawBoss(Surface* s) = 0;
    virtual void CollisionWithFire(JFireList* f, JExplodeRegionList* r) = 0;
    virtual void CollisionWithMyChara(JMyChara* my, JExplodeList* e) = 0;
    int Dead() { return bossChara[0] == nullptr; }
protected:
    int _ProcessCollisionWithFire(JEnemySubChara** part, JFireChara** fire, JExplodeRegionList* r, int bonus);
    void HitMain(JFireChara** fire, JExplodeRegionList* r, int part, int count, int bonus);
    JEnemySubChara* NewPart(int resId, int script, int frames);
    void Attach(int part);
    void MovePart(int part, JBulletList* b, JMyChara* my);
    JScript* bossScript[7] = {};
    JEnemySubChara* bossChara[7] = {};
    int nParts = 0;
};
JBossBase* CreateBossByLetter(char c);

// ---------------------------------------------------------------- JGame
class JInputText;
class JGame {
public:
    enum ProcessType { NON = 0, GAME, EVENT, GAMEOVER, GAMEOVER_FADEOUT, CLEARED, FADEIN,
                       CLEARED_FADEOUT, NAME_ENTRY, TO_CONTINUE };
    enum BGMType { BGM_NON = 0, BGM_NORMAL, BGM_BOSS };
    JGame();
    ~JGame();
    void SetupEpisode(int ep);
    void SetupLevel(int level);
    void SetupPotions(int potions);
    void InGame();
    void StartEvent(JEventProcess::EventType t, ProcessType p);

    int episode = 0, gameLevel = 0;
    JMap* map;
    JMyChara* myChara;
    JFireList* fireList;
    JBulletList* bulletList;
    JExplodeList* explodeList;
    JPotionList* potionList;
    JEnemyList* enemyList;
    JBossBase* bossChara = nullptr;
    JExplodeRegionList* expRegList;
    JNumber* number;
    JInputText* inputText;
    JSprite256* background = nullptr;
    Image* backgroundImg = nullptr;
    JSprite256 *hearts, *textLife, *textPower, *textStage, *textPotions;
    BGMType bgmType = BGM_NON;
    ProcessType procType = NON;
    int procTicks = 0;
    JEventProcess* powerUpProc;
    Surface* lpBackBuffer;

private:
    void CheckCollision();
    void DestroyLevelData();
    void DrawScreen();
    void DrawStatus();
    int RankingPosition();
    void PlayBGM(BGMType t);
};

// ---------------------------------------------------------------- title.cpp screens
class JInputText {
public:
    enum ProcessType { NON = 0, INPUT, DONE };
    JInputText();
    ~JInputText();
    void SetSurface(Surface* s) { lpSurface = s; ascii->SetSurface(s); }
    void SetMessage(const std::string& m) { msg = m; }
    void SetCenterOfScreen();
    void SetText(const std::string& t, int lim);
    void StartInput();
    void InInput();
    int Done() const { return procType == DONE; }
    const std::string& GetText() const { return text; }
private:
    void DrawScreen();
    Surface* lpSurface = nullptr;
    JSprite256 *cursor, *inputBox1, *inputBox2, *inputBox3, *winFrame1, *winFrame2, *winFrame3;
    JAscii* ascii;
    int x = 0, y = 0, limit = 10;
    std::string msg, text;
    ProcessType procType = NON;
    // port: arcade-style letter picker for gamepads (up/down letter, A add, B delete, Start done)
    bool padActive = false;
    int padChar = 1;  // index into the picker alphabet ('A')
    unsigned blink = 0;
};

class JScrollingBackground {
public:
    void DrawTiles(JSprite256* tile, unsigned ticks, Surface* s);
};

class JTitle {
public:
    enum ProcessType { NON = 0, FADEIN, TITLE, FADEOUT };
    JTitle(); ~JTitle();
    void SetupTitle();
    void InTitle();
private:
    JScript* scriptBmpChara; JPotionChara* bmpChara;
    JSprite256 *background, *titlePetWings, *bigWing1, *bigWing2, *cursorLeft, *cursorRight, *menuItems;
    JSprite256* optionsItem;  // port: extra "OPTIONS" entry
    JAscii* ascii;
    int selMenu = 0; ProcessType procType = NON; int procTicks = 0; unsigned timeTicks = 0;
    Surface* lpBackBuffer;
};

class JRanking {
public:
    enum ProcessType { NON = 0, FADEIN, RANKING, FADEOUT };
    JRanking(); ~JRanking();
    void SetupRanking();
    void InRanking();
private:
    JSprite256 *background, *textRanking, *rankBack;
    JAscii* ascii; JNumber* number;
    ProcessType procType = NON; int procTicks = 0; unsigned timeTicks = 0;
    Surface* lpBackBuffer;
};

class JContinue {
public:
    enum ProcessType { NON = 0, FADEIN, CONTINUE, FADEOUT };
    JContinue(); ~JContinue();
    void SetupContinue();
    void InContinue();
private:
    JSprite256 *background, *bmpChara, *textContinue, *textYes, *textNo, *cursorLeft, *cursorRight;
    JAscii* ascii;
    int selMenu = 0; ProcessType procType = NON; int procTicks = 0; unsigned timeTicks = 0;
    Surface* lpBackBuffer;
};

class JEnding {
public:
    enum ProcessType { NON = 0, FADEIN, ENDING, FADEOUT };
    JEnding(); ~JEnding();
    void SetupEnding();
    void InEnding();
private:
    JScript* scriptChara; JPotionChara* chara;
    JSprite256 *background, *textTheEnd, *textStuff;
    ProcessType procType = NON; int procTicks = 0;
    Surface* lpBackBuffer;
};

class JJsoftLogo {
public:
    JJsoftLogo(); ~JJsoftLogo();
    void SetupLogo();
    void InLogo();
private:
    JSprite256* logo; unsigned timeTicks = 0; Surface* lpBackBuffer;
};

// Port addition: video / language options, drawn with the game's own fonts.
class JOptions {
public:
    enum ProcessType { NON = 0, FADEIN, OPTIONS, FADEOUT };
    JOptions(); ~JOptions();
    void SetupOptions();
    void InOptions();
private:
    enum Row { ROW_SCALE, ROW_FILTER, ROW_ASPECT, ROW_WIDE, ROW_FULLSCREEN, ROW_MUSIC, ROW_SFX, ROW_LANGUAGE, ROW_BACK };
    std::vector<int> rows;
    void Change(int row, int delta);
    std::string ValueText(int row);
    JSprite256 *background, *heading, *cursorLeft, *cursorRight;
    JAscii *fontRed, *fontBlue;
    int sel = 0; ProcessType procType = NON; int procTicks = 0; unsigned timeTicks = 0;
    Surface* lpBackBuffer;
};

// ---------------------------------------------------------------- mainapp.cpp
enum GAMESTATE { IN_NON = 0, START_LOGO, IN_LOGO, START_GAME, IN_GAME, START_TITLE, IN_TITLE,
                 START_RANKING, IN_RANKING, START_CONTINUE, IN_CONTINUE, START_ENDING, IN_ENDING,
                 OPEN_HELP, EXIT,
                 START_OPTIONS, IN_OPTIONS };  // port addition
struct APPDATA {
    int fFlippingPaused = 0;
    int keyCommandEnabled = 1;
    unsigned long delay = 40;  // ms per frame (25 fps)
    int startingEpisode = 0, startingGameLevel = 0, startingPotions = 0;
    GAMESTATE gameState = START_LOGO;
    unsigned frameCount = 0;
};
struct RANKDATA { long potions; char name[16]; };

APPDATA* GetAppData();
JGame* GetCurrentGame();
RANKDATA* GetRankData(int i);
void LoadRankList();
void ApplyRankingText(const std::string& text);
std::string RankingText();
void SaveRankList();
void GameProcess();       // one 40 ms frame of the original main loop
void AppShutdown();
