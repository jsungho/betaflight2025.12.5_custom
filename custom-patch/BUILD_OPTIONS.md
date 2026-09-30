# Betaflight 2025.12.5 커스텀 빌드 옵션 (기체별, 보드별 빌드)

베이스: `jsungho/betaflight2025.12.5_custom` / 브랜치 `custom-patch/alt-hold-throttle-range`
커스텀 패치(4종): `alt_hold_full_low_is_max_descend`, `alt_hold_deadband_low`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only` (참고: betaflight/betaflight#15775)

이 저장소의 펌웨어는 **보드별(`make <보드이름>`)** 로 빌드해서, 그 보드/기체가 실제로 쓰지 않는 기능을 빼 플래시 사용량을 줄였다(이전에 있던 통합 타겟(MCU 단위) hex는 제거되었다).
결과 파일은 기체 이름이 들어간 hex(`..._custom_v7_slim.hex`)이며, **각 기체에 맞는 파일 하나만** 올려야 한다.

**v4: 서보(USE_SERVOS)와 배터리-컨티뉴(USE_BATTERY_CONTINUE)를 전 기체에서 제거했고, OSD는 디지털(MSP DisplayPort 등, `USE_OSD_HD`)만 남기고 아날로그 OSD(`USE_OSD_SD`)와 MAX7456 드라이버(`USE_MAX7456`)를 제거했다.** 사용자 지시(2026-09): 이 저장소의 기체는 전부 디지털 VTX(Walksnail 등)만 쓰고 서보/아날로그 OSD를 쓰지 않음.

**v5 (현재): Alt Hold 진입 스틱 래치(Entry Stick Latch)를 추가했다.** `jsungho/betaflight2026.6.x_custom`(2026.6.2 저장소) 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `665f62a9e`의 `src/main/flight/alt_hold_multirotor.c` 변경을 2025.12.5 코드 구조에 맞게 이식했다. CLI 항목 없음(코드 고정), PG 버전 변경 없음. 아래 "v5: Alt Hold 진입 스틱 래치" 절 참고.

## 기체별 빌드옵션 표

기체 CLI(2025.12.5 diff all)에서 실제로 쓰는 기능만 남기고, 쓰지 않는 기능은 빌드에서 뺐다.

| 기체 | 보드 (빌드 타깃) | MCU | 자력계 | PINIO | LED 스트립 | 수신기 프로토콜 | 텔레메트리 | F722 추가로 켠 옵션 | Flash |
|---|---|---|---|---|---|---|---|---|---|
| MARIO5 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 (사용) | 제거 | **CRSF** | CRSF | 해당 없음 | 39.37% |
| AOS_UL7_O4 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 (사용) | 제거 | **FPort** | SmartPort (FPort 텔레메트리) | 해당 없음 | 39.16% |
| Mark4_6in | JHEF405PRO | F405 | 포함 | **제거** | 제거 | **SBUS** | 없음 | 해당 없음 | 39.69% |
| TJRC_10 | MATEKF722SE | F722 | 포함 | 포함 (사용) | 포함 | CRSF | CRSF | ALT / GPS / POS | 78.90% |
| 8IN-KOPIS_X8 | SPEEDYBEEF7V3 | F722 | 포함 | 포함 (사용) | 제거 (1) | CRSF | CRSF | ALT / GPS / POS | 76.37% |
| CHIMERA7 | FLYWOOF722PROV2 | F722 | 포함 | 포함 (사용) | 제거 | CRSF | CRSF | ALT / GPS / POS | 74.08% |
| AOS_UL7_X8 | MATEKF722HD | F722 | 포함 | **제거** | 포함 | CRSF | CRSF | ALT / GPS / POS | 76.67% |
| Explorer LR4 | JHEF7DUAL | F722 | 포함 | **제거** | 포함 | CRSF | CRSF | ALT / GPS / POS | 76.73% |
| Pavo25 V2 | JHEF7DUAL | F722 | **제거** (센서 없음) | 포함 (사용) | 제거 (1) | **CRSF** | CRSF | ALT / GPS / POS | 73.53% |
| X8_5INCH | MATEKH743 | H743 | 포함 | **제거** | 포함 | **FPort** | SmartPort (FPort 텔레메트리) | 해당 없음 | 24.35% |

- 자력계 = `USE_MAG` (드라이버 자동 포함). 보드 config 빌드는 이 옵션을 자동으로 켜지 않으므로 `build_custom.sh`가 Pavo25 V2를 제외한 모든 기체에 `-DUSE_MAG`를 명시한다.
- ALT = `USE_ALTITUDE_HOLD`, GPS = `USE_GPS`, POS = `USE_POSITION_HOLD`
- **모든 기체에서 Alt Hold / Position Hold / GPS·GPS Rescue, OSD(MSP DisplayPort, 디지털 전용), 블랙박스, 커스텀 파라미터 4종은 포함된다.**
- **Pavo25 V2만 자력계(USE_MAG) 제외** — 사용자 확인: 이 기체는 자력계 센서 자체가 없음.
- **전 기체 공통으로 서보(USE_SERVOS)와 배터리-컨티뉴(USE_BATTERY_CONTINUE)는 제거, OSD는 디지털(USE_OSD_HD)만 유지, 아날로그 OSD(USE_OSD_SD)/MAX7456은 제거** (v4, 아래 "v4: 서보/배터리-컨티뉴/아날로그 OSD 제거" 절 참고).

주석
1. 8IN-KOPIS_X8, Pavo25 V2는 CLI에 `feature LED_STRIP`이 켜져 있지만 `resource LED_STRIP 1 NONE`으로 핀이 비어 있고 LED 정의도 없어 LED 스트립을 뺐다.
2. **수신기 프로토콜은 사용자 확인 사항이다.** MARIO5 = CRSF, AOS_UL7_O4 = FPort, X8_5INCH = FPort로 가정하고 빌드했다. 이 3개 기체는 CLI `diff all`에 `serialrx_provider`가 없어(기본값 의존) CLI만으로는 실제 프로토콜을 확정할 수 없다. 나머지 7개 기체(Mark4_6in=SBUS, TJRC_10/8IN-KOPIS_X8/CHIMERA7/AOS_UL7_X8/Explorer LR4/Pavo25 V2=CRSF)는 CLI에 `serialrx_provider`가 없어도 해당 UART가 `RX_SERIAL`(64)로 명시 지정되어 있어 프로토콜 오판 위험이 낮다. 추정이 틀리면 수신기가 바인드되지 않으므로 **플래시 후 벤치에서 즉시 확인**한다.
3. Pavo25 V2의 CLI 덤프는 Betaflight **4.5.5** 기준(다른 9개는 2025.12.5)으로 상대적으로 오래됐다. 자력계 없음은 사용자가 직접 확인했지만, 플래시 전에 최신 `diff all`로 한 번 더 대조하는 것을 권장한다.

## F722에서 추가 옵션이 필요한 이유

2025.12.5 업스트림은 `common_pre.h`의 `TARGET_FLASH_SIZE >= 1024` 조건 안에서만 `USE_ALTITUDE_HOLD` / `USE_POSITION_HOLD` / `USE_GPS` / `USE_LED_STRIP`를 켠다.

`TARGET_FLASH_SIZE`는 `Makefile`에서 `MCU_FLASH_SIZE`(`STM32F7.mk`가 `STM32F722xx` → `512`로 고정)를 그대로 상속하며, 이 값은 `USE_CONFIG`(보드별/통합 여부)와 무관하다 — `TARGET_FLASH_SIZE >= 1024` 게이트 자체가 `USE_CONFIG` 조건절 밖에 있다. 직접 확인한 결과:

```
make TARGET=STM32F7X2 fwo -n   → -DTARGET_FLASH_SIZE=512
make CONFIG=MATEKF722SE fwo -n → -DTARGET_FLASH_SIZE=512   (동일)
```

즉 **F722(512KB)는 이 조건을 충족하지 못해 Alt Hold/Position Hold/GPS/LED 스트립이 기본적으로 빠진다.** 그래서 이 저장소의 F722 기체 빌드는 `-DUSE_ALTITUDE_HOLD -DUSE_GPS -DUSE_POSITION_HOLD`를 명시해서 F722 기체에 Alt Hold/Position Hold를 켰다. 실제로 검증한 결과 F722 기체 전부 71~76% 사용(hex 실측)으로, 다른 미사용 기능을 뺀 여유분 안에 들어간다.

F405(1MB)/H743(2MB)는 `MCU_FLASH_SIZE`가 1024/2048로 조건을 항상 충족하므로 별도 플래그 없이도 포함된다.

## v4: 서보/배터리-컨티뉴/아날로그 OSD 제거

사용자 지시로 전 기체(10개)에서 아래 3가지를 뺐다. 전 기체 CLI `diff all`을 재확인한 결과 서보 믹서 출력이나 `battery_continue` 설정을 쓰는 기체가 없어 제거해도 안전하다.

| 옵션 | 방법 | 비고 |
|---|---|---|
| 서보(`USE_SERVOS`) | `common_post.h`에 `CUSTOM_NO_SERVOS` → `#undef USE_SERVOS` 블록 추가, `build_custom.sh` COMMON에 `-DCUSTOM_NO_SERVOS` | 서보 믹서(테일서보, 비행기 등) 미사용 기체 전용 정리 |
| 배터리-컨티뉴(`USE_BATTERY_CONTINUE`) | `common_post.h`에 `CUSTOM_NO_BATTERY_CONTINUE` → `#undef USE_BATTERY_CONTINUE` 블록 추가, `build_custom.sh` COMMON에 `-DCUSTOM_NO_BATTERY_CONTINUE` | 브라운아웃 중 아밍 유지 기능, 미사용 |
| 아날로그 OSD(`USE_OSD_SD`)/MAX7456(`USE_MAX7456`) | `build_custom.sh` COMMON에 `-DUSE_OSD_HD` 추가 | `common_pre.h`가 "`USE_OSD_SD`/`USE_OSD_HD` 둘 다 안 정해지면 둘 다 켠다" 구조라, `USE_OSD_HD`를 먼저 정의(predefine)하면 `USE_OSD_SD`가 정의되지 않고, 이어서 `common_pre.h` 뒷부분의 "`USE_OSD_SD`가 없으면 `USE_MAX7456`도 undef" 규칙이 자동으로 적용되어 별도 undef 없이 아날로그 OSD 칩 드라이버까지 함께 빠진다. 디지털 VTX(MSP DisplayPort, Walksnail 등)만 쓰는 이 저장소의 전 기체에 적용 가능 |

검증: 전 기체 재빌드(hex 10개, `_v4_slim` 접미사) 성공, 플래시 오버플로 없음. `#pragma message` 진단 삽입(임시, 빌드 후 원복)으로 한 보드에서 `USE_SERVOS`/`USE_BATTERY_CONTINUE`/`USE_OSD_SD`/`USE_MAX7456`이 모두 빠지고 `USE_OSD_HD`만 남는 것을 프리프로세서 레벨에서 직접 확인. 4개 커스텀 CLI 파라미터, Alt Hold/Position Hold, 자력계(Pavo25 V2 제외)는 v3와 동일하게 전부 유지됨을 hex 문자열 검색으로 재확인.

## v5: Alt Hold 진입 스틱 래치 (Entry Stick Latch)

**배경**: Alt Hold/Position Hold 진입 순간 스로틀 스틱이 호버 위치에서 살짝 벗어나 있으면, 기존 로직은 그 스틱 위치를 곧바로 `alt_hold_deadband`/`alt_hold_deadband_low` 기준과 비교해 즉시 상승·하강 커맨드로 해석할 수 있다. 즉 모드 진입 직후 스틱이 정확히 중앙(호버)에 있지 않으면 의도치 않은 고도 변화가 시작될 수 있다.

**원본**: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `665f62a9e`("Alt Hold: hold altitude on entry until throttle stick moves 5%")의 `src/main/flight/alt_hold_multirotor.c` 변경.

**동작**:
- Alt Hold 진입 순간(`altHoldProcessTransitions()`의 비활성→활성 전환 시점)의 `rcCommand[THROTTLE]`을 `entryThrottle`로 저장하고 `entryLatched = true`로 래치를 건다.
- 래치가 걸려 있는 동안은 스틱 위치와 무관하게 고도를 그대로 유지한다(`stickFactor = 0`). `alt_hold_full_low_is_max_descend`도 이 구간에서는 무시된다.
- 스틱이 진입 시점 값에서 PWM 기준 5%(1000~2000 범위의 5% = 50) 이상 움직이면 래치가 풀리고, 그 이후부터는 기존 커스텀 로직(`alt_hold_hover_throttle`, `alt_hold_deadband`, `alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`)이 호버 스로틀 대비 스틱의 절대 위치 기준으로 그대로 적용된다.
- 한 번 풀린 래치는 Alt Hold를 껐다가 다시 켜기 전까지 재적용되지 않는다.
- 페일세이프/GPS Rescue 하강 오버라이드는 래치 상태와 무관하게 그대로 최우선 적용된다(`failsafeIsActive()` 분기가 래치 로직 이후 무조건 `stickFactor`를 덮어씀).
- 5%는 코드에 고정(`ALT_HOLD_ENTRY_LATCH_RELEASE_PWM`)했다 — CLI 항목 없음, `PG_ALTHOLD_CONFIG` 버전 변경 없음. Position Hold의 좌우/전후 스틱 로직은 영향받지 않는다.

**이식 시 구조 차이**: 2025.12.5는 2026.6.2와 `alt_hold_multirotor.c`의 함수/변수 구조가 다르다(예: 2026.6.2 전용 `autopilotCaptureHoverThrottleForAltHold()` 없음). 동일한 `altHoldState_t` 구조체에 `entryLatched`/`entryThrottle` 필드를 추가하고, 이 저장소의 `altHoldProcessTransitions()` / `altHoldUpdateTargetAltitude()`에 같은 동작을 재현하는 방식으로 이식했다.

```c
// alt_hold_multirotor.c
#define ALT_HOLD_ENTRY_LATCH_RELEASE_PWM  (0.05f * (PWM_RANGE_MAX - PWM_RANGE_MIN))

// altHoldProcessTransitions(): 비활성->활성 전환 시
altHold.entryThrottle = rcCommand[THROTTLE];
altHold.entryLatched = true;

// altHoldUpdateTargetAltitude(): 매 틱
if (altHold.entryLatched &&
    fabsf(rcCommand[THROTTLE] - altHold.entryThrottle) >= ALT_HOLD_ENTRY_LATCH_RELEASE_PWM) {
    altHold.entryLatched = false;
}
if (altHold.allowStickAdjustment && !altHold.entryLatched) {
    // 기존 stickFactor 계산 (데드밴드/최대하강 포함)
}
```

검증: 전 기체 재빌드(hex 10개, `_v5_slim` 접미사) 성공, 컴파일 경고/오류 없음, 플래시 오버플로 없음(24.33~78.80% 사용). 4개 커스텀 CLI 파라미터 및 자력계 관련 문자열이 hex 바이너리에 그대로 포함됨을 재확인. `#pragma message` 진단 삽입(임시, 검증 후 원복)으로 `ALT_HOLD_ENTRY_LATCH_RELEASE_PWM`이 전처리기 단계에서 실제로 정의됨을 확인.

## v6: Alt Hold 해제 대기 (Exit Hold) + OSD "ALT WAIT" 표시

**원본**: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `b83985b`("Alt Hold: keep holding after switch-off until throttle reaches hover +/-5%, OSD ALT WAIT")와 커밋 `e4593eb`("Alt Hold exit hold: use ap_hover_throttle as reference")를 2025.12.5 코드 구조에 맞게 이식. 진입 스틱 래치 커밋(`665f62a`)은 이번 이식 대상이 아니며 v5에서 이미 반영되어 있다.

**배경**: 기존에는 Alt Hold 스위치를 끄는 순간 즉시 모드가 해제되어 스틱이 호버 위치와 다르면 스로틀이 갑자기 바뀔 수 있었다.

**동작**:
- Alt Hold 스위치를 끈 순간에 Alt Hold가 활성 상태였다면 즉시 해제하지 않고 `exitPending` 상태로 전환해 고도를 계속 유지한다. 대기 중에는 스틱에 의한 고도 조절을 완전히 무시한다(`altHoldUpdateTargetAltitude()`의 `stickFactor` 계산 분기에 `!altHold.exitPending` 조건 추가).
- 대기 중 스로틀 스틱이 `ap_hover_throttle`(=`autopilotConfig()->hoverThrottle`) ±5%(PWM 50, 1000~2000 범위) 안에 들어오면 그 순간 Alt Hold를 완전히 해제한다. 기준은 `ap_hover_throttle`만 쓰며, `alt_hold_hover_throttle`이나 `thr_mid`는 기준이 아니다. `ap_hover_throttle`이 0이면 Alt Hold의 실효 호버 값(`altHold.hoverThrottle` — `alt_hold_hover_throttle`이 설정돼 있으면 그 값, 아니면 `ap_hover_throttle` 상속분과 동일한 값)으로 대체한다.
- 스위치를 끈 순간 스틱이 이미 그 구간 안에 있으면 바로 해제된다(대기 없음).
- 대기 중 스위치를 다시 켜면 대기를 취소하고 일반 Alt Hold로 복귀한다 — 이때 진입 스틱 래치(v5)가 새로 걸린다(현재 스틱 위치를 다시 `entryThrottle`로 저장).
- 페일세이프/GPS Rescue 하강 오버라이드는 대기 상태와 무관하게 그대로 최우선 적용된다(기존 `failsafeIsActive()` 분기는 변경되지 않음).
- `fc/core.c`의 Alt Hold 모드 판정에서 `IS_RC_MODE_ACTIVE(BOXALTHOLD)` 대신 `altHoldRequestActive(IS_RC_MODE_ACTIVE(BOXALTHOLD))`를 호출한다(매크로 `ALT_HOLD_SWITCH_REQUEST()`로 감싸 `USE_WING` 빌드에서는 기존 `IS_RC_MODE_ACTIVE(BOXALTHOLD)`를 그대로 씀). 모드가 꺼지는 `else` 분기(디스암, GPS Rescue 전환 등)에서는 `altHoldClearExitPending()`으로 대기를 지운다.
- OSD 비행모드 요소(`osdElementFlymode`)에서 대기 중이면 `"ALT WAIT"`를 경고색(`DISPLAYPORT_SEVERITY_WARNING`)으로 표시한다. 우선순위는 `!FS!` → `RESC` → `HEAD` → `PASS` → **`ALT WAIT`** → `POSH` → `ALTH` 순서(FAILSAFE/RESCUE/HEADFREE/PASSTHRU 다음, POSH보다 앞).
- 5%는 코드에 고정(`ALT_HOLD_EXIT_HOVER_BAND_PWM`)했다 — CLI 항목 없음, PG 버전 변경 없음. Position Hold는 변경하지 않았다.

**이식 시 구조 차이**: 2026.6.2는 `autopilotGetEffectiveHoverThrottlePwm()`(2026.6.2 전용 함수)을 폴백으로 썼지만, 2025.12.5에는 이 함수가 없다. 대신 이 저장소가 v2에서 이미 계산해 쓰고 있는 `altHold.hoverThrottle`(`altHoldInit()`에서 `alt_hold_hover_throttle ? alt_hold_hover_throttle : ap_hover_throttle`로 결정됨)을 동일한 역할의 폴백으로 사용했다 — `ap_hover_throttle`이 0일 때만 쓰이므로 사용자가 지정한 "Alt Hold의 실효 호버 값" 요구사항과 일치한다. `fc/core.c`는 2026.6.2의 `processRxModes()`에 있는 `AUTOPILOT_MODE`/`flightPlanNavIsRescueDescentActive()` 조건이 2025.12.5에는 없어(해당 기능 자체가 없음) 그 부분은 제외하고 `IS_RC_MODE_ACTIVE(BOXALTHOLD)` → `ALT_HOLD_SWITCH_REQUEST()` 치환만 반영했다.

```c
// alt_hold_multirotor.c
#define ALT_HOLD_EXIT_HOVER_BAND_PWM  (0.05f * (PWM_RANGE_MAX - PWM_RANGE_MIN))

bool altHoldRequestActive(bool switchOn)
{
    const bool wasSwitchOn = altHold.prevSwitchOn;
    altHold.prevSwitchOn = switchOn;
    if (switchOn) {
        if (altHold.exitPending) {
            altHold.exitPending = false;
            altHold.entryThrottle = rcCommand[THROTTLE];
            altHold.entryLatched = true;
        }
        return true;
    }
    if (wasSwitchOn && altHold.isActive) {
        altHold.exitPending = true;
    }
    if (altHold.exitPending) {
        const uint16_t apHover = autopilotConfig()->hoverThrottle;
        const float hoverPwm = apHover != 0 ? (float)apHover : altHold.hoverThrottle;
        if (fabsf(rcCommand[THROTTLE] - hoverPwm) <= ALT_HOLD_EXIT_HOVER_BAND_PWM) {
            altHold.exitPending = false;
        }
    }
    return altHold.exitPending;
}
```

```c
// fc/core.c
#ifndef USE_WING
#define ALT_HOLD_SWITCH_REQUEST() altHoldRequestActive(IS_RC_MODE_ACTIVE(BOXALTHOLD))
#else
#define ALT_HOLD_SWITCH_REQUEST() IS_RC_MODE_ACTIVE(BOXALTHOLD)
#endif
    if (ARMING_FLAG(ARMED) && !FLIGHT_MODE(GPS_RESCUE_MODE)
        && (ALT_HOLD_SWITCH_REQUEST() || failsafeIsActive())
        && sensors(SENSOR_ACC) && isAltitudeAvailable() && wasThrottleRaised()) {
        ENABLE_FLIGHT_MODE(ALT_HOLD_MODE);
    } else {
        DISABLE_FLIGHT_MODE(ALT_HOLD_MODE);
#ifndef USE_WING
        altHoldClearExitPending();
#endif
    }
```

```c
// osd/osd_elements.c (osdElementFlymode)
#if defined(USE_ALTITUDE_HOLD) && !defined(USE_WING)
    } else if (isAltHoldExitPending()) {
        strcpy(element->buff, "ALT WAIT");
        element->attr = DISPLAYPORT_SEVERITY_WARNING;
#endif
    } else if (FLIGHT_MODE(POS_HOLD_MODE)) {
```

검증: 전 기체 재빌드(hex 10개, `_v6_slim` 접미사) 성공, 컴파일 경고/오류 없음, 플래시 오버플로 없음(24.35~78.90% 사용). 4개 커스텀 CLI 파라미터, 자력계, `"ALT WAIT"` 문자열이 hex 바이너리에 그대로 포함됨을 확인. `#pragma message` 진단(임시, 검증 후 원복)으로 `ALT_HOLD_EXIT_HOVER_BAND_PWM`이 전처리기 단계에서 실제로 정의됨을 확인.

## v7: 해제 대기 — 호버 구간 건너뜀 결함 수정 + 시뮬레이션 테스트

**원본**: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `415e8c4`("Alt Hold exit hold: release when stick crosses hover (fast flick); sim tests")를 2025.12.5 코드 구조에 맞게 이식.

**문제**: v6의 해제 대기는 매 사이클(100Hz, 10ms 간격) `|스틱 - ap_hover_throttle| <= 50`인지만 검사했다. 조종자가 스틱을 매우 빠르게 움직이면, 한 사이클과 다음 사이클 사이에 스틱 값이 이 ±5% 구간을 통째로 건너뛸 수 있다(예: 한 틱엔 구간보다 한참 위, 다음 틱엔 구간보다 한참 아래) — 이 경우 구간 안에 "머무른" 샘플이 한 번도 없어 영원히 해제되지 않는 결함이 있었다.

**수정**: 해제 대기가 시작되는 순간과 매 사이클마다 직전 스로틀 값(`exitPrevThrottle`)을 저장해두고, `(스틱 - ap_hover_throttle)`의 부호가 직전 샘플과 이번 샘플 사이에서 바뀌었으면(=호버 값을 가로질렀으면) 구간 안에 정확히 들어오지 않았어도 해제한다. 위쪽에서 아래로, 아래쪽에서 위로 양방향 모두 적용.

```c
// alt_hold_multirotor.c
typedef struct {
    ...
    bool exitPending;
    bool prevSwitchOn;
    float exitPrevThrottle;   // v7: 직전 스로틀 샘플(호버 가로지름 판정용)
} altHoldState_t;

bool altHoldRequestActive(bool switchOn)
{
    ...
    if (wasSwitchOn && altHold.isActive) {
        altHold.exitPending = true;
        altHold.exitPrevThrottle = rcCommand[THROTTLE];   // 대기 시작 시점 값으로 시드
    }
    if (altHold.exitPending) {
        const uint16_t apHover = autopilotConfig()->hoverThrottle;
        const float hoverPwm = apHover != 0 ? (float)apHover : altHold.hoverThrottle;
        const float prevDelta = altHold.exitPrevThrottle - hoverPwm;
        const float delta = rcCommand[THROTTLE] - hoverPwm;
        altHold.exitPrevThrottle = rcCommand[THROTTLE];
        // 구간 안이거나, 직전 샘플 대비 호버 값을 가로질렀으면 해제
        if (fabsf(delta) <= ALT_HOLD_EXIT_HOVER_BAND_PWM || (prevDelta < 0.0f) != (delta < 0.0f)) {
            altHold.exitPending = false;
        }
    }
    return altHold.exitPending;
}
```

`fc/core.c`, `osd/osd_elements.c`, `alt_hold_multirotor.h`는 v6에서 이미 반영된 구조 그대로 변경 없음 — v7은 `alt_hold_multirotor.c` 한 파일의 해제 판정 로직만 손댔다.

**시뮬레이션 테스트**: 2026.6.2 저장소의 `src/test/unit/althold_unittest.cc`에 있는 `AltholdCustomSim` 테스트 스위트(해제 대기 관련 부분만)를 이 저장소의 althold 유닛테스트에 이식했다. 2025.12.5는 `autopilotGetEffectiveHoverThrottlePwm()` 같은 2026.6.2 전용 API가 없어 `autopilotConfigMutable()`/`altHoldConfigMutable()`/`rxConfigMutable()->mincheck` 등 이 저장소의 실제 `pg/autopilot.h`, `pg/alt_hold.h` 구조에 맞춰 스텁을 다시 맞췄다(`getAltitudeCm`/`getAltitudeDerivative`/`getCosTiltAngle`/`calculateThrottleStatus`를 테스트가 제어할 수 있는 외부 변수 기반으로 변경). 호스트(x86) 빌드라 실제 타깃 빌드와 무관하게 `cd src/test && make test_althold_unittest`로 바로 실행 가능하다.

검증 시나리오(13개 테스트, 전부 통과): 진입 스틱 래치가 고도를 유지/해제하는 경우(3개, v5), 해제 대기가 호버 구간 밖에서는 고도를 유지하다가 `ap_hover_throttle` ±5% 진입 시 해제되는 경우, 기준이 `alt_hold_hover_throttle`이 아니라 `ap_hover_throttle`임을 구분하는 경우, 스위치를 끈 순간 스틱이 이미 구간 안이면 즉시 해제되는 경우, 빠른 스틱 이동으로 구간을 위→아래/아래→위로 건너뛰어도 해제되는 경우(v7 수정 대상), 대기 중 스위치 재투입 시 재래치되는 경우, 디스암 시 대기가 지워지는 경우, Alt Hold에 진입한 적이 없는 상태에서 스위치를 끄면 대기가 시작되지 않는 경우.

검증(빌드): 전 기체 재빌드(hex 10개, `_v7_slim` 접미사) 성공, 컴파일 경고/오류 없음, 플래시 오버플로 없음(24.35~78.90% 사용, v6 대비 사실상 동일 — 필드 1개와 분기 하나 추가라 증가분 미미). 4개 커스텀 CLI 파라미터, 자력계, `"ALT WAIT"` 문자열이 hex 바이너리에 그대로 포함됨을 확인. `#pragma message` 진단(임시, 검증 후 원복)으로 `ALT_HOLD_EXIT_HOVER_BAND_PWM`이 v7 코드에서도 계속 정의됨을 확인.

## 빌드 방법

```bash
# 사전 준비
git submodule update --init --depth 1 src/config

# 전체 10개 기체
custom-patch/build_custom.sh

# 특정 보드 또는 기체 라벨만
custom-patch/build_custom.sh MATEKF722SE JHEF7DUAL
```

결과는 `custom-patch/firmware/2025.12.5/`에 `betaflight_2025.12.5_<MCU>_<보드>_<기체>_custom_v7_slim.hex` 형식으로 생성된다.

## 검증한 내용

- 전 기체 빌드/링크 성공 (플래시 오버플로 없음, v7 기준 24.35~78.90% 사용)
- 서보/배터리-컨티뉴/아날로그 OSD(MAX7456) 제거 후에도 4개 커스텀 CLI 파라미터, Alt Hold/Position Hold, 자력계(Pavo25 V2 제외)가 hex 문자열 검색으로 전부 유지됨을 확인
- 4개 커스텀 CLI 파라미터(`alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only`) 문자열이 전 기체 바이너리에 포함됨을 확인
- `altHoldInit`, `updatePosHold` 심볼로 Alt Hold / Position Hold가 전 기체(F405/F722/H743)에 실제로 링크됨을 확인
- 자력계 심볼(`compassConfig` 등) 존재 여부로 Pavo25 V2만 자력계가 빠졌고 나머지는 포함됨을 확인 (Explorer LR4: 5개 심볼 존재 vs Pavo25 V2 재현 빌드: 0개)
- Alt Hold 진입 스틱 래치(v5): `#pragma message` 진단으로 `ALT_HOLD_ENTRY_LATCH_RELEASE_PWM`이 전처리기 단계에서 정의됨을 확인.
- Alt Hold 해제 대기 + OSD "ALT WAIT"(v6): `"ALT WAIT"` 문자열이 전 기체 hex에 포함됨을 확인, `#pragma message` 진단으로 `ALT_HOLD_EXIT_HOVER_BAND_PWM`이 전처리기 단계에서 정의됨을 확인.
- 해제 대기의 호버 구간 건너뜀 결함 수정(v7): 호스트 유닛테스트(`src/test/unit/althold_unittest.cc`, `AltholdCustomSim` 스위트) 13개 전부 통과 — 진입 래치/해제 대기/구간 건너뜀(위·아래 양방향)/스위치 재투입/디스암 시나리오를 시뮬레이션으로 확인. 전 기체 재빌드에서 `"ALT WAIT"` 문자열·4개 CLI 파라미터 유지, `#pragma message` 진단으로 `ALT_HOLD_EXIT_HOVER_BAND_PWM`이 v7 코드에서도 계속 정의됨을 확인.
- 실비행 동작(래치 해제 타이밍, 해제 대기 해제 타이밍, 빠른 스틱 이동 시 실제 가로지름 판정, OSD 표시 등)은 벤치·비행 시험으로 별도 검증 필요 — 유닛테스트는 로직 검증이지 IMU/RC 잡음·스케줄러 타이밍까지 반영한 검증은 아님

## 사용상 주의

- 보드가 다르면 잘못된 hex다. 같은 MCU라도 보드별 파일이 다르므로 기체와 파일명을 확인한 뒤 플래시한다.
- 플래시 후 기존 CLI diff(프로젝트 문서 보관분)를 재적용해야 한다.
- 컴파일·링크·심볼 확인까지만 했고 기체 부팅과 비행은 검증하지 않았다. 프롭 제거 벤치 테스트를 먼저 한다.
- **해제 대기(v6/v7) 중에는 `ALT_HOLD_MODE`가 계속 켜진 상태다.** 즉 자세 제어는 앵글(자동 수평) 그대로이고, 대기 중 스로틀 스틱을 내려도 고도가 내려가지 않는다(목표 고도를 그대로 유지) — 설계된 동작이므로 놀라지 말 것. 스틱이 `ap_hover_throttle` ±5% 안에 들어오거나 그 값을 가로질러야 해제된다.
