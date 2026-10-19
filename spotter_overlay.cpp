#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <charconv>
#include <string_view>
#include <vector>
#include <algorithm>
#include <atomic>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

// ============================================================================
// 1. EMBEDDED IRACING SHARED MEMORY DEFINITIONS
// ============================================================================
constexpr int IRSDK_MAX_BUFS     = 4;
constexpr int IRSDK_MAX_CARS     = 64;
constexpr int IRSDK_MAX_STRING   = 32;
constexpr int IRSDK_MAX_DESC     = 64;
constexpr int IRSDK_ST_CONNECTED = 1;

enum irsdk_TrkLoc : int {
    irsdk_NotInWorld     = -1,
    irsdk_OffTrack       = 0,
    irsdk_InPitStall     = 1,
    irsdk_AproachingPits = 2,
    irsdk_OnTrack        = 3
};

enum irsdk_CarLeftRight : int {
    irsdk_LROff          = 0,
    irsdk_LRClear        = 1,
    irsdk_LRCarLeft      = 2,
    irsdk_LRCarRight     = 3,
    irsdk_LRCarLeftRight = 4,
    irsdk_LR2CarsLeft    = 5,
    irsdk_LR2CarsRight   = 6
};

#pragma pack(push, 1)
struct irsdk_varBuf {
    int tickCount;
    int bufOffset;
    int pad[2];
};

struct irsdk_header {
    int ver;
    int status;
    int tickRate;
    int sessionInfoUpdate;
    int sessionInfoLen;
    int sessionInfoOffset;
    int numVars;
    int varHeaderOffset;
    int numBuf;
    int bufLen;
    int pad1[2];
    irsdk_varBuf varBuf[IRSDK_MAX_BUFS];
};

struct irsdk_varHeader {
    int  type;
    int  offset;
    int  count;
    bool countAsTime;
    char pad[3];
    char name[IRSDK_MAX_STRING];
    char desc[IRSDK_MAX_DESC];
    char unit[IRSDK_MAX_STRING];
};
#pragma pack(pop)

// ============================================================================
// 2. SPOTTER TUNING CONSTANTS & GEOMETRY
// ============================================================================
constexpr int   WIN_WIDTH          = 520;
constexpr int   WIN_HEIGHT         = 340;
constexpr float BAR_WIDTH          = 26.0f;
constexpr float BAR_HEIGHT         = 290.0f;
constexpr float BAR_TOP_Y          = 25.0f;
constexpr float LEFT_BAR_X         = 24.0f;
constexpr float RIGHT_BAR_X        = WIN_WIDTH - 24.0f - BAR_WIDTH;

constexpr float DEFAULT_CAR_LEN_M  = 4.70f;  // Fallback if class/model is unknown
constexpr float SPLINE_BUFFER_M    = 0.35f;  // Hairpin centerline projection tolerance
constexpr float SEARCH_RADIUS_M    = 55.0f;  // High-closing-speed multi-class lookahead (55m)
constexpr float MAX_FRAME_JUMP_M   = 8.5f;   // Continuity gate for extreme speed deltas
constexpr float EDGE_CAP_FRAC          = 0.07f;  // 7% warning cap when approaching/clearing

constexpr float FRONT_BAR_X            = 76.0f;
constexpr float FRONT_BAR_Y            = 25.0f;
constexpr float FRONT_BAR_WIDTH        = WIN_WIDTH - 2.0f * FRONT_BAR_X; // 368.0f horizontal span between L/R pillars
constexpr float FRONT_BAR_HEIGHT       = 22.0f;
constexpr float FRONT_DRAFT_MAX_GAP_M  = 3.50f;  // Activate front bonnet bar when nose-to-tail gap <= 3.50m
constexpr float FRONT_PULLOUT_GAP_M    = 1.20f;  // Amber "Pull Out Now" threshold <= 1.20m
constexpr float FRONT_DANGER_GAP_M     = 0.35f;  // Red "Bonnet Contact Imminent" threshold <= 0.35m
constexpr float FRONT_MAX_OVERLAP_M    = -0.45f; // Allow slight nose overlap before CarLeftRight switches to side-by-side

constexpr DWORD CHIME_COOLDOWN_MS      = 280;    // Anti-flutter cooldown per side

// ============================================================================
// 3. MULTI-CLASS CHASSIS LENGTH DATABASE & ZERO-ALLOCATION YAML PARSER
// ============================================================================
struct CarLengthEntry {
    std::string_view token;
    float            lengthM;
};

static wchar_t g_iniPath[MAX_PATH] = {};

// Tier 1: Substring match on DriverInfo -> Drivers -> CarPath (exact to the cm)
constexpr CarLengthEntry kChassisLengthTable[] = {
    // Lightweight / Club / British Track Cars
    { "caterham420r",          3.35f }, // Caterham Seven 420R (3350 mm)
    { "caterham",              3.35f }, // Caterham Seven series
    { "specracer",             3.66f }, // SCCA Spec Racer Ford Gen3
    { "radicalsr10",           4.08f }, // Radical SR10
    { "radicalsr8",            4.10f }, // Radical SR8
    { "mx5",                   3.92f }, // Global Mazda MX-5 Cup (ND)
    { "toyotagr86",            4.27f }, // Toyota GR86
    { "renaultcliocup",        4.05f }, // Renault Clio R.S. Line
    { "clio",                  4.05f },
    { "solstice",              3.99f }, // Pontiac Solstice
    { "jetty",                 4.55f }, // VW Jetta TDI Cup
    { "fr500s",                4.77f }, // Ford Mustang FR500S
    { "kiaoptima",             4.85f }, // Kia Optima

    // GTP / LMDh / LMH / LMP1 Hypercars
    { "porsche963gtp",         5.10f },
    { "cadillacvseriesrgtp",   5.10f },
    { "acuraarx06gtp",         5.10f },
    { "bmwlmdh",               5.10f },
    { "ferrari499p",           4.99f },
    { "porsche919",            4.65f },
    { "audir18",               4.65f },
    { "hpdarx01c",             4.65f },
    { "nissan_gtp_zxt",        4.80f },

    // LMP2 & LMP3
    { "dallarap217",           4.75f },
    { "ligierjsp320",          4.61f },
    { "riley_mkxx",            4.72f },

    // GT3 & GTE
    { "bmwm4gt3",              5.02f },
    { "fordmustanggt3",        4.91f },
    { "mercedesamggt3",        4.75f },
    { "chevycorvettez06gt3r",  4.72f },
    { "c8rvettegte",           4.63f },
    { "mclaren720sgt3",        4.66f },
    { "porsche992gt3r",        4.63f },
    { "porsche991rsr",         4.56f },
    { "acuransxevo22gt3",      4.59f },
    { "ferrari296gt3",         4.57f },
    { "ferrari488gt3",         4.57f },
    { "ferrari488gte",         4.57f },
    { "audir8lmsevo2gt3",      4.56f },
    { "audir8gt3",             4.56f },
    { "lamborghinievogt3",     4.55f },
    { "fordgt_gt3",            4.64f },
    { "bmwz4gt3",              4.40f },
    { "mclarenmp4",            4.54f },

    // Porsche Cup, GT4, TCR & Supercars
    { "porsche992cup",         4.57f },
    { "porsche911cup",         4.56f },
    { "bmwm4evogt4",           4.80f },
    { "bmwm4gt4",              4.80f },
    { "mercedesamggt4",        4.62f },
    { "mclaren570sgt4",        4.53f },
    { "amvantagegt4",          4.47f },
    { "porsche718gt4",         4.46f },
    { "hyundaielantracn7",     4.71f },
    { "hyundaivelostern",      4.25f },
    { "hondacivictyper",       4.59f },
    { "audirs3lms",            4.59f },
    { "supercars",             4.95f },

    // Formula / Open Wheel
    { "mercedesw13",           5.60f },
    { "mercedesw12",           5.60f },
    { "mclarenmp430",          5.13f },
    { "williamsfw31",          4.80f },
    { "superformulasf23",      5.23f },
    { "dallarair18",           5.12f },
    { "dallarair01",           4.90f },
    { "lotus79",               4.47f },
    { "lotus49",               4.01f },
    { "superformulalights324", 4.59f },
    { "dallaraf3",             4.59f },
    { "formularenault35",      4.73f },
    { "formularenault20",      4.30f },
    { "fiaf4",                 4.35f },
    { "usf2000",               4.22f },
    { "indylights",            4.88f },
    { "raygr22",               4.01f }, // Ray FF1600
    { "rt2000",                3.91f }, // Skip Barber Formula 2000
    { "formulavee",            3.65f }, // Formula Vee

    // NASCAR / Oval / Short Track
    { "nascarcupnextgen",      4.91f },
    { "nascartruck",           5.24f },
    { "nascarxfinity",         4.96f },
    { "arca",                  5.00f },
    { "latemodel",             4.90f },
    { "streetstock",           5.05f },
    { "legends",               3.71f },
    { "midget",                3.05f },
    { "sprint",                3.55f }
};

// Tier 2: Fallback match on CarClassShortName / CarScreenNameShort
constexpr CarLengthEntry kClassFallbackTable[] = {
    { "Caterham", 3.35f },
    { "420R",     3.35f },
    { "GTP",      5.10f },
    { "LMP2",     4.75f },
    { "LMP3",     4.61f },
    { "GTE",      4.60f },
    { "GT3",      4.72f },
    { "GT4",      4.58f },
    { "TCR",      4.60f },
    { "PCup",     4.57f },
    { "SF23",     5.23f },
    { "F3",       4.59f },
    { "F4",       4.35f },
    { "FF1600",   4.01f },
    { "Vee",      3.65f },
    { "MX-5",     3.92f },
    { "GR86",     4.27f },
    { "Clio",     4.05f },
    { "SRF",      3.66f }
};

struct SessionMetadata {
    float trackLenM                  = 5000.0f;
    int   paceCarIdx                 = 0;
    float carLenM[IRSDK_MAX_CARS]    = {};
    bool  isPaceCar[IRSDK_MAX_CARS]  = {};

    SessionMetadata() {
        std::fill(std::begin(carLenM), std::end(carLenM), DEFAULT_CAR_LEN_M);
    }
};

static std::string_view TrimYamlValue(std::string_view raw) {
    while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t' || raw.front() == '"')) {
        raw.remove_prefix(1);
    }
    while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t' ||
                            raw.back() == '\r' || raw.back() == '\n' || raw.back() == '"')) {
        raw.remove_suffix(1);
    }
    return raw;
}

static std::string_view ExtractYamlLineValue(std::string_view block, std::string_view key) {
    size_t pos = block.find(key);
    if (pos == std::string_view::npos) return {};
    size_t valStart = pos + key.size();
    size_t lineEnd  = block.find('\n', valStart);
    if (lineEnd == std::string_view::npos) lineEnd = block.size();
    return TrimYamlValue(block.substr(valStart, lineEnd - valStart));
}

static int ParseIntView(std::string_view sv, int fallback = -1) {
    int value = fallback;
    if (!sv.empty()) {
        std::from_chars(sv.data(), sv.data() + sv.size(), value);
    }
    return value;
}

static float ResolveCarLengthMeters(std::string_view carPath,
                                    std::string_view classShortName,
                                    std::string_view screenNameShort) {
    // 0. Check user-configurable [CarLengths] in spotter_overlay.ini first
    char iniPathA[MAX_PATH] = {};
    if (g_iniPath[0] != L'\0') {
        WideCharToMultiByte(CP_ACP, 0, g_iniPath, -1, iniPathA, MAX_PATH, nullptr, nullptr);
    }
    char keyBuf[64] = {};
    if (!carPath.empty()) {
        size_t n = std::min(carPath.size(), sizeof(keyBuf) - 1);
        std::memcpy(keyBuf, carPath.data(), n);
        keyBuf[n] = '\0';
        if (iniPathA[0] != '\0') {
            char valBuf[32] = {};
            GetPrivateProfileStringA("CarLengths", keyBuf, "", valBuf, sizeof(valBuf), iniPathA);
            if (valBuf[0] != '\0') {
                float customLen = 0.0f;
                auto [p, ec] = std::from_chars(valBuf, valBuf + std::strlen(valBuf), customLen);
                if (ec == std::errc() && customLen >= 2.0f && customLen <= 8.0f) {
                    return customLen;
                }
            }
        }
    }

    float resolved = DEFAULT_CAR_LEN_M;
    bool  matched  = false;

    // 1. Substring lookup on CarPath
    if (!carPath.empty()) {
        for (const auto& entry : kChassisLengthTable) {
            if (carPath.find(entry.token) != std::string_view::npos) {
                resolved = entry.lengthM;
                matched  = true;
                break;
            }
        }
    }

    // 2. Fallback lookup on CarClassShortName / CarScreenNameShort
    if (!matched) {
        for (const auto& entry : kClassFallbackTable) {
            if ((!classShortName.empty() && classShortName.find(entry.token) != std::string_view::npos) ||
                (!screenNameShort.empty() && screenNameShort.find(entry.token) != std::string_view::npos)) {
                resolved = entry.lengthM;
                matched  = true;
                break;
            }
        }
    }

    // 3. Auto-record the discovered CarPath and its resolved length (in meters) into spotter_overlay.ini
    //    so any future iRacing car can be inspected or customized without recompiling
    if (keyBuf[0] != '\0' && iniPathA[0] != '\0') {
        char valStr[32] = {};
        int whole = static_cast<int>(resolved);
        int frac  = static_cast<int>(std::round((resolved - whole) * 100.0f));
        wsprintfA(valStr, "%d.%02d", whole, frac);
        WritePrivateProfileStringA("CarLengths", keyBuf, valStr, iniPathA);
    }

    return resolved;
}

static void ParseSessionYaml(const char* yamlPtr, int maxLen, SessionMetadata& meta) {
    if (!yamlPtr || maxLen <= 0) return;

    size_t actualLen = strnlen(yamlPtr, static_cast<size_t>(maxLen));
    std::string_view yaml(yamlPtr, actualLen);

    std::string_view tlVal = ExtractYamlLineValue(yaml, "TrackLength:");
    if (!tlVal.empty()) {
        float val = 0.0f;
        auto [ptr, ec] = std::from_chars(tlVal.data(), tlVal.data() + tlVal.size(), val);
        if (ec == std::errc() && val > 0.1f) {
            std::string_view unit(ptr, static_cast<size_t>((tlVal.data() + tlVal.size()) - ptr));
            meta.trackLenM = (unit.find("mi") != std::string_view::npos)
                             ? (val * 1609.344f)
                             : (val * 1000.0f);
        }
    }

    std::string_view pcVal = ExtractYamlLineValue(yaml, "PaceCarIdx:");
    if (!pcVal.empty()) {
        meta.paceCarIdx = ParseIntView(pcVal, 0);
    }

    std::fill(std::begin(meta.carLenM), std::end(meta.carLenM), DEFAULT_CAR_LEN_M);
    std::fill(std::begin(meta.isPaceCar), std::end(meta.isPaceCar), false);
    if (meta.paceCarIdx >= 0 && meta.paceCarIdx < IRSDK_MAX_CARS) {
        meta.isPaceCar[meta.paceCarIdx] = true;
    }

    size_t drvSectionPos = yaml.find("DriverInfo:");
    if (drvSectionPos == std::string_view::npos) return;
    size_t driversListPos = yaml.find("Drivers:", drvSectionPos);
    if (driversListPos == std::string_view::npos) return;

    size_t sectionEnd = yaml.size();
    size_t scanPos    = driversListPos + 8;
    while (scanPos < yaml.size()) {
        size_t nl = yaml.find('\n', scanPos);
        if (nl == std::string_view::npos || nl + 1 >= yaml.size()) break;
        char firstChar = yaml[nl + 1];
        if ((firstChar >= 'A' && firstChar <= 'Z') || firstChar == '.') {
            sectionEnd = nl + 1;
            break;
        }
        scanPos = nl + 1;
    }

    std::string_view driversBlock = yaml.substr(driversListPos, sectionEnd - driversListPos);

    constexpr std::string_view kCarIdxMarker = "- CarIdx:";
    size_t entryStart = driversBlock.find(kCarIdxMarker);
    while (entryStart != std::string_view::npos) {
        size_t nextEntry = driversBlock.find(kCarIdxMarker, entryStart + kCarIdxMarker.size());
        std::string_view entryBlock = (nextEntry != std::string_view::npos)
            ? driversBlock.substr(entryStart, nextEntry - entryStart)
            : driversBlock.substr(entryStart);

        std::string_view idxStr = ExtractYamlLineValue(entryBlock, "- CarIdx:");
        int carIdx = ParseIntView(idxStr, -1);

        if (carIdx >= 0 && carIdx < IRSDK_MAX_CARS) {
            std::string_view carPath         = ExtractYamlLineValue(entryBlock, "CarPath:");
            std::string_view classShortName  = ExtractYamlLineValue(entryBlock, "CarClassShortName:");
            std::string_view screenNameShort = ExtractYamlLineValue(entryBlock, "CarScreenNameShort:");
            std::string_view isPaceCarStr    = ExtractYamlLineValue(entryBlock, "CarIsPaceCar:");

            meta.carLenM[carIdx] = ResolveCarLengthMeters(carPath, classShortName, screenNameShort);
            if (ParseIntView(isPaceCarStr, 0) == 1) {
                meta.isPaceCar[carIdx] = true;
            }
        }
        entryStart = nextEntry;
    }
}

// ============================================================================
// 4. ZERO-ALLOCATION STEREO CHIME SYNTHESIZER (LEFT / RIGHT EAR)
// ============================================================================
class StereoChimeEngine {
public:
    static constexpr int SAMPLE_RATE   = 44100;
    static constexpr int CHANNELS      = 2;
    static constexpr int DURATION_MS   = 135;
    static constexpr int NUM_FRAMES    = (SAMPLE_RATE * DURATION_MS) / 1000;
    static constexpr int TOTAL_SAMPLES = NUM_FRAMES * CHANNELS;

    bool init() {
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = CHANNELS;
        wfx.nSamplesPerSec  = SAMPLE_RATE;
        wfx.wBitsPerSample  = 16;
        wfx.nBlockAlign     = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        if (waveOutOpen(&m_hWaveLeft,  WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
        if (waveOutOpen(&m_hWaveRight, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;

        SynthesizePannedChime(m_pcmLeft,  1.0f, 0.0f, 880.0f, 1174.66f);
        SynthesizePannedChime(m_pcmRight, 0.0f, 1.0f, 880.0f, 1174.66f);

        m_hdrLeft.lpData         = reinterpret_cast<LPSTR>(m_pcmLeft);
        m_hdrLeft.dwBufferLength = sizeof(m_pcmLeft);
        waveOutPrepareHeader(m_hWaveLeft, &m_hdrLeft, sizeof(WAVEHDR));

        m_hdrRight.lpData         = reinterpret_cast<LPSTR>(m_pcmRight);
        m_hdrRight.dwBufferLength = sizeof(m_pcmRight);
        waveOutPrepareHeader(m_hWaveRight, &m_hdrRight, sizeof(WAVEHDR));

        m_initialized = true;
        return true;
    }

    void shutdown() {
        if (!m_initialized) return;
        if (m_hWaveLeft) {
            waveOutReset(m_hWaveLeft);
            waveOutUnprepareHeader(m_hWaveLeft, &m_hdrLeft, sizeof(WAVEHDR));
            waveOutClose(m_hWaveLeft);
            m_hWaveLeft = nullptr;
        }
        if (m_hWaveRight) {
            waveOutReset(m_hWaveRight);
            waveOutUnprepareHeader(m_hWaveRight, &m_hdrRight, sizeof(WAVEHDR));
            waveOutClose(m_hWaveRight);
            m_hWaveRight = nullptr;
        }
        m_initialized = false;
    }

    ~StereoChimeEngine() { shutdown(); }

    void playLeft() {
        if (!m_initialized || m_muted) return;
        waveOutReset(m_hWaveLeft);
        waveOutWrite(m_hWaveLeft, &m_hdrLeft, sizeof(WAVEHDR));
    }

    void playRight() {
        if (!m_initialized || m_muted) return;
        waveOutReset(m_hWaveRight);
        waveOutWrite(m_hWaveRight, &m_hdrRight, sizeof(WAVEHDR));
    }

    void update(bool leftActive, int leftCarIdx, bool rightActive, int rightCarIdx) {
        DWORD now = GetTickCount();

        // Trigger chime when side transitions from inactive->active OR when a new carIdx takes over the side
        bool newLeftThreat  = leftActive  && (!m_prevLeftActive  || (leftCarIdx >= 0  && leftCarIdx != m_prevLeftCarIdx));
        bool newRightThreat = rightActive && (!m_prevRightActive || (rightCarIdx >= 0 && rightCarIdx != m_prevRightCarIdx));

        if (newLeftThreat && (now - m_lastLeftTick >= CHIME_COOLDOWN_MS)) {
            playLeft();
            m_lastLeftTick = now;
        }
        if (newRightThreat && (now - m_lastRightTick >= CHIME_COOLDOWN_MS)) {
            playRight();
            m_lastRightTick = now;
        }

        m_prevLeftActive   = leftActive;
        m_prevRightActive  = rightActive;
        m_prevLeftCarIdx   = leftActive  ? leftCarIdx  : -1;
        m_prevRightCarIdx  = rightActive ? rightCarIdx : -1;
    }

    void resetState() {
        m_prevLeftActive  = false;
        m_prevRightActive = false;
        m_prevLeftCarIdx  = -1;
        m_prevRightCarIdx = -1;
    }

    void setMuted(bool muted) { m_muted = muted; }
    bool isMuted() const { return m_muted; }

private:
    static void SynthesizePannedChime(int16_t* outStereo, float gainL, float gainR,
                                      float freq1Hz, float freq2Hz) {
        constexpr float kTwoPi     = 6.283185307f;
        constexpr int   splitFrame = (NUM_FRAMES * 40) / 100;
        float phase = 0.0f;

        for (int i = 0; i < NUM_FRAMES; ++i) {
            const bool  secondNote   = (i >= splitFrame);
            const int   localIdx     = secondNote ? (i - splitFrame) : i;
            const int   localLen     = secondNote ? (NUM_FRAMES - splitFrame) : splitFrame;
            const float freq         = secondNote ? freq2Hz : freq1Hz;

            const float tLocal       = static_cast<float>(localIdx) / static_cast<float>(localLen);
            const float attackFrames = 0.003f * SAMPLE_RATE;
            const float attack       = (localIdx < attackFrames)
                                       ? (0.5f * (1.0f - std::cos(3.14159265f * localIdx / attackFrames)))
                                       : 1.0f;
            const float decay        = std::exp(-4.2f * tLocal);

            phase += (kTwoPi * freq) / static_cast<float>(SAMPLE_RATE);
            if (phase > kTwoPi) phase -= kTwoPi;

            const float wave   = 0.82f * std::sin(phase) + 0.18f * std::sin(2.0f * phase);
            const float sample = wave * attack * decay * 16500.0f;

            outStereo[i * 2 + 0] = static_cast<int16_t>(sample * gainL);
            outStereo[i * 2 + 1] = static_cast<int16_t>(sample * gainR);
        }
    }

    HWAVEOUT m_hWaveLeft               = nullptr;
    HWAVEOUT m_hWaveRight              = nullptr;
    WAVEHDR  m_hdrLeft                 = {};
    WAVEHDR  m_hdrRight                = {};
    int16_t  m_pcmLeft[TOTAL_SAMPLES]  = {};
    int16_t  m_pcmRight[TOTAL_SAMPLES] = {};
    bool     m_initialized             = false;
    bool     m_muted                   = false;
    bool     m_prevLeftActive          = false;
    bool     m_prevRightActive         = false;
    int      m_prevLeftCarIdx          = -1;
    int      m_prevRightCarIdx         = -1;
    DWORD    m_lastLeftTick            = 0;
    DWORD    m_lastRightTick           = 0;
};

// ============================================================================
// 5. TELEMETRY SNAPSHOT & OFFSET CACHE
// ============================================================================
struct TelemetryOffsets {
    int isOnTrack          = -1;
    int isReplayPlaying    = -1;
    int playerCarIdx       = -1;
    int carLeftRight       = -1;
    int carIdxLapDistPct   = -1;
    int carIdxTrackSurface = -1;
    int carIdxOnPitRoad    = -1;
    bool valid() const {
        return isOnTrack >= 0 && playerCarIdx >= 0 && carLeftRight >= 0 &&
               carIdxLapDistPct >= 0 && carIdxTrackSurface >= 0 && carIdxOnPitRoad >= 0;
    }
};

struct FrameSnapshot {
    int   tickCount                     = 0;
    bool  isOnTrack                     = false;
    bool  isReplayPlaying               = false;
    int   playerCarIdx                  = -1;
    int   carLeftRight                  = irsdk_LROff;
    float lapDistPct[IRSDK_MAX_CARS]    = {};
    int   trackSurface[IRSDK_MAX_CARS]  = {};
    bool  onPitRoad[IRSDK_MAX_CARS]     = {};

    FrameSnapshot() {
        std::fill(std::begin(lapDistPct), std::end(lapDistPct), -1.0f);
        std::fill(std::begin(trackSurface), std::end(trackSurface), static_cast<int>(irsdk_NotInWorld));
    }
};

struct BarRenderState {
    bool  active    = false;
    float yTopFrac  = 0.0f; // [0.0 = Player Front Bumper, 1.0 = Player Rear Bumper]
    float yBotFrac  = 0.0f;
    bool  isCapOnly = false;
    int   carIdx    = -1;
};

enum class FrontZone : uint8_t {
    None    = 0,
    Draft   = 1, // 1.20m < gap <= 3.50m (Cyan/Green: in slipstream, closing)
    PullOut = 2, // 0.35m < gap <= 1.20m (Amber/Orange: pull out of draft now)
    Danger  = 3  // gap <= 0.35m         (Crimson Red: bonnet touch imminent)
};

struct FrontProximityState {
    bool      active     = false;
    float     bumperGapM = 99.0f; // Exact physical nose-to-tail gap in meters (0.0m = bumper contact)
    float     fillFrac   = 0.0f;  // [0.12 at 3.50m -> 1.00 at <= 0.0m] horizontal expansion from center outward
    FrontZone zone       = FrontZone::None;
    int       carIdx     = -1;
};

// ============================================================================
// 6. ASYMMETRIC MULTI-CLASS SPOTTER ALGORITHM (WITH VELOCITY LOOKAHEAD)
// ============================================================================
class SpotterProcessor {
public:
    int   lockedLeftIdx                  = -1;
    int   lockedRightIdx                 = -1;
    float prevLeftDistM                  = 0.0f;
    float prevRightDistM                 = 0.0f;
    float prevCarDeltaM[IRSDK_MAX_CARS]  = {};
    bool  hasPrevCarDelta[IRSDK_MAX_CARS]= {};

    struct Candidate {
        int   carIdx;
        float deltaM;       // Center-to-center distance (+ ahead, - behind)
        float absDeltaM;
        float rearBumperM;  // Opponent rear bumper in Player frame:  deltaM - H_opp
        float frontBumperM; // Opponent front bumper in Player frame: deltaM + H_opp
        float threatScoreM; // Velocity-weighted distance score (lower = higher priority)
        bool  overlapping;  // True if physical bumpers overlap Player's [-H_p, +H_p] hull
    };

    static BarRenderState ComputeAsymmetricBarSpan(float playerHalfHullM,
                                                   float oppRearBumperM,
                                                   float oppFrontBumperM,
                                                   int   carIdx) {
        BarRenderState bar{};
        bar.active = true;
        bar.carIdx = carIdx;

        if (oppRearBumperM >= playerHalfHullM) {
            bar.yTopFrac  = 0.0f;
            bar.yBotFrac  = EDGE_CAP_FRAC;
            bar.isCapOnly = true;
            return bar;
        }
        if (oppFrontBumperM <= -playerHalfHullM) {
            bar.yTopFrac  = 1.0f - EDGE_CAP_FRAC;
            bar.yBotFrac  = 1.0f;
            bar.isCapOnly = true;
            return bar;
        }

        const float invPlayerSpan = 1.0f / (2.0f * playerHalfHullM);
        float normFront = (playerHalfHullM - oppFrontBumperM) * invPlayerSpan;
        float normRear  = (playerHalfHullM - oppRearBumperM)  * invPlayerSpan;

        bar.yTopFrac  = std::clamp(normFront, 0.0f, 1.0f - EDGE_CAP_FRAC);
        bar.yBotFrac  = std::clamp(normRear,  EDGE_CAP_FRAC, 1.0f);
        if (bar.yBotFrac - bar.yTopFrac < EDGE_CAP_FRAC) {
            bar.yBotFrac = std::min(1.0f, bar.yTopFrac + EDGE_CAP_FRAC);
        }
        bar.isCapOnly = false;
        return bar;
    }

    void Evaluate(const FrameSnapshot& snap, const SessionMetadata& meta,
                  BarRenderState& outLeft, BarRenderState& outRight) {
        FrontProximityState unusedFront{};
        Evaluate(snap, meta, outLeft, outRight, unusedFront);
    }

    void Evaluate(const FrameSnapshot& snap, const SessionMetadata& meta,
                  BarRenderState& outLeft, BarRenderState& outRight,
                  FrontProximityState& outFront) {
        outLeft  = {};
        outRight = {};
        outFront = {};

        if (!snap.isOnTrack || snap.isReplayPlaying ||
            snap.playerCarIdx < 0 || snap.playerCarIdx >= IRSDK_MAX_CARS) {
            lockedLeftIdx  = -1;
            lockedRightIdx = -1;
            std::fill(std::begin(hasPrevCarDelta), std::end(hasPrevCarDelta), false);
            return;
        }

        const int   pIdx          = snap.playerCarIdx;
        const float pPct          = snap.lapDistPct[pIdx];
        const bool  pInPit        = snap.onPitRoad[pIdx];
        const float pHalfHullM    = 0.5f * (meta.carLenM[pIdx] + SPLINE_BUFFER_M);
        const float pTrueHalfLenM = 0.5f * meta.carLenM[pIdx];
        if (pPct < 0.0f) return;

        Candidate cands[IRSDK_MAX_CARS];
        int numCands = 0;

        for (int i = 0; i < IRSDK_MAX_CARS; ++i) {
            if (i == pIdx || meta.isPaceCar[i]) {
                hasPrevCarDelta[i] = false;
                continue;
            }

            int surf = snap.trackSurface[i];
            if (surf != irsdk_OnTrack && surf != irsdk_OffTrack) {
                hasPrevCarDelta[i] = false;
                continue;
            }
            if (snap.onPitRoad[i] != pInPit) {
                hasPrevCarDelta[i] = false;
                continue;
            }

            float oppPct = snap.lapDistPct[i];
            if (oppPct < 0.0f) {
                hasPrevCarDelta[i] = false;
                continue;
            }

            float dp     = std::remainderf(oppPct - pPct, 1.0f);
            float deltaM = dp * meta.trackLenM;
            float absM   = std::fabs(deltaM);

            // Compute longitudinal relative velocity (m/tick; positive when opponent moves forward relative to player)
            float relVelMPerTick = 0.0f;
            if (hasPrevCarDelta[i]) {
                float step = deltaM - prevCarDeltaM[i];
                if (std::fabs(step) < 12.0f) {
                    relVelMPerTick = step;
                }
            }
            prevCarDeltaM[i]   = deltaM;
            hasPrevCarDelta[i] = true;

            if (absM <= SEARCH_RADIUS_M) {
                float oppHalfHullM = 0.5f * (meta.carLenM[i] + SPLINE_BUFFER_M);
                float rearBumper   = deltaM - oppHalfHullM;
                float frontBumper  = deltaM + oppHalfHullM;
                bool  isOverlap    = (rearBumper < pHalfHullM) && (frontBumper > -pHalfHullM);

                // Ignore cars that have already cleared > 3.5m ahead and are still pulling away faster than us
                if (!isOverlap && rearBumper > (pHalfHullM + 3.5f) && relVelMPerTick > 0.05f) {
                    continue;
                }

                // Threat score:
                // 1. Physically overlapping cars always win (score near 0)
                // 2. Cars behind us that are closing fast get a lookahead bonus (18 ticks = ~0.3s projection)
                // 3. Cars ahead of us that are pulling away get penalized
                float projectedDelta = deltaM + relVelMPerTick * 18.0f;
                float threatScore    = isOverlap
                                       ? (absM * 0.05f)
                                       : std::min(absM, std::fabs(projectedDelta));
                if (!isOverlap && deltaM > 0.0f && relVelMPerTick > 0.02f) {
                    threatScore += 15.0f; // Pulling away ahead
                }

                cands[numCands++] = {
                    i,
                    deltaM,
                    absM,
                    rearBumper,
                    frontBumper,
                    threatScore,
                    isOverlap
                };
            }
        }

        std::sort(cands, cands + numCands, [](const Candidate& a, const Candidate& b) {
            return a.threatScoreM < b.threatScoreM;
        });

        auto findCandidate = [&](int targetIdx) -> const Candidate* {
            if (targetIdx < 0) return nullptr;
            for (int i = 0; i < numCands; ++i) {
                if (cands[i].carIdx == targetIdx) return &cands[i];
            }
            return nullptr;
        };

        const int lr = snap.carLeftRight;

        // 1. Evaluate Left / Right side-by-side spotter pillars
        if (lr <= irsdk_LRClear) {
            // If iRacing's CarLeftRight cleared early (or suppressed due to low player speed), keep locked cars active
            // for as long as their bumpers physically overlap our hull!
            const Candidate* prevL = findCandidate(lockedLeftIdx);
            const Candidate* prevR = findCandidate(lockedRightIdx);
            if (prevL && prevL->overlapping) {
                prevLeftDistM = prevL->deltaM;
                outLeft = ComputeAsymmetricBarSpan(pHalfHullM, prevL->rearBumperM, prevL->frontBumperM, prevL->carIdx);
            } else {
                lockedLeftIdx = -1;
            }
            if (prevR && prevR->overlapping) {
                prevRightDistM = prevR->deltaM;
                outRight = ComputeAsymmetricBarSpan(pHalfHullM, prevR->rearBumperM, prevR->frontBumperM, prevR->carIdx);
            } else {
                lockedRightIdx = -1;
            }
        } else if (lr == irsdk_LRCarLeft || lr == irsdk_LR2CarsLeft) {
            lockedRightIdx = -1;
            if (numCands > 0) {
                const Candidate* primary = &cands[0];
                if (const Candidate* prev = findCandidate(lockedLeftIdx)) {
                    if (prev->overlapping && std::fabs(prev->deltaM - prevLeftDistM) <= MAX_FRAME_JUMP_M) {
                        primary = prev;
                    }
                }
                lockedLeftIdx = primary->carIdx;
                prevLeftDistM = primary->deltaM;

                float minRearM  = primary->rearBumperM;
                float maxFrontM = primary->frontBumperM;
                if (lr == irsdk_LR2CarsLeft && numCands >= 2) {
                    const Candidate* second = (cands[0].carIdx == primary->carIdx) ? &cands[1] : &cands[0];
                    minRearM  = std::min(minRearM,  second->rearBumperM);
                    maxFrontM = std::max(maxFrontM, second->frontBumperM);
                }
                outLeft = ComputeAsymmetricBarSpan(pHalfHullM, minRearM, maxFrontM, primary->carIdx);
            }
        } else if (lr == irsdk_LRCarRight || lr == irsdk_LR2CarsRight) {
            lockedLeftIdx = -1;
            if (numCands > 0) {
                const Candidate* primary = &cands[0];
                if (const Candidate* prev = findCandidate(lockedRightIdx)) {
                    if (prev->overlapping && std::fabs(prev->deltaM - prevRightDistM) <= MAX_FRAME_JUMP_M) {
                        primary = prev;
                    }
                }
                lockedRightIdx = primary->carIdx;
                prevRightDistM = primary->deltaM;

                float minRearM  = primary->rearBumperM;
                float maxFrontM = primary->frontBumperM;
                if (lr == irsdk_LR2CarsRight && numCands >= 2) {
                    const Candidate* second = (cands[0].carIdx == primary->carIdx) ? &cands[1] : &cands[0];
                    minRearM  = std::min(minRearM,  second->rearBumperM);
                    maxFrontM = std::max(maxFrontM, second->frontBumperM);
                }
                outRight = ComputeAsymmetricBarSpan(pHalfHullM, minRearM, maxFrontM, primary->carIdx);
            }
        } else if (lr == irsdk_LRCarLeftRight) {
            const Candidate* leftCand  = findCandidate(lockedLeftIdx);
            const Candidate* rightCand = findCandidate(lockedRightIdx);

            if (leftCand && (!leftCand->overlapping && std::fabs(leftCand->deltaM - prevLeftDistM) > MAX_FRAME_JUMP_M)) {
                leftCand = nullptr;
            }
            if (rightCand && (!rightCand->overlapping && std::fabs(rightCand->deltaM - prevRightDistM) > MAX_FRAME_JUMP_M)) {
                rightCand = nullptr;
            }

            for (int i = 0; i < numCands; ++i) {
                if (!leftCand && (!rightCand || cands[i].carIdx != rightCand->carIdx)) {
                    leftCand = &cands[i];
                } else if (!rightCand && (!leftCand || cands[i].carIdx != leftCand->carIdx)) {
                    rightCand = &cands[i];
                }
            }

            if (leftCand) {
                lockedLeftIdx = leftCand->carIdx;
                prevLeftDistM = leftCand->deltaM;
                outLeft = ComputeAsymmetricBarSpan(pHalfHullM, leftCand->rearBumperM, leftCand->frontBumperM, leftCand->carIdx);
            }
            if (rightCand) {
                lockedRightIdx = rightCand->carIdx;
                prevRightDistM = rightCand->deltaM;
                outRight = ComputeAsymmetricBarSpan(pHalfHullM, rightCand->rearBumperM, rightCand->frontBumperM, rightCand->carIdx);
            }
        }

        // 2. Evaluate Front Bonnet / Draft Pull-Out Proximity Indicator
        //    Active only while drafting behind a car ahead (0m..3.5m nose-to-tail gap) and automatically hides
        //    once iRacing's spotter indicates you have pulled alongside (outLeft.active or outRight.active).
        if (!outLeft.active && !outRight.active && lr <= irsdk_LRClear) {
            float bestGapM   = 999.0f;
            int   bestCarIdx = -1;

            for (int i = 0; i < numCands; ++i) {
                const Candidate& c = cands[i];
                if (c.deltaM <= 0.0f) continue; // Must be ahead of player center

                const float oppTrueHalfLenM = 0.5f * meta.carLenM[c.carIdx];
                const float bumperGapM      = c.deltaM - (pTrueHalfLenM + oppTrueHalfLenM);

                if (bumperGapM >= FRONT_MAX_OVERLAP_M && bumperGapM <= FRONT_DRAFT_MAX_GAP_M) {
                    if (bumperGapM < bestGapM) {
                        bestGapM   = bumperGapM;
                        bestCarIdx = c.carIdx;
                    }
                }
            }

            if (bestCarIdx >= 0) {
                outFront.active     = true;
                outFront.bumperGapM = bestGapM;
                outFront.carIdx     = bestCarIdx;

                const float normClose = 1.0f - std::clamp(bestGapM / FRONT_DRAFT_MAX_GAP_M, 0.0f, 1.0f);
                outFront.fillFrac     = 0.12f + 0.88f * normClose;

                if (bestGapM <= FRONT_DANGER_GAP_M) {
                    outFront.zone = FrontZone::Danger;
                } else if (bestGapM <= FRONT_PULLOUT_GAP_M) {
                    outFront.zone = FrontZone::PullOut;
                } else {
                    outFront.zone = FrontZone::Draft;
                }
            }
        }
    }
};

// ============================================================================
// 7. SEQLOCK SHARED MEMORY READER
// ============================================================================
static void RefreshOffsets(const irsdk_header* hdr, const char* basePtr, TelemetryOffsets& off) {
    off = {};
    const auto* vars = reinterpret_cast<const irsdk_varHeader*>(basePtr + hdr->varHeaderOffset);
    for (int i = 0; i < hdr->numVars; ++i) {
        const char* name = vars[i].name;
        if (std::strcmp(name, "IsOnTrack") == 0)               off.isOnTrack          = vars[i].offset;
        else if (std::strcmp(name, "IsReplayPlaying") == 0)    off.isReplayPlaying    = vars[i].offset;
        else if (std::strcmp(name, "PlayerCarIdx") == 0)       off.playerCarIdx       = vars[i].offset;
        else if (std::strcmp(name, "CarLeftRight") == 0)       off.carLeftRight       = vars[i].offset;
        else if (std::strcmp(name, "CarIdxLapDistPct") == 0)   off.carIdxLapDistPct   = vars[i].offset;
        else if (std::strcmp(name, "CarIdxTrackSurface") == 0) off.carIdxTrackSurface = vars[i].offset;
        else if (std::strcmp(name, "CarIdxOnPitRoad") == 0)    off.carIdxOnPitRoad    = vars[i].offset;
    }
}

static bool ReadSnapshotSeqlock(const irsdk_header* hdr, const char* basePtr,
                                const TelemetryOffsets& off, FrameSnapshot& outSnap) {
    if (!(hdr->status & IRSDK_ST_CONNECTED) || !off.valid()) return false;

    int latestIdx = 0;
    int maxTick   = hdr->varBuf[0].tickCount;
    for (int i = 1; i < hdr->numBuf; ++i) {
        if (hdr->varBuf[i].tickCount > maxTick) {
            maxTick   = hdr->varBuf[i].tickCount;
            latestIdx = i;
        }
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        int tickBefore = hdr->varBuf[latestIdx].tickCount;
        std::atomic_thread_fence(std::memory_order_acquire);

        const char* buf = basePtr + hdr->varBuf[latestIdx].bufOffset;
        outSnap.tickCount       = tickBefore;
        outSnap.isOnTrack       = *reinterpret_cast<const bool*>(buf + off.isOnTrack);
        outSnap.isReplayPlaying = (off.isReplayPlaying >= 0)
                                  ? *reinterpret_cast<const bool*>(buf + off.isReplayPlaying)
                                  : false;
        outSnap.playerCarIdx    = *reinterpret_cast<const int*>(buf + off.playerCarIdx);
        outSnap.carLeftRight    = *reinterpret_cast<const int*>(buf + off.carLeftRight);

        std::memcpy(outSnap.lapDistPct,   buf + off.carIdxLapDistPct,   sizeof(outSnap.lapDistPct));
        std::memcpy(outSnap.trackSurface, buf + off.carIdxTrackSurface, sizeof(outSnap.trackSurface));
        std::memcpy(outSnap.onPitRoad,    buf + off.carIdxOnPitRoad,    sizeof(outSnap.onPitRoad));

        std::atomic_thread_fence(std::memory_order_acquire);
        if (hdr->varBuf[latestIdx].tickCount == tickBefore) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// 8. PER-PIXEL ALPHA DWM LAYERED CANVAS (ANTI-ALIASED SDF RASTERIZER)
// ============================================================================
struct ColorRGBA {
    uint8_t r, g, b, a;
};

class LayeredCanvas {
public:
    bool init(int width, int height) {
        m_width     = width;
        m_height    = height;
        m_hdcScreen = GetDC(nullptr);
        m_hdcMem    = CreateCompatibleDC(m_hdcScreen);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = width;
        bmi.bmiHeader.biHeight      = -height; // Top-down DIB
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        m_hBitmap = CreateDIBSection(m_hdcMem, &bmi, DIB_RGB_COLORS,
                                     reinterpret_cast<void**>(&m_pixels), nullptr, 0);
        if (!m_hBitmap || !m_pixels) return false;
        m_hOldBmp = SelectObject(m_hdcMem, m_hBitmap);

        m_hFont = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        return true;
    }

    ~LayeredCanvas() {
        if (m_hFont)   DeleteObject(m_hFont);
        if (m_hdcMem) {
            if (m_hOldBmp) SelectObject(m_hdcMem, m_hOldBmp);
            if (m_hBitmap) DeleteObject(m_hBitmap);
            DeleteDC(m_hdcMem);
        }
        if (m_hdcScreen) ReleaseDC(nullptr, m_hdcScreen);
    }

    void clear() {
        std::memset(m_pixels, 0, static_cast<size_t>(m_width * m_height) * sizeof(uint32_t));
    }

    void drawRoundedRect(float x0, float y0, float x1, float y1, float radius,
                         ColorRGBA fill, ColorRGBA border = {0, 0, 0, 0}, float borderThickness = 0.0f) {
        const float cx = 0.5f * (x0 + x1);
        const float cy = 0.5f * (y0 + y1);
        const float hx = 0.5f * (x1 - x0) - radius;
        const float hy = 0.5f * (y1 - y0) - radius;

        const int minX = std::max(0, static_cast<int>(std::floor(x0 - 2.0f)));
        const int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(x1 + 2.0f)));
        const int minY = std::max(0, static_cast<int>(std::floor(y0 - 2.0f)));
        const int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(y1 + 2.0f)));

        for (int py = minY; py <= maxY; ++py) {
            uint32_t* row = m_pixels + py * m_width;
            const float dy = std::fabs((py + 0.5f) - cy) - hy;
            for (int px = minX; px <= maxX; ++px) {
                const float dx = std::fabs((px + 0.5f) - cx) - hx;
                const float ox = std::max(dx, 0.0f);
                const float oy = std::max(dy, 0.0f);
                const float dist = std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f) - radius;

                const float outerCov = std::clamp(0.5f - dist, 0.0f, 1.0f);
                if (outerCov <= 0.0f) continue;

                ColorRGBA src = fill;
                if (borderThickness > 0.0f && border.a > 0) {
                    const float innerDist = dist + borderThickness;
                    const float borderBlend = std::clamp(0.5f + innerDist, 0.0f, 1.0f);
                    src.r = static_cast<uint8_t>(fill.r + (border.r - fill.r) * borderBlend);
                    src.g = static_cast<uint8_t>(fill.g + (border.g - fill.g) * borderBlend);
                    src.b = static_cast<uint8_t>(fill.b + (border.b - fill.b) * borderBlend);
                    src.a = static_cast<uint8_t>(fill.a + (border.a - fill.a) * borderBlend);
                }

                const float effAlpha = (src.a / 255.0f) * outerCov;
                blendPixel(row[px], src.r, src.g, src.b, effAlpha);
            }
        }
    }

    void drawCenteredLabel(const wchar_t* text, int yTop, int yBottom, ColorRGBA color) {
        const int boxH = std::max(1, yBottom - yTop);
        std::vector<uint32_t> saved(static_cast<size_t>(m_width * boxH));
        for (int y = 0; y < boxH; ++y) {
            int py = yTop + y;
            if (py >= 0 && py < m_height) {
                std::memcpy(&saved[static_cast<size_t>(y * m_width)], m_pixels + py * m_width,
                            static_cast<size_t>(m_width) * sizeof(uint32_t));
                std::memset(m_pixels + py * m_width, 0, static_cast<size_t>(m_width) * sizeof(uint32_t));
            }
        }

        HGDIOBJ oldFont = SelectObject(m_hdcMem, m_hFont);
        SetBkMode(m_hdcMem, TRANSPARENT);
        SetTextColor(m_hdcMem, RGB(255, 255, 255));
        RECT rc = { 56, yTop, m_width - 56, yBottom };
        DrawTextW(m_hdcMem, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(m_hdcMem, oldFont);

        for (int y = 0; y < boxH; ++y) {
            int py = yTop + y;
            if (py < 0 || py >= m_height) continue;
            uint32_t* row      = m_pixels + py * m_width;
            uint32_t* savedRow = &saved[static_cast<size_t>(y * m_width)];
            for (int x = 0; x < m_width; ++x) {
                uint32_t maskPx = row[x];
                uint8_t  lum    = static_cast<uint8_t>((maskPx >> 16) & 0xFF);
                row[x]          = savedRow[x];
                if (lum > 0) {
                    float alpha = (lum / 255.0f) * (color.a / 255.0f);
                    blendPixel(row[x], color.r, color.g, color.b, alpha);
                }
            }
        }
    }

    void present(HWND hwnd) {
        RECT rc{};
        GetWindowRect(hwnd, &rc);
        POINT ptWin = { rc.left, rc.top };
        SIZE  szWin = { m_width, m_height };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hwnd, m_hdcScreen, &ptWin, &szWin, m_hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
    }

private:
    static void blendPixel(uint32_t& dst, uint8_t sr, uint8_t sg, uint8_t sb, float sa) {
        const uint8_t da = static_cast<uint8_t>(dst >> 24);
        const uint8_t dr = static_cast<uint8_t>((dst >> 16) & 0xFF);
        const uint8_t dg = static_cast<uint8_t>((dst >> 8)  & 0xFF);
        const uint8_t db = static_cast<uint8_t>(dst & 0xFF);

        const float invSa = 1.0f - sa;
        const uint8_t outA = static_cast<uint8_t>(std::min(255.0f, sa * 255.0f + da * invSa));
        const uint8_t outR = static_cast<uint8_t>(std::min(255.0f, sr * sa     + dr * invSa));
        const uint8_t outG = static_cast<uint8_t>(std::min(255.0f, sg * sa     + dg * invSa));
        const uint8_t outB = static_cast<uint8_t>(std::min(255.0f, sb * sa     + db * invSa));

        dst = (static_cast<uint32_t>(outA) << 24) |
              (static_cast<uint32_t>(outR) << 16) |
              (static_cast<uint32_t>(outG) << 8)  |
              static_cast<uint32_t>(outB);
    }

    int       m_width     = 0;
    int       m_height    = 0;
    HDC       m_hdcScreen = nullptr;
    HDC       m_hdcMem    = nullptr;
    HBITMAP   m_hBitmap   = nullptr;
    HGDIOBJ   m_hOldBmp   = nullptr;
    HFONT     m_hFont     = nullptr;
    uint32_t* m_pixels    = nullptr;
};

// ============================================================================
// 9. OVERLAY WINDOW STATE, PERSISTENCE & WNDPROC
// ============================================================================
struct OverlayAppState {
    bool              moveMode    = false;
    bool              dirtyRedraw = true;
    wchar_t           iniPath[MAX_PATH] = {};
    StereoChimeEngine chime{};
};

static OverlayAppState g_app;

static void InitConfigPath() {
    GetModuleFileNameW(nullptr, g_app.iniPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(g_app.iniPath, L'\\');
    if (lastSlash) {
        wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash + 1 - g_app.iniPath), L"spotter_overlay.ini");
    } else {
        wcscpy_s(g_app.iniPath, MAX_PATH, L".\\spotter_overlay.ini");
    }
    wcscpy_s(g_iniPath, MAX_PATH, g_app.iniPath);
}

static bool LoadWindowConfig(int& inoutX, int& inoutY, bool& outMuted) {
    DWORD attrs = GetFileAttributesW(g_app.iniPath);
    bool exists = (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    inoutX   = GetPrivateProfileIntW(L"Overlay", L"PosX", inoutX, g_app.iniPath);
    inoutY   = GetPrivateProfileIntW(L"Overlay", L"PosY", inoutY, g_app.iniPath);
    outMuted = GetPrivateProfileIntW(L"Overlay", L"Muted", 0,     g_app.iniPath) != 0;
    return exists;
}

static void SaveWindowConfig(HWND hwnd, bool muted) {
    RECT rc{};
    if (GetWindowRect(hwnd, &rc)) {
        wchar_t buf[32];
        wsprintfW(buf, L"%d", static_cast<int>(rc.left));
        WritePrivateProfileStringW(L"Overlay", L"PosX", buf, g_app.iniPath);
        wsprintfW(buf, L"%d", static_cast<int>(rc.top));
        WritePrivateProfileStringW(L"Overlay", L"PosY", buf, g_app.iniPath);
    }
    WritePrivateProfileStringW(L"Overlay", L"Muted", muted ? L"1" : L"0", g_app.iniPath);
}

static void SetOverlayMoveMode(HWND hwnd, bool enableMove) {
    g_app.moveMode    = enableMove;
    g_app.dirtyRedraw = true;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (enableMove) {
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
        g_app.chime.playLeft();
    } else {
        exStyle |= WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
        SaveWindowConfig(hwnd, g_app.chime.isMuted());
        g_app.chime.playRight();
    }
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        // Never steal focus or switch Windows desktops away from iRacing when clicked
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        if (g_app.moveMode) {
            return HTCAPTION; // Drag anywhere inside the overlay window while unlocked
        }
        return HTTRANSPARENT;

    case WM_NCLBUTTONDBLCLK:
        if (g_app.moveMode) {
            POINT pt = { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
            ScreenToClient(hwnd, &pt);
            if (pt.x < WIN_WIDTH / 2) g_app.chime.playLeft();
            else                      g_app.chime.playRight();
            return 0;
        }
        break;

    case WM_EXITSIZEMOVE:
        SaveWindowConfig(hwnd, g_app.chime.isMuted());
        g_app.dirtyRedraw = true;
        return 0;

    case WM_NCRBUTTONUP:
    case WM_RBUTTONUP:
        if (g_app.moveMode) {
            SetOverlayMoveMode(hwnd, false); // Right-click locks the overlay immediately
        }
        return 0;

    case WM_KEYDOWN:
        if (g_app.moveMode && (wParam == VK_ESCAPE || wParam == VK_RETURN)) {
            SetOverlayMoveMode(hwnd, false);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// 10. MAIN LOOP
// ============================================================================
#ifndef SPOTTER_TEST_BUILD
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // If launched from a sandboxed non-interactive desktop (e.g. "exebox-*"), relaunch onto "WinSta0\\Default"
    HDESK hCurDesk = GetThreadDesktop(GetCurrentThreadId());
    wchar_t deskName[128] = {};
    DWORD deskNameLen = 0;
    if (GetUserObjectInformationW(hCurDesk, UOI_NAME, deskName, sizeof(deskName), &deskNameLen)) {
        if (_wcsicmp(deskName, L"Default") != 0) {
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            STARTUPINFOW si = { sizeof(si) };
            si.lpDesktop = const_cast<LPWSTR>(L"WinSta0\\Default");
            PROCESS_INFORMATION pi = {};
            if (CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                return 0;
            }
        }
    }

    // Enable native 1:1 physical pixel rendering on high-DPI monitors (1440p / 4K)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    InitConfigPath();

    int  screenW   = GetSystemMetrics(SM_CXSCREEN);
    int  screenH   = GetSystemMetrics(SM_CYSCREEN);
    int  posX      = (screenW - WIN_WIDTH) / 2;
    int  posY      = (screenH - WIN_HEIGHT) / 2;
    bool muted     = false;
    bool hasConfig = LoadWindowConfig(posX, posY, muted);

    g_app.chime.init();
    g_app.chime.setMuted(muted);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, OverlayWndProc,
                       0, 0, hInst, nullptr, LoadCursorW(nullptr, MAKEINTRESOURCEW(32646)),
                       nullptr, nullptr, L"IRSpotterMicroOverlay", nullptr };
    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, L"iRacing Spotter", WS_POPUP,
                                posX, posY, WIN_WIDTH, WIN_HEIGHT, nullptr, nullptr, hInst, nullptr);

    LayeredCanvas canvas{};
    if (!canvas.init(WIN_WIDTH, WIN_HEIGHT)) return 1;

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    constexpr ColorRGBA kTrackBgColor    = {  20,  22,  26, 175 };
    constexpr ColorRGBA kTrackBorder     = {  80,  88, 100, 195 };
    constexpr ColorRGBA kOverlapColor    = { 255, 145,   0, 245 };
    constexpr ColorRGBA kCapColor        = { 255,  48,  48, 242 };
    constexpr ColorRGBA kFrontDraftColor = {   0, 225, 175, 240 }; // Cyan-Green: In slipstream (3.5m - 1.2m)
    constexpr ColorRGBA kFrontPullColor  = { 255, 165,   0, 248 }; // Amber-Orange: Pull out now (1.2m - 0.35m)
    constexpr ColorRGBA kFrontDangerColor= { 255,  38,  38, 252 }; // Crimson Red: Bonnet touch imminent (< 0.35m)
    constexpr ColorRGBA kMoveBgColor     = {  14,  28,  46, 195 };
    constexpr ColorRGBA kMoveBorderColor = {  55, 195, 255, 245 };

    auto drawSingleBar = [&](float x, const BarRenderState& st) {
        if (!st.active) return;
        canvas.drawRoundedRect(x, BAR_TOP_Y, x + BAR_WIDTH, BAR_TOP_Y + BAR_HEIGHT,
                               7.0f, kTrackBgColor, kTrackBorder, 1.5f);

        float fillTop = BAR_TOP_Y + st.yTopFrac * BAR_HEIGHT;
        float fillBot = BAR_TOP_Y + st.yBotFrac * BAR_HEIGHT;
        canvas.drawRoundedRect(x + 3.0f, fillTop, x + BAR_WIDTH - 3.0f, fillBot,
                               5.0f, st.isCapOnly ? kCapColor : kOverlapColor);
    };

    auto drawFrontBar = [&](const FrontProximityState& st) {
        if (!st.active) return;
        // 1. Outer horizontal gauge track between the Left and Right pillars
        canvas.drawRoundedRect(FRONT_BAR_X, FRONT_BAR_Y,
                               FRONT_BAR_X + FRONT_BAR_WIDTH, FRONT_BAR_Y + FRONT_BAR_HEIGHT,
                               7.0f, kTrackBgColor, kTrackBorder, 1.5f);

        // 2. Symmetrical horizontal fill expanding outward from center (x = WIN_WIDTH / 2)
        const float centerX  = 0.5f * static_cast<float>(WIN_WIDTH);
        const float maxInner = FRONT_BAR_WIDTH - 6.0f;
        const float halfSpan = 0.5f * maxInner * std::clamp(st.fillFrac, 0.08f, 1.0f);

        ColorRGBA fillCol = kFrontDraftColor;
        if (st.zone == FrontZone::PullOut)     fillCol = kFrontPullColor;
        else if (st.zone == FrontZone::Danger) fillCol = kFrontDangerColor;

        canvas.drawRoundedRect(centerX - halfSpan, FRONT_BAR_Y + 3.0f,
                               centerX + halfSpan, FRONT_BAR_Y + FRONT_BAR_HEIGHT - 3.0f,
                               5.0f, fillCol);

        // 3. Reference ticks at the 1.20m "Pull Out Now" threshold so driver can judge closing rate in peripheral vision
        constexpr float kPullOutNorm = 1.0f - (FRONT_PULLOUT_GAP_M / FRONT_DRAFT_MAX_GAP_M);
        constexpr float kPullOutFrac = 0.12f + 0.88f * kPullOutNorm;
        const float tickOffset = 0.5f * maxInner * kPullOutFrac;
        const ColorRGBA tickColor = { 235, 240, 248, 185 };
        canvas.drawRoundedRect(centerX - tickOffset - 1.2f, FRONT_BAR_Y + 1.5f,
                               centerX - tickOffset + 1.2f, FRONT_BAR_Y + FRONT_BAR_HEIGHT - 1.5f,
                               1.0f, tickColor);
        canvas.drawRoundedRect(centerX + tickOffset - 1.2f, FRONT_BAR_Y + 1.5f,
                               centerX + tickOffset + 1.2f, FRONT_BAR_Y + FRONT_BAR_HEIGHT - 1.5f,
                               1.0f, tickColor);
    };

    auto drawMoveModeGuides = [&]() {
        canvas.drawRoundedRect(2.0f, 2.0f, WIN_WIDTH - 2.0f, WIN_HEIGHT - 2.0f,
                               12.0f, kMoveBgColor, kMoveBorderColor, 2.2f);

        BarRenderState      sampleLeft  = { true, 0.15f, 0.85f, false, -1 };
        BarRenderState      sampleRight = { true, 0.0f,  EDGE_CAP_FRAC, true, -1 };
        FrontProximityState sampleFront = { true, 1.05f, 0.72f, FrontZone::PullOut, -1 };
        drawSingleBar(LEFT_BAR_X,  sampleLeft);
        drawSingleBar(RIGHT_BAR_X, sampleRight);
        drawFrontBar(sampleFront);

        canvas.drawCenteredLabel(L"iRACING MULTI-CLASS SPOTTER",         85,  120, {  80, 210, 255, 255 });
        canvas.drawCenteredLabel(L"Drag anywhere to position HUD",       128, 160, { 240, 244, 248, 255 });
        canvas.drawCenteredLabel(L"Double-Click L/R side to test Chime", 168, 200, { 255, 185,  75, 255 });
        canvas.drawCenteredLabel(L"Right-Click or Ctrl+Shift+M to Lock", 208, 240, { 150, 240, 150, 255 });
    };

    // Only start in unlocked Move Mode on the very first run when no spotter_overlay.ini exists yet
    if (!hasConfig) {
        SetOverlayMoveMode(hwnd, true);
    }

    HANDLE hMemMap         = nullptr;
    HANDLE hDataEvent      = nullptr;
    const char* pSharedMem = nullptr;

    TelemetryOffsets offsets{};
    SessionMetadata  sessionMeta{};
    SpotterProcessor processor{};
    int  lastSessionUpdate = -1;
    int  lastTickCount     = -1;
    bool canvasIsClear     = false;
    bool prevMoveCombo     = false;
    bool prevMuteCombo     = false;

    auto pollHotkeys = [&]() {
        bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        bool moveCombo = ctrlDown && shiftDown && ((GetAsyncKeyState('M') & 0x8000) != 0);
        bool muteCombo = ctrlDown && shiftDown && ((GetAsyncKeyState('A') & 0x8000) != 0);

        if (moveCombo && !prevMoveCombo) {
            SetOverlayMoveMode(hwnd, !g_app.moveMode);
        }
        if (muteCombo && !prevMuteCombo) {
            bool newMuted = !g_app.chime.isMuted();
            g_app.chime.setMuted(newMuted);
            SaveWindowConfig(hwnd, newMuted);
            if (!newMuted) g_app.chime.playLeft();
        }
        prevMoveCombo = moveCombo;
        prevMuteCombo = muteCombo;
    };

    MSG msg = {};
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        pollHotkeys();

        if (g_app.moveMode) {
            if (g_app.dirtyRedraw || canvasIsClear) {
                canvas.clear();
                drawMoveModeGuides();
                canvas.present(hwnd);
                canvasIsClear     = false;
                g_app.dirtyRedraw = false;
            }
            Sleep(16);
            continue;
        }

        if (!pSharedMem) {
            hMemMap    = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\IRSDKMemMapFileName");
            hDataEvent = OpenEventW(SYNCHRONIZE, FALSE, L"Local\\IRSDKDataValidEvent");
            if (hMemMap && hDataEvent) {
                pSharedMem        = static_cast<const char*>(MapViewOfFile(hMemMap, FILE_MAP_READ, 0, 0, 0));
                lastSessionUpdate = -1;
                lastTickCount     = -1;
            } else {
                if (hMemMap)    { CloseHandle(hMemMap);    hMemMap = nullptr; }
                if (hDataEvent) { CloseHandle(hDataEvent); hDataEvent = nullptr; }
                if (!canvasIsClear) {
                    canvas.clear();
                    canvas.present(hwnd);
                    canvasIsClear = true;
                }
                Sleep(30);
                continue;
            }
        }

        // Wait up to 8ms for IRSDKDataValidEvent, but NEVER abort on WAIT_TIMEOUT because IRSDKDataValidEvent
        // is an auto-reset event that other telemetry apps (SimHub, CrewChief, TradingPaints) can consume!
        WaitForSingleObject(hDataEvent, 8);
        const auto* hdr = reinterpret_cast<const irsdk_header*>(pSharedMem);

        if (!(hdr->status & IRSDK_ST_CONNECTED)) {
            g_app.chime.resetState();
            if (!canvasIsClear) {
                canvas.clear();
                canvas.present(hwnd);
                canvasIsClear = true;
            }
            UnmapViewOfFile(pSharedMem); pSharedMem = nullptr;
            CloseHandle(hMemMap);        hMemMap    = nullptr;
            CloseHandle(hDataEvent);     hDataEvent = nullptr;
            Sleep(30);
            continue;
        }

        if (hdr->sessionInfoUpdate != lastSessionUpdate) {
            RefreshOffsets(hdr, pSharedMem, offsets);
            ParseSessionYaml(pSharedMem + hdr->sessionInfoOffset, hdr->sessionInfoLen, sessionMeta);
            lastSessionUpdate = hdr->sessionInfoUpdate;
        }

        FrameSnapshot snap{};
        if (!ReadSnapshotSeqlock(hdr, pSharedMem, offsets, snap)) continue;
        if (snap.tickCount == lastTickCount && !g_app.dirtyRedraw) continue;
        lastTickCount     = snap.tickCount;
        g_app.dirtyRedraw = false;

        BarRenderState      leftBar{}, rightBar{};
        FrontProximityState frontBar{};
        processor.Evaluate(snap, sessionMeta, leftBar, rightBar, frontBar);

        g_app.chime.update(leftBar.active, leftBar.carIdx, rightBar.active, rightBar.carIdx);

        if (!leftBar.active && !rightBar.active && !frontBar.active) {
            if (!canvasIsClear) {
                canvas.clear();
                canvas.present(hwnd);
                canvasIsClear = true;
            }
            continue;
        }

        canvas.clear();
        drawSingleBar(LEFT_BAR_X,  leftBar);
        drawSingleBar(RIGHT_BAR_X, rightBar);
        drawFrontBar(frontBar);
        canvas.present(hwnd);
        canvasIsClear = false;
    }
}
#endif // SPOTTER_TEST_BUILD
