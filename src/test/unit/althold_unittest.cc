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

    #include "sensors/acceleration.h"
    #include "sensors/gyro.h"

    PG_REGISTER(accelerometerConfig_t, accelerometerConfig, PG_ACCELEROMETER_CONFIG, 0);
    PG_REGISTER(altHoldConfig_t, altHoldConfig, PG_ALTHOLD_CONFIG, 0);
    PG_REGISTER(autopilotConfig_t, autopilotConfig, PG_AUTOPILOT, 0);
    PG_REGISTER(gyroConfig_t, gyroConfig, PG_GYRO_CONFIG, 0);
    PG_REGISTER(positionConfig_t, positionConfig, PG_POSITION, 0);
    PG_REGISTER(rcControlsConfig_t, rcControlsConfig, PG_RC_CONTROLS_CONFIG, 0);

    bool failsafeIsActive(void) { return false; }
    timeUs_t currentTimeUs = 0;
    bool isAltHoldActive();
    extern float testAltitudeCm;
    extern float testAltitudeDerivativeCmS;
    extern float testCosTiltAngle;
    extern throttleStatus_e testThrottleStatus;
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
}
