// standard.cpp, JDib.cpp, JSprite.cpp, JScript.cpp, map.cpp, JAscii.cpp
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include "game.h"

// ============================================================== standard.cpp
static float SIN_TABLE32[32];
static float COS_TABLE32[32];
static const double PI2 = 6.283185307179586;   // 0x401921fb54442d28
static const double PI_32 = 0.09817477042468103; // 0x3fb921fb54442d28
static const double PI_16 = 0.19634954084936207; // 0x3fc921fb54442d28

void InitSinCosTables32() {
    for (int i = 0; i < 32; i++) {
        double a = (double)i * PI_16;
        double s = std::sin(a) * 1000.0;
        double c = std::cos(a) * 1000.0;
        s = s < 0.0 ? s - 0.5 : s + 0.5;
        c = c < 0.0 ? c - 0.5 : c + 0.5;
        SIN_TABLE32[i] = (float)(int)s / 1000.0f;
        COS_TABLE32[i] = (float)(int)c / 1000.0f;
    }
}
float Sin32(int d) { return d < 0 ? 0.0f : SIN_TABLE32[d & 0x1f]; }
float Cos32(int d) { return d < 0 ? 0.0f : COS_TABLE32[d & 0x1f]; }

int FindDirection32(int x1, int y1, int x2, int y2) {
    double a = std::atan2((double)(y1 - y2), (double)(x2 - x1));
    if (a < 0.0) a += PI2;
    a += PI_32;
    if (PI2 <= a) a -= PI2;
    return (int)(a / PI_16);
}

// MSVC rand() (LCG) so the game "feels" identical; seeded from the clock like GetTickCount().
static uint32_t g_holdrand = 1;
static int msvc_rand() {
    g_holdrand = g_holdrand * 214013u + 2531011u;
    return (int)((g_holdrand >> 16) & 0x7fff);
}
long Random(long n) { return n ? msvc_rand() % n : 0; }
void Randomize() { g_holdrand = (uint32_t)time(nullptr) ^ (uint32_t)clock(); }
long Round(double v) { return v >= 0.0 ? (long)(v + 0.5) : (long)(v - 0.5); }

// ============================================================== JDib
// Images are cached per (language, name) and never freed, so sprites created before a
// language switch keep valid pointers.
static std::map<std::string, std::unique_ptr<Image>> g_images;

static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

// 8-bit BMP only: images are drawn as raw indices with the game's single 256-colour palette.
static bool LoadBmp8(const std::string& path, Image& img) {
    std::vector<uint8_t> f;
    if (!LoadFile(path, f) || f.size() <= 54 || f[0] != 'B' || f[1] != 'M' || rd16(&f[28]) != 8) return false;
    uint32_t off = rd32(&f[10]);
    int w = (int)rd32(&f[18]);
    int h = (int)rd32(&f[22]);
    bool bottomUp = h > 0;
    if (h < 0) h = -h;
    int row = (w + 3) & ~3;
    if (w <= 0 || h <= 0 || off + (size_t)row * h > f.size()) return false;
    img.w = w; img.h = h; img.px.resize((size_t)w * h);
    for (int y = 0; y < h; y++) {
        int sy = bottomUp ? h - 1 - y : y;
        memcpy(&img.px[(size_t)y * w], &f[off + (size_t)sy * row], w);
    }
    return true;
}

static Image* FindImage(const std::string& name, bool required) {
    const std::string& lang = GetConfig().language;
    std::string key = lang + "/" + name;
    auto it = g_images.find(key);
    if (it != g_images.end()) return it->second.get();
    auto img = std::make_unique<Image>();
    bool ok = LoadBmp8(DataPath("lang/" + lang + "/gfx/" + name + ".bmp"), *img) ||
              LoadBmp8(DataPath("gfx/" + name + ".bmp"), *img);
    if (!ok) {
        if (!required) { g_images[key] = nullptr; return nullptr; }
        fprintf(stderr, "PetWings: missing/invalid bitmap %s\n", name.c_str());
        img->w = img->h = 1; img->px.assign(1, 0);
    }
    Image* r = img.get();
    g_images[key] = std::move(img);
    return r;
}

Image* GetImage(int resId) { return FindImage(std::to_string(resId), true); }
Image* GetNamedImage(const std::string& name) { return FindImage(name, false); }

Image* GetOptionsMenuImage() {
    if (Image* custom = GetNamedImage("menu_options")) return custom;
    static std::map<std::string, std::unique_ptr<Image>> built;
    auto& slot = built[GetConfig().language];
    if (slot) return slot.get();
    // Glyph columns in bitmap 255 (5 rows of 26 px: EPISODE 1/2/3, RANKING, EXIT)
    struct G { int row, x0, x1; };
    const G O = {0, 70, 93}, P = {0, 21, 40}, T = {4, 52, 73}, I = {0, 41, 48}, N = {3, 45, 66}, S = {0, 49, 69};
    const G word[] = {O, P, T, I, O, N, S};
    Image* menu = GetImage(0xff);
    int fh = menu->h / 5;
    int w = 1;
    for (const G& g : word) w += g.x1 - g.x0 + 1;
    slot = std::make_unique<Image>();
    slot->w = w; slot->h = fh;
    slot->px.assign((size_t)w * fh, menu->px[0]);  // top-left pixel stays the transparent colour
    int x = 1;
    for (const G& g : word) {
        for (int y = 0; y < fh; y++)
            for (int sx = g.x0; sx <= g.x1; sx++) {
                uint8_t c = menu->px[(size_t)(g.row * fh + y) * menu->w + sx];
                if (c != menu->px[0]) slot->px[(size_t)y * w + x + sx - g.x0] = c;
            }
        x += g.x1 - g.x0 + 1;
    }
    return slot.get();
}

void DrawWideImage(Image* img, Surface* s, int y0) {
    Backdrop& b = GetBackdrop();
    b.type = Backdrop::IMAGE; b.img = img; b.y = y0;
    int ox = (s->w - img->w) / 2;
    for (int y = 0; y < img->h; y++) {
        int ty = y0 + y;
        if (ty < 0 || ty >= SCREEN_H) continue;
        const uint8_t* src = &img->px[(size_t)y * img->w];
        uint8_t* dst = &s->px[(size_t)ty * s->w];
        for (int x = 0; x < s->w; x++) {
            int sx = x - ox;
            if (sx < 0) sx = -sx - 1;
            if (sx >= img->w) sx = 2 * img->w - sx - 1;
            if (sx < 0) sx = 0;
            if (sx >= img->w) sx = img->w - 1;
            dst[x] = src[sx];
        }
    }
}

// ============================================================== backdrop
static Backdrop g_backdrop;
static int g_fadeBand = 0;
Backdrop& GetBackdrop() { return g_backdrop; }
int GetFadeBand() { return g_fadeBand; }
void ResetFrameBackdrop() { g_backdrop = Backdrop(); g_fadeBand = 0; }
void NoteFadeBand(int band) { if (band > g_fadeBand) g_fadeBand = band; }

// ============================================================== language table
static std::map<std::string, std::string> g_lang;

static void LangParse(const std::string& file, std::map<std::string, std::string>& out) {
    std::vector<uint8_t> f;
    if (!LoadFile(file, f)) return;
    std::string s(f.begin(), f.end());
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        std::string line = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? s.size() : nl + 1;
        while (!line.empty() && (line.back() == '\r')) line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos || line[a] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(a, eq - a);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        std::string val = line.substr(eq + 1);
        size_t v0 = val.find_first_not_of(" \t");
        val = v0 == std::string::npos ? "" : val.substr(v0);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        else while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        out[key] = val;
    }
}

void LangLoad(const std::string& lang) {
    g_lang.clear();
    LangParse(DataPath("lang/en.txt"), g_lang);
    if (lang != "en") LangParse(DataPath("lang/" + lang + ".txt"), g_lang);
}

const std::string& Tr(const std::string& key) {
    auto it = g_lang.find(key);
    if (it != g_lang.end()) return it->second;
    return g_lang[key] = key;  // missing entries show their key, which makes them easy to spot
}

std::string LangDisplayName(const std::string& lang) {
    std::map<std::string, std::string> t;
    LangParse(DataPath("lang/" + lang + ".txt"), t);
    auto it = t.find("LANGUAGE_NAME");
    return it != t.end() ? it->second : lang;
}

// ============================================================== JSprite256
JSprite256::JSprite256(int resId, int frames) : numFrames(frames) { SetupSprite(GetImage(resId)); }
JSprite256::JSprite256(Image* i, int frames) : numFrames(frames) { SetupSprite(i); }

void JSprite256::SetupSprite(Image* i) {
    img = i;
    transColor = i->px.empty() ? 0 : i->px[0];
    width = i->w;
    height = i->h / numFrames;
}

int JSprite256::GetIntX() { return (int)Round((double)posX); }
int JSprite256::GetIntY() { return (int)Round((double)posY); }

void JSprite256::Move() {
    posX = Cos32(direct) * speed + posX;
    posY = posY - Sin32(direct) * speed;
}

void JSprite256::DrawAtPos(int dx, int dy, Surface* s) {
    int srcTop = frameIndex * height;
    for (int y = 0; y < height; y++) {
        int ty = dy + y, sy = srcTop + y;
        if (ty < 0 || ty >= SCREEN_H || sy < 0 || sy >= img->h) continue;
        const uint8_t* src = &img->px[(size_t)sy * img->w];
        uint8_t* dst = &s->px[(size_t)ty * s->w];
        int x0 = dx < 0 ? -dx : 0;
        int x1 = dx + width > s->w ? s->w - dx : width;
        if (transOn) {
            for (int x = x0; x < x1; x++) {
                uint8_t c = src[x];
                if (c != transColor) dst[dx + x] = c;
            }
        } else if (x1 > x0) {
            memcpy(dst + dx + x0, src + x0, x1 - x0);
        }
    }
}

void JSprite256::Draw(Surface* s) { DrawAtPos(GetIntX(), GetIntY(), s); }

int JSprite256::Collision_Rect(JSprite256* o) {
    int ax = GetIntX(), ay = GetIntY(), bx = o->GetIntX(), by = o->GetIntY();
    if (o->width - 1 + bx < ax || width - 1 + ax < bx || o->height - 1 + by < ay || ay + height - 1 < by)
        return 0;
    return 1;
}

int JSprite256::Collision_Full(JSprite256* o) {
    int ax = GetIntX(), ay = GetIntY();
    int ar = width + ax, ab = height + ay;
    int bx = o->GetIntX(), by = o->GetIntY();
    int br = o->width + bx, bb = o->height + by;
    if (br - 1 < ax || ar - 1 < bx || bb - 1 < ay || ab - 1 < by) return 0;
    int l = bx <= ax ? ax : bx;
    int t = by <= ay ? ay : by;
    int r = (br - 1 < ar - 1) ? br : ar;
    int b = (bb - 1 < ab - 1) ? bb : ab;
    for (int y = t; y < b; y++) {
        int sa = frameIndex * height + (y - ay);
        int sb = o->frameIndex * o->height + (y - by);
        if (sa < 0 || sa >= img->h || sb < 0 || sb >= o->img->h) continue;
        const uint8_t* pa = &img->px[(size_t)sa * img->w];
        const uint8_t* pb = &o->img->px[(size_t)sb * o->img->w];
        for (int x = l; x < r; x++) {
            if (pa[x - ax] != transColor && pb[x - bx] != o->transColor) return 1;
        }
    }
    return 0;
}

int JSprite256::OutOfScreen_Full() {
    int y = GetIntY(), x = GetIntX();
    if (-height < y && -width < x && y < SCREEN_H && x < DDGetScreenWidth()) return 0;
    return 1;
}

int JSprite256::OutOfScreen_Partial() {
    int y = GetIntY(), x = GetIntX();
    // Note: the original compares y against (screen height - width); kept as-is.
    if (y < 0 || x < 0 || SCREEN_H - width < y || DDGetScreenWidth() - width < x) return 1;
    return 0;
}

void JSprite256::IncrementAnimation() {
    int d = animDelay + 1;
    int n = numFrames * d;
    animIndex = n ? (animIndex + 1) % n : 0;
    frameIndex = animIndex / d;
    if (animIndex == 0 && animFirstTime == 1) animFirstTime = 0;
}

// ============================================================== JScript
std::vector<std::string> GetStringListFromResource(int resId) {
    std::vector<std::string> out;
    std::vector<uint8_t> f;
    if (!LoadFile(DataPath("script/" + std::to_string(resId) + ".txt"), f)) {
        fprintf(stderr, "PetWings: missing script %d\n", resId);
        return out;
    }
    std::string s(f.begin(), f.end());
    size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        std::string line = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        for (char& c : line) if (c == '\r' || c == '\n') c = ' ';
        out.push_back(line);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return out;
}

static std::string Strip(const std::string& s, char ch) {
    size_t a = s.find_first_not_of(ch);
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(ch);
    return s.substr(a, b - a + 1);
}

JScript::JScript(int resId) {
    static const char* names[] = {nullptr, "MOVE", "STOP", "SHIFT_X", "SHIFT_Y", "SPEED", "SPEED_PERCENT",
        "DIRECTION", "DIRECTION_TO_MYCHARA", "DIRECTION_REVERSE", "ROTATE_LEFT", "ROTATE_RIGHT", "LABEL",
        "GOTO_LABEL", "SET_FRAME", "INCR_FRAME", "REPEAT", "REPEAT_END", "SHOOT", "SET_BULLET_OFFSET",
        "PLAY_SOUND", "GOSUB", "RETURN"};
    for (const std::string& raw : GetStringListFromResource(resId)) {
        std::string line = Strip(raw, ' ');
        if (line.empty()) continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '~') break;
        std::string cmd, v1 = "0", v2 = "0";
        size_t sp = line.find(' ');
        if (sp == std::string::npos || sp == 0) {
            cmd = line;
        } else {
            cmd = Strip(line.substr(0, sp), ' ');
            std::string args = Strip(line.substr(sp), ' ');
            size_t comma = args.find(',');
            if (comma == std::string::npos || comma == 0) {
                v1 = args;
            } else {
                v1 = Strip(args.substr(0, comma), ',');
                v2 = Strip(args.substr(comma), ',');
            }
        }
        JScriptItem it;
        it.value1 = atol(v1.c_str());
        it.value2 = atol(v2.c_str());
        for (int a = 1; a <= RETURN; a++)
            if (cmd == names[a]) { it.action = a; break; }
        items.push_back(it);
    }
    if (items.empty()) items.push_back(JScriptItem{STOP, 1000000, 0});
}

// ============================================================== JMap
void JMap::Reset(int episode) {
    int id = 0;
    if (episode == 1) id = 198;
    else if (episode == 2) id = 199;
    else if (episode == 3) id = 200;
    else if (episode == 4) id = 294;
    mapList.clear();
    for (const std::string& raw : GetStringListFromResource(id)) {
        std::string line = Strip(raw, ' ');
        if (line.empty()) continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '~') break;
        mapList.push_back(line);
    }
}

char JMap::GetMapData(int x, int y) {
    if ((unsigned)(gameLevel - 1) < 16 && y >= 0 && y < (int)mapList.size()) {
        size_t idx = (size_t)y + (size_t)(gameLevel * 3 - 3) * 2;
        if (idx >= mapList.size()) return 0;
        const std::string& s = mapList[idx];
        if (x >= 0 && x < (int)s.size()) return s[x];
    }
    return 0;
}

// ============================================================== JAscii / JNumber
JAscii::JAscii() {
    asciiSet1 = new JSprite256(0xcd, 0x26);
    asciiSet2 = new JSprite256(0xcc, 0x26);
}
JAscii::~JAscii() { delete asciiSet1; delete asciiSet2; }

int JAscii::DrawText(int x, int y, const char* text) {
    if (!lpSurface) return x;
    JSprite256* f = fontType == 0 ? asciiSet1 : asciiSet2;
    int n = (int)strlen(text);
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        int fr = -1;
        if (c >= 'A' && c <= 'Z') fr = c - 'A';
        else if (c >= 'a' && c <= 'z') fr = c - 'a';
        else if (c >= '0' && c <= '9') fr = c - 0x16;
        else if (c == '-') fr = 0x24;
        else if (c == '.') fr = 0x25;
        else if (c == ':') {  // port: no colon glyph in the font, build it from two dots
            f->SetFrameIndex(0x25);
            f->SetPosition(f->Width() * i + x, y);
            f->Draw(lpSurface);
            f->SetPosition(f->Width() * i + x, y - f->Height() / 3);
            f->Draw(lpSurface);
        }
        if (fr >= 0) {
            f->SetFrameIndex(fr);
            f->SetPosition(f->Width() * i + x, y);
            f->Draw(lpSurface);
        }
    }
    return f->Width() * n + x;
}

int JAscii::TextWidth(const char* text) { return asciiSet1->Width() * (int)strlen(text); }

JNumber::JNumber() { numberSet1 = new JSprite256(0x6c, 0xb); }
JNumber::~JNumber() { delete numberSet1; }

int JNumber::DrawDash(int x, int y) {
    if (!lpSurface) return x;
    numberSet1->SetFrameIndex(10);
    numberSet1->SetPosition(x, y);
    numberSet1->Draw(lpSurface);
    return numberSet1->Width() + x;
}

int JNumber::DrawNumbers(int x, int y, int value, int digits) {
    if (!lpSurface) return x;
    JSprite256* f = numberSet1;
    char buf[16];
    snprintf(buf, sizeof buf, "%d", value);
    int len = (int)strlen(buf);
    if (digits > 0 && len < digits) {
        for (int i = digits - len; i > 0; i--) {
            f->SetFrameIndex(0);
            f->SetPosition(x, y);
            f->Draw(lpSurface);
            x += f->Width();
        }
    }
    for (int i = 0, rem = len; i < len; i++, rem--) {
        if (rem <= digits) {
            f->SetFrameIndex(buf[i] - '0');
            f->SetPosition(x, y);
            f->Draw(lpSurface);
            x += f->Width();
        }
    }
    return x;
}
