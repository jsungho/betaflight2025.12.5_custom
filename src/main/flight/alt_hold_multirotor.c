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

#include "platform.h"

#ifndef USE_WING

#include "math.h"

#ifdef USE_ALTITUDE_HOLD

#include "build/debug.h"
#include "common/maths.h"
#include "config/config.h"

#include "fc/rc.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"

#include "flight/autopilot.h"
#include "flight/failsafe.h"
#include "flight/position.h"

#include "rx/rx.h"
#include "pg/autopilot.h"
#ifdef USE_GPS_RESCUE
#include "pg/gps_rescue.h"
#endif

#include "alt_hold.h"

static const float taskIntervalSeconds = HZ_TO_INTERVAL(ALTHOLD_TASK_RATE_HZ); // i.e. 0.01 s

// custom-patch: entry stick latch. On entering Alt Hold the throttle stick is not allowed to change the
// altitude target until it has moved this far (in stick microseconds, 5% of the 1000..2000 travel) from where
// it was on entry. Only then do alt_hold_deadband / alt_hold_deadband_low / alt_hold_hover_throttle /
// alt_hold_full_low_is_max_descend apply. Fixed in code, no CLI setting.
#define ALT_HOLD_ENTRY_LATCH_RELEASE_PWM   (0.05f * (PWM_RANGE_MAX - PWM_RANGE_MIN))

// custom-patch: exit hold. When the Alt Hold switch is turned off, altitude is still held until the throttle stick
// comes within +/-5% of the hover throttle, so the hand-over to manual throttle does not change thrust.
#define ALT_HOLD_EXIT_HOVER_BAND_PWM       (0.05f * (PWM_RANGE_MAX - PWM_RANGE_MIN))

typedef struct {
    bool isActive;
    float targetAltitudeCm;
    float maxVelocity;
    float targetVelocity;
    float deadband;      // high-side (climb) deadband, as a fraction of stick travel
    float deadbandLow;   // custom-patch: low-side (descend) deadband, independent of deadband; see betaflight/betaflight#15775
    bool allowStickAdjustment;
    bool fullLowIsMaxDescend; // custom-patch: see betaflight/betaflight#15775
    bool entryLatched;   // custom-patch: throttle stick still within 5% of its position on entry
    float entryThrottle; // custom-patch: rcCommand[THROTTLE] captured on entry
    bool exitPending;    // custom-patch: switch is off, still holding altitude until stick reaches hover
    bool prevSwitchOn;   // custom-patch: previous state of the Alt Hold switch request
    float exitPrevThrottle; // custom-patch: previous throttle sample while exit hold is pending (band crossing check)
} altHoldState_t;

altHoldState_t altHold;

// custom-patch: landing assist. In Alt Hold with Airmode OFF the pilot is assumed to be landing, so the
// vertical rate cap (alt_hold_climb_rate) is replaced by gps_rescue_descend_rate based values near the ground:
//   altitude <= 5 m : 2 x gps_rescue_descend_rate
//   altitude <= 2 m : 1 x gps_rescue_descend_rate
// otherwise alt_hold_climb_rate is used unchanged. Also drives the "ALTHOLD : LANDING" OSD message.
// Each threshold pair is a hysteresis latch (turns on at the lower value, off only above the higher value)
// to avoid rapid toggling near the boundary; see betaflight/betaflight#15775
#define ALT_HOLD_LANDING_ALT_HIGH_CM    500.0f  // 5.0 m: landing assist (x2) turns on at or below this altitude
#define ALT_HOLD_LANDING_ALT_OFF_CM     550.0f  // 5.5 m: ... and turns off only above this altitude
#define ALT_HOLD_LANDING_ALT_LOW_ON_CM  180.0f  // 1.8 m: x1 descend rate turns on at or below this altitude
#define ALT_HOLD_LANDING_ALT_LOW_OFF_CM 220.0f  // 2.2 m: ... and turns off (back to x2) only above this altitude

static bool altHoldLandingLatched;     // 5.0 m on / 5.5 m off
static bool altHoldLandingNearLatched; // 1.8 m on / 2.2 m off

// custom-patch: Alt Hold + Airmode OFF + armed = landing assist; also used directly by the OSD warning.
// Updates the 5 m latch as a side effect each time it is called; see betaflight/betaflight#15775
bool isAltHoldLandingMode(void)
{
    if (!(ARMING_FLAG(ARMED) && FLIGHT_MODE(ALT_HOLD_MODE) && !isAirmodeEnabled())) {
        altHoldLandingLatched = false;
        altHoldLandingNearLatched = false;
        return false;
    }
    const float altitudeCm = getAltitudeCm();
    altHoldLandingLatched = altHoldLandingLatched ? (altitudeCm <= ALT_HOLD_LANDING_ALT_OFF_CM)
                                                   : (altitudeCm <= ALT_HOLD_LANDING_ALT_HIGH_CM);
    return altHoldLandingLatched;
}

// custom-patch: Alt Hold's vertical rate cap; replaced by gps_rescue_descend_rate based landing assist
// near the ground (failsafe auto-landing and GPS Rescue keep their own, separate rate handling).
static float altHoldMaxClimbRate(void)
{
#ifdef USE_GPS_RESCUE
    if (isAltHoldLandingMode() && !failsafeIsActive()) {
        const float descendRateCmS = (float)gpsRescueConfig()->descendRate;
        const float altitudeCm = getAltitudeCm();
        altHoldLandingNearLatched = altHoldLandingNearLatched ? (altitudeCm <= ALT_HOLD_LANDING_ALT_LOW_OFF_CM)
                                                               : (altitudeCm <= ALT_HOLD_LANDING_ALT_LOW_ON_CM);
        if (altHoldLandingNearLatched) {
            return descendRateCmS;
        }
        // isAltHoldLandingMode() above already applied the 5.0 m on / 5.5 m off hysteresis
        return descendRateCmS * 2.0f;
    }
#endif
    return altHold.maxVelocity;
}

static void altHoldReset(void)
{
    resetAltitudeControl();
    altHold.targetAltitudeCm = getAltitudeCm();
    altHold.targetVelocity = 0.0f;
}

// custom-patch: alt_hold_hover_throttle is only honoured inside this valid window; the CLI range stays 0..1700
// but any nonzero value outside 1100..1700 (e.g. 1 or 500) is treated as 0 ("inherit ap_hover_throttle")
#define ALT_HOLD_HOVER_THROTTLE_VALID_MIN 1100
#define ALT_HOLD_HOVER_THROTTLE_VALID_MAX 1700

static uint16_t altHoldValidHoverThrottle(void)
{
    const uint16_t hover = altHoldConfig()->hoverThrottle;
    return (hover >= ALT_HOLD_HOVER_THROTTLE_VALID_MIN && hover <= ALT_HOLD_HOVER_THROTTLE_VALID_MAX) ? hover : 0;
}

#define AP_HOVER_THROTTLE_DEFAULT 1275 // same as the ap_hover_throttle PG default (pg/autopilot_multirotor.c)

// custom-patch: hover values are kept in two separate variables, both filled on Alt Hold entry and zeroed on exit
//  - altHoldOverrideHoverPwm: validated alt_hold_hover_throttle (0 if unset or out of range). Only the pilot's own
//    Alt Hold / Position Hold switch may use it.
//  - altHoldCapturedHoverPwm: stock behaviour, the stick position at entry; only captured when ap_hover_throttle is 0.
static uint16_t altHoldOverrideHoverPwm;
static uint16_t altHoldCapturedHoverPwm;

static void altHoldCaptureHoverThrottle(void)
{
    altHoldOverrideHoverPwm = altHoldValidHoverThrottle();
    if (autopilotConfig()->hoverThrottle != 0) {
        altHoldCapturedHoverPwm = 0;
        return;     // no early return for the override: the stick capture below is always done when ap_hover_throttle is 0
    }
    altHoldCapturedHoverPwm = lrintf(constrainf(rcCommand[THROTTLE], autopilotConfig()->throttleMin, autopilotConfig()->throttleMax));
}

static void altHoldClearHoverThrottle(void)
{
    altHoldOverrideHoverPwm = 0;
    altHoldCapturedHoverPwm = 0;
}

// custom-patch: hover baseline for Alt Hold/Position Hold, evaluated at every use. Priority:
//  a) altHoldOverrideHoverPwm, only for the pilot's switch: not during failsafe landing (it also runs through
//     ALT_HOLD_MODE) and not in GPS_RESCUE_MODE (excluded directly, so GPS Rescue never reads it in the cycle before
//     the flight mode update drops ALT_HOLD_MODE); this tree has no AUTOPILOT_MODE flight mode
//  b) ap_hover_throttle   c) stick captured on entry   d) default
static float altHoldGetBaseHoverThrottle(void)
{
    if (autopilotConfig()->hoverThrottle != 0) {
        return autopilotConfig()->hoverThrottle;
    }
    return altHoldCapturedHoverPwm != 0 ? altHoldCapturedHoverPwm : AP_HOVER_THROTTLE_DEFAULT;
}

static float altHoldGetHoverThrottle(void)
{
    if (altHoldOverrideHoverPwm != 0 && !failsafeIsActive() && !FLIGHT_MODE(GPS_RESCUE_MODE)) {
        return altHoldOverrideHoverPwm;
    }
    return altHoldGetBaseHoverThrottle();
}

void altHoldInit(void)
{
    altHold.isActive = false;
    altHoldLandingLatched = false;
    altHoldLandingNearLatched = false;
    altHold.exitPending = false;
    altHold.prevSwitchOn = false;
    altHoldClearHoverThrottle();
    altHold.deadband = altHoldConfig()->deadband / 100.0f;
    altHold.deadbandLow = altHoldConfig()->deadbandLow / 100.0f; // custom-patch: see betaflight/betaflight#15775
    altHold.allowStickAdjustment = altHoldConfig()->deadband;
    altHold.fullLowIsMaxDescend = altHoldConfig()->fullLowIsMaxDescend; // custom-patch: see betaflight/betaflight#15775
    altHold.maxVelocity = altHoldConfig()->climbRate * 10.0f; // 50 in CLI means 500cm/s
    altHoldReset();
}

static void altHoldProcessTransitions(void) {

    if (FLIGHT_MODE(ALT_HOLD_MODE)) {
        if (!altHold.isActive) {
            altHoldReset();
            // custom-patch: hold altitude until the pilot moves the throttle stick 5% from where it is now
            altHold.entryThrottle = rcCommand[THROTTLE];
            altHold.entryLatched = true;
            altHoldCaptureHoverThrottle();   // custom-patch: see altHoldGetHoverThrottle()
            altHold.isActive = true;
        }
    } else {
        altHold.isActive = false;
        altHoldLandingLatched = false;
        altHoldLandingNearLatched = false;
        altHoldClearHoverThrottle();
    }

    // ** the transition out of alt hold (exiting altHold) may be rough.  Some notes... **
    // The original PR had a gradual transition from hold throttle to pilot control throttle
    // using !(altHoldRequested && altHold.isActive) to run an exit function
    // a cross-fade factor was sent to mixer.c based on time since the flight mode request was terminated
    // it was removed primarily to simplify this PR

    // hence in this PR's the user's throttle needs to be close to the hover throttle value on exiting altHold
    // its not so bad because the 'target adjustment' by throttle requires that
    // user throttle must be not more than half way out from hover for a stable hold
}

static void altHoldUpdateTargetAltitude(void)
{
    // User can adjust the target altitude with throttle, but only when
    // - throttle is outside deadband, and
    // - throttle is not low (zero), and
    // - deadband is not configured to zero

    float stickFactor = 0.0f;

    // custom-patch: release the entry latch once the stick has moved 5% from its position on entry;
    // until then the stick is ignored and the target altitude holds
    if (altHold.entryLatched && fabsf(rcCommand[THROTTLE] - altHold.entryThrottle) >= ALT_HOLD_ENTRY_LATCH_RELEASE_PWM) {
        altHold.entryLatched = false;
    }

    if (altHold.allowStickAdjustment && !altHold.entryLatched && !altHold.exitPending) {
        if (calculateThrottleStatus() != THROTTLE_LOW) {
            const float rcThrottle = rcCommand[THROTTLE];
            // custom-patch: low (descend) and high (climb) thresholds are now independently configurable
            // via alt_hold_deadband (high) and alt_hold_deadband_low (low), and are centered on
            // altHoldGetHoverThrottle() (valid alt_hold_hover_throttle if set, else ap_hover_throttle); see betaflight/betaflight#15775
            const float hoverThrottle = altHoldGetHoverThrottle();
            const float lowThreshold = hoverThrottle - altHold.deadbandLow * (hoverThrottle - PWM_RANGE_MIN);
            const float highThreshold = hoverThrottle + altHold.deadband * (PWM_RANGE_MAX - hoverThrottle);

            if (rcThrottle < lowThreshold) {
                stickFactor = scaleRangef(rcThrottle, PWM_RANGE_MIN, lowThreshold, -1.0f, 0.0f);
            } else if (rcThrottle > highThreshold) {
                stickFactor = scaleRangef(rcThrottle, highThreshold, PWM_RANGE_MAX, 0.0f, 1.0f);
            }
        } else if (altHold.fullLowIsMaxDescend) {
            // custom-patch: opt-in - throttle below min_check (THROTTLE_LOW) commands max descend
            // instead of the stock forced-hover behavior; see betaflight/betaflight#15775
            stickFactor = -1.0f;
        }
    }

    // if failsafe is active, and we get here, we are in failsafe landing mode, it controls throttle
    if (failsafeIsActive()) {
        // descend at up to 10 times faster when high
        // default landing timeout is now 60s; must to get the quad down within this limit
        // need a rapid descent when initiated high, and must slow down closer to ground
        // this code doubles descent rate at 20m, to max 10x (10m/s on defaults) at 200m
        // the deceleration may be a bit rocky if it starts very high up
        // constant (set) deceleration target in the last 2m
        stickFactor = -(0.9f + constrainf(getAltitudeCm() / 2000.0f, 0.1f, 9.0f));
    }
    // custom-patch: landing assist replaces the climb-rate cap near the ground when Alt Hold + Airmode OFF
    const float maxVelocity = altHoldMaxClimbRate();
    altHold.targetVelocity = stickFactor * maxVelocity;

    // custom-patch (v12): landing assist can lower maxVelocity below the lead the target already has over the
    // current altitude (e.g. 700 -> 270 cm at 5 m). The 1 s gate below would then freeze the target (stick ignored)
    // until the quad closes the gap. Pull the target back inside the new gate so the stick keeps working.
    // Not applied to the normal cap (altHold.maxVelocity) or failsafe, which keep the stock behaviour.
    if (maxVelocity < altHold.maxVelocity) {
        const float limitCm = maxVelocity * 1.0f /* s */ * 0.9f;  // 0.9: stay strictly inside the "<" gate below
        const float altitudeCm = getAltitudeCm();
        altHold.targetAltitudeCm = constrainf(altHold.targetAltitudeCm, altitudeCm - limitCm, altitudeCm + limitCm);
    }

    // prevent stick input from moving target altitude too far away from current altitude
    // otherwise it can be difficult to bring target altitude close to current altitude in a reasonable time
    // using maxVelocity means the stick can bring altitude target to current within 1s
    // this constrains the P and I response to user target changes, but not D of F responses
    // Range is compared to distance that might be traveled in one second
    if (fabsf(getAltitudeCm() - altHold.targetAltitudeCm) < maxVelocity * 1.0f /* s */) {
        altHold.targetAltitudeCm += altHold.targetVelocity * taskIntervalSeconds;
    }
}

static void altHoldUpdate(void)
{
    // check if the user has changed the target altitude using sticks
    if (altHoldConfig()->climbRate) {
        altHoldUpdateTargetAltitude();
    }
    // custom-patch: pass Alt Hold/Position Hold's own hoverThrottle instead of the GPS-Rescue-shared value; see betaflight/betaflight#15775
    altitudeControl(altHold.targetAltitudeCm, taskIntervalSeconds, altHold.targetVelocity, altHoldGetHoverThrottle());
}

void updateAltHold(timeUs_t currentTimeUs) {
    UNUSED(currentTimeUs);

    // check for enabling Alt Hold, otherwise do as little as possible while inactive
    altHoldProcessTransitions();

    if (altHold.isActive) {
        altHoldUpdate();
    }
}

bool isAltHoldActive(void) {
    return altHold.isActive;
}

bool isAltHoldExitPending(void)
{
    return altHold.exitPending;
}

void altHoldClearExitPending(void)
{
    altHold.exitPending = false;
    altHold.prevSwitchOn = false;
}

// custom-patch: called every cycle from the flight mode update instead of IS_RC_MODE_ACTIVE(BOXALTHOLD)
bool altHoldRequestActive(bool switchOn)
{
    const bool wasSwitchOn = altHold.prevSwitchOn;
    altHold.prevSwitchOn = switchOn;

    if (switchOn) {
        if (altHold.exitPending) {
            // switched back on while waiting: behave like a fresh entry, latch the stick where it is now
            altHold.exitPending = false;
            altHold.entryThrottle = rcCommand[THROTTLE];
            altHold.entryLatched = true;
        }
        return true;
    }

    if (wasSwitchOn && altHold.isActive) {
        // switch just turned off while Alt Hold was active: keep holding unless the stick is already at hover
        altHold.exitPending = true;
        altHold.exitPrevThrottle = rcCommand[THROTTLE];
    }

    if (altHold.exitPending) {
        // custom-patch: reference is ap_hover_throttle, not alt_hold_hover_throttle/thr_mid; if it is 0 fall back to
        // the stick captured on entry, then the valid alt_hold_hover_throttle, then the default
        const uint16_t apHover = autopilotConfig()->hoverThrottle;
        const float hoverPwm = apHover != 0 ? (float)apHover
            : altHoldCapturedHoverPwm != 0 ? (float)altHoldCapturedHoverPwm
            : altHoldOverrideHoverPwm != 0 ? (float)altHoldOverrideHoverPwm : (float)AP_HOVER_THROTTLE_DEFAULT;
        const float prevDelta = altHold.exitPrevThrottle - hoverPwm;
        const float delta = rcCommand[THROTTLE] - hoverPwm;
        altHold.exitPrevThrottle = rcCommand[THROTTLE];
        // custom-patch: release when the stick is inside the band, or has crossed hover since the last sample
        // (a fast stick flick can jump clean over the +/-5% band between two samples, skipping it entirely)
        if (fabsf(delta) <= ALT_HOLD_EXIT_HOVER_BAND_PWM || (prevDelta < 0.0f) != (delta < 0.0f)) {
            altHold.exitPending = false;   // stick at/through hover: release to manual throttle
        }
    }
    return altHold.exitPending;
}
#endif

#endif // !USE_WING
