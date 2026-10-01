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

    bool failsafeIsActive(void) { return false; }
    timeUs_t currentTimeUs = 0;
    bool isAltHoldActive();
    bool isAltHoldLandingMode(void);
    extern float testAltitudeCm;
    extern float testAltitudeDerivativeCmS;
    extern float testCosTiltAngle;
    extern throttleStatus_e testThrottleStatus;
    extern bool testAirmodeEnabled;
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
    run(false, 1240, 10);              // 1240 < 1250: just outside band
    EXPECT_TRUE(modeOn());
    run(false, 1255, 1);               // inside 1300 +/- 50
    EXPECT_FALSE(modeOn());
    EXPECT_FALSE(isAltHoldExitPending());
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
    void TearDown() override { armingFlags &= ~ARMED; testAirmodeEnabled = true; }
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
