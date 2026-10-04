/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Betaflight. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

extern "C" {

    #include "platform.h"
    #include "build/debug.h"
    #include "pg/pg_ids.h"

    #include "common/filter.h"
    #include "common/vector.h"

    #include "fc/core.h"
    #include "fc/rc_controls.h"
    #include "fc/runtime_config.h"

    #include "flight/alt_hold.h"
    #include "flight/autopilot.h"
    #include "flight/failsafe.h"
    #include "flight/imu.h"
    #include "flight/pid.h"
    #include "flight/position.h"

    #include "io/gps.h"

    #include "rx/rx.h"

    #include "pg/alt_hold.h"
    #include "pg/autopilot.h"
#ifdef USE_GPS_RESCUE
    #include "pg/gps_rescue.h"
#endif

    #include "sensors/acceleration.h"
    #include "sensors/gyro.h"

    PG_REGISTER(accelerometerConfig_t, accelerometerConfig, PG_ACCELEROMETER_CONFIG, 0);
    PG_REGISTER(altHoldConfig_t, altHoldConfig, PG_ALTHOLD_CONFIG, 0);
    PG_REGISTER(autopilotConfig_t, autopilotConfig, PG_AUTOPILOT, 0);
#ifdef USE_GPS_RESCUE
    PG_REGISTER(gpsRescueConfig_t, gpsRescueConfig, PG_GPS_RESCUE, 0);
#endif
    PG_REGISTER(gyroConfig_t, gyroConfig, PG_GYRO_CONFIG, 0);
    PG_REGISTER(positionConfig_t, positionConfig, PG_POSITION, 0);
    PG_REGISTER(rcControlsConfig_t, rcControlsConfig, PG_RC_CONTROLS_CONFIG, 0);

    bool testFailsafeActive = false;
    bool failsafeIsActive(void) { return testFailsafeActive; }
    // v13: newest rcData-derived throttle (what updateRcCommands() will produce); <0 = same as rcCommand (no stale frame)
    float testRcThrottleNow = -1.0f;
    float getRcCommandThrottleFromRcData(void) { return testRcThrottleNow >= 0.0f ? testRcThrottleNow : rcCommand[THROTTLE]; }
    timeUs_t currentTimeUs = 0;
    bool isAltHoldActive();
    bool isAltHoldLandingMode(void);
    extern float testAltitudeCm;
    extern float testAltitudeDerivativeCmS;
    extern float testCosTiltAngle;
    extern throttleStatus_e testThrottleStatus;
    extern bool testAirmodeEnabled;
    extern bool testFailsafeActive;
    extern float testRcThrottleNow;
    // prefix of altHoldState_t (alt_hold_multirotor.c), for observing the commanded vertical velocity
    extern struct { bool isActive; float targetAltitudeCm; float maxVelocity; float targetVelocity; } altHold;
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

uint32_t millisRW;
uint32_t millis() {
    return millisRW;
}

TEST(AltholdUnittest, altHoldTransitionsTest)
{
    updateAltHold(currentTimeUs);
    EXPECT_EQ(isAltHoldActive(), false);

    flightModeFlags |= ALT_HOLD_MODE;
    millisRW = 42;
    updateAltHold(currentTimeUs);
    EXPECT_EQ(isAltHoldActive(), true);

    flightModeFlags ^= ALT_HOLD_MODE;
    millisRW = 56;
    updateAltHold(currentTimeUs);
    EXPECT_EQ(isAltHoldActive(), false);

    flightModeFlags |= ALT_HOLD_MODE;
    millisRW = 64;
    updateAltHold(currentTimeUs);
    EXPECT_EQ(isAltHoldActive(), true);
}

TEST(AltholdUnittest, altHoldTransitionsTestUnfinishedExitEnter)
{
    altHoldInit();
    EXPECT_EQ(isAltHoldActive(), false);

    flightModeFlags |= ALT_HOLD_MODE;
    millisRW = 42;
    updateAltHold(currentTimeUs);
    EXPECT_EQ(isAltHoldActive(), true);
}

// ---- custom-patch: entry latch / exit hold simulation ----
// Ported from jsungho/betaflight2026.6.x_custom, custom-patch/alt-hold-throttle-range-2026.6.2,
// commit 415e8c4 (AltholdCustomSim), adapted to the 2025.12.5 autopilot_multirotor.c API
// (this repo's altitudeControl()/getAutopilotThrottle() signatures and pg/autopilot.h layout).
extern "C" {
    bool altHoldRequestActive(bool switchOn);
    void altHoldClearExitPending(void);
    bool isAltHoldExitPending(void);
}

class AltholdCustomSim : public ::testing::Test {
protected:
    bool armed = true;
    void SetUp() override {
        memset(rcCommand, 0, sizeof(rcCommand));
        flightModeFlags = 0;
        testAltitudeCm = 0.0f;
        testAltitudeDerivativeCmS = 0.0f;
        testCosTiltAngle = 1.0f;
        testThrottleStatus = THROTTLE_HIGH;
        autopilotConfigMutable()->hoverThrottle = 1300;   // ap_hover_throttle
        autopilotConfigMutable()->throttleMin = 1100;
        autopilotConfigMutable()->throttleMax = 1900;
        autopilotConfigMutable()->altitudeP = 15;
        autopilotConfigMutable()->altitudeI = 15;
        autopilotConfigMutable()->altitudeD = 15;
        altHoldConfigMutable()->hoverThrottle = 1400;     // alt_hold_hover_throttle
        altHoldConfigMutable()->deadband = 25;
        altHoldConfigMutable()->deadbandLow = 0;
        altHoldConfigMutable()->fullLowIsMaxDescend = true;
        altHoldConfigMutable()->climbRate = 70;
        rxConfigMutable()->mincheck = 1050;
        rxConfigMutable()->rc_smoothing = false;          // tests opt in to RC smoothing ON explicitly
        autopilotInit();
        altHoldInit();
        altHoldClearExitPending();
        resetAltitudeControl();
    }
    // one 100 Hz cycle: emulate fc/core.c's Alt Hold mode decision, then the Alt Hold task
    void step(bool sw, float stick) {
        rcCommand[THROTTLE] = stick;
        testThrottleStatus = stick < 1050 ? THROTTLE_LOW : THROTTLE_HIGH;
        if (armed && altHoldRequestActive(sw)) {
            flightModeFlags |= ALT_HOLD_MODE;
        } else {
            flightModeFlags &= ~ALT_HOLD_MODE;
            altHoldClearExitPending();
        }
        updateAltHold(currentTimeUs);
    }
    void run(bool sw, float stick, int n) { for (int i = 0; i < n; i++) step(sw, stick); }
    // v13: real RX order: flight-mode decision runs while rcCommand[THROTTLE] still holds the OLD frame's value,
    // then updateRcCommands() publishes the new one
    void stepStale(bool sw, float oldStick, float newStick) {
        rcCommand[THROTTLE] = oldStick;
        testRcThrottleNow = newStick;
        testThrottleStatus = oldStick < 1050 ? THROTTLE_LOW : THROTTLE_HIGH;
        if (armed && altHoldRequestActive(sw)) {
            flightModeFlags |= ALT_HOLD_MODE;
        } else {
            flightModeFlags &= ~ALT_HOLD_MODE;
            altHoldClearExitPending();
        }
        rcCommand[THROTTLE] = newStick;                 // updateRcCommands()
        testRcThrottleNow = -1.0f;
        testThrottleStatus = newStick < 1050 ? THROTTLE_LOW : THROTTLE_HIGH;
        updateAltHold(currentTimeUs);
    }
    float thrPwm() const { return 1000.0f + 1000.0f * getAutopilotThrottle(); }
    bool modeOn() const { return flightModeFlags & ALT_HOLD_MODE; }
};

TEST_F(AltholdCustomSim, EntryLatchHoldsAltitudeWhileStickBelowHover)
{
    run(true, 1200, 300);             // enter with stick well below hover(1400), 3 s
    EXPECT_TRUE(isAltHoldActive());
    const float held = thrPwm();
    EXPECT_GT(held, 1350.0f);          // holding: no descent despite stick < hover
    run(true, 1230, 300);              // +30us (<5%): still latched
    EXPECT_GT(thrPwm(), 1350.0f);
}

TEST_F(AltholdCustomSim, EntryLatchReleasesAt5PercentAndAbsolutePositionApplies)
{
    run(true, 1200, 100);
    run(true, 1140, 300);              // moved 60us (>=50): released, stick below hover => descend
    EXPECT_LT(thrPwm(), 1300.0f);
}

TEST_F(AltholdCustomSim, EntryLatchIgnoresFullLowMaxDescendUntilMoved)
{
    run(true, 1000, 300);              // THROTTLE_LOW at entry, fullLowIsMaxDescend ON
    EXPECT_GT(thrPwm(), 1350.0f);   // still holding
    run(true, 1040, 300);              // +40: still latched
    EXPECT_GT(thrPwm(), 1350.0f);
    run(true, 1060, 300);              // +60: released, stick below threshold => max descend
    EXPECT_LT(thrPwm(), 1300.0f);
}

TEST_F(AltholdCustomSim, ExitHoldKeepsAltitudeUntilStickReachesApHoverBand)
{
    run(true, 1400, 100);
    run(false, 1100, 1);               // switch off, stick far below ap_hover(1300)
    EXPECT_TRUE(isAltHoldExitPending());
    EXPECT_TRUE(modeOn());
    run(false, 1150, 200);
    EXPECT_TRUE(modeOn());
    EXPECT_GT(thrPwm(), 1350.0f);   // altitude still held, not following stick
    run(false, 1200, 10);              // v15: band centre is ap_hover in rcCommand units (1300 -> 1263), band 1213..1313
    EXPECT_TRUE(modeOn());
    run(false, 1215, 1);               // inside the band
    EXPECT_FALSE(modeOn());
    EXPECT_FALSE(isAltHoldExitPending());
}

// v15: ap_hover_throttle (PWM, normalised with min_check) is converted to the rcCommand scale before the band check
TEST_F(AltholdCustomSim, ExitBandCentreIsApHoverConvertedToRcCommandScale)
{
    run(true, 1400, 100);
    run(false, 1600, 1);               // far above
    run(false, 1320, 5);               // 1320 > 1263 + 50: outside (would be inside if compared to raw 1300)
    EXPECT_TRUE(modeOn());
    run(false, 1310, 1);               // inside 1263 + 50
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, ExitBandCentreFollowsMinCheck)
{
    rxConfigMutable()->mincheck = 1100;                // 1300 -> 1000 + 1000 * 200 / 900 = 1222
    run(true, 1400, 100);
    run(false, 1600, 1);
    run(false, 1280, 5);               // 1280 > 1222 + 50
    EXPECT_TRUE(modeOn());
    run(false, 1270, 1);               // inside
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, ExitHoldUsesApHoverNotAltHoldHover)
{
    run(true, 1400, 100);
    run(false, 1500, 1);               // above both hover values
    run(false, 1380, 5);               // inside alt_hold_hover(1400) band, outside ap_hover(1300) band, no crossing
    EXPECT_TRUE(modeOn());
    run(false, 1360, 5);               // still above ap_hover + 50
    EXPECT_TRUE(modeOn());
    run(false, 1300, 1);               // inside ap_hover band
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, ExitHoldImmediateWhenStickAlreadyAtHover)
{
    run(true, 1400, 100);
    run(false, 1310, 1);
    EXPECT_FALSE(modeOn());
    EXPECT_FALSE(isAltHoldExitPending());
}

TEST_F(AltholdCustomSim, ExitHoldReleasesWhenStickSkipsBandQuickly)
{
    run(true, 1400, 100);
    run(false, 1100, 1);
    run(false, 1100, 50);
    EXPECT_TRUE(modeOn());
    run(false, 1500, 1);               // fast flick straight across the band in one sample
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, ExitHoldReleasesWhenStickSkipsBandDownward)
{
    run(true, 1400, 100);
    run(false, 1600, 1);
    run(false, 1600, 50);
    EXPECT_TRUE(modeOn());
    run(false, 1000, 1);
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, SwitchBackOnDuringExitHoldRelatches)
{
    run(true, 1400, 100);
    run(false, 1100, 50);
    EXPECT_TRUE(isAltHoldExitPending());
    run(true, 1100, 300);              // switch on again: fresh entry latch at 1100
    EXPECT_FALSE(isAltHoldExitPending());
    EXPECT_TRUE(modeOn());
    EXPECT_GT(thrPwm(), 1300.0f);   // no descent from the low stick
}

TEST_F(AltholdCustomSim, DisarmClearsExitHold)
{
    run(true, 1400, 100);
    run(false, 1100, 50);
    EXPECT_TRUE(isAltHoldExitPending());
    armed = false;
    step(false, 1100);
    EXPECT_FALSE(isAltHoldExitPending());
    EXPECT_FALSE(modeOn());
    armed = true;
    step(false, 1100);                 // switch still off after re-arm: must not resume hold
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, SwitchOffWhileNotActiveDoesNotStartExitHold)
{
    run(false, 1100, 50);              // never entered
    EXPECT_FALSE(isAltHoldExitPending());
    EXPECT_FALSE(modeOn());
}

// ---- custom-patch: landing assist (Alt Hold + Airmode OFF) ----
// Ported from jsungho/betaflight2026.6.x_custom, custom-patch/alt-hold-throttle-range-2026.6.2,
// commits 90d5d9c/f29a459/0f78e2e/e2c4e83, adapted to 2025.12.5 (getAltitudeCm()/altHold.maxVelocity).
// ---- custom-patch: alt_hold_hover_throttle validation / pilot-switch-only use ----
// climbRate = 0 freezes the target, so with zero error the commanded throttle is exactly the hover baseline.
class AltholdHoverThrottle : public AltholdCustomSim {
protected:
    void SetUp() override {
        AltholdCustomSim::SetUp();
        altHoldConfigMutable()->climbRate = 0;
        altHoldInit();
        testFailsafeActive = false;
    }
    void TearDown() override { testFailsafeActive = false; }
    // throttle altitudeControl() outputs for a hover baseline (mincheck 1050 .. 2000 scaling, see altitudeControl())
    static float pwmFor(float hover) { return 1000.0f + 1000.0f * (hover - 1050.0f) / 950.0f; }
};

TEST_F(AltholdHoverThrottle, ValidAltHoldHoverIsUsedForSwitchAltHold)
{
    run(true, 1400, 50);
    EXPECT_TRUE(modeOn());
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);     // alt_hold_hover_throttle (1400), not ap_hover_throttle (1300)
}

TEST_F(AltholdHoverThrottle, FailsafeUsesApHoverAndReturnsToAltHoldHover)
{
    run(true, 1400, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);
    testFailsafeActive = true;
    run(true, 1400, 1);
    EXPECT_NEAR(thrPwm(), pwmFor(1300), 1.0f);     // failsafe landing: ap_hover_throttle
    testFailsafeActive = false;
    run(true, 1400, 1);
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);     // released: back to alt_hold_hover_throttle
}

TEST_F(AltholdHoverThrottle, OutOfRangeAltHoldHoverIsIgnored)
{
    const uint16_t invalid[] = { 1, 500, 1099, 1701 };
    for (uint16_t v : invalid) {
        altHoldConfigMutable()->hoverThrottle = v;
        altHoldInit();
        run(false, 1000, 1);                       // leave Alt Hold between cases
        flightModeFlags = 0;
        run(true, 1300, 50);
        EXPECT_TRUE(modeOn()) << v;
        EXPECT_NEAR(thrPwm(), pwmFor(1300), 1.0f) << "alt_hold_hover_throttle=" << v;   // ap_hover_throttle
    }
}

TEST_F(AltholdHoverThrottle, BoundaryValuesAre1100And1700)
{
    altHoldConfigMutable()->hoverThrottle = 1100;
    altHoldInit();
    run(true, 1100, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1100), 1.0f);
    altHoldConfigMutable()->hoverThrottle = 1700;
    altHoldInit();                                 // hover values are fixed at Alt Hold entry, so re-enter
    run(true, 1700, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1700), 1.0f);
}

TEST_F(AltholdHoverThrottle, ApHoverZeroUsesEntryStickCaptureOnlyDuringFailsafe)
{
    autopilotConfigMutable()->hoverThrottle = 0;           // ap_hover_throttle unset
    altHoldInit();
    run(true, 1200, 50);                                   // entry stick = 1200 -> captured value
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);             // normal: validated alt_hold_hover_throttle
    testFailsafeActive = true;
    run(true, 1200, 1);
    EXPECT_NEAR(thrPwm(), pwmFor(1200), 1.0f);             // failsafe: entry stick capture, not the dedicated value
    testFailsafeActive = false;
    run(true, 1200, 1);
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);
}

TEST_F(AltholdHoverThrottle, ApHoverZeroNoDedicatedValueUsesStickThenDefault)
{
    autopilotConfigMutable()->hoverThrottle = 0;
    altHoldConfigMutable()->hoverThrottle = 500;           // invalid -> 0
    altHoldInit();
    run(true, 1200, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1200), 1.0f);             // captured stick is the only candidate
}

TEST_F(AltholdHoverThrottle, ClearedOnExitAndRecapturedOnReentry)
{
    autopilotConfigMutable()->hoverThrottle = 0;
    altHoldInit();
    run(true, 1200, 50);
    run(false, 1200, 1);                                   // switch off, stick at 1200 (== captured hover): releases at once
    EXPECT_FALSE(modeOn());
    run(false, 1200, 1);                                   // mode off: both values zeroed
    run(true, 1250, 50);                                   // fresh entry captures 1250
    testFailsafeActive = true;
    run(true, 1250, 1);
    EXPECT_NEAR(thrPwm(), pwmFor(1250), 1.0f);
}

TEST_F(AltholdHoverThrottle, GpsRescueModeFlagBypassesDedicatedValueImmediately)
{
    run(true, 1400, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);
    flightModeFlags |= GPS_RESCUE_MODE;                    // before the flight mode update drops ALT_HOLD_MODE
    updateAltHold(currentTimeUs);                          // Alt Hold task still runs once with ALT_HOLD_MODE set
    EXPECT_NEAR(thrPwm(), pwmFor(1300), 1.0f);             // ap_hover_throttle
    flightModeFlags &= ~GPS_RESCUE_MODE;
    updateAltHold(currentTimeUs);
    EXPECT_NEAR(thrPwm(), pwmFor(1400), 1.0f);
}

TEST_F(AltholdHoverThrottle, ZeroInheritsApHover)
{
    altHoldConfigMutable()->hoverThrottle = 0;
    altHoldInit();
    run(true, 1300, 50);
    EXPECT_NEAR(thrPwm(), pwmFor(1300), 1.0f);
}

// v13: switch off + stick drop in the same RX frame must not release on the previous frame's stick value
TEST_F(AltholdCustomSim, ExitDecisionUsesNewFrameThrottleNotStaleRcCommand)
{
    rxConfigMutable()->rc_smoothing = false;           // plain per-frame rcCommand: manual output after release = new frame
    run(true, 1300, 50);                               // Alt Hold on, stick at ap_hover_throttle (1300)
    stepStale(false, 1300, 1000);                      // switch off AND stick to 1000 in the same frame
    EXPECT_TRUE(modeOn());                             // still holding (old stale 1300 would have released at once)
    EXPECT_TRUE(isAltHoldExitPending());
    run(false, 1000, 5);
    EXPECT_TRUE(modeOn());                             // stick far from hover: keeps holding
    run(false, 1300, 1);                               // stick back to hover: now released
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, ExitDecisionReleasesWhenNewFrameIsAtHover)
{
    rxConfigMutable()->rc_smoothing = false;
    run(true, 1000 + 1, 1);                            // (entry value irrelevant)
    run(true, 1500, 50);
    stepStale(false, 1000, 1300);                      // stale value far from hover, new frame exactly at hover
    EXPECT_FALSE(modeOn());                            // released on the new value
}

// v14: RC smoothing ON. rcCommand[THROTTLE] is the filter output, which is also what the mixer uses after release.
// Release must be judged on that value, not on the unfiltered newest frame.
TEST_F(AltholdCustomSim, SmoothingOnExitWaitsForFilteredThrottleToReachHover)
{
    rxConfigMutable()->rc_smoothing = true;
    run(true, 1300, 50);
    run(false, 1000, 5);                               // ALT WAIT, stick low
    EXPECT_TRUE(isAltHoldExitPending());
    stepStale(false, 1000, 1300);                      // raw jumps to hover, filter output still at 1000
    EXPECT_TRUE(modeOn());                             // not released: manual output would still be ~1000 (dip)
    run(false, 1300, 1);                               // filter output has caught up
    EXPECT_FALSE(modeOn());
}

TEST_F(AltholdCustomSim, SmoothingOnSwitchOffWithStickDropReleasesContinuously)
{
    rxConfigMutable()->rc_smoothing = true;
    run(true, 1300, 50);
    stepStale(false, 1300, 1000);                      // filter output still at hover: hand-over is continuous, then follows the stick
    EXPECT_FALSE(modeOn());
}

// v14: entry latch capture and comparison use the same raw value, so a stationary stick never releases the latch
TEST_F(AltholdCustomSim, ReentryLatchNotReleasedByFilterCatchUp)
{
    rxConfigMutable()->rc_smoothing = true;
    run(true, 1300, 5);
    run(true, 1600, 5);                                // latch released by real stick movement
    run(false, 1600, 1);                               // switch off: ALT WAIT (stick far from hover)
    EXPECT_TRUE(isAltHoldExitPending());
    // switch back on while the stick is flicked to 1500: raw is 1500 but the filtered rcCommand still reads 1100
    rcCommand[THROTTLE] = 1100;
    testRcThrottleNow = 1500;
    testThrottleStatus = THROTTLE_HIGH;
    EXPECT_TRUE(altHoldRequestActive(true));
    flightModeFlags |= ALT_HOLD_MODE;
    for (int i = 0; i < 10; i++) {
        updateAltHold(currentTimeUs);                  // stick stationary at raw 1500 while the filter catches up
    }
    EXPECT_NEAR(altHold.targetVelocity, 0.0f, 0.001f); // latch still holding: no climb/descend command
    testRcThrottleNow = -1.0f;
}

#ifdef USE_GPS_RESCUE
class AltholdLandingAssist : public AltholdCustomSim {
protected:
    void SetUp() override {
        AltholdCustomSim::SetUp();
        armingFlags |= ARMED;
        testAirmodeEnabled = false;
        gpsRescueConfigMutable()->descendRate = 150;      // gps_rescue_descend_rate (cm/s)
        altHoldConfigMutable()->climbRate = 70;           // 700 cm/s
        altHoldInit();
        run(true, 1400, 20);                              // enter, latch
        run(true, 1000, 5);                               // stick full low (released latch), fullLowIsMaxDescend
    }
    void TearDown() override { armingFlags &= ~ARMED; testAirmodeEnabled = true; testFailsafeActive = false; }
};

TEST_F(AltholdLandingAssist, AboveFiveMetersUsesAltHoldClimbRate)
{
    testAltitudeCm = 800.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, BelowFiveMetersUsesTwiceDescendRate)
{
    testAltitudeCm = 450.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
    testAltitudeCm = 201.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, BelowTwoMetersUsesDescendRate)
{
    testAltitudeCm = 150.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -150.0f, 1.0f);
    testAltitudeCm = 0.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -150.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, AirmodeOnKeepsAltHoldClimbRateEvenLow)
{
    testAltitudeCm = 150.0f;
    testAirmodeEnabled = true;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, PartialStickScalesWithLandingRate)
{
    testAltitudeCm = 150.0f;
    run(true, 1200, 3);                                   // halfway between hover(1400) and 1000 => factor -0.5
    EXPECT_NEAR(altHold.targetVelocity, -75.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, ClimbAlsoCappedWhileLanding)
{
    testAltitudeCm = 150.0f;
    run(true, 2000, 3);                                   // full stick up, airmode off, below 2 m
    EXPECT_NEAR(altHold.targetVelocity, 150.0f, 1.0f);
}

// v12: landing assist lowering the rate cap must not freeze the target (stick ignored) when the existing
// target lead is larger than the new 1 s gate (e.g. 350 cm lead vs 300 cm gate below 5 m).
TEST_F(AltholdLandingAssist, RateDropDoesNotFreezeTargetWhenLeadExceedsNewGate)
{
    flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
    run(false, 1000, 1);
    testAltitudeCm = 800.0f;
    run(true, 1400, 1);                                // fresh entry at 8 m
    run(true, 1000, 150);                              // full low: target leads 700 cm below the (fixed) altitude
    EXPECT_NEAR(800.0f - altHold.targetAltitudeCm, 700.0f, 10.0f);
    testAltitudeCm = 450.0f;                           // below 5 m: cap 300 cm/s, lead would be 350 > gate 300
    run(true, 2000, 1);                                // pilot commands full climb
    const float t1 = altHold.targetAltitudeCm;
    EXPECT_LE(450.0f - t1, 300.0f);                    // lead pulled inside the new gate
    run(true, 2000, 50);                               // 0.5 s of climb command
    EXPECT_NEAR(altHold.targetAltitudeCm - t1, 150.0f, 5.0f);   // +300 cm/s x 0.5 s: stick moves the target
}

TEST_F(AltholdLandingAssist, NearGroundRateDropAlsoPullsTargetIn)
{
    flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
    run(false, 1000, 1);
    testAltitudeCm = 300.0f;
    run(true, 1400, 1);
    run(true, 1000, 150);                              // 5 m zone (cap 300): lead held at 0.9 x gate = 270 cm
    EXPECT_NEAR(300.0f - altHold.targetAltitudeCm, 270.0f, 10.0f);
    testAltitudeCm = 170.0f;                           // below 1.8 m: cap 150 cm/s, gate 150, lead would be 170
    run(true, 2000, 1);
    EXPECT_LE(170.0f - altHold.targetAltitudeCm, 150.0f);       // pulled inside the new gate
    const float t1 = altHold.targetAltitudeCm;
    run(true, 2000, 50);
    EXPECT_NEAR(altHold.targetAltitudeCm - t1, 75.0f, 5.0f);    // +150 cm/s x 0.5 s: stick moves the target
}

// v13: holding (stick in the deadband / entry latch) must keep the target even if the quad sags below it
TEST_F(AltholdLandingAssist, HoldingKeepsTargetWhenQuadSags)
{
    flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
    run(false, 1000, 1);
    testAltitudeCm = 150.0f;
    run(true, 1400, 1);                                // entry at 1.5 m, stick at hover -> entry latch holds
    testAltitudeCm = 0.0f;                             // quad sags to the ground, error 150 > 0.9 x gate(150)
    run(true, 1400, 50);
    EXPECT_NEAR(altHold.targetAltitudeCm, 150.0f, 0.5f);   // no clamp while the stick is not moving the target
}

// v13: landing assist must never raise the cap above alt_hold_climb_rate
TEST_F(AltholdLandingAssist, LandingCapNeverAboveClimbRate)
{
    altHoldConfigMutable()->climbRate = 10;            // 100 cm/s, below gps_rescue_descend_rate x1 (150) and x2 (300)
    flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
    run(false, 1000, 1);
    testAltitudeCm = 450.0f;
    run(true, 1400, 1);
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -100.0f, 1.0f);    // x2 rule would give -300
    testAltitudeCm = 150.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -100.0f, 1.0f);    // x1 rule would give -150
}

TEST_F(AltholdLandingAssist, NormalCapStillFreezesFarTargetAsStock)
{
    // outside landing assist (airmode ON) the stock gate behaviour is unchanged
    flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
    testAirmodeEnabled = true;
    run(false, 1000, 1);
    testAltitudeCm = 800.0f;
    run(true, 1400, 1);
    testAltitudeCm = 0.0f;                             // 800 cm away > 700 gate
    const float t0 = altHold.targetAltitudeCm;
    run(true, 2000, 20);
    EXPECT_NEAR(altHold.targetAltitudeCm, t0, 0.5f);
}

TEST_F(AltholdLandingAssist, LandingModeHasHysteresisFiveOnFivePointFiveOff)
{
    testAltitudeCm = 501.0f;
    isAltHoldLandingMode();                    // SetUp latched at altitude 0; leave it by going high first
    testAltitudeCm = 800.0f;
    EXPECT_FALSE(isAltHoldLandingMode());      // above 5.5 m: off
    testAltitudeCm = 520.0f;
    EXPECT_FALSE(isAltHoldLandingMode());      // between 5.0 and 5.5 m while off: stays off
    testAltitudeCm = 500.0f;
    EXPECT_TRUE(isAltHoldLandingMode());       // reaches 5.0 m: on
    testAltitudeCm = 549.0f;
    EXPECT_TRUE(isAltHoldLandingMode());       // between 5.0 and 5.5 m while on: stays on
    testAltitudeCm = 551.0f;
    EXPECT_FALSE(isAltHoldLandingMode());      // above 5.5 m: off
    testAltitudeCm = 100.0f;
    EXPECT_TRUE(isAltHoldLandingMode());
}

TEST_F(AltholdLandingAssist, TwoMeterBoundaryHasHysteresisOneEightOnTwoTwoOff)
{
    testAltitudeCm = 400.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);   // x2
    testAltitudeCm = 190.0f;                              // 1.9 m coming down: not yet x1
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
    testAltitudeCm = 180.0f;                              // 1.8 m: x1
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -150.0f, 1.0f);
    testAltitudeCm = 215.0f;                              // 2.15 m wobbling up: stays x1
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -150.0f, 1.0f);
    testAltitudeCm = 225.0f;                              // above 2.2 m: back to x2
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
    testAltitudeCm = 210.0f;                              // between 1.8 and 2.2 while x2: stays x2
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, HysteresisAlsoAppliesToRateCap)
{
    testAltitudeCm = 800.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
    testAltitudeCm = 520.0f;                   // coming down through 5.2 m: not yet landing assist
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
    testAltitudeCm = 480.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
    testAltitudeCm = 530.0f;                   // wobbling up to 5.3 m: stays at x2 rate
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);
    testAltitudeCm = 560.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, FailsafeKeepsItsOwnDescentLogic)
{
    // failsafe landing is excluded from landing assist: stickFactor = -(0.9 + clamp(alt/2000, 0.1, 9)) times the
    // normal alt_hold_climb_rate cap (700), i.e. -1.0 * 700 at 1.5 m, not the landing assist 150
    testAltitudeCm = 150.0f;
    testFailsafeActive = true;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -700.0f, 1.0f);
}

TEST_F(AltholdLandingAssist, NearLatchIsClearedWhenAltHoldExits)
{
    testAltitudeCm = 150.0f;
    run(true, 1000, 3);
    EXPECT_NEAR(altHold.targetVelocity, -150.0f, 1.0f);   // 1.8 m latch on
    run(false, 1300, 3);                                  // switch off with stick at ap_hover: exits at once
    EXPECT_FALSE(modeOn());
    testAltitudeCm = 210.0f;                              // inside the 1.8..2.2 m hysteresis window
    run(true, 1400, 20);                                  // fresh entry
    run(true, 1000, 5);
    EXPECT_NEAR(altHold.targetVelocity, -300.0f, 1.0f);   // stale latch would wrongly give -150
}

TEST_F(AltholdLandingAssist, LandingModeFlagFollowsAirmodeAndArming)
{
    EXPECT_TRUE(isAltHoldLandingMode());
    testAirmodeEnabled = true;
    EXPECT_FALSE(isAltHoldLandingMode());
    testAirmodeEnabled = false;
    EXPECT_TRUE(isAltHoldLandingMode());
    armingFlags &= ~ARMED;
    EXPECT_FALSE(isAltHoldLandingMode());
}
#endif // USE_GPS_RESCUE

// STUBS

extern "C" {
    uint8_t armingFlags = 0;
    int16_t debug[DEBUG16_VALUE_COUNT];
    uint8_t debugMode;
    uint16_t flightModeFlags = 0;
    uint8_t stateFlags = 0;

    acc_t acc;
    attitudeEulerAngles_t attitude;
    gpsSolutionData_t gpsSol;

    float testAltitudeCm = 0.0f;
    float testAltitudeDerivativeCmS = 0.0f;
    float testCosTiltAngle = 1.0f;
    throttleStatus_e testThrottleStatus = THROTTLE_LOW;

    float getAltitudeCm(void) { return testAltitudeCm; }
    float getAltitudeDerivative(void) { return testAltitudeDerivativeCmS; }
    float getCosTiltAngle(void) { return testCosTiltAngle; }
    float getGpsDataIntervalSeconds(void) { return 0.01f; }
    float getGpsDataFrequencyHz(void) { return 10.0f; }
    float rcCommand[4];

    bool gpsHasNewData(uint16_t* gpsStamp) {
        UNUSED(*gpsStamp);
        return true;
    }

    void GPS_distance2d(const gpsLocation_t* /*from*/, const gpsLocation_t* /*to*/, vector2_t* /*dest*/) { }

    void parseRcChannels(const char *input, rxConfig_t *rxConfig) {
        UNUSED(input);
        UNUSED(rxConfig);
    }

    throttleStatus_e calculateThrottleStatus() {
        return testThrottleStatus;
    }

    bool testAirmodeEnabled = true;
    bool isAirmodeEnabled(void) { return testAirmodeEnabled; }
}

// ======================================================================================================
// v14 closed-loop simulation: simple vertical plant + RC smoothing filter + real Alt Hold / altitudeControl.
//  - stick is expressed in rcCommand units (what updateRcCommands() produces from the RC link)
//  - smoothing ON: rcCommand[THROTTLE] = pt3-like filter output of the raw stick (testRcThrottleNow = raw stick)
//  - Alt Hold ON: motor thrust = autopilot throttle; OFF: motor thrust = (rcCommand - 1000) / 1000
//  - 100 Hz cycle (the Alt Hold task rate)
// ======================================================================================================
#ifdef USE_GPS_RESCUE
class AltholdClosedLoop : public AltholdCustomSim {
protected:
    float z = 0.0f, v = 0.0f;            // cm, cm/s
    float thrustFrac = 0.27f;            // motor thrust fraction (lagged)
    float hoverFrac = 0.368f;            // true hover thrust fraction of the aircraft
    float f1 = 1000, f2 = 1000, f3 = 1000;   // pt3 filter stages
    bool smoothing = true;
    float cutoffAlpha = 0.39f;           // per-stage coefficient at 100 Hz for ~20 Hz pt3
    float rawStick = 1450.0f;
    float outPwmEq = 1270.0f, prevOutPwmEq = 1270.0f;
    float maxStep = 0.0f;                // largest single-cycle change of commanded throttle (rcCommand units)
    float minZ = 1e9f, maxV = -1e9f, minV = 1e9f;
    float maxLead = 0.0f;                // max |target - z| while Alt Hold active
    float t = 0.0f;
    float releaseStep = 0.0f; bool prevOn = false; float minZRaw = 1e9f;

    void SetUp() override {
        AltholdCustomSim::SetUp();
        armingFlags |= ARMED;
        testAirmodeEnabled = false;
        gpsRescueConfigMutable()->descendRate = 135;
        altHoldConfigMutable()->climbRate = 70;
        altHoldConfigMutable()->deadband = 25;
        altHoldConfigMutable()->deadbandLow = 0;
        altHoldConfigMutable()->hoverThrottle = 1400;
        autopilotConfigMutable()->hoverThrottle = 1300;
        autopilotConfigMutable()->throttleMin = 1150;
        autopilotConfigMutable()->throttleMax = 1900;
        altHoldInit();
        testCosTiltAngle = 1.0f;
    }
    void TearDown() override { armingFlags &= ~ARMED; testAirmodeEnabled = true; testFailsafeActive = false; testRcThrottleNow = -1.0f; }

    void initState(float z0, float stick) {
        z = z0; v = 0; thrustFrac = hoverFrac; rawStick = stick;
        f1 = f2 = f3 = stick;
        rcCommand[THROTTLE] = smoothing ? f3 : stick;
        flightModeFlags = 0; altHoldInit(); altHoldClearExitPending();
        rxConfigMutable()->rc_smoothing = smoothing;
        testAltitudeCm = z; testAltitudeDerivativeCmS = v;
        t = 0; releaseStep = 0; prevOn = false; minZRaw = 1e9f; maxStep = 0; minZ = 1e9f; maxV = -1e9f; minV = 1e9f; maxLead = 0;
        outPwmEq = prevOutPwmEq = stick;
    }
    // one 10 ms cycle. sw = Alt Hold switch, stick = raw stick (rcCommand units)
    void cycle(bool sw, float stick) {
        const float dt = 0.01f;
        rawStick = stick;
        // RC link frame: modes decided while rcCommand still holds the previous (filtered) value
        testRcThrottleNow = stick;                          // newest rcData-derived throttle
        testThrottleStatus = rcCommand[THROTTLE] < 1050 ? THROTTLE_LOW : THROTTLE_HIGH;
        const bool swRequest = altHoldRequestActive(sw);              // evaluated first, like core.c's ALT_HOLD_SWITCH_REQUEST()
        if (armed && (swRequest || testFailsafeActive)) flightModeFlags |= ALT_HOLD_MODE;
        else { flightModeFlags &= ~ALT_HOLD_MODE; altHoldClearExitPending(); }
        // updateRcCommands() then the PID loop smoothing
        if (smoothing) {
            f1 += cutoffAlpha * (stick - f1); f2 += cutoffAlpha * (f1 - f2); f3 += cutoffAlpha * (f2 - f3);
            rcCommand[THROTTLE] = f3;
        } else {
            rcCommand[THROTTLE] = stick;
        }
        testThrottleStatus = rcCommand[THROTTLE] < 1050 ? THROTTLE_LOW : THROTTLE_HIGH;
        testAltitudeCm = z; testAltitudeDerivativeCmS = v;
        // the firmware's getRcCommandThrottleFromRcData() still reads the newest rcData in the Alt Hold task:
        // keep testRcThrottleNow = stick through updateAltHold() (entry latch / re-entry capture use the raw input)
        updateAltHold(currentTimeUs);
        testRcThrottleNow = -1.0f;
        const bool on = flightModeFlags & ALT_HOLD_MODE;
        outPwmEq = on ? thrPwm() : rcCommand[THROTTLE];
        outPwmEq = constrainf(outPwmEq, 1000.0f, 2000.0f);
        if (prevOn && !on) { releaseStep = outPwmEq - prevOutPwmEq; }
        prevOn = on;
        maxStep = fmaxf(maxStep, fabsf(outPwmEq - prevOutPwmEq));
        prevOutPwmEq = outPwmEq;
        // plant
        const float cmd = (outPwmEq - 1000.0f) / 1000.0f;
        thrustFrac += (cmd - thrustFrac) * (dt / 0.04f);    // motor/prop lag 40 ms
        float a = 981.0f * (thrustFrac / hoverFrac - 1.0f) - 0.25f * v;   // thrust accel minus drag
        v += a * dt; z += v * dt;
        minZRaw = fminf(minZRaw, z);
        if (z <= 0.0f) { z = 0.0f; if (v < 0) v = 0; if (a < 0) v = 0; }
        t += dt;
        minZ = fminf(minZ, z); maxV = fmaxf(maxV, v); minV = fminf(minV, v);
        if (on) maxLead = fmaxf(maxLead, fabsf(altHold.targetAltitudeCm - z));
    }
    void runFor(bool sw, float stick, float seconds) { for (int i = 0; i < (int)(seconds * 100); i++) cycle(sw, stick); }
};

TEST_F(AltholdClosedLoop, S1_HoldsAltitudeAtEntry)
{
    initState(1000.0f, 1450.0f);
    runFor(true, 1450.0f, 10.0f);
    printf("[S1] hold 10 s: z=%.0f (start 1000) minZ=%.0f vRange=[%.0f,%.0f]\n", z, minZ, minV, maxV);
    EXPECT_NEAR(z, 1000.0f, 60.0f);
}

TEST_F(AltholdClosedLoop, S2_FullLowDescentFrom12mLandsWithoutOvershoot)
{
    initState(1200.0f, 1450.0f);
    runFor(true, 1450.0f, 2.0f);                                   // entry latch hold
    float vTouch = 0; float tTouch = -1, vAt5 = 0, vAt2 = 0; bool s5 = false, s2 = false;
    for (int i = 0; i < 6000 && z > 0.5f; i++) {
        cycle(true, 1000.0f);
        if (!s5 && z <= 500.0f) { s5 = true; vAt5 = v; }
        if (!s2 && z <= 200.0f) { s2 = true; vAt2 = v; }
        if (z > 3.0f) vTouch = v;
    }
    tTouch = t;
    printf("[S2] descent 12m: t=%.1fs v(5m)=%.0f v(2m)=%.0f touchdown v=%.0f vMin=%.0f maxLead=%.0f\n", tTouch, vAt5, vAt2, vTouch, minV, maxLead);
    EXPECT_GT(z, -1.0f);
    EXPECT_LE(z, 0.5f);                                            // actually landed
    EXPECT_LT(tTouch, 30.0f);                                      // within the time limit
    EXPECT_GE(minZRaw, -15.0f);                                    // unclamped altitude: no meaningful undershoot (plant clamps z at 0)
    EXPECT_GE(minV, -760.0f);                                      // never faster than the normal cap (700) + margin
    EXPECT_LE(vAt2, 0.0f);
    EXPECT_GE(vAt2, -330.0f);
    EXPECT_GE(vTouch, -260.0f);                                    // touchdown speed (v11 was -349 in the same plant)
}

TEST_F(AltholdClosedLoop, S3_GoAroundAt4mResponds)
{
    initState(1200.0f, 1450.0f);
    runFor(true, 1450.0f, 1.0f);
    while (z > 400.0f) cycle(true, 1000.0f);                       // descending, just crossed 4 m
    const float zAbort = z, vAbort = v;
    float tUp = -1;
    for (int i = 0; i < 400; i++) {
        cycle(true, 1900.0f);                                      // abort: full stick up
        if (tUp < 0 && v > 0) tUp = i * 0.01f;
    }
    printf("[S3] go-around at 4m: z=%.0f v=%.0f -> stick up; min z=%.0f, v turns positive after %.2fs, z after 4s=%.0f\n", zAbort, vAbort, minZ, tUp, z);
    EXPECT_GE(tUp, 0.0f);
    EXPECT_LE(tUp, 2.0f);                                          // v11: never recovered in this plant
    // known limit: from -3.7 m/s at 4 m the plant still touches the ground once (see custom-patch notes), so no minZ bound
}

TEST_F(AltholdClosedLoop, S4_ExitHoldHandOver_FastStickToHover)
{
    for (int sm = 1; sm >= 0; sm--) {
        smoothing = sm;
        hoverFrac = 0.263f;                                        // real hover == ap_hover_throttle (1300)
        altHoldConfigMutable()->hoverThrottle = 0;                 // Alt Hold feed-forward = ap_hover_throttle too
        altHoldInit();
        initState(1000.0f, 1500.0f);
        runFor(true, 1500.0f, 1.0f);
        runFor(true, 1700.0f, 1.0f);                               // latch released, stick high
        runFor(false, 1700.0f, 0.3f);                              // switch off: ALT WAIT (stick far from hover)
        EXPECT_TRUE(isAltHoldExitPending());
        const float zBefore = z;
        maxStep = 0.0f;
        for (int i = 0; i < 30; i++) cycle(false, 1700.0f - (1700.0f - 1263.0f) * (i + 1) / 30.0f);   // 0.3 s flick to ap_hover_throttle
        float releasedAt = -1; (void)releasedAt;
        runFor(false, 1263.0f, 1.5f);
        printf("[S4] smoothing=%d: z before=%.0f after=%.0f minV=%.0f maxV=%.0f release step(pwm units)=%.1f mode=%d\n",
               sm, zBefore, z, minV, maxV, releaseStep, (int)modeOn());
        EXPECT_FALSE(modeOn());
        EXPECT_GT(z, 800.0f);
        EXPECT_LT(fabsf(releaseStep), 100.0f);
    }
}

TEST_F(AltholdClosedLoop, S5_SwitchOffWithStickDropSameFrame)
{
    for (int sm = 1; sm >= 0; sm--) {
      for (int atHover = 1; atHover >= 0; atHover--) {
        smoothing = sm;
        const float holdStick = atHover ? 1263.0f : 1450.0f;      // 1263 = ap_hover_throttle(1300) on the rcCommand scale
        hoverFrac = 0.263f; altHoldConfigMutable()->hoverThrottle = 0; altHoldInit();
        initState(1000.0f, holdStick);
        runFor(true, holdStick, 2.0f);
        const float zBefore = z;
        maxStep = 0; minV = 1e9f; releaseStep = 0;
        cycle(false, 1000.0f);                                     // switch off and stick to 1000 in the same frame
        const bool releasedImmediately = !modeOn();
        runFor(false, 1000.0f, 0.5f);
        printf("[S5] smoothing=%d stickAtHover=%d: releasedImmediately=%d releaseStep=%.1f pending=%d mode=%d z %.0f -> %.0f minV=%.0f\n",
               sm, atHover, (int)releasedImmediately, releaseStep, (int)isAltHoldExitPending(), (int)modeOn(), zBefore, z, minV);
        if (sm && atHover) {
            // filtered rcCommand is still at hover on the switch-off frame: immediate and continuous release, then manual descends
            EXPECT_TRUE(releasedImmediately);
            EXPECT_FALSE(isAltHoldExitPending());
            EXPECT_LT(fabsf(releaseStep), 25.0f);
            EXPECT_LT(z, zBefore - 5.0f);
        } else if (sm) {
            // filter output still above the band: ALT WAIT until the filtered value crosses hover (a few cycles), then manual
            EXPECT_FALSE(releasedImmediately);
            EXPECT_FALSE(isAltHoldExitPending());
            EXPECT_FALSE(modeOn());
            EXPECT_LT(z, zBefore - 5.0f);
        } else {
            // smoothing OFF: raw stick is already far below hover on the switch-off frame -> ALT WAIT keeps holding the altitude
            EXPECT_FALSE(releasedImmediately);
            EXPECT_TRUE(isAltHoldExitPending());
            EXPECT_TRUE(modeOn());
            EXPECT_LT(maxStep, 5.0f);
            EXPECT_NEAR(z, zBefore, 40.0f);
        }
      }
    }
}

TEST_F(AltholdClosedLoop, S6_FailsafeLandingFrom30m_SwitchOff)
{
    // core.c activates Alt Hold for failsafe landing even when the switch is off
    initState(3000.0f, 1368.0f);                                   // manual hover (thrust 0.368)
    runFor(false, 1368.0f, 1.0f);
    EXPECT_FALSE(modeOn());
    testFailsafeActive = true;
    cycle(false, 1368.0f);
    EXPECT_TRUE(modeOn());                                         // failsafe alone engages the mode
    for (int i = 0; i < 12000 && z > 0.5f; i++) cycle(false, 1368.0f);
    const float tLand = t;
    printf("[S6] failsafe landing (switch off) from 30m: t=%.1fs z=%.0f vMin=%.0f vAtEnd=%.0f minZRaw=%.0f\n", tLand, z, minV, v, minZRaw);
    EXPECT_LE(z, 0.5f);
    EXPECT_LT(tLand, 60.0f);
    EXPECT_GE(minZRaw, -15.0f);
    testFailsafeActive = false;                                    // failsafe recovered, switch still off
    cycle(false, 1368.0f);
    EXPECT_FALSE(modeOn());
    EXPECT_FALSE(isAltHoldExitPending());
}

TEST_F(AltholdClosedLoop, S6b_FailsafeEndsWhileSwitchTurnedOffDuringIt)
{
    initState(2000.0f, 1450.0f);
    runFor(true, 1450.0f, 1.0f);                                   // pilot Alt Hold
    testFailsafeActive = true;
    runFor(false, 1000.0f, 0.5f);                                  // switch off during failsafe, stick low: ALT WAIT pending, held by failsafe
    EXPECT_TRUE(modeOn());
    testFailsafeActive = false;                                    // failsafe ends: no switch, stick far from hover
    cycle(false, 1000.0f);
    printf("[S6b] failsafe ended after switch off: mode=%d pending=%d\n", (int)modeOn(), (int)isAltHoldExitPending());
    EXPECT_EQ(modeOn(), isAltHoldExitPending());                   // never "pending" without the mode (and vice versa)
}

TEST_F(AltholdClosedLoop, S7_LandingZoneHoldKeepsTargetAfterGust)
{
    initState(150.0f, 1450.0f);                                    // hold at 1.5 m (latched, stick at hover)
    runFor(true, 1450.0f, 1.0f);
    v = -100.0f;                                                   // downdraft kick
    runFor(true, 1450.0f, 4.0f);
    printf("[S7] low hold after -100 cm/s gust: z=%.0f (target %.0f) minZ=%.0f\n", z, altHold.targetAltitudeCm, minZ);
    EXPECT_NEAR(altHold.targetAltitudeCm, 150.0f, 1.0f);
    EXPECT_NEAR(z, 150.0f, 40.0f);
}

TEST_F(AltholdClosedLoop, S8_ClimbRateTenNeverExceeds100InLandingZone)
{
    altHoldConfigMutable()->climbRate = 10;
    initState(450.0f, 1450.0f);
    runFor(true, 1450.0f, 1.0f);
    minV = 1e9f;
    runFor(true, 1000.0f, 3.0f);
    printf("[S8] climb_rate=10 descend from 4.5 m: vMin=%.0f (cap 100 expected)\n", minV);
    EXPECT_GE(minV, -130.0f);
}

TEST_F(AltholdClosedLoop, S9_RandomStressInvariants)
{
    uint32_t rng = 12345;
    auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; };
    int violations = 0, switchesOn = 0, exitsPending = 0, vNan = 0, vRange = 0, vPend = 0, vLead = 0;
    for (int run = 0; run < 60; run++) {
        smoothing = rnd() < 0.7f;
        altHoldConfigMutable()->climbRate = (uint8_t)(10 + rnd() * 100);
        gpsRescueConfigMutable()->descendRate = (uint16_t)(60 + rnd() * 200);
        testAirmodeEnabled = rnd() < 0.2f;
        initState(rnd() * 1500.0f, 1000.0f + rnd() * 700.0f);
        bool sw = rnd() < 0.5f; float stick = rawStick;
        for (int i = 0; i < 3000; i++) {                          // 30 s
            if (rnd() < 0.01f) sw = !sw;
            if (rnd() < 0.03f) stick = 1000.0f + rnd() * 1000.0f;
            if (rnd() < 0.02f) testFailsafeActive = !testFailsafeActive;
            cycle(sw, stick);
            if (sw) switchesOn++;
            if (isAltHoldExitPending()) exitsPending++;
            const bool on = flightModeFlags & ALT_HOLD_MODE;
            if (!std::isfinite(z) || !std::isfinite(v) || !std::isfinite(altHold.targetAltitudeCm)) vNan++, violations++;
            if (outPwmEq < 999.0f || outPwmEq > 2001.0f) vRange++, violations++;
            if (isAltHoldExitPending() && !on) vPend++, violations++;       // pending implies mode on
            if (on && fabsf(altHold.targetAltitudeCm - z) > 3000.0f) vLead++, violations++;   // runaway target
        }
        testFailsafeActive = false;
    }
    printf("[S9] nan=%d range=%d pending-without-mode=%d lead>gate=%d\n", vNan, vRange, vPend, vLead);
    printf("[S9] random stress: 60 runs x 30 s, violations=%d (switchOn cycles=%d, ALT WAIT cycles=%d)\n", violations, switchesOn, exitsPending);
    EXPECT_EQ(violations, 0);
}
#endif
