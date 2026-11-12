// SDL2 platform layer for the PetWings source port (Linux / Web via Emscripten / mobile browsers).
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <map>
#include "game.h"

#define STB_VORBIS_HEADER_ONLY
#include "../third_party/stb_vorbis.c"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// ======================================================================= state
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_screenTex = nullptr;  // composed frame, up to MAX_SCREEN_W x 480
static SDL_Texture* g_discTex = nullptr;
static Surface g_backBuffer;                // what the game draws into (logical play field)
static Surface g_out;                       // composed frame: play field + widescreen borders + OSD
static uint8_t g_palette[256 * 3];
static uint32_t g_rgb[256], g_rgbDark[256];
static bool g_running = true;
static bool g_paused = false;
static bool g_frameDirty = true;
static double g_accum = 0;
static uint64_t g_lastCounter = 0;
static SDL_GameController* g_pad = nullptr;
static VideoConfig g_config;
static int g_pendingWidth = -1;
static std::string g_osdLine1, g_osdLine2;
static uint32_t g_osdUntil = 0;
static bool g_testMode = false;

// ======================================================================= video
Surface* DDGetBackBuffer() { return &g_backBuffer; }
int DDGetScreenWidth() { return g_backBuffer.w; }
const uint8_t* GetPalette() { return g_palette; }

void FillBufferRect(Surface* s, uint8_t c) { memset(s->px, c, (size_t)s->w * SCREEN_H); }

void FadeOut(Surface* s, int percent) {
    int p = std::max(0, std::min(100, percent));
    if (p == 100) { memset(s->px, 0, (size_t)s->w * SCREEN_H); NoteFadeBand(SCREEN_H / 2); return; }
    if (p < 1) return;
    int band = (int)((float)SCREEN_H * 0.5f * ((float)p / 100.0f));
    NoteFadeBand(band);
    memset(s->px, 0, (size_t)band * s->w);
    memset(s->px + (size_t)(SCREEN_H - band) * s->w, 0, (size_t)band * s->w);
}

uint8_t PaletteMatch(uint32_t rgb) {
    int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255, best = 0, bestD = 1 << 30;
    for (int i = 0; i < 256; i++) {
        int dr = g_palette[i * 3] - r, dg = g_palette[i * 3 + 1] - g, db = g_palette[i * 3 + 2] - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bestD) { bestD = d; best = i; }
    }
    return (uint8_t)best;
}

static void LoadPalette() {
    std::vector<uint8_t> f;
    if (LoadFile(DataPath("gfx/106.bmp"), f) && f.size() >= 54 + 1024) {
        for (int i = 0; i < 256; i++) {
            g_palette[i * 3 + 0] = f[54 + i * 4 + 2];
            g_palette[i * 3 + 1] = f[54 + i * 4 + 1];
            g_palette[i * 3 + 2] = f[54 + i * 4 + 0];
        }
    } else {
        for (int i = 0; i < 256; i++) g_palette[i * 3] = g_palette[i * 3 + 1] = g_palette[i * 3 + 2] = (uint8_t)i;
    }
    for (int i = 0; i < 256; i++) {
        const uint8_t* c = &g_palette[i * 3];
        g_rgb[i] = 0xFF000000u | (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
        // widescreen "borders": the extended scenery is shown darker than the real play field
        g_rgbDark[i] = 0xFF000000u | (uint32_t)(c[0] * 2 / 5) << 16 | (uint32_t)(c[1] * 2 / 5) << 8 | (c[2] * 2 / 5);
    }
}

// ======================================================================= input
struct InputState { bool key[256]; int joyDir; int joyButtons; };  // bit0 face button, bit1 B, bit2 Start
static InputState g_cur{}, g_prev{};
static bool g_keyHeld[256], g_keyLatch[256];
static int g_padButtonsHeld, g_padButtonsLatch;
static std::string g_textBuffer;

// touch controls
static bool g_touchMode = false;
struct Finger { float x, y; };
static std::map<SDL_FingerID, Finger> g_fingers;
static bool g_touchFireLatch = false, g_touchFireWasDown = false;
static int g_touchDirLatch = -1, g_touchDirPrev = -1;
struct Layout { SDL_Rect game; float padX, padY, padR, fireX, fireY, fireR, pauseX, pauseY, pauseR; bool overlay; };
static Layout g_layout;

static int DikFromScancode(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03; case SDL_SCANCODE_3: return 0x04;
    case SDL_SCANCODE_4: return 0x05; case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
    case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09; case SDL_SCANCODE_9: return 0x0A;
    case SDL_SCANCODE_0: return 0x0B; case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11; case SDL_SCANCODE_E: return 0x12;
    case SDL_SCANCODE_R: return 0x13; case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17; case SDL_SCANCODE_O: return 0x18;
    case SDL_SCANCODE_P: return 0x19; case SDL_SCANCODE_RETURN: return 0x1C; case SDL_SCANCODE_LCTRL: return 0x1D;
    case SDL_SCANCODE_A: return 0x1E; case SDL_SCANCODE_S: return 0x1F; case SDL_SCANCODE_D: return 0x20;
    case SDL_SCANCODE_F: return 0x21; case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25; case SDL_SCANCODE_L: return 0x26;
    case SDL_SCANCODE_Z: return 0x2C; case SDL_SCANCODE_X: return 0x2D; case SDL_SCANCODE_C: return 0x2E;
    case SDL_SCANCODE_V: return 0x2F; case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32; case SDL_SCANCODE_PERIOD: return 0x34; case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_KP_7: return 0x47; case SDL_SCANCODE_KP_8: return 0x48; case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A; case SDL_SCANCODE_KP_4: return 0x4B; case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_KP_6: return 0x4D; case SDL_SCANCODE_KP_1: return 0x4F; case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51; case SDL_SCANCODE_KP_0: return 0x52; case SDL_SCANCODE_KP_PERIOD: return 0x53;
    case SDL_SCANCODE_KP_ENTER: return 0x9C; case SDL_SCANCODE_RCTRL: return 0x9D;
    case SDL_SCANCODE_UP: return 0xC8; case SDL_SCANCODE_LEFT: return 0xCB;
    case SDL_SCANCODE_RIGHT: return 0xCD; case SDL_SCANCODE_DOWN: return 0xD0;
    default: return -1;
    }
}

// JDinput.cpp ConvertKeyCodeToAscii
static char ConvertKeyCodeToAscii(int dik) {
    static const char* row1 = "QWERTYUIOP";  // 0x10..0x19
    static const char* row2 = "ASDFGHJKL";   // 0x1E..0x26
    static const char* row3 = "ZXCVBNM";     // 0x2C..0x32
    if (dik >= 0x02 && dik <= 0x0A) return (char)('1' + dik - 0x02);
    switch (dik) {
    case 0x4F: return '1'; case 0x50: return '2'; case 0x51: return '3'; case 0x4B: return '4';
    case 0x4C: return '5'; case 0x4D: return '6'; case 0x47: return '7'; case 0x48: return '8';
    case 0x49: return '9'; case 0x0B: case 0x52: return '0'; case 0x0C: case 0x4A: return '-';
    case 0x0E: case 0xCB: return '\b'; case 0x1C: case 0x9C: return '\n';
    case 0x34: case 0x53: return '.'; case 0x39: return ' ';
    }
    if (dik >= 0x10 && dik <= 0x19) return row1[dik - 0x10];
    if (dik >= 0x1E && dik <= 0x26) return row2[dik - 0x1E];
    if (dik >= 0x2C && dik <= 0x32) return row3[dik - 0x2C];
    return 0;
}

static int TouchDirection() {
    for (auto& [id, f] : g_fingers) {
        float dx = f.x - g_layout.padX, dy = f.y - g_layout.padY;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > g_layout.padR * 1.6f || d < g_layout.padR * 0.18f) continue;
        if (std::fabs(dx) >= std::fabs(dy)) return dx > 0 ? 0 : 16;
        return dy > 0 ? 24 : 8;
    }
    return -1;
}
static bool TouchFireDown() {
    for (auto& [id, f] : g_fingers) {
        float dx = f.x - g_layout.fireX, dy = f.y - g_layout.fireY;
        if (dx * dx + dy * dy <= (g_layout.fireR * 1.5f) * (g_layout.fireR * 1.5f)) return true;
    }
    return false;
}
static void UpdateTouchEdges() {
    bool fire = TouchFireDown();
    if (fire && !g_touchFireWasDown) g_touchFireLatch = true;
    g_touchFireWasDown = fire;
    int dir = TouchDirection();
    if (dir != -1 && dir != g_touchDirPrev) g_touchDirLatch = dir;
    g_touchDirPrev = dir;
}

static int PadDirection() {
    if (!g_pad) return -1;
    bool l = SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bool r = SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    bool u = SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool d = SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    int ax = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
    const int dz = 16000;
    r |= ax > dz; l |= ax < -dz; d |= ay > dz; u |= ay < -dz;
    // same 8-way mapping as DIGetJoystickDirection
    if (r) return d ? 28 : u ? 4 : 0;
    if (l) return d ? 20 : u ? 12 : 16;
    if (d) return 24;
    if (u) return 8;
    return -1;
}

void DIUpdateInputState() {
    g_prev = g_cur;
    for (int i = 0; i < 256; i++) { g_cur.key[i] = g_keyHeld[i] || g_keyLatch[i]; g_keyLatch[i] = false; }
    int dir = PadDirection();
    if (dir == -1) dir = TouchDirection();
    if (dir == -1 && g_touchDirLatch != -1) dir = g_touchDirLatch;  // a tap shorter than one frame
    g_touchDirLatch = -1;
    g_cur.joyDir = dir;
    g_cur.joyButtons = g_padButtonsHeld | g_padButtonsLatch | ((TouchFireDown() || g_touchFireLatch) ? 1 : 0);
    g_padButtonsLatch = 0;
    g_touchFireLatch = false;
}

int DIGetKeyboardPressed(int dik) { return g_cur.key[dik & 255] && !g_prev.key[dik & 255]; }

int DIGetKeyjoyDirectionX() {
    if (g_cur.key[DIK_RIGHT]) return 0;
    if (g_cur.key[DIK_LEFT]) return 16;
    return -1;
}

static int DirX(int d) {
    if (d == 0 || d == 4 || d == 28) return 0;
    if (d == 12 || d == 16 || d == 20) return 16;
    return -1;
}
int DIGetJoystickDirectionX(int which) { return DirX(which ? g_cur.joyDir : g_prev.joyDir); }
int DIGetJoystickDirectionPressed(int dir) { return g_prev.joyDir != dir && g_cur.joyDir == dir; }
int DIGetJoystickButtonPressed(int b) {
    int m = 1 << b;
    return (g_cur.joyButtons & m) && !(g_prev.joyButtons & m);
}
bool PlatformHasGamepad() { return g_pad != nullptr; }

void DIGetKeyboardBuffer(std::string& out) { out = g_textBuffer; g_textBuffer.clear(); }
void DIClearKeyboardBuffer() { g_textBuffer.clear(); }

void PlatformRequestTextEntry(int maxLen) {
#ifdef __EMSCRIPTEN__
    if (!g_touchMode) return;
    char* r = (char*)EM_ASM_PTR({
        var s = window.prompt("Enter your name", "") || "";
        s = s.toUpperCase().replace(/[^A-Z0-9 .-]/g, "").substring(0, $0);
        return stringToNewUTF8(s);
    }, maxLen);
    if (r) { g_textBuffer = std::string(r) + "\n"; free(r); }
#else
    (void)maxLen;
#endif
}

// ======================================================================= audio
struct Sample { std::vector<float> data; };  // mono, 22050 Hz
struct Voice { const Sample* s = nullptr; double pos = 0; };
struct SoundSlot { Sample sample; std::vector<Voice> voices; unsigned next = 0; };
static SDL_AudioDeviceID g_audioDev = 0;
static int g_audioRate = 44100;
static std::vector<SoundSlot> g_sounds;
static std::vector<uint8_t> g_musicData[4];
static stb_vorbis* g_music = nullptr;
static int g_musicChannels = 2, g_musicRate = 44100;
static bool g_musicPaused = false;
static std::vector<float> g_musicBuf;
static size_t g_musicBufPos = 0;
static double g_musicFrac = 0;
static const float SFX_GAIN = 0.55f, MUSIC_GAIN = 0.8f;

static void FillMusic(float* out, int frames) {
    // Produces stereo frames at g_audioRate from the vorbis stream (linear resampling if needed).
    double step = (double)g_musicRate / g_audioRate;
    const float mg = MUSIC_GAIN * GetConfig().musicVolume / 10.0f;
    for (int i = 0; i < frames; i++) {
        while (g_musicBufPos + 1 >= g_musicBuf.size() / 2) {
            float tmp[2048 * 2];
            int got = stb_vorbis_get_samples_float_interleaved(g_music, 2, tmp, 2048 * 2);
            if (got <= 0) { stb_vorbis_seek_start(g_music); got = stb_vorbis_get_samples_float_interleaved(g_music, 2, tmp, 2048 * 2); }
            if (got <= 0) return;
            std::vector<float> keep(g_musicBuf.begin() + (std::min)(g_musicBuf.size(), g_musicBufPos * 2), g_musicBuf.end());
            keep.insert(keep.end(), tmp, tmp + got * 2);
            g_musicBuf.swap(keep);
            g_musicBufPos = 0;
        }
        size_t p = g_musicBufPos;
        float f = (float)g_musicFrac;
        out[i * 2] += mg * (g_musicBuf[p * 2] * (1 - f) + g_musicBuf[(p + 1) * 2] * f);
        out[i * 2 + 1] += mg * (g_musicBuf[p * 2 + 1] * (1 - f) + g_musicBuf[(p + 1) * 2 + 1] * f);
        g_musicFrac += step;
        while (g_musicFrac >= 1.0) { g_musicFrac -= 1.0; g_musicBufPos++; }
    }
}

static void AudioCallback(void*, Uint8* stream, int len) {
    int frames = len / (int)(sizeof(int16_t) * 2);
    static std::vector<float> mix;
    mix.assign((size_t)frames * 2, 0.0f);
    if (g_music && !g_musicPaused) FillMusic(mix.data(), frames);
    double step = 22050.0 / g_audioRate;
    const float sg = SFX_GAIN * GetConfig().sfxVolume / 10.0f;
    for (auto& slot : g_sounds) {
        for (auto& v : slot.voices) {
            if (!v.s) continue;
            const auto& d = v.s->data;
            for (int i = 0; i < frames; i++) {
                size_t p = (size_t)v.pos;
                if (p >= d.size()) { v.s = nullptr; break; }
                float smp = d[p] * sg;
                mix[i * 2] += smp;
                mix[i * 2 + 1] += smp;
                v.pos += step;
            }
        }
    }
    int16_t* out = (int16_t*)stream;
    for (int i = 0; i < frames * 2; i++) {
        float x = std::max(-1.0f, std::min(1.0f, mix[i]));
        out[i] = (int16_t)(x * 32767.0f);
    }
}

void SoundInit() {
    // JSoundList order: index -> (resource id, simultaneous buffers)
    static const int ids[18][2] = {{0x8e, 1}, {0x90, 2}, {0x8f, 2}, {0x91, 1}, {0xb6, 1}, {0xb7, 1},
        {0xb8, 1}, {0xb9, 1}, {0xba, 1}, {0xc9, 1}, {0xca, 1}, {0xcb, 1}, {0xd5, 1}, {0xf3, 1},
        {0xf5, 1}, {0x112, 1}, {0x111, 1}, {0x136, 1}};
    g_sounds.resize(18);
    for (int i = 0; i < 18; i++) {
        SoundSlot& slot = g_sounds[i];
        slot.voices.resize(ids[i][1]);
        std::vector<uint8_t> f;
        if (!LoadFile(DataPath("sfx/" + std::to_string(ids[i][0]) + ".wav"), f)) continue;
        // walk RIFF chunks like JSoundList::AddWave
        size_t p = 12;
        int bits = 8;
        while (p + 8 <= f.size()) {
            uint32_t id = f[p] | f[p + 1] << 8 | f[p + 2] << 16 | (uint32_t)f[p + 3] << 24;
            uint32_t sz = f[p + 4] | f[p + 5] << 8 | f[p + 6] << 16 | (uint32_t)f[p + 7] << 24;
            if (id == 0x20746d66 && p + 8 + 16 <= f.size()) bits = f[p + 8 + 14];
            if (id == 0x61746164) {
                size_t end = std::min(f.size(), p + 8 + (size_t)sz);
                for (size_t q = p + 8; q < end; q += bits / 8)
                    slot.sample.data.push_back(bits == 8 ? (f[q] - 128) / 128.0f
                                                         : (int16_t)(f[q] | f[q + 1] << 8) / 32768.0f);
                break;
            }
            p += 8 + ((sz + 1) & ~1u);
        }
    }
    for (int t = 1; t <= 3; t++) {
        char name[32];
        snprintf(name, sizeof name, "music/music%02d.ogg", t);
        if (!LoadFile(DataPath(name), g_musicData[t])) fprintf(stderr, "PetWings: missing %s\n", name);
    }
}

void SoundPlay(unsigned index) {
    if (!g_audioDev || index >= g_sounds.size()) return;
    SDL_LockAudioDevice(g_audioDev);
    SoundSlot& slot = g_sounds[index];
    if (!slot.voices.empty() && !slot.sample.data.empty()) {
        Voice& v = slot.voices[slot.next];
        slot.next = (slot.next + 1) % slot.voices.size();
        v.s = &slot.sample;
        v.pos = 0;
    }
    SDL_UnlockAudioDevice(g_audioDev);
}

void StopMidi() {
    if (!g_audioDev) return;
    SDL_LockAudioDevice(g_audioDev);
    if (g_music) stb_vorbis_close(g_music);
    g_music = nullptr;
    SDL_UnlockAudioDevice(g_audioDev);
}

void PlayMidi(int track) {
    if (!g_audioDev || track < 1 || track > 3 || g_musicData[track].empty()) return;
    StopMidi();
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_memory(g_musicData[track].data(), (int)g_musicData[track].size(), &err, nullptr);
    if (!v) return;
    stb_vorbis_info info = stb_vorbis_get_info(v);
    SDL_LockAudioDevice(g_audioDev);
    g_music = v;
    g_musicChannels = info.channels;
    g_musicRate = (int)info.sample_rate;
    g_musicBuf.clear();
    g_musicBufPos = 0;
    g_musicFrac = 0;
    g_musicPaused = false;
    SDL_UnlockAudioDevice(g_audioDev);
}
void PauseMidi() { g_musicPaused = true; }
void ResumeMidi() { g_musicPaused = false; }

// ======================================================================= files & storage
std::string DataPath(const std::string& rel) {
#ifdef __EMSCRIPTEN__
    return "/data/" + rel;
#else
    static std::string base;
    if (base.empty()) {
        const char* env = getenv("PETWINGS_DATA");
        std::vector<std::string> cands;
        if (env) cands.push_back(std::string(env) + "/");
        if (char* b = SDL_GetBasePath()) { cands.push_back(std::string(b) + "data/"); cands.push_back(std::string(b) + "../data/"); SDL_free(b); }
        cands.push_back("data/");
        cands.push_back("/usr/share/petwings/data/");
        for (auto& c : cands) {
            if (FILE* f = fopen((c + "gfx/106.bmp").c_str(), "rb")) { fclose(f); base = c; break; }
        }
        if (base.empty()) base = "data/";
    }
    return base + rel;
#endif
}

bool FileExists(const std::string& path) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    SDL_RWclose(rw);
    return true;
}

std::vector<std::string> ListLanguages() {
    std::vector<std::string> out;
    std::string dir = DataPath("lang/");
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.compare(n.size() - 4, 4, ".txt") == 0) out.push_back(n.substr(0, n.size() - 4));
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        if (a == "en" || b == "en") return a == "en" && b != "en";
        return a < b;
    });
    if (out.empty()) out.push_back("en");
    return out;
}

bool LoadFile(const std::string& path, std::vector<uint8_t>& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    Sint64 n = SDL_RWsize(rw);
    out.resize(n > 0 ? (size_t)n : 0);
    size_t got = n > 0 ? SDL_RWread(rw, out.data(), 1, (size_t)n) : 0;
    SDL_RWclose(rw);
    out.resize(got);
    return got > 0;
}

std::string StorageLoad(const char* key) {
#ifdef __EMSCRIPTEN__
    char* s = (char*)EM_ASM_PTR({
        try { var v = localStorage.getItem("petwings_" + UTF8ToString($0)); if (v) return stringToNewUTF8(v); } catch (e) {}
        return 0;
    }, key);
    if (!s) return "";
    std::string r(s);
    free(s);
    return r;
#else
    char* pref = SDL_GetPrefPath("SaitoGames", "PetWings");
    if (!pref) return "";
    std::string path = std::string(pref) + key + ".txt";
    SDL_free(pref);
    std::vector<uint8_t> f;
    if (!LoadFile(path, f)) return "";
    return std::string(f.begin(), f.end());
#endif
}

void StorageSave(const char* key, const std::string& value) {
    if (g_testMode) return;  // automated runs never touch the real high scores
#ifdef __EMSCRIPTEN__
    EM_ASM({ try { localStorage.setItem("petwings_" + UTF8ToString($0), UTF8ToString($1)); } catch (e) {} },
           key, value.c_str());
#else
    char* pref = SDL_GetPrefPath("SaitoGames", "PetWings");
    if (!pref) return;
    std::string path = std::string(pref) + key + ".txt";
    SDL_free(pref);
    if (SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "wb")) {
        SDL_RWwrite(rw, value.data(), 1, value.size());
        SDL_RWclose(rw);
    }
#endif
}

void PlatformQuit() { g_running = false; }
bool PlatformIsWeb() {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
}

// ======================================================================= shared ranking
#ifdef __EMSCRIPTEN__
static RankingStatus g_rankingStatus = RANKING_OFFLINE;
extern "C" EMSCRIPTEN_KEEPALIVE void PetWingsRankingUpdate(const char* text, int ok) {
    g_rankingStatus = ok ? RANKING_ONLINE : RANKING_OFFLINE;
    if (ok && text) {
        ApplyRankingText(text);
        SaveRankList();  // cache for offline use
    }
}
// Pending submissions are kept in localStorage and sent in order before every refresh,
// so scores made while offline reach the server later.
EM_JS(void, js_ranking_sync, (), {
    if (Module.__rankBusy) { Module.__rankAgain = true; return; }
    Module.__rankBusy = true;
    var KEY = 'petwings_pending_scores';
    var pending; try { pending = JSON.parse(localStorage.getItem(KEY) || '[]'); } catch (e) { pending = []; }
    function report(entries, ok) {
        var s = entries ? entries.map(function (e) { return e.potions + ',' + e.name; }).join('\n') : "";
        var p = stringToNewUTF8(s);
        Module._PetWingsRankingUpdate(p, ok ? 1 : 0);
        _free(p);
    }
    function done() {
        Module.__rankBusy = false;
        if (Module.__rankAgain) { Module.__rankAgain = false; js_ranking_sync(); }
    }
    function send() {
        if (!pending.length) {
            return fetch('api/ranking?limit=8', {cache: 'no-store'})
                .then(function (r) { if (!r.ok) throw r.status; return r.json(); })
                .then(function (j) { report(j.entries, true); done(); })
                .catch(function () { report(null, false); done(); });
        }
        fetch('api/ranking', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: pending[0]})
            .then(function (r) {
                // 4xx: the server will never accept it (bad data / rate limit) -> drop it
                if (r.ok || (r.status >= 400 && r.status < 500)) {
                    pending.shift();
                    try { localStorage.setItem(KEY, JSON.stringify(pending)); } catch (e) {}
                    return send();
                }
                throw r.status;
            })
            .catch(function () { report(null, false); done(); });
    }
    send();
});
EM_JS(void, js_ranking_queue, (const char* json), {
    var KEY = 'petwings_pending_scores';
    var pending; try { pending = JSON.parse(localStorage.getItem(KEY) || '[]'); } catch (e) { pending = []; }
    pending.push(UTF8ToString(json));
    try { localStorage.setItem(KEY, JSON.stringify(pending.slice(-20))); } catch (e) {}
});
void PlatformRankingRefresh() { js_ranking_sync(); }
void PlatformRankingSubmit(const std::string& name, long potions, int episode, int level, bool cleared) {
    std::string esc;
    for (char ch : name) if (ch != '"' && ch != '\\' && (unsigned char)ch >= 32) esc += ch;
    std::string json = "{\"name\":\"" + esc + "\",\"potions\":" + std::to_string(potions) +
                       ",\"episode\":" + std::to_string(episode) + ",\"level\":" + std::to_string(level) +
                       ",\"cleared\":" + (cleared ? "true" : "false") + "}";
    js_ranking_queue(json.c_str());
    js_ranking_sync();
}
RankingStatus PlatformRankingStatus() { return g_rankingStatus; }
#else
// Native build: the high scores stay local (no network code on the desktop).
void PlatformRankingRefresh() {}
void PlatformRankingSubmit(const std::string&, long, int, int, bool) {}
RankingStatus PlatformRankingStatus() { return RANKING_LOCAL; }
#endif

// ======================================================================= pause / misc
static void SetPaused(bool p) {
    if (g_paused == p) return;
    g_paused = p;
    GetAppData()->fFlippingPaused = p;
    if (p) PauseMidi(); else ResumeMidi();
}

static void BackToTitle() {
    GAMESTATE s = GetAppData()->gameState;
    if (s == IN_GAME || s == IN_RANKING || s == IN_CONTINUE || s == IN_ENDING || s == IN_OPTIONS) {
        StopMidi();
        GetAppData()->keyCommandEnabled = 1;
        GetAppData()->gameState = START_TITLE;
    } else if (s == IN_TITLE || s == IN_LOGO) {
        if (!PlatformIsWeb()) PlatformQuit();
    }
}

// ======================================================================= video config
VideoConfig& GetConfig() { return g_config; }
bool PlatformHasFullscreenOption() { return !PlatformIsWeb(); }

static const char* SCALE_NAMES[] = {"integer", "fit", "stretch"};
static const char* FILTER_NAMES[] = {"nearest", "linear"};
static const char* ASPECT_NAMES[] = {"4:3", "16:9", "21:9"};
static const char* WIDE_NAMES[] = {"expanded", "borders"};

static int OutputWidth(const VideoConfig& c) {
    return c.aspect == ASPECT_16_9 ? 854 : c.aspect == ASPECT_21_9 ? MAX_SCREEN_W : BASE_W;
}
static int LogicalWidth(const VideoConfig& c) {
    return (c.aspect == ASPECT_4_3 || c.wide == WIDE_BORDERS) ? BASE_W : OutputWidth(c);
}

static int FindName(const char* const* names, int n, const std::string& v) {
    for (int i = 0; i < n; i++) if (v == names[i]) return i;
    return -1;
}

// key/value used by the config file, the command line (--key=value) and the web URL (?key=value)
static bool SetOption(VideoConfig& c, const std::string& key, const std::string& v) {
    int i;
    if (key == "scale" && (i = FindName(SCALE_NAMES, SCALE_COUNT, v)) >= 0) c.scale = i;
    else if (key == "filter" && (i = FindName(FILTER_NAMES, FILTER_COUNT, v)) >= 0) c.filter = i;
    else if (key == "aspect" && (i = FindName(ASPECT_NAMES, ASPECT_COUNT, v)) >= 0) c.aspect = i;
    else if (key == "widescreen" && (i = FindName(WIDE_NAMES, WIDE_COUNT, v)) >= 0) c.wide = i;
    else if (key == "fullscreen") c.fullscreen = (v == "1" || v == "on" || v == "true" || v.empty());
    else if ((key == "language" || key == "lang") && !v.empty()) c.language = v;
    else if (key == "music" || key == "sfx") {
        int n = atoi(v.c_str());
        if (v.empty() || n < 0 || n > 10 || (n == 0 && v != "0")) return false;
        (key == "music" ? c.musicVolume : c.sfxVolume) = n;
    }
    else if (key == "turbo") c.turbo = (v.empty() || v == "1" || v == "on" || v == "true");
    else return false;
    return true;
}

static void LoadConfig() {
    std::string s = StorageLoad("config");
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        std::string line = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? s.size() : nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        size_t eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') SetOption(g_config, line.substr(0, eq), line.substr(eq + 1));
    }
}

static void SaveConfig() {
    const VideoConfig& c = g_config;
    std::string s = std::string("# PetWings port settings\n") +
        "scale=" + SCALE_NAMES[c.scale] + "\nfilter=" + FILTER_NAMES[c.filter] +
        "\naspect=" + ASPECT_NAMES[c.aspect] + "\nwidescreen=" + WIDE_NAMES[c.wide] +
        "\nfullscreen=" + (c.fullscreen ? "1" : "0") + "\nmusic=" + std::to_string(c.musicVolume) +
        "\nsfx=" + std::to_string(c.sfxVolume) + "\nlanguage=" + c.language + "\n";
    StorageSave("config", s);
}

static void ApplyFullscreen() {
    if (!g_window || PlatformIsWeb()) return;
    bool fs = SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (fs != (bool)g_config.fullscreen)
        SDL_SetWindowFullscreen(g_window, g_config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void ConfigChanged() {
    SaveConfig();
    ApplyFullscreen();
    int w = LogicalWidth(g_config);
    g_pendingWidth = w != g_backBuffer.w ? w : -1;
    g_frameDirty = true;
}

void ApplyPendingVideo() {
    if (g_pendingWidth < 0) return;
    g_backBuffer.w = g_pendingWidth;
    FillBufferRect(&g_backBuffer, 0);
    g_pendingWidth = -1;
}

static void ShowOsd(const std::string& a, const std::string& b = "") {
    g_osdLine1 = a;
    g_osdLine2 = b;
    g_osdUntil = SDL_GetTicks() + 2000;
    g_frameDirty = true;
}

// F5-F8: cycle a video option, save it and show what changed
static void VideoHotkey(int which) {
    VideoConfig& c = g_config;
    std::string line;
    switch (which) {
    case 0: c.scale = (c.scale + 1) % SCALE_COUNT;
        line = Tr("OSD_SCALE") + " " + Tr(std::string("VAL_SCALE_") + (c.scale == 0 ? "INTEGER" : c.scale == 1 ? "FIT" : "STRETCH")); break;
    case 1: c.filter = (c.filter + 1) % FILTER_COUNT;
        line = Tr("OSD_FILTER") + " " + Tr(c.filter == 0 ? "VAL_FILTER_NEAREST" : "VAL_FILTER_LINEAR"); break;
    case 2: c.aspect = (c.aspect + 1) % ASPECT_COUNT;
        line = Tr("OSD_ASPECT") + " " + Tr(c.aspect == 0 ? "VAL_ASPECT_4_3" : c.aspect == 1 ? "VAL_ASPECT_16_9" : "VAL_ASPECT_21_9"); break;
    case 3: c.wide = (c.wide + 1) % WIDE_COUNT;
        line = Tr("OSD_WIDESCREEN") + " " + Tr(c.wide == 0 ? "VAL_WIDE_EXPANDED" : "VAL_WIDE_BORDERS"); break;
    }
    ConfigChanged();
    std::string line2;
    if (g_pendingWidth >= 0) {
        // the logical play field can only change between screens
        GAMESTATE st = GetAppData()->gameState;
        if (st == IN_TITLE || st == IN_RANKING || st == IN_LOGO) GetAppData()->gameState = START_TITLE;
        else line2 = Tr("OSD_NEXT_SCREEN");
    }
    ShowOsd(line, line2);
}

// ======================================================================= layout & overlay
static SDL_Rect PlaceGame(const SDL_Rect& R, int cw, int ch) {
    if (g_config.scale == SCALE_STRETCH) return R;
    float sc = std::min((float)R.w / cw, (float)R.h / ch);
    if (g_config.scale == SCALE_INTEGER && sc >= 1.0f) sc = std::floor(sc);
    int w = (int)(cw * sc), h = (int)(ch * sc);
    return {R.x + (R.w - w) / 2, R.y + (R.h - h) / 2, w, h};
}

static void ComputeLayout(int W, int H) {
    Layout& L = g_layout;
    L.overlay = false;
    int cw = std::max(OutputWidth(g_config), g_backBuffer.w);
    bool portrait = H > W;
    if (g_touchMode && portrait) {
        SDL_Rect R = {0, 0, W, (int)(H * 0.62f)};
        L.game = PlaceGame(R, cw, SCREEN_H);
        float top = (float)(L.game.y + L.game.h), area = (float)H - top;
        L.padR = std::min(area * 0.30f, W * 0.20f);
        L.padX = L.padR * 1.35f;
        L.padY = top + area * 0.5f;
        L.fireR = L.padR * 0.85f;
        L.fireX = W - L.fireR * 1.5f;
        L.fireY = L.padY;
    } else {
        L.game = PlaceGame({0, 0, W, H}, cw, SCREEN_H);
        float margin = (float)std::min(L.game.x, W - (L.game.x + L.game.w));
        if (margin * 0.42f >= H * 0.085f) {  // controls fit in the side bars
            L.padR = std::min(H * 0.17f, margin * 0.42f);
            L.overlay = false;
        } else {
            L.padR = H * 0.13f;
            L.overlay = true;
        }
        L.padX = L.overlay ? L.padR * 1.3f : margin * 0.5f;
        L.padY = H - L.padR * 1.5f;
        L.fireR = L.padR * 0.85f;
        L.fireX = L.overlay ? W - L.fireR * 1.5f : W - margin * 0.5f;
        L.fireY = L.padY;
    }
    L.pauseR = std::max(18.0f, std::min(W, H) * 0.045f);
    L.pauseX = W - L.pauseR * 1.4f;
    L.pauseY = L.pauseR * 1.4f;
}

static void CreateDisc() {
    const int N = 128;
    std::vector<uint32_t> px(N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float dx = x + 0.5f - N / 2.0f, dy = y + 0.5f - N / 2.0f;
            float d = std::sqrt(dx * dx + dy * dy) / (N / 2.0f);
            float a = std::max(0.0f, std::min(1.0f, (1.0f - d) * 40.0f));    // anti-aliased edge
            float ring = d > 0.9f ? 1.0f : 0.45f;
            px[y * N + x] = ((uint32_t)(a * ring * 255) << 24) | 0xFFFFFF;
        }
    g_discTex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, N, N);
    SDL_UpdateTexture(g_discTex, nullptr, px.data(), N * 4);
    SDL_SetTextureBlendMode(g_discTex, SDL_BLENDMODE_BLEND);
}

static void DrawDisc(float cx, float cy, float r, Uint8 alpha, Uint8 rr = 255, Uint8 gg = 255, Uint8 bb = 255) {
    SDL_FRect d = {cx - r, cy - r, r * 2, r * 2};
    SDL_SetTextureColorMod(g_discTex, rr, gg, bb);
    SDL_SetTextureAlphaMod(g_discTex, alpha);
    SDL_RenderCopyF(g_renderer, g_discTex, nullptr, &d);
}

static void DrawTriangle(float cx, float cy, float size, int dir, Uint8 alpha) {
    // dir: 0 right, 8 up, 16 left, 24 down
    float a = (float)dir / 32.0f * 6.2831853f;
    float ux = std::cos(a), uy = -std::sin(a);
    SDL_Vertex v[3];
    SDL_Color c = {255, 255, 255, alpha};
    v[0] = {{cx + ux * size, cy + uy * size}, c, {0, 0}};
    v[1] = {{cx - ux * size * 0.5f - uy * size * 0.8f, cy - uy * size * 0.5f + ux * size * 0.8f}, c, {0, 0}};
    v[2] = {{cx - ux * size * 0.5f + uy * size * 0.8f, cy - uy * size * 0.5f - ux * size * 0.8f}, c, {0, 0}};
    SDL_RenderGeometry(g_renderer, nullptr, v, 3, nullptr, 0);
}

static void DrawTouchOverlay() {
    const Layout& L = g_layout;
    Uint8 base = L.overlay ? 70 : 110;
    int held = TouchDirection();
    DrawDisc(L.padX, L.padY, L.padR, base);
    for (int dir : {0, 8, 16, 24}) {
        float a = (float)dir / 32.0f * 6.2831853f;
        float tx = L.padX + std::cos(a) * L.padR * 0.62f, ty = L.padY - std::sin(a) * L.padR * 0.62f;
        DrawTriangle(tx, ty, L.padR * 0.22f, dir, held == dir ? 255 : (Uint8)(base + 80));
    }
    bool fire = TouchFireDown();
    DrawDisc(L.fireX, L.fireY, L.fireR, fire ? 200 : base, 255, 200, 120);
    DrawDisc(L.fireX, L.fireY, L.fireR * 0.45f, fire ? 230 : (Uint8)(base + 60), 255, 160, 80);
    // pause button
    DrawDisc(L.pauseX, L.pauseY, L.pauseR, base);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, (Uint8)(base + 100));
    if (g_paused) {
        DrawTriangle(L.pauseX + L.pauseR * 0.1f, L.pauseY, L.pauseR * 0.5f, 0, (Uint8)(base + 100));
    } else {
        SDL_FRect b1 = {L.pauseX - L.pauseR * 0.35f, L.pauseY - L.pauseR * 0.4f, L.pauseR * 0.22f, L.pauseR * 0.8f};
        SDL_FRect b2 = {L.pauseX + L.pauseR * 0.13f, L.pauseY - L.pauseR * 0.4f, L.pauseR * 0.22f, L.pauseR * 0.8f};
        SDL_RenderFillRectF(g_renderer, &b1);
        SDL_RenderFillRectF(g_renderer, &b2);
    }
}

// ======================================================================= frame
// Side pixel for the "borders" widescreen mode: the current screen's backdrop, extended.
static inline uint8_t BackdropPixel(const Backdrop& b, int gx, int y) {
    switch (b.type) {
    case Backdrop::FILL: return b.color;
    case Backdrop::IMAGE: {
        int iy = y - b.y;
        if (!b.img || iy < 0 || iy >= b.img->h) return 0;
        int iw = b.img->w;
        int sx = gx - (BASE_W - iw) / 2;
        if (sx < 0) sx = -sx - 1;
        if (sx >= iw) sx = 2 * iw - sx - 1;
        sx = std::max(0, std::min(iw - 1, sx));
        return b.img->px[(size_t)iy * iw + sx];
    }
    case Backdrop::TILES: {
        if (!b.img) return 0;
        int t = (int)(b.ticks % 80);
        int tx = ((gx + t) % b.img->w + b.img->w) % b.img->w;
        int ty = (y + t) % b.img->h;
        return b.img->px[(size_t)ty * b.img->w + tx];
    }
    default: return 0;
    }
}

static JAscii* g_osdFont = nullptr;

static void DrawOsd() {
    if (g_osdLine1.empty()) return;
    if (!g_osdFont) { g_osdFont = new JAscii(); g_osdFont->SetFontType(0); }
    g_osdFont->SetSurface(&g_out);
    int lines = g_osdLine2.empty() ? 1 : 2;
    int gh = g_osdFont->GlyphHeight() + 6;
    int w = std::max(g_osdFont->TextWidth(g_osdLine1.c_str()), g_osdFont->TextWidth(g_osdLine2.c_str())) + 24;
    int x0 = (g_out.w - w) / 2, y0 = 196, h = gh * lines + 12;
    for (int y = y0; y < y0 + h; y++) memset(&g_out.px[(size_t)y * g_out.w + x0], 0, w);
    for (int i = 0; i < lines; i++) {
        const std::string& t = i == 0 ? g_osdLine1 : g_osdLine2;
        g_osdFont->DrawText((g_out.w - g_osdFont->TextWidth(t.c_str())) / 2, y0 + 9 + i * gh, t.c_str());
    }
}

static void ComposeFrame() {
    const int gw = g_backBuffer.w;
    const int outW = std::max(OutputWidth(g_config), gw);
    const int ox = (outW - gw) / 2;
    g_out.w = outW;
    for (int y = 0; y < SCREEN_H; y++)
        memcpy(&g_out.px[(size_t)y * outW + ox], &g_backBuffer.px[(size_t)y * gw], gw);
    if (ox > 0) {
        const Backdrop& b = GetBackdrop();
        int band = GetFadeBand();
        for (int y = 0; y < SCREEN_H; y++) {
            uint8_t* row = &g_out.px[(size_t)y * outW];
            bool curtain = y < band || y >= SCREEN_H - band;
            for (int x = 0; x < ox; x++) row[x] = curtain ? 0 : BackdropPixel(b, x - ox, y);
            for (int x = ox + gw; x < outW; x++) row[x] = curtain ? 0 : BackdropPixel(b, x - ox, y);
        }
    }
    DrawOsd();
}

static void UploadFrame() {
    ComposeFrame();
    const int outW = g_out.w, ox = (outW - g_backBuffer.w) / 2;
    void* pixels;
    int pitch;
    if (SDL_LockTexture(g_screenTex, nullptr, &pixels, &pitch) != 0) return;
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)pixels + (size_t)y * pitch);
        const uint8_t* src = &g_out.px[(size_t)y * outW];
        for (int x = 0; x < outW; x++) {
            bool side = x < ox || x >= outW - ox;
            row[x] = (side ? g_rgbDark : g_rgb)[src[x]];
        }
    }
    SDL_UnlockTexture(g_screenTex);
}

static void RenderFrame() {
    int W, H;
    SDL_GetRendererOutputSize(g_renderer, &W, &H);
    if (g_frameDirty) { UploadFrame(); g_frameDirty = false; }
    ComputeLayout(W, H);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_SetTextureScaleMode(g_screenTex, g_config.filter == FILTER_LINEAR ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    Uint8 mod = g_paused ? 110 : 255;
    SDL_SetTextureColorMod(g_screenTex, mod, mod, mod);
    SDL_Rect src = {0, 0, g_out.w, SCREEN_H};
    SDL_RenderCopy(g_renderer, g_screenTex, &src, &g_layout.game);
    if (g_touchMode) DrawTouchOverlay();
    SDL_RenderPresent(g_renderer);
}

static void HandleEvent(const SDL_Event& e) {
    int W, H;
    SDL_GetRendererOutputSize(g_renderer, &W, &H);
    switch (e.type) {
    case SDL_QUIT: g_running = false; break;
    case SDL_KEYDOWN: {
        if (e.key.repeat) break;
        SDL_Scancode sc = e.key.keysym.scancode;
        if (!PlatformIsWeb() && (sc == SDL_SCANCODE_F11 || (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)))) {
            g_config.fullscreen = !g_config.fullscreen;
            ConfigChanged();
            break;
        }
        if (sc >= SDL_SCANCODE_F5 && sc <= SDL_SCANCODE_F8) { VideoHotkey(sc - SDL_SCANCODE_F5); break; }
        if (GetAppData()->keyCommandEnabled &&
            (sc == SDL_SCANCODE_MINUS || sc == SDL_SCANCODE_EQUALS || sc == SDL_SCANCODE_KP_MINUS || sc == SDL_SCANCODE_KP_PLUS)) {
            bool up = sc == SDL_SCANCODE_EQUALS || sc == SDL_SCANCODE_KP_PLUS;
            bool sfx = e.key.keysym.mod & KMOD_SHIFT;
            int& v = sfx ? g_config.sfxVolume : g_config.musicVolume;
            v = std::max(0, std::min(10, v + (up ? 1 : -1)));
            ConfigChanged();
            if (sfx) SoundPlay(11);
            ShowOsd(Tr(sfx ? "OSD_SFX" : "OSD_MUSIC") + " " + (v ? std::to_string(v) : Tr("VAL_OFF")));
            break;
        }
        if (sc == SDL_SCANCODE_ESCAPE) { SetPaused(false); BackToTitle(); break; }
        APPDATA* app = GetAppData();
        if (app->keyCommandEnabled && sc == SDL_SCANCODE_P) { SetPaused(!g_paused); break; }
        if (g_config.turbo && app->keyCommandEnabled && sc == SDL_SCANCODE_S) { app->delay = app->delay == 10 ? 40 : 10; break; }
        int dik = DikFromScancode(sc);
        if (dik >= 0) {
            g_keyHeld[dik] = g_keyLatch[dik] = true;
            if (char c = ConvertKeyCodeToAscii(dik)) g_textBuffer += c;
        }
        break;
    }
    case SDL_KEYUP: {
        int dik = DikFromScancode(e.key.keysym.scancode);
        if (dik >= 0) g_keyHeld[dik] = false;
        break;
    }
    case SDL_CONTROLLERDEVICEADDED:
        if (!g_pad) g_pad = SDL_GameControllerOpen(e.cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (g_pad && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_pad)) == e.cdevice.which) {
            SDL_GameControllerClose(g_pad);
            g_pad = nullptr;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN: {
        int b = e.cbutton.button;
        if (b == SDL_CONTROLLER_BUTTON_START) {
            // during name entry Start confirms the name; otherwise it pauses
            if (!GetAppData()->keyCommandEnabled) { g_padButtonsHeld |= 4; g_padButtonsLatch |= 4; }
            else SetPaused(!g_paused);
            break;
        }
        if (b == SDL_CONTROLLER_BUTTON_BACK) { SetPaused(false); BackToTitle(); break; }
        if (b <= SDL_CONTROLLER_BUTTON_Y) {
            int m = 1 | (b == SDL_CONTROLLER_BUTTON_B ? 2 : 0);
            g_padButtonsHeld |= m;
            g_padButtonsLatch |= m;
        }
        break;
    }
    case SDL_CONTROLLERBUTTONUP: {
        int b = e.cbutton.button;
        if (b == SDL_CONTROLLER_BUTTON_START) g_padButtonsHeld &= ~4;
        if (b <= SDL_CONTROLLER_BUTTON_Y) {
            // keep bit0 while another face button is still held
            bool other = false;
            for (int k = SDL_CONTROLLER_BUTTON_A; k <= SDL_CONTROLLER_BUTTON_Y; k++)
                if (k != b && g_pad && SDL_GameControllerGetButton(g_pad, (SDL_GameControllerButton)k)) other = true;
            if (!other) g_padButtonsHeld &= ~1;
            if (b == SDL_CONTROLLER_BUTTON_B) g_padButtonsHeld &= ~2;
        }
        break;
    }
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION: {
        g_touchMode = true;
        ComputeLayout(W, H);
        float x = e.tfinger.x * W, y = e.tfinger.y * H;
        if (e.type == SDL_FINGERDOWN) {
            float dx = x - g_layout.pauseX, dy = y - g_layout.pauseY;
            if (dx * dx + dy * dy <= g_layout.pauseR * g_layout.pauseR * 2.2f) { SetPaused(!g_paused); break; }
            if (g_paused) SetPaused(false);
        }
        g_fingers[e.tfinger.fingerId] = {x, y};
        UpdateTouchEdges();
        break;
    }
    case SDL_FINGERUP:
        g_fingers.erase(e.tfinger.fingerId);
        UpdateTouchEdges();
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) SetPaused(true);
        else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && !g_touchMode) SetPaused(false);
        else if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) g_frameDirty = true;
        break;
    default: break;
    }
}

static void MainLoopIteration() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) HandleEvent(e);
#ifdef __EMSCRIPTEN__
    double cssW, cssH;
    if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS) {
        int ww, wh;
        SDL_GetWindowSize(g_window, &ww, &wh);
        if ((int)cssW != ww || (int)cssH != wh) SDL_SetWindowSize(g_window, (int)cssW, (int)cssH);
    }
#endif
    if (!g_osdLine1.empty() && SDL_TICKS_PASSED(SDL_GetTicks(), g_osdUntil)) {
        g_osdLine1.clear();
        g_osdLine2.clear();
        g_frameDirty = true;
    }
    uint64_t now = SDL_GetPerformanceCounter();
    double dt = (double)(now - g_lastCounter) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    g_lastCounter = now;
    APPDATA* app = GetAppData();
    if (!g_paused) {
        g_accum += std::min(dt, 250.0);
        int steps = 0;
        while (g_accum >= (double)app->delay && steps < 5 && g_running) {
            g_accum -= (double)app->delay;
            GameProcess();
            g_frameDirty = true;
            steps++;
        }
        if (steps == 5) g_accum = 0;
    }
    RenderFrame();
}

#ifdef __EMSCRIPTEN__
static void EmLoop() {
    MainLoopIteration();
    if (!g_running) emscripten_cancel_main_loop();
}
#endif

// ----------------------------------------------------------------------- headless test mode
// PETWINGS_TEST=<frames> PETWINGS_SHOTS=<dir> [PETWINGS_START=<episode>:<level>:<potions>]
// Runs the game as fast as possible with a simple scripted player and dumps screenshots.
static void SaveShot(const std::string& path) {
    // PETWINGS_SHOT_COMPOSED=1 saves the composed frame (widescreen borders + OSD) instead
    bool composed = getenv("PETWINGS_SHOT_COMPOSED") != nullptr;
    if (composed) ComposeFrame();
    const Surface& src = composed ? g_out : g_backBuffer;
    const int ox = (g_out.w - g_backBuffer.w) / 2;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, src.w, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < src.w; x++) {
            bool side = composed && (x < ox || x >= src.w - ox);
            ((uint32_t*)((uint8_t*)s->pixels + (size_t)y * s->pitch))[x] = (side ? g_rgbDark : g_rgb)[src.px[(size_t)y * src.w + x]];
        }
    SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
}

static int RunTest(int frames, const char* shots) {
    const char* start = getenv("PETWINGS_START");
    int shotEvery = getenv("PETWINGS_SHOT_EVERY") ? atoi(getenv("PETWINGS_SHOT_EVERY")) : 100;
    if (start && std::string(start) == "options") {
        GetAppData()->gameState = START_OPTIONS;
    } else if (start && std::string(start) == "title") {
        GetAppData()->gameState = START_TITLE;
    } else if (start) {
        APPDATA* a = GetAppData();
        sscanf(start, "%d:%d:%d", &a->startingEpisode, &a->startingGameLevel, &a->startingPotions);
        a->gameState = START_GAME;
    }
    GAMESTATE last = IN_NON;
    int dirTimer = 0, dir = 0;
    for (int f = 0; f < frames && g_running; f++) {
        APPDATA* a = GetAppData();
        // scripted player: confirm menus, mash fire, wander left/right, type a name when asked
        memset(g_keyHeld, 0, sizeof g_keyHeld);
        if (getenv("PETWINGS_IDLE")) { GameProcess(); if (shots && f % shotEvery == 0) SaveShot(std::string(shots) + "/f" + std::to_string(100000 + f).substr(1) + ".bmp"); continue; }
        if (getenv("PETWINGS_KEY_AT") && f == atoi(getenv("PETWINGS_KEY_AT"))) {  // synthetic key press
            SDL_Event ev{};
            ev.type = SDL_KEYDOWN;
            ev.key.keysym.scancode = (SDL_Scancode)atoi(getenv("PETWINGS_KEY"));
            HandleEvent(ev);
        }
        if (getenv("PETWINGS_HOTKEY_AT") && f == atoi(getenv("PETWINGS_HOTKEY_AT"))) VideoHotkey(atoi(getenv("PETWINGS_HOTKEY")));
        if (a->gameState == IN_TITLE || a->gameState == IN_CONTINUE || a->gameState == IN_RANKING ||
            a->gameState == IN_ENDING) {
            if (f % 30 == 0) g_keyLatch[DIK_RETURN] = true;
        }
        if (a->gameState == IN_GAME) {
            if (f % 2 == 0) g_keyLatch[DIK_SPACE] = true;
            if (--dirTimer <= 0) { dirTimer = 10 + (int)Random(30); dir = (int)Random(3); }
            if (dir == 1) g_keyHeld[DIK_LEFT] = true;
            if (dir == 2) g_keyHeld[DIK_RIGHT] = true;
            if (!a->keyCommandEnabled && getenv("PETWINGS_PADNAME")) {
                // gamepad letter picker: up x3 (A->D), A, A, up, A, B, Start  => "DD"
                static int k = 0;
                const char* seq = "UUUAAUABS";
                if (f % 4 == 0 && seq[k]) {
                    char c = seq[k++];
                    if (c == 'U') g_touchDirLatch = 8;
                    if (c == 'A') g_padButtonsLatch |= 1;
                    if (c == 'B') g_padButtonsLatch |= 3;
                    if (c == 'S') g_padButtonsLatch |= 4;
                }
            } else if (!a->keyCommandEnabled && f % 60 == 59) g_textBuffer += "TESTER\n";
        }
        if (getenv("PETWINGS_GOD") && GetCurrentGame()) GetCurrentGame()->myChara->life = 5;
        GameProcess();
        if (a->gameState == IN_GAME && GetCurrentGame() && GetCurrentGame()->procType == JGame::CLEARED_FADEOUT &&
            GetCurrentGame()->procTicks == 1)
            fprintf(stderr, "[frame %d] cleared episode %d level %d\n", f, GetCurrentGame()->episode, GetCurrentGame()->gameLevel);
        static int lastProc = -1;
        if (GetCurrentGame() && (int)GetCurrentGame()->procType != lastProc) {
            lastProc = (int)GetCurrentGame()->procType;
            if (getenv("PETWINGS_LOG_PROC")) fprintf(stderr, "[frame %d] proc %d\n", f, lastProc);
        }
        if (a->gameState != last) { fprintf(stderr, "[frame %d] state %d\n", f, (int)a->gameState); last = a->gameState; }
        if (shots && f % shotEvery == 0) SaveShot(std::string(shots) + "/f" + std::to_string(100000 + f).substr(1) + ".bmp");
        static int nameShots = 0;  // PETWINGS_SHOT_NAME=1: a few frames of the name entry screen
        if (shots && getenv("PETWINGS_SHOT_NAME") && GetCurrentGame() && GetCurrentGame()->procType == JGame::NAME_ENTRY &&
            nameShots < 12 && f % 3 == 0)
            SaveShot(std::string(shots) + "/name" + std::to_string(nameShots++) + ".bmp");
    }
    // PETWINGS_COMPOSE=WxH[,WxH...]: also save the final composed output (scaling + touch overlay)
    if (const char* comp = getenv("PETWINGS_COMPOSE")) {
        g_touchMode = true;
        std::string list = comp;
        size_t pos = 0;
        while (pos < list.size()) {
            size_t c = list.find(',', pos);
            std::string item = list.substr(pos, c == std::string::npos ? std::string::npos : c - pos);
            pos = c == std::string::npos ? list.size() : c + 1;
            int w = 0, h = 0;
            if (sscanf(item.c_str(), "%dx%d", &w, &h) != 2) continue;
            SDL_SetWindowSize(g_window, w, h);
            SDL_Event e;
            while (SDL_PollEvent(&e)) {}
            g_frameDirty = true;
            RenderFrame();
            int ow, oh;
            SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
            SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
            SDL_SaveBMP(s, (std::string(shots ? shots : ".") + "/compose_" + item + ".bmp").c_str());
            SDL_FreeSurface(s);
        }
    }
    if (getenv("PETWINGS_KEY_AT")) fprintf(stderr, "[test] frame delay %lu ms, music %d, sfx %d\n", GetAppData()->delay, g_config.musicVolume, g_config.sfxVolume);
    AppShutdown();
    if (g_audioDev) SDL_CloseAudioDevice(g_audioDev);
    g_audioDev = 0;
    SDL_Quit();
    return 0;
}

static void PrintUsage() {
    printf("PetWings source port\n"
           "  --scale=integer|fit|stretch\n"
           "  --filter=nearest|linear\n"
           "  --aspect=4:3|16:9|21:9\n"
           "  --widescreen=expanded|borders   (expanded: wider play field, borders: original 4:3 + scenery)\n"
           "  --fullscreen / --windowed\n"
           "  --lang=<code>                   (data/lang/<code>.txt)\n"
           "  --turbo                         enable the original hidden S key (4x game speed)\n"
           "Hotkeys: F5 scale, F6 filter, F7 aspect, F8 widescreen, F11 fullscreen, P pause, Esc title\n");
}

int main(int argc, char** argv) {
    // command line (and, on the web, the page URL parameters) override the saved settings
    std::vector<std::pair<std::string, std::string>> cli;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { PrintUsage(); return 0; }
        if (a.rfind("--", 0) != 0) continue;
        a = a.substr(2);
        size_t eq = a.find('=');
        std::string k = a.substr(0, eq), v = eq == std::string::npos ? "" : a.substr(eq + 1);
        if (k == "windowed") { k = "fullscreen"; v = "0"; }
        cli.push_back({k, v});
    }
    g_testMode = getenv("PETWINGS_TEST") != nullptr;

    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    LoadPalette();
    std::vector<uint8_t> probe;
    if (!LoadFile(DataPath("gfx/106.bmp"), probe)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PetWings",
            "Game data not found.\nRun tools/prepare_data.py with the original PetWings.exe first.", nullptr);
        return 1;
    }

    // defaults: pixel-perfect integer scaling on desktop, fit on web/mobile
    if (PlatformIsWeb()) g_config.scale = SCALE_FIT;
    if (!g_testMode) LoadConfig();
    g_config.turbo = 0;  // command line / URL only
    for (auto& [k, v] : cli)
        if (!SetOption(g_config, k, v)) fprintf(stderr, "PetWings: ignoring option --%s=%s\n", k.c_str(), v.c_str());
    LangLoad(g_config.language);
    g_backBuffer.w = LogicalWidth(g_config);

    int outW = OutputWidth(g_config), k = 2;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
        k = std::max(1, std::min((usable.w * 9 / 10) / outW, (usable.h * 9 / 10) / SCREEN_H));
        k = std::min(k, 3);
    }
    int winW = outW * k, winH = SCREEN_H * k;
    if (const char* comp = getenv("PETWINGS_COMPOSE")) sscanf(comp, "%dx%d", &winW, &winH);  // test captures
    g_window = SDL_CreateWindow("PetWings", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    g_screenTex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, MAX_SCREEN_W, SCREEN_H);
    CreateDisc();
    SDL_ShowCursor(SDL_DISABLE);
    ApplyFullscreen();
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = AudioCallback;
    g_audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (g_audioDev) { g_audioRate = have.freq; SDL_PauseAudioDevice(g_audioDev, 0); }
    else fprintf(stderr, "PetWings: no audio (%s)\n", SDL_GetError());

    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); break; }

    Randomize();
    InitSinCosTables32();
    SoundInit();
    LoadRankList();
    FillBufferRect(&g_backBuffer, 0);
    g_lastCounter = SDL_GetPerformanceCounter();
    if (const char* t = getenv("PETWINGS_TEST")) return RunTest(atoi(t), getenv("PETWINGS_SHOTS"));

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(EmLoop, 0, 1);
#else
    while (g_running) MainLoopIteration();
    AppShutdown();
    if (g_audioDev) SDL_CloseAudioDevice(g_audioDev);
    SDL_Quit();
#endif
    return 0;
}
