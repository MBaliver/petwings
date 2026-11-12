// boss.cpp - the five bosses, reconstructed from the original binary.
// Each boss is a main body (bossChara[0]) plus parts positioned relative to it.
#include "game.h"

JBossBase::~JBossBase() {
    for (auto& p : bossChara) SafeDelete(p);
    for (auto& s : bossScript) SafeDelete(s);
}

JEnemySubChara* JBossBase::NewPart(int resId, int script, int frames) {
    return new JEnemySubChara(resId, bossScript[script], frames);
}

// Parts keep their own (relative) position and add the main body's position as offset.
void JBossBase::Attach(int part) {
    bossChara[part]->SetOffset(bossChara[0]->GetIntX(), bossChara[0]->GetIntY());
}

void JBossBase::MovePart(int part, JBulletList* b, JMyChara* my) {
    if (!bossChara[part]) return;
    Attach(part);
    bossChara[part]->MoveScript(b, my);
}

// Damage to the main body; when destroyed, a long chain of explosions (with bonus items).
void JBossBase::HitMain(JFireChara** fire, JExplodeRegionList* r, int part, int count, int bonus) {
    JEnemySubChara*& p = bossChara[part];
    if (!*fire || !p || !p->Collision_Full(*fire)) return;
    p->DecreaseShield((*fire)->firePower);
    int fx = (*fire)->GetCenterX(), fy = (*fire)->GetCenterY();
    SafeDelete(*fire);
    if (p->shield < 1) {
        r->CreateExplodeRegion(p, count, bonus);
        SafeDelete(p);
    } else {
        r->expList->CreateExplode(fx, fy, 2, 1);
    }
}

int JBossBase::_ProcessCollisionWithFire(JEnemySubChara** part, JFireChara** fire, JExplodeRegionList* r, int bonus) {
    if (!*fire) return 0;
    if (!*part || !(*part)->Collision_Full(*fire)) return 0;
    (*part)->DecreaseShield((*fire)->firePower);
    int fx = (*fire)->GetCenterX(), fy = (*fire)->GetCenterY();
    SafeDelete(*fire);
    if ((*part)->shield > 0) {
        r->expList->CreateExplode(fx, fy, 2, 1);
        return 0;
    }
    r->CreateExplodeRegion(*part, bonus ? 100 : 48, bonus != 0);
    SafeDelete(*part);
    return 1;
}

// ------------------------------------------------------------------ Boss 1
class JBossChara01 : public JBossBase {
public:
    JBossChara01() {
        const int s[7] = {0xaf, 0xb0, 0xb1, 0xb4, 0xb4, 0xfe, 0xfe};
        for (int i = 0; i < 7; i++) bossScript[i] = new JScript(s[i]);
    }
    void CreateBoss() override {
        bossChara[0] = NewPart(0xae, 0, 1);
        bossChara[0]->SetCenterPosition(DDGetScreenWidth() / 2, 0);
        bossChara[0]->SetPosY(-bossChara[0]->Height());
        bossChara[0]->shield = 440;
        struct P { int res, frames, x, y; } parts[6] = {
            {0xad, 2, 103, -24}, {0xac, 2, -73, -31}, {0xb3, 7, 3, 34},
            {0xb3, 7, 219, 34}, {0xfd, 3, 15, 78}, {0xfd, 3, 231, 78}};
        for (int i = 0; i < 6; i++) {
            bossChara[i + 1] = NewPart(parts[i].res, i + 1, parts[i].frames);
            bossChara[i + 1]->SetPosition(parts[i].x, parts[i].y);
            bossChara[i + 1]->shield = 999999;
            Attach(i + 1);
        }
    }
    void MoveBoss(JBulletList* b, JMyChara* my) override {
        if (Dead()) return;
        bossChara[0]->MoveScript(b, my);
        for (int i = 1; i < 7; i++) MovePart(i, b, my);
    }
    void DrawBoss(Surface* s) override {
        if (Dead()) return;
        if (bossChara[2]) bossChara[2]->Draw(s);
        bossChara[0]->Draw(s);
        for (int i : {1, 3, 4, 5, 6}) if (bossChara[i]) bossChara[i]->Draw(s);
    }
    void CollisionWithFire(JFireList* f, JExplodeRegionList* r) override {
        if (Dead()) return;
        for (int i = 0; i < 12; i++) HitMain(&f->fireChara[i], r, 0, 100, 1);
    }
    void CollisionWithMyChara(JMyChara* my, JExplodeList* e) override {
        if (Dead()) return;
        for (int i = 0; i < 7; i++) if (bossChara[i] && bossChara[i]->Collision_Full(my)) my->DecreaseLife(e);
    }
};

// ------------------------------------------------------------------ Boss 2
class JBossChara02 : public JBossBase {
public:
    JBossChara02() {
        const int s[6] = {0xce, 0xda, 0xdd, 0xdc, 0xdb, 0xe0};
        for (int i = 0; i < 6; i++) bossScript[i] = new JScript(s[i]);
    }
    void CreateBoss() override {
        bossChara[0] = NewPart(0xd0, 0, 2);
        bossChara[0]->SetCenterPosition(-160 + ScreenOX(), 120);
        bossChara[0]->shield = 160;
        const int shift[4][2] = {{122, 0}, {0, 122}, {-122, 0}, {0, -122}};
        for (int i = 1; i <= 4; i++) {
            bossChara[i] = NewPart(0xd9, i, 4);
            bossChara[i]->SetCenterPosition(bossChara[0]->Width() / 2, bossChara[0]->Height() / 2);
            bossChara[i]->ShiftPosition(shift[i - 1][0], shift[i - 1][1]);
            bossChara[i]->shield = 999999;
            bossChara[i]->SetAnimationDelay(2);
            Attach(i);
        }
        bossChara[5] = NewPart(0xdf, 5, 4);
        bossChara[5]->SetPosition(130, 47);
        bossChara[5]->shield = 999999;
        Attach(5);
    }
    void MoveBoss(JBulletList* b, JMyChara* my) override {
        if (Dead()) return;
        bossChara[0]->MoveScript(b, my);
        for (int i = 1; i < 6; i++) {
            if (!bossChara[i]) continue;
            if (i < 5) bossChara[i]->IncrementAnimation();
            MovePart(i, b, my);
        }
    }
    void DrawBoss(Surface* s) override {
        if (Dead()) return;
        bossChara[0]->Draw(s);
        if (bossChara[5]) bossChara[5]->Draw(s);
        for (int i = 1; i <= 4; i++) if (bossChara[i]) bossChara[i]->Draw(s);
    }
    void CollisionWithFire(JFireList* f, JExplodeRegionList* r) override {
        if (Dead()) return;
        for (int i = 0; i < 12; i++) {
            HitMain(&f->fireChara[i], r, 0, 100, 1);
            for (int p = 1; p <= 5; p++) _ProcessCollisionWithFire(&bossChara[p], &f->fireChara[i], r, 0);
        }
    }
    void CollisionWithMyChara(JMyChara* my, JExplodeList* e) override {
        if (Dead()) return;
        for (int i = 0; i < 6; i++) if (bossChara[i] && bossChara[i]->Collision_Full(my)) my->DecreaseLife(e);
    }
};

// ------------------------------------------------------------------ Boss 3
class JBossChara03 : public JBossBase {
public:
    JBossChara03() {
        const int s[4] = {0xe4, 0xe6, 0xe5, 0xeb};
        for (int i = 0; i < 4; i++) bossScript[i] = new JScript(s[i]);
    }
    void CreateBoss() override {
        bossChara[0] = NewPart(0xe1, 0, 1);
        bossChara[0]->SetCenterPosition(DDGetScreenWidth() / 2, 0);
        bossChara[0]->SetPosY(520);
        bossChara[0]->shield = 70;
        bossChara[1] = NewPart(0xe3, 1, 1);
        bossChara[1]->SetPosition(-66, -34);
        bossChara[1]->shield = 420;
        Attach(1);
        bossChara[2] = NewPart(0xe2, 2, 2);
        bossChara[2]->SetPosition(54, 85);
        bossChara[2]->shield = 999999;
        Attach(2);
        bossChara[3] = NewPart(0xea, 3, 2);
        bossChara[3]->SetPosition(112, 77);
        bossChara[3]->shield = 999999;
        Attach(3);
    }
    void MoveBoss(JBulletList* b, JMyChara* my) override {
        if (Dead()) return;
        bossChara[0]->MoveScript(b, my);
        MovePart(3, b, my);
        if (bossChara[1]) {
            MovePart(1, b, my);
            MovePart(2, b, my);
        }
    }
    void DrawBoss(Surface* s) override {
        if (Dead()) return;
        bossChara[0]->Draw(s);
        JEnemySubChara* last;
        if (bossChara[1]) { bossChara[1]->Draw(s); last = bossChara[2]; }
        else last = bossChara[3];
        if (last) last->Draw(s);
    }
    void CollisionWithFire(JFireList* f, JExplodeRegionList* r) override {
        if (Dead()) return;
        for (int i = 0; i < 12; i++) {
            if (!bossChara[1]) _ProcessCollisionWithFire(&bossChara[0], &f->fireChara[i], r, 1);
            else HitMain(&f->fireChara[i], r, 1, 48, 0);
        }
    }
    void CollisionWithMyChara(JMyChara* my, JExplodeList* e) override {
        if (Dead()) return;
        if (bossChara[0]->Collision_Full(my)) my->DecreaseLife(e);
        if (bossChara[1] && bossChara[1]->Collision_Full(my)) my->DecreaseLife(e);
    }
};

// ------------------------------------------------------------------ Boss 4
class JBossChara04 : public JBossBase {
public:
    JBossChara04() {
        const int s[5] = {0xf2, 0xee, 0xef, 0xf0, 0xf1};
        for (int i = 0; i < 5; i++) bossScript[i] = new JScript(s[i]);
    }
    void CreateBoss() override {
        bossChara[0] = NewPart(0xec, 0, 4);
        bossChara[0]->SetCenterPosition(DDGetScreenWidth() / 2, 240);
        bossChara[0]->SetPosY(502);
        bossChara[0]->SetAnimationDelay(2);
        bossChara[0]->shield = 360;
        for (int i = 1; i <= 4; i++) {
            bossChara[i] = NewPart(0xed, i, 4);
            bossChara[i]->SetCenterPosition(bossChara[0]->Width() / 2, bossChara[0]->Height() / 2);
            bossChara[i]->shield = 180;
            bossChara[i]->SetAnimationDelay(2);
            Attach(i);
        }
    }
    void MoveBoss(JBulletList* b, JMyChara* my) override {
        if (Dead()) return;
        bossChara[0]->IncrementAnimation();
        bossChara[0]->MoveScript(b, my);
        for (int i = 1; i <= 4; i++) {
            if (!bossChara[i]) continue;
            bossChara[i]->IncrementAnimation();
            MovePart(i, b, my);
        }
    }
    void DrawBoss(Surface* s) override {
        if (Dead()) return;
        bossChara[0]->Draw(s);
        for (int i = 1; i <= 4; i++) if (bossChara[i]) bossChara[i]->Draw(s);
    }
    void CollisionWithFire(JFireList* f, JExplodeRegionList* r) override {
        if (Dead()) return;
        for (int i = 0; i < 12; i++) {
            HitMain(&f->fireChara[i], r, 0, 100, 1);
            for (int p = 1; p <= 4; p++) _ProcessCollisionWithFire(&bossChara[p], &f->fireChara[i], r, 0);
        }
    }
    void CollisionWithMyChara(JMyChara* my, JExplodeList* e) override {
        if (Dead()) return;
        for (int i = 0; i < 5; i++) if (bossChara[i] && bossChara[i]->Collision_Full(my)) my->DecreaseLife(e);
    }
};

// ------------------------------------------------------------------ Boss 5
class JBossChara05 : public JBossBase {
public:
    JBossChara05() {
        const int s[6] = {0xf8, 0xf9, 0x10c, 0x10a, 0x10e, 0x10f};
        for (int i = 0; i < 6; i++) bossScript[i] = new JScript(s[i]);
    }
    void CreateBoss() override {
        bossChara[0] = NewPart(0xf7, 0, 1);
        bossChara[0]->SetCenterPosition(DDGetScreenWidth() / 2, 0);
        bossChara[0]->SetPosY(-bossChara[0]->Height());
        bossChara[0]->shield = 600;
        struct P { int res, frames, x, y; } parts[3] = {{0xf6, 3, 95, -3}, {0x10b, 3, 131, 80}, {0x109, 2, 28, 102}};
        for (int i = 0; i < 3; i++) {
            bossChara[i + 1] = NewPart(parts[i].res, i + 1, parts[i].frames);
            bossChara[i + 1]->SetPosition(parts[i].x, parts[i].y);
            bossChara[i + 1]->shield = 999999;
            Attach(i + 1);
        }
        bossChara[4] = NewPart(0x10d, 4, 4);
        bossChara[4]->SetPosition(50, 48);
        bossChara[4]->ShiftPosition(-320, 0);
        bossChara[4]->shield = 30;
        bossChara[4]->SetAnimationDelay(2);
        Attach(4);
        bossChara[5] = NewPart(0x10d, 5, 4);
        bossChara[5]->SetPosition(210, 48);
        bossChara[5]->ShiftPosition(320, 0);
        bossChara[5]->shield = 30;
        bossChara[5]->SetAnimationDelay(2);
        Attach(5);
    }
    void MoveBoss(JBulletList* b, JMyChara* my) override {
        if (Dead()) return;
        bossChara[0]->MoveScript(b, my);
        for (int i = 1; i <= 3; i++) MovePart(i, b, my);
        for (int i = 4; i <= 5; i++) {
            if (!bossChara[i]) continue;
            bossChara[i]->IncrementAnimation();
            MovePart(i, b, my);
        }
    }
    void DrawBoss(Surface* s) override {
        if (Dead()) return;
        bossChara[0]->Draw(s);
        for (int i : {1, 4, 5, 3, 2}) if (bossChara[i]) bossChara[i]->Draw(s);
    }
    void CollisionWithFire(JFireList* f, JExplodeRegionList* r) override {
        if (Dead()) return;
        for (int i = 0; i < 12; i++) {
            HitMain(&f->fireChara[i], r, 0, 100, 1);
            _ProcessCollisionWithFire(&bossChara[4], &f->fireChara[i], r, 0);
            _ProcessCollisionWithFire(&bossChara[5], &f->fireChara[i], r, 0);
        }
    }
    void CollisionWithMyChara(JMyChara* my, JExplodeList* e) override {
        if (Dead()) return;
        for (int i = 0; i < 6; i++) if (bossChara[i] && bossChara[i]->Collision_Full(my)) my->DecreaseLife(e);
    }
};

JBossBase* CreateBossByLetter(char c) {
    switch (c) {
    case 'A': return new JBossChara01();
    case 'B': return new JBossChara02();
    case 'C': return new JBossChara03();
    case 'D': return new JBossChara04();
    case 'E': return new JBossChara05();
    default: return nullptr;
    }
}
