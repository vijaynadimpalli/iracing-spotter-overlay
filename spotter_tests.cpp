#define SPOTTER_TEST_BUILD
#include "spotter_overlay.cpp"

#include <cstdio>
#include <cmath>
#include <string>

static int g_totalTests  = 0;
static int g_failedTests = 0;

#define EXPECT_TRUE(cond, msg)                                                                 \
    do {                                                                                       \
        ++g_totalTests;                                                                        \
        if (!(cond)) {                                                                         \
            ++g_failedTests;                                                                   \
            std::printf("  [FAIL] Line %d: %s (%s)\n", __LINE__, msg, #cond);                  \
        }                                                                                      \
    } while (0)

#define EXPECT_NEAR(actual, expected, tol, msg)                                                \
    do {                                                                                       \
        ++g_totalTests;                                                                        \
        float _a = static_cast<float>(actual);                                                 \
        float _e = static_cast<float>(expected);                                               \
        if (std::fabs(_a - _e) > static_cast<float>(tol)) {                                    \
            ++g_failedTests;                                                                   \
            std::printf("  [FAIL] Line %d: %s (got %.4f, expected %.4f +/- %.4f)\n",           \
                        __LINE__, msg, _a, _e, static_cast<float>(tol));                       \
        }                                                                                      \
    } while (0)

// ============================================================================
// TEST 1: Live Rudskogen + Caterham 420R YAML & PaceCarIdx=64 Out-Of-Bounds Guard
// ============================================================================
static void Test_YamlParser_RudskogenCaterham420R() {
    std::printf("[RUN ] Test_YamlParser_RudskogenCaterham420R\n");
    g_iniPath[0] = L'\0'; // Disable .ini side-effects during unit test

    const char* yaml =
        "---\n"
        "WeekendInfo:\n"
        " TrackDisplayName: Rudskogen Motorsenter\n"
        " TrackLength: 3.1502 km\n"
        "DriverInfo:\n"
        " DriverCarIdx: 0\n"
        " PaceCarIdx: 64\n"
        " Drivers:\n"
        " - CarIdx: 0\n"
        "   UserName: Vijay Nadimpalli\n"
        "   CarPath: caterham caterham420r\n"
        "   CarIsPaceCar: 0\n"
        "   CarScreenName: Caterham 420R\n"
        "   CarScreenNameShort: Caterham 420R\n"
        "   CarClassShortName: \n"
        " - CarIdx: 1\n"
        "   UserName: AI Driver\n"
        "   CarPath: caterham caterham420r\n"
        "   CarIsPaceCar: 0\n"
        "   CarScreenNameShort: Caterham 420R\n"
        "   CarClassShortName: \n"
        "...\n";

    SessionMetadata meta{};
    ParseSessionYaml(yaml, static_cast<int>(std::strlen(yaml)), meta);

    EXPECT_NEAR(meta.trackLenM, 3150.2f, 0.5f, "Rudskogen track length in meters");
    EXPECT_TRUE(meta.paceCarIdx == 64, "PaceCarIdx=64 parsed without out-of-bounds write");
    EXPECT_NEAR(meta.carLenM[0], 3.35f, 0.01f, "Caterham 420R player car length (3.35m)");
    EXPECT_NEAR(meta.carLenM[1], 3.35f, 0.01f, "Caterham 420R opponent car length (3.35m)");
    EXPECT_TRUE(!meta.isPaceCar[0] && !meta.isPaceCar[1], "Neither Caterham is flagged as pace car");
}

// ============================================================================
// TEST 2: Multi-Class IMSA Grid, Miles Conversion, Class Fallback & INI Override
// ============================================================================
static void Test_YamlParser_MultiClassAndIniOverride() {
    std::printf("[RUN ] Test_YamlParser_MultiClassAndIniOverride\n");
    g_iniPath[0] = L'\0';

    const char* yaml =
        "WeekendInfo:\n"
        " TrackLength: 3.74 mi\n"
        "DriverInfo:\n"
        " PaceCarIdx: 0\n"
        " Drivers:\n"
        " - CarIdx: 0\n"
        "   CarPath: safety pcfr500s\n"
        "   CarIsPaceCar: 1\n"
        " - CarIdx: 2\n"
        "   CarPath: porsche963gtp\n"
        "   CarClassShortName: GTP\n"
        " - CarIdx: 5\n"
        "   CarPath: dallarap217\n"
        "   CarClassShortName: LMP2\n"
        " - CarIdx: 12\n"
        "   CarPath: ferrari296gt3\n"
        "   CarClassShortName: GT3\n"
        " - CarIdx: 19\n"
        "   CarPath: bmwm4evogt4\n"
        "   CarClassShortName: GT4\n"
        " - CarIdx: 25\n"
        "   CarPath: future_unreleased_gt3_2027\n"
        "   CarClassShortName: GT3\n"
        " - CarIdx: 30\n"
        "   CarPath: mystery_prototype_xyz\n"
        "   CarClassShortName: UNKNOWN\n"
        "SplitTimeInfo:\n"
        " Sectors:\n";

    SessionMetadata meta{};
    ParseSessionYaml(yaml, static_cast<int>(std::strlen(yaml)), meta);

    EXPECT_NEAR(meta.trackLenM, 3.74f * 1609.344f, 0.5f, "Imperial miles TrackLength conversion");
    EXPECT_TRUE(meta.isPaceCar[0], "Pace car correctly flagged");
    EXPECT_NEAR(meta.carLenM[2],  5.10f, 0.01f, "Porsche 963 GTP exact length");
    EXPECT_NEAR(meta.carLenM[5],  4.75f, 0.01f, "Dallara P217 LMP2 exact length");
    EXPECT_NEAR(meta.carLenM[12], 4.57f, 0.01f, "Ferrari 296 GT3 exact length");
    EXPECT_NEAR(meta.carLenM[19], 4.80f, 0.01f, "BMW M4 EVO GT4 substring priority");
    EXPECT_NEAR(meta.carLenM[25], 4.72f, 0.01f, "Unreleased GT3 falls back to GT3 class length (4.72m)");
    EXPECT_NEAR(meta.carLenM[30], DEFAULT_CAR_LEN_M, 0.01f, "Completely unknown car falls back to DEFAULT_CAR_LEN_M");

    // Test INI [CarLengths] custom override
    wchar_t tempIni[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempIni);
    wcscat_s(tempIni, L"spotter_test_override.ini");
    DeleteFileW(tempIni);
    wcscpy_s(g_iniPath, tempIni);

    WritePrivateProfileStringW(L"CarLengths", L"mystery_prototype_xyz", L"5.42", g_iniPath);
    ParseSessionYaml(yaml, static_cast<int>(std::strlen(yaml)), meta);
    EXPECT_NEAR(meta.carLenM[30], 5.42f, 0.01f, "INI [CarLengths] override takes precedence over default");

    DeleteFileW(tempIni);
    g_iniPath[0] = L'\0';
}

// ============================================================================
// TEST 3: Asymmetric Multi-Class Bumper Projection (Caterham 420R vs GTP)
// ============================================================================
static void Test_AsymmetricBumperProjection_CaterhamVsGTP() {
    std::printf("[RUN ] Test_AsymmetricBumperProjection_CaterhamVsGTP\n");

    const float hCaterham = 0.5f * (3.35f + SPLINE_BUFFER_M); // 1.85m half-hull
    const float hGTP      = 0.5f * (5.10f + SPLINE_BUFFER_M); // 2.725m half-hull

    // Case A: Driving Caterham (3.35m) door-to-door (deltaM = 0) with a GTP (5.10m)
    // GTP overhangs both front (+2.725 > +1.85) and rear (-2.725 < -1.85) bumpers of the Caterham!
    BarRenderState catBar = SpotterProcessor::ComputeAsymmetricBarSpan(hCaterham, -hGTP, +hGTP, 1);
    EXPECT_TRUE(catBar.active && !catBar.isCapOnly, "Caterham bar is active overlap");
    EXPECT_NEAR(catBar.yTopFrac, 0.0f, 0.001f, "GTP overhangs Caterham nose -> yTopFrac = 0.0");
    EXPECT_NEAR(catBar.yBotFrac, 1.0f, 0.001f, "GTP overhangs Caterham tail -> yBotFrac = 1.0");

    // Case B: Driving GTP (5.10m) door-to-door (deltaM = 0) with a Caterham (3.35m)
    // Caterham is shorter than GTP -> inset margins at both top and bottom of GTP's bar!
    BarRenderState gtpBar = SpotterProcessor::ComputeAsymmetricBarSpan(hGTP, -hCaterham, +hCaterham, 2);
    float expectedTop = (hGTP - hCaterham) / (2.0f * hGTP); // ~0.16055
    float expectedBot = (hGTP + hCaterham) / (2.0f * hGTP); // ~0.83945
    EXPECT_TRUE(gtpBar.active && !gtpBar.isCapOnly, "GTP bar is active overlap");
    EXPECT_NEAR(gtpBar.yTopFrac, expectedTop, 0.002f, "Caterham inset from GTP nose (~0.161)");
    EXPECT_NEAR(gtpBar.yBotFrac, expectedBot, 0.002f, "Caterham inset from GTP tail (~0.839)");

    // Case C: Approaching from behind (no physical bumper overlap yet -> bottom 7% red cap)
    BarRenderState behindCap = SpotterProcessor::ComputeAsymmetricBarSpan(hCaterham, -12.0f, -8.3f, 3);
    EXPECT_TRUE(behindCap.active && behindCap.isCapOnly, "Approaching car shows bottom warning cap");
    EXPECT_NEAR(behindCap.yTopFrac, 1.0f - EDGE_CAP_FRAC, 0.001f, "Bottom cap top = 0.93");
    EXPECT_NEAR(behindCap.yBotFrac, 1.0f, 0.001f, "Bottom cap bottom = 1.00");

    // Case D: Just cleared ahead (rear bumper ahead of player nose -> top 7% red cap)
    BarRenderState aheadCap = SpotterProcessor::ComputeAsymmetricBarSpan(hCaterham, +2.5f, +6.2f, 3);
    EXPECT_TRUE(aheadCap.active && aheadCap.isCapOnly, "Clearing car shows top warning cap");
    EXPECT_NEAR(aheadCap.yTopFrac, 0.0f, 0.001f, "Top cap top = 0.00");
    EXPECT_NEAR(aheadCap.yBotFrac, EDGE_CAP_FRAC, 0.001f, "Top cap bottom = 0.07");
}

// ============================================================================
// TEST 4: Start/Finish Line Wrap-Around (0.9995 -> 0.0005)
// ============================================================================
static void Test_StartFinishSplineWrapAround() {
    std::printf("[RUN ] Test_StartFinishSplineWrapAround\n");

    SessionMetadata meta{};
    meta.trackLenM   = 3150.2f; // Rudskogen
    meta.carLenM[0]  = 3.35f;   // Player Caterham
    meta.carLenM[1]  = 3.35f;   // Opponent Caterham

    FrameSnapshot snap{};
    snap.isOnTrack        = true;
    snap.playerCarIdx     = 0;
    snap.carLeftRight     = irsdk_LRCarLeft;
    snap.trackSurface[0]  = irsdk_OnTrack;
    snap.trackSurface[1]  = irsdk_OnTrack;
    // Player is 1.57m before Start/Finish line; Opponent is 1.57m past Start/Finish line (total delta = +3.15m)
    snap.lapDistPct[0]    = 0.9995f;
    snap.lapDistPct[1]    = 0.0005f;

    SpotterProcessor proc{};
    BarRenderState left{}, right{};
    proc.Evaluate(snap, meta, left, right);

    // At delta = +3.15m with 2*hCaterham = 3.70m, bumpers overlap by 0.55m at the player's front nose!
    EXPECT_TRUE(left.active && !left.isCapOnly, "Wrap-around across S/F line detects physical front-nose overlap");
    EXPECT_NEAR(left.yTopFrac, 0.0f, 0.001f, "Opponent nose is ahead of player nose across S/F line");
    EXPECT_TRUE(left.yBotFrac > EDGE_CAP_FRAC && left.yBotFrac < 0.30f, "Opponent tail overlaps front ~15% of player bar");
    EXPECT_TRUE(!right.active, "Right bar remains inactive");
}

// ============================================================================
// TEST 5: Extreme Speed-Delta Overtake Chain When Player Slows Down
// ============================================================================
static void Test_HighSpeedDeltaOvertakeChain_SlowPlayer() {
    std::printf("[RUN ] Test_HighSpeedDeltaOvertakeChain_SlowPlayer\n");

    SessionMetadata meta{};
    meta.trackLenM  = 3150.2f;
    meta.carLenM[0] = 3.35f; // Player slowed down in Caterham 420R
    meta.carLenM[1] = 3.35f; // Car 1: Already passed player (+11m ahead and pulling away)
    meta.carLenM[2] = 3.35f; // Car 2: Flying up from behind (-42m -> +10m at +50 m/s = 0.833 m/tick)

    SpotterProcessor proc{};
    StereoChimeEngine chime{};
    chime.setMuted(true); // Don't play actual audio during automated test

    FrameSnapshot snap{};
    snap.isOnTrack       = true;
    snap.playerCarIdx    = 0;
    snap.carLeftRight    = irsdk_LRCarLeft;
    snap.trackSurface[0] = irsdk_OnTrack;
    snap.trackSurface[1] = irsdk_OnTrack;
    snap.trackSurface[2] = irsdk_OnTrack;
    snap.lapDistPct[0]   = 0.5000f;

    bool sawBottomApproachingCap = false;
    bool sawMidDoorOverlap       = false;
    bool sawTopClearingCap       = false;
    bool everLockedWrongCarAhead = false;

    // Simulate 65 ticks (~1.08 seconds) of Car 2 flying past at +50 m/s while Car 1 pulls away ahead
    for (int tick = 0; tick < 65; ++tick) {
        float car1DeltaM = 11.0f + tick * 0.60f;  // Pulling away ahead (+11m -> +50m)
        float car2DeltaM = -42.0f + tick * 0.85f; // Approaching fast from behind (-42m -> +13.25m)

        snap.tickCount     = tick + 1;
        snap.lapDistPct[1] = snap.lapDistPct[0] + (car1DeltaM / meta.trackLenM);
        snap.lapDistPct[2] = snap.lapDistPct[0] + (car2DeltaM / meta.trackLenM);

        BarRenderState left{}, right{};
        proc.Evaluate(snap, meta, left, right);

        // After tick 1 (once velocity is known), processor must NEVER lock onto Car 1 pulling away ahead
        if (tick >= 1 && left.active) {
            if (left.carIdx == 1) everLockedWrongCarAhead = true;
            if (left.carIdx == 2) {
                if (left.isCapOnly && left.yTopFrac > 0.90f) sawBottomApproachingCap = true;
                if (!left.isCapOnly && left.yTopFrac < 0.25f && left.yBotFrac > 0.75f) sawMidDoorOverlap = true;
                if (left.isCapOnly && left.yBotFrac <= EDGE_CAP_FRAC + 0.001f) sawTopClearingCap = true;
            }
        }
    }

    EXPECT_TRUE(!everLockedWrongCarAhead, "Never locked onto Car 1 pulling away ahead while Car 2 was closing from behind");
    EXPECT_TRUE(sawBottomApproachingCap,  "Captured Car 2 approaching at -42m with bottom red warning cap");
    EXPECT_TRUE(sawMidDoorOverlap,        "Captured Car 2 door-to-door overlap despite +180 km/h speed delta");
    EXPECT_TRUE(sawTopClearingCap,        "Captured Car 2 clearing nose with top red warning cap");
}

// ============================================================================
// TEST 6: Early CarLeftRight Dropout Holdover During Low-Speed Overlap
// ============================================================================
static void Test_EarlyCarLeftRightDropoutHoldover() {
    std::printf("[RUN ] Test_EarlyCarLeftRightDropoutHoldover\n");

    SessionMetadata meta{};
    meta.trackLenM  = 3150.2f;
    meta.carLenM[0] = 3.35f;
    meta.carLenM[1] = 3.35f;

    SpotterProcessor proc{};
    FrameSnapshot snap{};
    snap.isOnTrack       = true;
    snap.playerCarIdx    = 0;
    snap.trackSurface[0] = irsdk_OnTrack;
    snap.trackSurface[1] = irsdk_OnTrack;
    snap.lapDistPct[0]   = 0.3000f;
    snap.lapDistPct[1]   = 0.3000f; // Door-to-door (deltaM = 0.0m)

    // Frame 1: iRacing reports CarLeft
    snap.carLeftRight = irsdk_LRCarLeft;
    BarRenderState l1{}, r1{};
    proc.Evaluate(snap, meta, l1, r1);
    EXPECT_TRUE(l1.active && !l1.isCapOnly, "Frame 1: Car locked on Left during overlap");

    // Frame 2: Player slows down and iRacing prematurely drops CarLeftRight to irsdk_LRClear while still door-to-door!
    snap.carLeftRight  = irsdk_LRClear;
    snap.lapDistPct[1] = snap.lapDistPct[0] + (0.8f / meta.trackLenM); // Still overlapping (+0.8m)
    BarRenderState l2{}, r2{};
    proc.Evaluate(snap, meta, l2, r2);
    EXPECT_TRUE(l2.active && !l2.isCapOnly, "Frame 2: Holdover keeps Left bar active while bumpers still physically overlap");

    // Frame 3: Opponent physically clears player's front bumper (+5.0m ahead) while CarLeftRight is Clear
    snap.lapDistPct[1] = snap.lapDistPct[0] + (5.0f / meta.trackLenM);
    BarRenderState l3{}, r3{};
    proc.Evaluate(snap, meta, l3, r3);
    EXPECT_TRUE(!l3.active, "Frame 3: Holdover releases immediately once bumpers physically clear");
}

// ============================================================================
// TEST 7: 3-Wide Identity Lock & 2-Cars Stacked Multi-Class Span Merge
// ============================================================================
static void Test_ThreeWideAndStackedMultiClass() {
    std::printf("[RUN ] Test_ThreeWideAndStackedMultiClass\n");

    SessionMetadata meta{};
    meta.trackLenM  = 5000.0f;
    meta.carLenM[0] = 4.57f; // Player: Ferrari 296 GT3
    meta.carLenM[1] = 5.10f; // Car 1 (Left): Porsche 963 GTP
    meta.carLenM[2] = 3.35f; // Car 2 (Right): Caterham 420R

    SpotterProcessor proc{};
    FrameSnapshot snap{};
    snap.isOnTrack       = true;
    snap.playerCarIdx    = 0;
    snap.trackSurface[0] = irsdk_OnTrack;
    snap.trackSurface[1] = irsdk_OnTrack;
    snap.trackSurface[2] = irsdk_OnTrack;
    snap.lapDistPct[0]   = 0.5000f;

    // Step 1: Car 1 pulls alongside on Left first (LRCarLeft)
    snap.carLeftRight  = irsdk_LRCarLeft;
    snap.lapDistPct[1] = 0.5000f - (1.0f / meta.trackLenM); // -1.0m on Left
    snap.lapDistPct[2] = 0.5000f - (8.0f / meta.trackLenM); // -8.0m behind
    BarRenderState left{}, right{};
    proc.Evaluate(snap, meta, left, right);
    EXPECT_TRUE(left.active && left.carIdx == 1 && !right.active, "Step 1: Car 1 locked on Left");

    // Step 2: Car 2 dives down the inside on Right -> 3-Wide (LRCarLeftRight)
    snap.carLeftRight  = irsdk_LRCarLeftRight;
    snap.lapDistPct[1] = 0.5000f + (0.5f / meta.trackLenM); // Car 1 at +0.5m on Left
    snap.lapDistPct[2] = 0.5000f + (0.0f / meta.trackLenM); // Car 2 at 0.0m on Right (closer to 0 than Car 1!)
    proc.Evaluate(snap, meta, left, right);
    EXPECT_TRUE(left.active  && left.carIdx == 1,  "Step 2 (3-Wide): Left bar stays locked to Car 1 without swapping");
    EXPECT_TRUE(right.active && right.carIdx == 2, "Step 2 (3-Wide): Right bar locks onto Car 2");

    // Step 3: 2 Cars Stacked on Left (LR2CarsLeft) -> merges Rearmost to Frontmost bumper
    snap.carLeftRight  = irsdk_LR2CarsLeft;
    snap.lapDistPct[1] = 0.5000f + (2.0f / meta.trackLenM); // Ahead on Left
    snap.lapDistPct[2] = 0.5000f - (2.0f / meta.trackLenM); // Behind on Left
    proc.Evaluate(snap, meta, left, right);
    EXPECT_TRUE(left.active && !right.active, "Step 3 (LR2CarsLeft): Merged Left bar active");
    EXPECT_NEAR(left.yTopFrac, 0.0f, 0.001f,  "Step 3 (LR2CarsLeft): Front car overhangs player nose -> yTopFrac = 0.0");
    EXPECT_NEAR(left.yBotFrac, 1.0f, 0.001f,  "Step 3 (LR2CarsLeft): Rear car overhangs player tail -> yBotFrac = 1.0");
}

// ============================================================================
// TEST 8: Pit Road Isolation & Pace Car Filtering
// ============================================================================
static void Test_PitRoadAndPaceCarFiltering() {
    std::printf("[RUN ] Test_PitRoadAndPaceCarFiltering\n");

    SessionMetadata meta{};
    meta.trackLenM    = 3150.2f;
    meta.isPaceCar[1] = true; // Car 1 is Pace Car

    FrameSnapshot snap{};
    snap.isOnTrack       = true;
    snap.playerCarIdx    = 0;
    snap.carLeftRight    = irsdk_LRCarLeft;
    snap.trackSurface[0] = irsdk_OnTrack;
    snap.onPitRoad[0]    = false;
    snap.lapDistPct[0]   = 0.2500f;

    // Car 1 is Pace Car right next to player; Car 2 is on Pit Road right next to player
    snap.trackSurface[1] = irsdk_OnTrack;
    snap.lapDistPct[1]   = 0.2500f;
    snap.trackSurface[2] = irsdk_OnTrack;
    snap.onPitRoad[2]    = true;
    snap.lapDistPct[2]   = 0.2500f;

    SpotterProcessor proc{};
    BarRenderState left{}, right{};
    proc.Evaluate(snap, meta, left, right);
    EXPECT_TRUE(!left.active && !right.active, "Pace car and pit-road car are ignored while player is on track");
}

// ============================================================================
// TEST 9: Front Bonnet / Draft Pull-Out Proximity & Side-by-Side Handoff
// ============================================================================
static void Test_FrontBonnetDraftPullOutAndHandoff() {
    std::printf("[RUN ] Test_FrontBonnetDraftPullOutAndHandoff\n");

    SessionMetadata meta{};
    meta.trackLenM  = 3150.2f;
    meta.carLenM[0] = 3.35f; // Player Caterham 420R (half-len = 1.675m)
    meta.carLenM[1] = 3.35f; // Ahead Caterham 420R  (half-len = 1.675m, combined = 3.35m)
    meta.carLenM[2] = 5.10f; // Ahead Porsche 963 GTP (half-len = 2.550m, combined = 4.225m)

    SpotterProcessor proc{};
    BarRenderState left{}, right{};
    FrontProximityState front{};

    FrameSnapshot snap{};
    snap.isOnTrack       = true;
    snap.playerCarIdx    = 0;
    snap.carLeftRight    = irsdk_LRClear;
    snap.trackSurface[0] = irsdk_OnTrack;
    snap.trackSurface[1] = irsdk_OnTrack;
    snap.lapDistPct[0]   = 0.5000f;

    // 1. Gap = 4.50m (deltaM = 7.85m) -> outside 3.50m draft threshold, front bar inactive
    snap.lapDistPct[1] = 0.5000f + (7.85f / meta.trackLenM);
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(!front.active, "Gap 4.50m (> 3.50m): Front bonnet bar is inactive");

    // 2. Gap = 2.40m (deltaM = 5.75m) -> in Draft zone (Green/Cyan)
    snap.lapDistPct[1] = 0.5000f + (5.75f / meta.trackLenM);
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(front.active && front.carIdx == 1, "Gap 2.40m: Front bonnet bar activates for Car 1");
    EXPECT_TRUE(front.zone == FrontZone::Draft, "Gap 2.40m: Zone is FrontZone::Draft (Green/Cyan)");
    EXPECT_NEAR(front.bumperGapM, 2.40f, 0.02f, "Gap 2.40m: Accurate nose-to-tail bumperGapM");
    float draftFill = front.fillFrac;

    // 3. Gap = 0.90m (deltaM = 4.25m) -> in PullOut zone (Amber/Orange) & wider bar
    snap.lapDistPct[1] = 0.5000f + (4.25f / meta.trackLenM);
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(front.active && front.zone == FrontZone::PullOut, "Gap 0.90m: Zone is FrontZone::PullOut (Amber)");
    EXPECT_TRUE(front.fillFrac > draftFill, "Gap 0.90m: Horizontal bar expanded outward from center");

    // 4. Gap = 0.20m (deltaM = 3.55m) -> in Danger zone (Crimson Red, bonnet touch imminent!)
    snap.lapDistPct[1] = 0.5000f + (3.55f / meta.trackLenM);
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(front.active && front.zone == FrontZone::Danger, "Gap 0.20m: Zone is FrontZone::Danger (Red)");
    EXPECT_TRUE(front.fillFrac > 0.93f, "Gap 0.20m: Horizontal bar is nearly full width (>93%)");

    // 5. Driver pulls out to overtake -> CarLeftRight becomes irsdk_LRCarRight
    //    Front bonnet bar must immediately hide and hand off to Right vertical pillar!
    snap.carLeftRight = irsdk_LRCarRight;
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(!front.active, "Pulled alongside (LRCarRight): Front bonnet bar automatically hides");
    EXPECT_TRUE(right.active && right.carIdx == 1, "Pulled alongside (LRCarRight): Right vertical pillar takes over Car 1");

    // 6. Multi-Class Bonnet Check: At identical center-to-center deltaM = 4.65m,
    //    a Caterham ahead leaves a 1.30m gap (Draft), whereas a long GTP ahead leaves only a 0.425m gap (PullOut)!
    snap.carLeftRight    = irsdk_LRClear;
    snap.trackSurface[1] = irsdk_NotInWorld;
    snap.trackSurface[2] = irsdk_OnTrack;
    snap.lapDistPct[2]   = 0.5000f + (4.65f / meta.trackLenM);
    proc.Evaluate(snap, meta, left, right, front);
    EXPECT_TRUE(front.active && front.carIdx == 2, "Multi-class GTP ahead: Front bar locks onto Car 2");
    EXPECT_NEAR(front.bumperGapM, 0.425f, 0.02f, "Multi-class GTP ahead: Accounts for longer GTP rear overhang (0.425m gap)");
    EXPECT_TRUE(front.zone == FrontZone::PullOut, "Multi-class GTP ahead: Triggers PullOut warning earlier than shorter car");
}

int main() {
    std::printf("============================================================\n");
    std::printf(" Running iRacing Spotter Overlay Unit & Integration Tests\n");
    std::printf("============================================================\n");

    Test_YamlParser_RudskogenCaterham420R();
    Test_YamlParser_MultiClassAndIniOverride();
    Test_AsymmetricBumperProjection_CaterhamVsGTP();
    Test_StartFinishSplineWrapAround();
    Test_HighSpeedDeltaOvertakeChain_SlowPlayer();
    Test_EarlyCarLeftRightDropoutHoldover();
    Test_ThreeWideAndStackedMultiClass();
    Test_PitRoadAndPaceCarFiltering();
    Test_FrontBonnetDraftPullOutAndHandoff();

    std::printf("============================================================\n");
    if (g_failedTests == 0) {
        std::printf(" [PASS] All %d assertions passed!\n", g_totalTests);
        std::printf("============================================================\n");
        return 0;
    } else {
        std::printf(" [FAIL] %d / %d assertions failed!\n", g_failedTests, g_totalTests);
        std::printf("============================================================\n");
        return 1;
    }
}
