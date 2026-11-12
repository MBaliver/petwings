// game.cpp - reconstructed from the original PetWings binary.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game.h"

static const int POWER_UP_COUNT[5] = {50, 100, 150, 200, 250};
static int totalShield = 0;

static int CMod32(int v) { return v % 32; }  // C semantics, like the original `& 0x8000001f` dance

static int _LimitDirectionTo(int dir, int range) {
    int h = range / 2;
    if (dir < 8) return h + 24;
    if (24 - h <= dir && dir <= h + 24) return dir;
    return dir < 24 - h ? 24 - h : h + 24;
}

// ============================================================== JEnemyChara
JEnemyChara::JEnemyChara(int resId, JScript* script, int frames, STATE st)
    : JSprite256(resId, frames), state(st), moveScript(script) {
    SetSpeed(2.5f);
    SetAnimationDelay(5);
    SetDirection(0);
}

static long FindLabel(JScript* s, long label) {
    unsigned long i = 0;
    for (; i < s->Count(); i++) {
        const JScriptItem* it = s->GetScriptItem(i);
        if (it->action == LABEL && it->value1 == label) break;
    }
    return (long)i;
}

void JEnemyChara::MoveScript(JBulletList* bullets, JMyChara* my) {
    if (state == WALK) {
        JSprite256::Move();
        if (bulletRate != 0 && Random(bulletRate) == 0)
            bullets->CreateBullet(GetCenterX(), GetCenterY(), 1, my);
        if (timeToAttack == 0) {
            state = ATTACK;
            scriptIndex = 0; currentAction = UNKNOWN; currentValue1 = currentValue2 = 0;
            repeatIndex = 0; repeatValue = 0; gosubIndex = -1;
            return;
        }
        timeToAttack--;
        return;
    }
    if (state != ATTACK) return;

    for (int guard = 0; guard < 100000; guard++) {
        switch (currentAction) {
        case UNKNOWN: {
            const JScriptItem* it = moveScript->GetScriptItem(scriptIndex);
            currentAction = it->action;
            currentValue1 = it->value1;
            currentValue2 = it->value2;
            scriptIndex = (scriptIndex + 1) % moveScript->Count();
            continue;
        }
        case MOVE:
            if (currentValue1 > 0) { JSprite256::Move(); currentValue1--; return; }
            break;
        case STOP:
            if (currentValue1 > 0) { currentValue1--; return; }
            break;
        case SHIFT_X: ShiftPosition((int)currentValue1, 0); break;
        case SHIFT_Y: ShiftPosition(0, (int)currentValue1); break;
        case SPEED: SetSpeed((float)currentValue1); break;
        case SPEED_PERCENT: SetSpeed(GetSpeed() * ((float)currentValue1 / 100.0f)); break;
        case DIRECTION: SetDirection((int)currentValue1); break;
        case DIRECTION_TO_MYCHARA: {
            int d = FindDirection32(GetCenterX(), GetCenterY(), my->GetCenterX(), my->GetCenterY());
            if (currentValue1 > 0) d = _LimitDirectionTo(d, (int)currentValue1);
            SetDirection(d);
            break;
        }
        case DIRECTION_REVERSE: {
            int d = GetDirection();
            if (d != -1) d = CMod32(d + 16);
            SetDirection(d);
            break;
        }
        case ROTATE_LEFT:
            if (GetDirection() >= 0) SetDirection(CMod32((int)currentValue1 + GetDirection()));
            break;
        case ROTATE_RIGHT:
            if (GetDirection() >= 0) SetDirection(CMod32(GetDirection() - (int)currentValue1 + 32));
            break;
        case GOTO_LABEL: {
            long i = FindLabel(moveScript, currentValue1);
            if ((unsigned long)i < moveScript->Count()) scriptIndex = i;
            break;
        }
        case SET_FRAME: SetFrameIndex((int)currentValue1); break;
        case INCR_FRAME: IncrementFrame(); break;
        case REPEAT: repeatValue = currentValue1; repeatIndex = scriptIndex; break;
        case REPEAT_END:
            if (repeatValue < 2) repeatValue = 0;
            else { scriptIndex = repeatIndex; repeatValue--; }
            break;
        case SHOOT:
            bullets->CreateBullet(GetCenterX() + bulletOffsetX, GetCenterY() + bulletOffsetY,
                                  (int)currentValue1, my);
            break;
        case SET_BULLET_OFFSET:
            bulletOffsetX = (int)currentValue1; bulletOffsetY = (int)currentValue2;
            break;
        case PLAY_SOUND: SoundPlay((unsigned)currentValue1); break;
        case GOSUB: {
            gosubIndex = (long)scriptIndex;
            long i = FindLabel(moveScript, currentValue1);
            if ((unsigned long)i < moveScript->Count()) scriptIndex = i;
            break;
        }
        case RETURN:
            scriptIndex = gosubIndex >= 0 ? (unsigned long)gosubIndex : 0;
            gosubIndex = -1;
            break;
        default:  // LABEL and anything unknown
            break;
        }
        currentAction = UNKNOWN;
    }
}

// ============================================================== JPotionChara
void JPotionChara::Move() {
    for (int guard = 0; guard < 100000; guard++) {
        switch (currentAction) {
        case UNKNOWN: {
            const JScriptItem* it = moveScript->GetScriptItem(scriptIndex);
            currentAction = it->action;
            currentValue = it->value1;
            scriptIndex = (scriptIndex + 1) % moveScript->Count();
            continue;
        }
        case MOVE:
            if (currentValue > 0) { JSprite256::Move(); currentValue--; return; }
            break;
        case STOP:
            if (currentValue > 0) { currentValue--; return; }
            break;
        case SHIFT_X: ShiftPosition((int)currentValue, 0); break;
        case SHIFT_Y: ShiftPosition(0, (int)currentValue); break;
        case SPEED: SetSpeed((float)currentValue); break;
        case SPEED_PERCENT: SetSpeed(GetSpeed() * ((float)currentValue / 100.0f)); break;
        case DIRECTION: SetDirection((int)currentValue); break;
        case ROTATE_LEFT:
            if (GetDirection() >= 0) SetDirection(CMod32((int)currentValue + GetDirection()));
            break;
        case ROTATE_RIGHT:
            if (GetDirection() >= 0) SetDirection(CMod32(GetDirection() - (int)currentValue + 32));
            break;
        case GOTO_LABEL: {
            long i = FindLabel(moveScript, currentValue);
            if ((unsigned long)i < moveScript->Count()) scriptIndex = i;
            break;
        }
        case REPEAT: repeatValue = currentValue; repeatIndex = scriptIndex; break;
        case REPEAT_END:
            if (repeatValue < 2) repeatValue = 0;
            else { scriptIndex = repeatIndex; repeatValue--; }
            break;
        case PLAY_SOUND: SoundPlay((unsigned)currentValue); break;
        default: break;
        }
        currentAction = UNKNOWN;
    }
}

// ============================================================== JMyChara
JMyChara::JMyChara(JGame* g) : JSprite256(0x74, 4), game(g) {
    myCharaFlash = new JSprite256(0x9e, 4);
    SetSpeed(8.0f);
    SetAnimationDelay(2);
}
JMyChara::~JMyChara() { delete myCharaFlash; }

void JMyChara::Move() {
    JSprite256::Move();
    if (GetIntX() < 0) { SetPosX(0); return; }
    if (DDGetScreenWidth() - Width() < GetIntX()) SetPosX(DDGetScreenWidth() - Width());
}

void JMyChara::Reset() {
    state = NORMAL;
    SetCenterPosition(DDGetScreenWidth() / 2, SCREEN_H + (-6 - Height()));
}

void JMyChara::DecreaseLife(JExplodeList* expl) {
    if (state != NORMAL) return;
    life--;
    SoundPlay(9);
    if (life < 1) {
        life = 0;
        state = DEAD;
        game->StartEvent(JEventProcess::GAMEOVER, JGame::GAMEOVER);
        expl->CreateExplode(GetCenterX(), GetCenterY(), 2, 1);
        return;
    }
    state = FLASH;
    flashTimeOut = 75;
}

// ============================================================== JFireList
JFireList::JFireList() { fireDib1 = GetImage(0x7d); fireDib2 = GetImage(0x82); }
JFireList::~JFireList() { for (auto& f : fireChara) SafeDelete(f); }
int JFireList::GetTotalNull() { int n = 0; for (auto f : fireChara) if (!f) n++; return n; }
int JFireList::GetNullIndex() { for (int i = 0; i < 12; i++) if (!fireChara[i]) return i; return -1; }

JFireChara* JFireList::NewFire(Image* img, int dir) {
    int i = GetNullIndex();
    JFireChara* f = new JFireChara(img);
    fireChara[i] = f;
    f->SetDirection(dir);
    f->firePower = 1;
    return f;
}

void JFireList::CreateFire(int x, int y, int level) {
    JFireChara* f;
    switch (level) {
    case 0:
        if (GetTotalNull() - 9 < 1) return;
        break;
    case 1:
        if (GetTotalNull() - 8 < 2) return;
        f = NewFire(fireDib1, 8); f->SetCenterPosition(x - f->Width(), y - f->Height() / 3);
        break;
    case 2:
        if (GetTotalNull() - 6 < 3) return;
        f = NewFire(fireDib2, 7); f->SetCenterPosition(f->Width() / 2 + x, y);
        f = NewFire(fireDib1, 8); f->SetCenterPosition(x - f->Width(), y - f->Height() / 3);
        break;
    case 3:
        if (GetTotalNull() - 4 < 4) return;
        f = NewFire(fireDib2, 7); f->SetCenterPosition(f->Width() / 2 + x, y);
        f = NewFire(fireDib2, 9); f->SetCenterPosition(x - f->Width() / 2, y);
        f = NewFire(fireDib1, 8); f->SetCenterPosition(x - f->Width(), y - f->Height() / 3);
        break;
    case 4:
        if (GetTotalNull() - 2 < 5) return;
        f = NewFire(fireDib2, 6); f->SetCenterPosition(f->Width() + x, y);
        f = NewFire(fireDib2, 7); f->SetCenterPosition(f->Width() / 2 + x, y);
        f = NewFire(fireDib2, 9); f->SetCenterPosition(x - f->Width() / 2, y);
        f = NewFire(fireDib1, 8); f->SetCenterPosition(x - f->Width(), y - f->Height() / 3);
        break;
    case 5:
        if (GetTotalNull() < 6) return;
        f = NewFire(fireDib2, 6); f->SetCenterPosition(f->Width() + x, y);
        f = NewFire(fireDib2, 10); f->SetCenterPosition(x - f->Width(), y);
        f = NewFire(fireDib2, 7); f->SetCenterPosition(f->Width() / 2 + x, y);
        f = NewFire(fireDib2, 9); f->SetCenterPosition(x - f->Width() / 2, y);
        f = NewFire(fireDib1, 8); f->SetCenterPosition(x - f->Width(), y - f->Height() / 3);
        break;
    default:
        return;
    }
    // the last (or only) straight shot
    f = NewFire(fireDib1, 8);
    if (level == 0) f->SetCenterPosition(x, y - f->Height() / 3);
    else f->SetCenterPosition(f->Width() + x, y - f->Height() / 3);
    SoundPlay(0);
}

// ============================================================== JBulletList
JBulletList::JBulletList() {
    bulletDib1 = GetImage(0x7e); bulletDib2 = GetImage(0xb2); bulletDib2top = GetImage(0x107);
    bulletDib2btm = GetImage(0x108); bulletDib3 = GetImage(0xb5); bulletDib4 = GetImage(0xde);
    bulletDib5up = GetImage(0xe8); bulletDib5down = GetImage(0xe9); bulletDib6 = GetImage(0x110);
}
JBulletList::~JBulletList() { for (auto& b : bulletChara) SafeDelete(b); }
int JBulletList::GetTotalNull() { int n = 0; for (auto b : bulletChara) if (!b) n++; return n; }
int JBulletList::GetNullIndex() { for (int i = 0; i < 60; i++) if (!bulletChara[i]) return i; return -1; }

JBulletChara* JBulletList::Spawn(Image* img, int frames, float speed, int dir, int x, int y, bool front) {
    int i = GetNullIndex();
    if (i < 0) return nullptr;
    JBulletChara* b = new JBulletChara(img, frames);
    bulletChara[i] = b;
    b->SetSpeed(speed);
    b->SetDirection(dir);
    b->SetCenterPosition(x, y);
    if (front) b->drawState = JBulletChara::FRONT;
    return b;
}

void JBulletList::CreateBullet(int x, int y, int type, JMyChara* my) {
    const bool front = type > 99;
    const int t = type < 100 ? type : type - 100;
    auto aimed = [&]() {
        return _LimitDirectionTo(FindDirection32(x, y, my->GetCenterX(), my->GetCenterY()), 7);
    };
    switch (t) {
    case 1:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib3, 2, 8.0f, 24, x, y, front);
        return;
    case 3: {
        if (GetTotalNull() < 1) return;
        int d = aimed();
        Spawn(bulletDib1, 2, 7.0f, d, x, y, front);
        return;
    }
    case 4: {
        if (GetTotalNull() < 1) return;
        int d = aimed();
        Spawn(bulletDib6, 4, 9.0f, d, x, y, front);
        SoundPlay(13);
        return;
    }
    case 5:
        if (GetTotalNull() < 3) return;
        for (int d : {22, 26, 24}) Spawn(bulletDib1, 2, 7.0f, d, x, y, front);
        return;
    case 6:
        if (GetTotalNull() < 5) return;
        for (int d : {20, 28, 22, 26, 24}) Spawn(bulletDib1, 2, 7.0f, d, x, y, front);
        return;
    case 7: {
        if (GetTotalNull() < 1) return;
        int d = (int)Random(7) + 21;
        Spawn(bulletDib1, 2, 7.0f, d, x, y, front);
        return;
    }
    case 8:
        if (GetTotalNull() < 3) return;
        for (int d : {21, 27, 24}) Spawn(bulletDib6, 4, 9.0f, d, x, y, front);
        SoundPlay(13);
        return;
    case 9:
        if (GetTotalNull() < 4) return;
        for (int d : {21, 27, 23, 25}) Spawn(bulletDib1, 2, 7.0f, d, x, y, front);
        return;
    case 10:
        if (GetTotalNull() < 4) return;
        for (int d : {21, 27, 23, 25}) Spawn(bulletDib6, 4, 9.0f, d, x, y, front);
        SoundPlay(13);
        return;
    case 11:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib4, 2, 20.0f, 24, x, y, front);
        SoundPlay(13);
        return;
    case 12:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib5up, 2, 20.0f, 8, x, y, front);
        SoundPlay(14);
        return;
    case 13: {
        if (GetTotalNull() < 1) return;
        int i = GetNullIndex();
        JBulletChara* b = new JBulletChara(bulletDib5down, 2);
        bulletChara[i] = b;
        b->SetSpeed(16.0f);
        b->SetDirection(24);
        int ny = -(b->Height() / 2);
        int nx = (int)Random(DDGetScreenWidth());
        b->SetCenterPosition(nx, ny);
        if (front) b->drawState = JBulletChara::FRONT;
        return;
    }
    case 14:
        if (GetTotalNull() < 4) return;
        for (int d : {21, 27, 23, 25}) Spawn(bulletDib5down, 2, 12.0f, d, x, y, front);
        SoundPlay(14);
        return;
    case 15:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib2btm, 1, 16.0f, 24, x, y, front);
        SoundPlay(13);
        return;
    case 16:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib2, 1, 16.0f, 24, x, y, front);
        return;
    case 17:
        if (GetTotalNull() < 1) return;
        Spawn(bulletDib2top, 1, 16.0f, 24, x, y, front);
        return;
    case 20:
        if (GetTotalNull() < 16) return;
        for (int d = 24; d < 56; d += 2) Spawn(bulletDib6, 4, 9.0f, d, x, y, front);
        SoundPlay(13);
        return;
    case 21:
        if (GetTotalNull() < 16) return;
        for (int d = 25; d < 57; d += 2) Spawn(bulletDib6, 4, 9.0f, d, x, y, front);
        SoundPlay(13);
        return;
    default:
        return;
    }
}

// ============================================================== JExplodeList
JExplodeList::JExplodeList() { expDib1 = GetImage(0x80); expDib2 = GetImage(0x81); }
JExplodeList::~JExplodeList() { for (auto& e : expChara) SafeDelete(e); }

void JExplodeList::CreateExplode(int x, int y, int type, int sound) {
    if (type != 1 && type != 2) return;
    int i = 0;
    while (expChara[i]) { if (++i > 39) return; }
    JSprite256* e = new JSprite256(type == 1 ? expDib1 : expDib2, type == 1 ? 9 : 5);
    expChara[i] = e;
    e->SetAnimationDelay(0);
    e->SetCenterPosition(x, y);
    if (sound) SoundPlay(type == 1 ? 2 : 1);
}
void JExplodeList::CreateSmall(int x, int y) { CreateExplode(x, y, 2, 1); }

// ============================================================== JPotionList
JPotionList::JPotionList() {
    potionDib = GetImage(0x86);
    heartDib = GetImage(0x114);
    moveScript1 = new JScript(0x88);
    moveScript2 = new JScript(0x8a);
    moveScript3 = new JScript(0x89);
}
JPotionList::~JPotionList() {
    for (auto& p : potionChara) SafeDelete(p);
    delete moveScript1; delete moveScript2; delete moveScript3;
}

static void CreatePotionOrHeart(JPotionList* l, Image* img, JPotionChara::PotionType t, int x, int y, int kind) {
    int i = 0;
    while (l->potionChara[i]) { if (++i > 24) return; }
    JScript* s = kind == 0 ? l->moveScript1 : kind == 1 ? l->moveScript2 : kind == 2 ? l->moveScript3 : nullptr;
    if (!s) return;
    l->potionChara[i] = new JPotionChara(img, s, t);
    l->potionChara[i]->SetCenterPosition(x, y);
}
void JPotionList::CreatePotion(int x, int y, int kind) { CreatePotionOrHeart(this, potionDib, JPotionChara::POTION, x, y, kind); }
void JPotionList::CreateHeart(int x, int y, int kind) { CreatePotionOrHeart(this, heartDib, JPotionChara::HEART, x, y, kind); }

// ============================================================== JEnemyList
JEnemyList::JEnemyList() {
    static const int ids[9] = {0x83, 0xaa, 0xa7, 0xa8, 0xa9, 0xa6, 0x103, 0x106, 0x105};
    for (int i = 0; i < 9; i++) moveScript[i] = new JScript(ids[i]);
}
JEnemyList::~JEnemyList() {
    for (auto& e : enemyChara) SafeDelete(e);
    for (auto& s : moveScript) delete s;
}

int JEnemyList::AllDead() {
    for (auto e : enemyChara) if (e) return 0;
    return 1;
}

char JEnemyList::CreateLevelEnemySet(JMap* map) {
    struct Kind { int res, shield; unsigned short rate; };
    static const Kind kinds[9] = {
        {0x76, 1, 900}, {0x78, 3, 800}, {0x7a, 5, 700}, {0x8d, 3, 700}, {0x9d, 20, 500},
        {0x9f, 20, 500}, {0x100, 12, 500}, {0x101, 4, 700}, {0x102, 7, 600},
    };
    totalShield = 0;
    int rowY = 32, count = 0;
    for (int row = 0; row < 6; row++) {
        long rowH = 0;
        int cx = (DDGetScreenWidth() - 484) / 2 + 22;
        for (int col = 0; col < 11; col++, cx += 44) {
            char c = map->GetMapData(col, row);
            if (c == '0' || count >= 60) continue;
            if (c < '1' || c > '9') return c;
            const Kind& k = kinds[c - '1'];
            JEnemyChara* e = new JEnemyChara(k.res, moveScript[c - '1'], 2, JEnemyChara::WALK);
            enemyChara[count] = e;
            e->shield = k.shield;
            e->bulletRate = k.rate;
            e->SetCenterPosition(cx, 0);
            e->SetPosY(rowY);
            e->timeToAttack = 0;
            totalShield += e->shield;
            rowH = Max(rowH, e->Height() + 4);
            count++;
        }
        rowY += (int)rowH;
    }
    // Random attack order: first attacker after 60 frames, then one every 34 frames.
    unsigned long t = 60;
    for (int n = count; n > 0; n--) {
        int i = (int)Random(count);
        while (enemyChara[i]->timeToAttack != 0) i = (i - 1 >= 0 ? i : count) - 1;
        enemyChara[i]->timeToAttack = t;
        t += 34;
    }
    return 0;
}

void JEnemyList::CollisionWithFire(JFireList* fire, JExplodeList* expl, JPotionList* pot) {
    for (auto& e : enemyChara) {
        if (!e) continue;
        for (int i = 0; i < 12; i++) {
            JFireChara* f = fire->fireChara[i];
            if (!f || !e || !e->Collision_Full(f)) continue;
            e->DecreaseShield(f->firePower);
            int fx = f->GetCenterX(), fy = f->GetCenterY();
            SafeDelete(fire->fireChara[i]);
            if (e->shield < 1) {
                int ex = e->GetCenterX(), ey = e->GetCenterY();
                SafeDelete(e);
                expl->CreateExplode(ex, ey, 1, 1);
                pot->CreatePotion(ex, ey, (int)Random(3));
                break;
            }
            expl->CreateExplode(fx, fy, 2, 1);
        }
    }
}

// ============================================================== JEventProcess
JEventProcess::JEventProcess() {
    dibPowerUp = GetImage(0x8b); dibPowerUpBmp = GetImage(0x9a); dibGetReady = GetImage(0xa0);
    dibGame = GetImage(0xa1); dibOver = GetImage(0xa2);
    scriptPowerUp = new JScript(0x8c); scriptPowerUp2 = new JScript(0x9b);
    scriptGetReady = new JScript(0xa4); scriptGame = new JScript(0xa5); scriptOver = new JScript(0xa3);
}
JEventProcess::~JEventProcess() {
    Clear();
    delete scriptPowerUp; delete scriptPowerUp2; delete scriptGetReady; delete scriptGame; delete scriptOver;
}

void JEventProcess::StartProcess(EventType t) {
    eventType = t;
    if (t == GETREADY) {
        sprite1 = new JPotionChara(dibGetReady, scriptGetReady, JPotionChara::OTHER);
        sprite1->SetCenterPosition(0, SCREEN_H / 2);
        sprite1->SetPosX(-sprite1->Width() + ScreenOX());
    } else if (t == GAMEOVER) {
        sprite1 = new JPotionChara(dibGame, scriptGame, JPotionChara::OTHER);
        sprite1->SetPosition(-sprite1->Width() + ScreenOX(), -sprite1->Height());
        sprite2 = new JPotionChara(dibOver, scriptOver, JPotionChara::OTHER);
        sprite2->SetPosition(BASE_W + ScreenOX(), SCREEN_H);
    } else if (t == POWERUP) {
        sprite1 = new JPotionChara(dibPowerUp, scriptPowerUp, JPotionChara::OTHER);
        sprite1->SetCenterPosition(DDGetScreenWidth() / 2, 0);
        sprite1->SetPosY(-sprite1->Height());
        sprite2 = new JPotionChara(dibPowerUpBmp, scriptPowerUp2, JPotionChara::OTHER);
        sprite2->SetCenterPosition(DDGetScreenWidth() / 2, 0);
        sprite2->SetPosY(SCREEN_H);
    }
}

void JEventProcess::Move() {
    if (sprite1) { sprite1->Move(); if (sprite1->OutOfScreen_Full()) SafeDelete(sprite1); }
    if (sprite2) { sprite2->Move(); if (sprite2->OutOfScreen_Full()) SafeDelete(sprite2); }
}

// ============================================================== JExplodeRegionList
void JExplodeRegionList::CreateExplodeRegion(JSprite256* s, int count, int bonus) {
    regionList.push_back({s->GetIntX(), s->GetIntY(), s->Width(), s->Height(), count, bonus});
}

void JExplodeRegionList::Process() {
    for (size_t i = 0; i < regionList.size(); i++) {
        ExplodeRegion& r = regionList[i];
        for (int k = 0; k < 2; k++) {
            long rx = Random(r.w);
            long ry = Random(r.h);
            bool beat = k == 0 && r.count % 4 == 0;
            expList->CreateExplode((int)rx + r.x, (int)ry + r.y, 1, beat ? 1 : 0);
            if (r.bonus && beat) potionList->CreatePotion((int)rx + r.x, (int)ry + r.y, (int)Random(3));
        }
        r.count--;
        if (r.bonus && r.count == 0)
            potionList->CreateHeart(r.w / 2 + r.x, r.h / 2 + r.y, (int)Random(3));
    }
    for (size_t i = 0; i < regionList.size();) {
        if (regionList[i].count < 1) regionList.erase(regionList.begin() + i);
        else i++;
    }
}

// ============================================================== JGame
JGame::JGame() {
    map = new JMap();
    fireList = new JFireList();
    bulletList = new JBulletList();
    explodeList = new JExplodeList();
    potionList = new JPotionList();
    enemyList = new JEnemyList();
    expRegList = new JExplodeRegionList(explodeList, potionList);
    number = new JNumber();
    inputText = new JInputText();
    hearts = new JSprite256(0x96, 4);
    textLife = new JSprite256(0x95, 1);
    textPower = new JSprite256(0x94, 1);
    textStage = new JSprite256(0x92, 1);
    textPotions = new JSprite256(0x97, 1);
    powerUpProc = new JEventProcess();
    lpBackBuffer = DDGetBackBuffer();
    number->SetSurface(lpBackBuffer);
    number->SetFontType(0);
    inputText->SetSurface(lpBackBuffer);
    inputText->SetMessage(Tr("NAME_ENTRY_PROMPT"));
    inputText->SetCenterOfScreen();
    hearts->SetAnimationDelay(3);
    myChara = new JMyChara(this);
}

JGame::~JGame() {
    delete background; delete bossChara;
    delete map; delete fireList; delete bulletList; delete explodeList; delete potionList;
    delete enemyList; delete expRegList; delete number; delete inputText;
    delete hearts; delete textLife; delete textPower; delete textStage; delete textPotions;
    delete powerUpProc; delete myChara;
}

void JGame::StartEvent(JEventProcess::EventType t, ProcessType p) {
    procType = p;
    procTicks = 0;
    powerUpProc->Clear();
    powerUpProc->StartProcess(t);
}

void JGame::SetupEpisode(int ep) { episode = ep; map->Reset(ep); }

void JGame::SetupPotions(int p) {
    myChara->potionCount = p;
    myChara->fireLevel = 0;
    for (int i = 0; i < 5; i++) if (POWER_UP_COUNT[i] <= p) myChara->fireLevel++;
}

void JGame::PlayBGM(BGMType t) {
    if (bgmType == t) return;
    StopMidi();
    bgmType = t;
    if (t == BGM_NORMAL) PlayMidi(1);
    else if (t == BGM_BOSS) PlayMidi(2);
}

void JGame::DestroyLevelData() {
    SafeDelete(background);
    SafeDelete(bossChara);
    myChara->Reset();
    for (auto& f : fireList->fireChara) SafeDelete(f);
    for (auto& b : bulletList->bulletChara) SafeDelete(b);
    for (auto& e : explodeList->expChara) SafeDelete(e);
    for (auto& p : potionList->potionChara) SafeDelete(p);
    for (auto& e : enemyList->enemyChara) SafeDelete(e);
    expRegList->Reset();
}

void JGame::SetupLevel(int level) {
    DestroyLevelData();
    gameLevel = level;
    map->SetGameLevel(level);
    int bg = episode == 1 ? 0x6e : episode == 2 ? 0x7f : episode == 3 ? 0x85 : 0x128;
    background = new JSprite256(bg, 1);
    backgroundImg = GetImage(bg);
    background->SetPosition(0, 0);
    background->SetTransparent(0);
    char c = enemyList->CreateLevelEnemySet(map);
    if (c == 0) {
        PlayBGM(BGM_NORMAL);
    } else {
        bossChara = CreateBossByLetter(c);
        if (bossChara) bossChara->CreateBoss();
        PlayBGM(BGM_BOSS);
    }
    procTicks = 25;
    procType = FADEIN;
}

int JGame::RankingPosition() {
    for (int i = 0; i < 8; i++)
        if (GetRankData(i)->potions < myChara->potionCount) return i;
    return -1;
}

void JGame::CheckCollision() {
    enemyList->CollisionWithFire(fireList, explodeList, potionList);
    if (bossChara) bossChara->CollisionWithFire(fireList, expRegList);
    JMyChara* my = myChara;
    if (my->state != JMyChara::DEAD) {
        for (auto& p : potionList->potionChara) {
            if (!p || !p->Collision_Full(my)) continue;
            if (p->type == JPotionChara::POTION) {
                my->potionCount++;
                if (my->potionCount > 9999) my->potionCount = 9999;
                if (my->fireLevel < 5 && POWER_UP_COUNT[my->fireLevel] <= my->potionCount &&
                    my->state == JMyChara::NORMAL) {
                    my->fireLevel++;
                    StartEvent(JEventProcess::POWERUP, EVENT);
                }
            } else if (p->type == JPotionChara::HEART && my->state == JMyChara::NORMAL) {
                my->life++;
                SoundPlay(3);
            }
            SoundPlay(3);
            SafeDelete(p);
        }
        for (auto e : enemyList->enemyChara)
            if (e && e->Collision_Full(my)) my->DecreaseLife(explodeList);
        for (auto& b : bulletList->bulletChara) {
            if (b && b->Collision_Full(my)) {
                SafeDelete(b);
                my->DecreaseLife(explodeList);
            }
        }
        if (bossChara) bossChara->CollisionWithMyChara(myChara, explodeList);
    }
    expRegList->Process();
}

void JGame::InGame() {
    if (procType == GAME || procType == GAMEOVER || procType == CLEARED) {
        DIUpdateInputState();
        CheckCollision();
        for (auto b : bulletList->bulletChara) if (b) b->IncrementAnimation();
        for (auto e : enemyList->enemyChara) if (e && e->state == JEnemyChara::WALK) e->IncrementAnimation();
        for (auto f : fireList->fireChara) if (f) f->IncrementAnimation();
        myChara->IncrementAnimation();
        if (myChara->state == JMyChara::FLASH && --myChara->flashTimeOut < 1) myChara->state = JMyChara::NORMAL;
        for (auto& e : explodeList->expChara) {
            if (!e) continue;
            e->IncrementAnimation();
            if (!e->AnimationFirstTime()) SafeDelete(e);
        }
        for (auto& f : fireList->fireChara) {
            if (!f) continue;
            f->Move();
            if (f->OutOfScreen_Full()) SafeDelete(f);
        }
        for (auto& b : bulletList->bulletChara) {
            if (!b) continue;
            b->Move();
            int by = b->GetIntY(), bx = b->GetIntX();
            if (!(by < SCREEN_H && -200 < by && bx < DDGetScreenWidth() && -b->Width() < bx)) SafeDelete(b);
        }
        for (auto& p : potionList->potionChara) {
            if (!p) continue;
            p->Move();
            if (SCREEN_H <= p->GetIntY()) SafeDelete(p);
        }
        bool bounce = false;
        for (auto e : enemyList->enemyChara) {
            if (!e) continue;
            e->MoveScript(bulletList, myChara);
            if (e->state == JEnemyChara::WALK && e->OutOfScreen_Partial()) bounce = true;
            else if (e->state == JEnemyChara::ATTACK && SCREEN_H <= e->GetIntY())
                e->SetPosY(-20 - e->Height());
        }
        if (bounce) {
            for (auto e : enemyList->enemyChara)
                if (e && e->state == JEnemyChara::WALK && e->GetDirection() != -1)
                    e->SetDirection(CMod32(e->GetDirection() + 16));
        }
        if (bossChara) bossChara->MoveBoss(bulletList, myChara);
        int dir = DIGetKeyjoyDirectionX();
        if (dir == -1) dir = DIGetJoystickDirectionX(1);
        myChara->SetDirection(dir);
        myChara->Move();
        if ((DIGetKeyboardPressed(DIK_SPACE) || DIGetKeyboardPressed(DIK_LCONTROL) ||
             DIGetKeyboardPressed(DIK_RCONTROL) || DIGetJoystickButtonPressed(0)) &&
            myChara->state != JMyChara::DEAD) {
            fireList->CreateFire(myChara->GetCenterX(), myChara->GetCenterY(), myChara->fireLevel);
        }
    }

    bool startNameEntry = false;
    switch (procType) {
    case GAME:
        if (enemyList->AllDead() && expRegList->IsEmpty() && (!bossChara || bossChara->Dead())) {
            procType = CLEARED;
            procTicks = 70;
        }
        break;
    case EVENT:
        powerUpProc->Move();
        if (!powerUpProc->sprite1 && !powerUpProc->sprite2) procType = GAME;
        break;
    case CLEARED:
        if (procTicks < 1) { procType = CLEARED_FADEOUT; procTicks = 25; }
        break;
    case FADEIN:
        if (procTicks < 1) StartEvent(JEventProcess::GETREADY, EVENT);
        break;
    case CLEARED_FADEOUT:
        if (procTicks > 0) break;
        if (episode < 4 || gameLevel < 5) {
            if (gameLevel < 16) {
                procType = NON;
                SetupLevel(gameLevel + 1);
            } else if (episode < 4) {
                procType = NON;
                episode++;
                map->Reset(episode);
                SetupLevel(1);
            }
            break;
        }
        StopMidi();
        if (RankingPosition() < 0) {
            procType = NON;
            GetAppData()->gameState = START_ENDING;
            break;
        }
        procType = NAME_ENTRY;
        startNameEntry = true;
        break;
    case GAMEOVER: {
        JEventProcess* ev = powerUpProc;
        if (ev->sprite1) { ev->sprite1->Move(); if (ev->sprite1->OutOfScreen_Full()) SafeDelete(ev->sprite1); }
        if (ev->sprite2) { ev->sprite2->Move(); if (ev->sprite2->OutOfScreen_Full()) SafeDelete(ev->sprite2); }
        if (!ev->sprite1 && !ev->sprite2) { procType = GAMEOVER_FADEOUT; procTicks = 25; }
        break;
    }
    case GAMEOVER_FADEOUT:
        if (procTicks > 0) break;
        StopMidi();
        if (RankingPosition() < 0) { procType = TO_CONTINUE; break; }
        procType = NAME_ENTRY;
        startNameEntry = true;
        break;
    case NAME_ENTRY:
        if (!inputText->Done()) {
            inputText->InInput();
        } else {
            std::string name = inputText->GetText();
            size_t a = name.find_first_not_of(' '), b = name.find_last_not_of(' ');
            name = a == std::string::npos ? "" : name.substr(a, b - a + 1);
            for (int i = 0; i < 8; i++) {
                if (GetRankData(i)->potions < myChara->potionCount) {
                    for (int j = 6; j >= i; j--) *GetRankData(j + 1) = *GetRankData(j);
                    RANKDATA* r = GetRankData(i);
                    r->potions = myChara->potionCount;
                    memset(r->name, 0, sizeof r->name);
                    strncpy(r->name, name.c_str(), 15);
                    break;
                }
            }
            SaveRankList();
            if (getenv("PETWINGS_TEST")) fprintf(stderr, "[test] name entered: \"%s\" (%d potions)\n", name.c_str(), myChara->potionCount);
            // port: send it to the shared ranking (web build); cleared = finished the whole game
            PlatformRankingSubmit(name, myChara->potionCount, episode, gameLevel,
                                  myChara->state != JMyChara::DEAD && episode >= 4 && gameLevel >= 5);
            if (myChara->state == JMyChara::DEAD || episode < 4 || gameLevel < 5) {
                procType = TO_CONTINUE;
            } else {
                procType = NON;
                GetAppData()->gameState = START_ENDING;
            }
        }
        break;
    case TO_CONTINUE: {
        procType = NON;
        APPDATA* app = GetAppData();
        app->startingEpisode = episode;
        app->startingGameLevel = gameLevel;
        app->startingPotions = myChara->potionCount - 50;
        app->gameState = app->startingPotions < 0 ? START_TITLE : START_CONTINUE;
        break;
    }
    default:
        break;
    }
    if (startNameEntry) {
        inputText->SetText("", 15);
        inputText->StartInput();
    }
    if (procTicks > 0) procTicks--;
    DrawScreen();
}

void JGame::DrawScreen() {
    if (procType == NAME_ENTRY) return;
    if (procType == NON || procType == TO_CONTINUE) { FadeOut(lpBackBuffer, 100); return; }
    DrawWideImage(backgroundImg, lpBackBuffer, 0);  // port: mirrored edges on wide screens
    for (auto b : bulletList->bulletChara)
        if (b && b->drawState == JBulletChara::BACK) b->Draw(lpBackBuffer);
    for (auto e : enemyList->enemyChara) if (e) e->Draw(lpBackBuffer);
    if (bossChara) bossChara->DrawBoss(lpBackBuffer);
    for (auto b : bulletList->bulletChara)
        if (b && b->drawState == JBulletChara::FRONT) b->Draw(lpBackBuffer);
    for (auto f : fireList->fireChara) if (f) f->Draw(lpBackBuffer);
    for (auto p : potionList->potionChara) if (p) p->Draw(lpBackBuffer);
    if (myChara->state == JMyChara::NORMAL) {
        myChara->Draw(lpBackBuffer);
    } else if (myChara->state == JMyChara::FLASH) {
        myChara->myCharaFlash->SetPosition(myChara->GetIntX(), myChara->GetIntY());
        myChara->myCharaFlash->SetFrameIndex(myChara->GetFrameIndex());
        myChara->myCharaFlash->Draw(lpBackBuffer);
    }
    for (auto e : explodeList->expChara) if (e) e->Draw(lpBackBuffer);
    if (procType == EVENT || procType == GAMEOVER) {
        if (powerUpProc->sprite2) powerUpProc->sprite2->Draw(lpBackBuffer);
        if (powerUpProc->sprite1) powerUpProc->sprite1->Draw(lpBackBuffer);
    }
    DrawStatus();
    if (procType == FADEIN) FadeOut(lpBackBuffer, procTicks << 2);
    if (procType == CLEARED_FADEOUT || procType == GAMEOVER_FADEOUT)
        FadeOut(lpBackBuffer, (25 - procTicks) * 4);
}

void JGame::DrawStatus() {
    textLife->SetPosition(8, 452);
    textLife->Draw(lpBackBuffer);
    int w = textLife->Width();
    for (int i = 0; i < myChara->life; i++) {
        hearts->SetPosition(hearts->Width() * i + w + 18, 454);
        hearts->Draw(lpBackBuffer);
    }
    hearts->IncrementAnimation();
    textPotions->SetPosition(8, 4);
    textPotions->Draw(lpBackBuffer);
    number->DrawNumbers(textPotions->Width() + 18, 5, myChara->potionCount, 4);
    textPower->SetPosition(200, 4);
    textPower->Draw(lpBackBuffer);
    number->DrawNumbers(textPower->Width() + 210, 5, myChara->fireLevel + 1, 1);
    const int rx = 2 * ScreenOX();  // port: right-aligned HUD on wide screens
    textStage->SetPosition(504 + rx, 4);
    textStage->Draw(lpBackBuffer);
    int x = number->DrawNumbers(textStage->Width() + 514 + rx, 5, episode, 1);
    x = number->DrawDash(x, 5);
    number->DrawNumbers(x, 5, gameLevel, 2);
}
