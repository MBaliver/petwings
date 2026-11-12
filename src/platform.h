// Platform layer: replaces DirectDraw / DirectInput / DirectSound / MCI / registry
// from the original Windows build with SDL2 (native + Emscripten).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

constexpr int BASE_W = 640;        // the original 4:3 screen width
constexpr int SCREEN_H = 480;
constexpr int MAX_SCREEN_W = 1120; // 21:9 at 480 lines

// ---- DirectInput key codes used by the game (DIK_*) ----
enum : int {
    DIK_ESCAPE = 0x01, DIK_BACK = 0x0E, DIK_RETURN = 0x1C, DIK_LCONTROL = 0x1D,
    DIK_SPACE = 0x39, DIK_NUMPADENTER = 0x9C, DIK_RCONTROL = 0x9D,
    DIK_UP = 0xC8, DIK_LEFT = 0xCB, DIK_RIGHT = 0xCD, DIK_DOWN = 0xD0,
};

// ---- Video: 8-bit indexed back buffer. Width is 640 (original) or wider in the
//      "expanded" widescreen modes; rows are packed (stride == w).
struct Surface {
    int w = BASE_W;
    uint8_t px[MAX_SCREEN_W * SCREEN_H];
};
Surface* DDGetBackBuffer();
int DDGetScreenWidth();                       // logical width of the play field
inline int DDGetScreenHeight() { return SCREEN_H; }
inline int ScreenOX() { return (DDGetScreenWidth() - BASE_W) / 2; }  // offset of the 4:3 area
void FillBufferRect(Surface* s, uint8_t colorIndex);
void FadeOut(Surface* s, int percent);  // black curtain closing from top and bottom
const uint8_t* GetPalette();             // 256 * RGB
uint8_t PaletteMatch(uint32_t rgb);

// ---- Video options (port addition) ----
enum ScaleMode { SCALE_INTEGER = 0, SCALE_FIT, SCALE_STRETCH, SCALE_COUNT };
enum FilterMode { FILTER_NEAREST = 0, FILTER_LINEAR, FILTER_COUNT };
enum AspectMode { ASPECT_4_3 = 0, ASPECT_16_9, ASPECT_21_9, ASPECT_COUNT };
enum WideMode { WIDE_EXPANDED = 0, WIDE_BORDERS, WIDE_COUNT };
struct VideoConfig {
    int scale = SCALE_INTEGER;
    int filter = FILTER_NEAREST;
    int aspect = ASPECT_4_3;
    int wide = WIDE_EXPANDED;
    int fullscreen = 0;
    int musicVolume = 10;  // 0..10
    int sfxVolume = 10;    // 0..10
    int turbo = 0;  // original hidden "S" speed key; only enabled with --turbo (never saved)
    std::string language = "en";
};
VideoConfig& GetConfig();
void ConfigChanged();          // apply + save; logical width changes wait for the next screen
void ApplyPendingVideo();      // called by the game when a new screen starts
bool PlatformHasFullscreenOption();
std::vector<std::string> ListLanguages();

// ---- Input (DirectInput emulation) ----
void DIUpdateInputState();
int DIGetKeyboardPressed(int dik);
int DIGetKeyjoyDirectionX();
int DIGetJoystickDirectionX(int which);
int DIGetJoystickDirectionPressed(int dir);
// buttons: 0 = any face button (fire/confirm, as in the original), 1 = B (back/delete), 2 = Start
int DIGetJoystickButtonPressed(int button);
bool PlatformHasGamepad();
void DIGetKeyboardBuffer(std::string& out);
void DIClearKeyboardBuffer();
void PlatformRequestTextEntry(int maxLen);  // touch devices: ask for name via native prompt

// ---- Sound ----
void SoundInit();
void SoundPlay(unsigned index);  // JSoundList::Play(index, 0)

// ---- Music (MCI sequencer replacement; pre-rendered OGG, looped) ----
void PlayMidi(int track);  // 1..3
void StopMidi();
void PauseMidi();
void ResumeMidi();

// ---- Shared ranking (web build talks to /api/ranking on the same server) ----
enum RankingStatus { RANKING_LOCAL = 0, RANKING_ONLINE, RANKING_OFFLINE };
void PlatformRankingRefresh();                       // async; calls ApplyRankingText() when it arrives
void PlatformRankingSubmit(const std::string& name, long potions, int episode, int level, bool cleared);
RankingStatus PlatformRankingStatus();

// ---- Misc ----
std::string DataPath(const std::string& rel);
bool FileExists(const std::string& path);
bool LoadFile(const std::string& path, std::vector<uint8_t>& out);
std::string StorageLoad(const char* key);
void StorageSave(const char* key, const std::string& value);
void PlatformQuit();
bool PlatformIsWeb();
