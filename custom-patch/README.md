# Betaflight 2025.12.5 커스텀 펌웨어 (jsungho)

기체별로 필요한 기능만 넣은 Betaflight 2025.12.5 커스텀 hex 모음과 사용 설명서. **보드별(`make <보드이름>`) 빌드로 플래시 용량을 줄이고, F722 기체에는 Alt Hold/Position Hold를 새로 추가한** 버전이다. (이전에 있던 통합 타겟(MCU 단위) hex `firmware/v3/`는 제거되었다 — 이 폴더가 유일한 배포본이다.)

**v4: 전 기체에서 서보(USE_SERVOS)와 배터리-컨티뉴(USE_BATTERY_CONTINUE)를 제거했고, OSD는 디지털(MSP DisplayPort 등)만 남기고 아날로그 OSD/MAX7456 드라이버를 제거했다.** 이 저장소의 모든 기체가 디지털 VTX만 쓰고 서보를 쓰지 않기 때문.

**v5: Alt Hold 진입 스틱 래치(Entry Stick Latch)를 추가했다.** Alt Hold 진입 순간 스로틀 스틱 위치를 래치해, 스틱이 그 위치에서 5%(PWM 50) 이상 움직이기 전까지는 스틱 입력을 무시하고 고도를 그대로 유지한다. CLI 항목 없음(코드에 고정), PG 버전 변경 없음. 자세한 내용은 3-2절 참고.

**v6: Alt Hold 해제 대기(Exit Hold)와 OSD "ALT WAIT" 표시를 추가했다.** Alt Hold 스위치를 끈 순간에도 즉시 해제하지 않고, 스로틀 스틱이 `ap_hover_throttle` ±5%(PWM 50) 안에 들어올 때까지 고도를 계속 유지한다. 대기 중에는 OSD 경고창에 "ALT WAIT"(경고색, v11부터)가 뜬다. CLI 항목 없음(코드에 고정), PG 버전 변경 없음. 자세한 내용은 3-3절 참고.

**v7: 해제 대기의 "호버 구간 건너뜀" 결함을 수정했다.** 스틱을 빠르게 움직이면 매 사이클(10ms) 사이에 `ap_hover_throttle` ±5% 구간을 통째로 건너뛰어 해제가 안 되는 경우가 있었다 — 직전 사이클의 스틱 값을 같이 저장해, 이번 사이클과 직전 사이클 사이에 호버값을 가로질렀으면(구간 안에 들어오지 않고 지나쳤어도) 그 자리에서 해제하도록 고쳤다. 위/아래 양방향 모두 적용. CLI 항목 없음, PG 버전 변경 없음. 자세한 내용은 3-3절 참고. 호스트(PC)에서 돌리는 시뮬레이션 유닛테스트(`src/test/unit/althold_unittest.cc`)로 진입 래치/해제 대기/호버 가로지름/스위치 재투입/디스암 시나리오를 검증했다(v7 당시 13개 테스트, `_v7_slim`).

**v8: Alt Hold 착륙 보조(Landing Assist)를 추가했다.** Alt Hold 중 Airmode가 꺼져 있으면(=착륙 중으로 간주) 고도에 따라 수직 속도 상한을 `gps_rescue_descend_rate` 기반 값으로 낮춰, 착지 직전 과도한 상승/하강을 막는다. 히스테리시스(5.0m/5.5m, 1.8m/2.2m)로 경계에서 떨림이 없게 했고, OSD에 "ALTHOLD : LANDING"을 표시한다. CLI 항목 없음, PG 버전 변경 없음. 자세한 내용은 3-4절 참고. 호스트(PC) 시뮬레이션 유닛테스트에 `AltholdLandingAssist` 10개를 추가해 총 23개 테스트로 검증했다. 파일명 접미사가 `_v3_slim` → `_v4_slim` → `_v5_slim` → `_v6_slim` → `_v7_slim` → `_v8_slim` → `_v9_slim` → `_v10_slim`으로 바뀌었다.

**v9: `alt_hold_hover_throttle` 값 검증 + 적용 범위 한정.** (1) 허용값은 **0 또는 1100~1700**이다. CLI 범위(0~1700)는 그대로라 `1`, `500` 같은 값도 저장은 되지만, 1100 미만/1700 초과의 0이 아닌 값은 **무시(0으로 간주)** 되어 `ap_hover_throttle`을 쓴다. (2) 이 값은 **조종자가 Alt Hold / Position Hold 스위치로 켠 경우에만** 쓴다. 페일세이프 착륙(페일세이프도 `ALT_HOLD_MODE`를 켠다)과 GPS Rescue는 `ap_hover_throttle`을 쓴다(페일세이프가 해제되면 다시 `alt_hold_hover_throttle`). 이 버전(2025.12.5)에는 `AUTOPILOT_MODE`가 없어 해당 조건은 코드에 없다. CLI 항목 없음, PG 버전 변경 없음. 자세한 내용은 3-5절 참고. 호스트 유닛테스트 `AltholdHoverThrottle` 5개를 추가해 총 30개로 검증했다. 파일명 접미사는 `_v9_slim`.

**v10: 호버 변수 분리 + 우선순위 정리(v9 보완).** v9는 `alt_hold_hover_throttle`과 `ap_hover_throttle`이 0일 때의 스틱 캡처값을 구별하지 못했다. 이제 진입 시 `altHoldOverrideHoverPwm`(검증된 `alt_hold_hover_throttle`, 없으면 0)과 `altHoldCapturedHoverPwm`(`ap_hover_throttle`이 0일 때만 진입 순간 스틱 값, 아니면 0)을 따로 저장하고, Alt Hold 종료/초기화 시 둘 다 0으로 만든다. 호버 우선순위: ① 전용 값(조종자 스위치일 때만 — 페일세이프, GPS Rescue 중에는 제외) ② `ap_hover_throttle` ③ 진입 순간 스틱 캡처값 ④ 기본값 1275. GPS Rescue 플래그가 켜지는 즉시(Alt Hold 작업이 해제 처리하기 전 한 주기) 전용 값을 건너뛴다. 2025.12.5에는 `AUTOPILOT_MODE`가 없어 그 조건은 해당 없음. CLI 항목 없음, PG 버전 변경 없음. 자세한 내용은 3-5절 참고. 유닛테스트 `AltholdHoverThrottle` 9개(총 34개).

**v11: OSD "ALT WAIT"를 비행모드 칸에서 경고창으로 이동.** 8글자가 4글자용 칸을 넘어 옆 요소와 겹치던 문제 수정. CLI 항목 없음.

**v12: 착륙 보조 속도 상한 전환 시 목표 고도 고착 수정.** 착륙 보조가 상한을 낮출 때 기존 목표 선행량이 새 1초 문턱보다 크면 스틱이 목표 고도에 반영되지 않던 문제를 목표를 문턱 안으로 끌어당겨 해결. CLI 항목 없음.

**v13: 교차 검증(GPT) 지적 반영.** (1) 해제 대기 판정이 수신 처리 순서상 이전 프레임의 스로틀을 보던 문제: 스위치를 끄면서 스로틀을 내리면 이전 값(호버)으로 즉시 해제된 뒤 새 값으로 수동 전환될 수 있었다. 이제 같은 프레임의 최신 스로틀(`rcData` 기준)로 판단한다. (2) v12 목표 보정은 스틱이 목표를 움직이는 동안에만 적용(고도 유지·진입 래치·ALT WAIT 중에는 목표 보존). (3) 착륙 보조 속도 상한이 `alt_hold_climb_rate`보다 커지지 않게 제한(예: climb_rate 10이면 100 cm/s 초과 금지). CLI 항목 없음, PG 버전 변경 없음. 유닛테스트 41개.

**v14: RC 스무딩과 입력 기준 정리(교차 검증 2차).** v13의 최신 프레임 스로틀(`rcData` 기준)은 RC 스무딩 필터를 거치지 않아, 스무딩 ON(기본)일 때 기준이 어긋났다. (1) 해제 대기 해제 판정: 스무딩 ON이면 믹서가 해제 직후 실제로 쓰는 필터 출력(`rcCommand[THROTTLE]`)이 호버에 들어왔을 때만 해제(빠른 스틱 조작 시 필터가 따라오는 동안의 순간 저하 방지), OFF이면 최신 프레임 값 사용. (2) 진입 래치: 캡처와 비교를 항상 같은 `rcData` 기준 값으로 통일해, 스틱을 가만히 둬도 필터가 따라오는 동안 래치가 저절로 풀리던 문제 제거. (3) 문서 정정: `ap_hover_throttle`은 CLI로 0 설정 불가(1100~1700) — 0일 때 캡처 동작은 방어용 폴백임을 명시. CLI 항목 없음, PG 버전 변경 없음. 유닛테스트 44개.

**v15: 해제 대기 기준의 단위 정렬(전체 재분석 시뮬레이션 결과).** 해제 대기의 기준값 `ap_hover_throttle`은 PWM 값(`min_check` 기준 정규화)인데 비교 대상 `rcCommand[THROTTLE]`은 이미 `min_check`로 1000~2000으로 재매핑된 값이라 단위가 달라, 실제로는 호버 추력보다 약 14% 높은 스틱 위치(예: `ap_hover_throttle` 1300, `min_check` 1050이면 1300 대신 약 1263)에서 해제돼야 하는데 1300 근처에서 해제되던 문제를 수정했다. 이제 `ap_hover_throttle`을 `rcCommand` 단위로 변환해 ±5% 구간과 가로지름 판정을 한다(`min_check`가 다르면 중심도 따라 바뀜). 기준값 자체는 그대로 `ap_hover_throttle`이다. CLI 항목 없음, PG 버전 변경 없음. 유닛테스트 56개(폐루프 시뮬레이션 10개 포함, 호스트 전체 46개 스위트 통과, 시뮬레이션 결과는 BUILD_OPTIONS.md v15절).

**v16(현재): UART 수신 DMA 오타 2건 수정(교차 검증 5차).** 업스트림 2025.12.5 원본에 있던 오타로 우리 패치와는 무관하다. (1) `serial_uart_hw.c`: 수신 DMA 채널을 고를 때 `rxDmaopt` 대신 `txDmaopt`를 넘기던 것. (2) `serial_uart_hal.c`(H7/G4): 수신 DMA 초기화에서 `rxDMAHandle.Init.Request` 대신 `txDMAHandle.Init.Request`에 값을 넣던 것. **영향 범위**: UART DMA 기본값은 전부 미사용(`DMA_OPT_UNUSED`)이고 10개 기체 CLI에도 `dma` 설정이 없어 현재 기체에서는 발동하지 않던 잠재 결함이다(수신 DMA를 CLI로 직접 켠 경우에만 해당, H743 X8_5INCH 포함). 수정은 오타 교정 2줄이라 기본 동작은 그대로다. CLI 항목 없음, PG 버전 변경 없음.

- 브랜치: `custom-patch/alt-hold-throttle-range`
- 참고 이슈: betaflight/betaflight#15775
- 상세 빌드 옵션 표: [BUILD_OPTIONS.md](BUILD_OPTIONS.md)
- 재빌드: [build_custom.sh](build_custom.sh)
- 펌웨어: [`firmware/2025.12.5/`](firmware/2025.12.5/) (무결성: `SHA256SUMS.txt`)

> 컴파일·링크·바이너리 심볼 확인까지만 했고 실기체 비행은 검증하지 않았다. **프롭 제거 벤치 테스트 후** 사용한다.

## 1. 어떤 hex를 올리나 (기체별 1개만, 총 10개)

| 기체 | 파일 |
|---|---|
| MARIO5 (CRSF, PINIO 유지) | `betaflight_2025.12.5_STM32F405_SPEEDYBEEF405V4_MARIO5_custom_v16_slim.hex` |
| AOS_UL7_O4 (FPort, LED 스트립 제거) | `betaflight_2025.12.5_STM32F405_SPEEDYBEEF405V4_AOSUL7O4_custom_v16_slim.hex` |
| Mark4_6in | `betaflight_2025.12.5_STM32F405_JHEF405PRO_MARK4_6IN_custom_v16_slim.hex` |
| TJRC_10 | `betaflight_2025.12.5_STM32F7X2_MATEKF722SE_TJRC10_custom_v16_slim.hex` |
| 8IN-KOPIS_X8 | `betaflight_2025.12.5_STM32F7X2_SPEEDYBEEF7V3_8INKOPISX8_custom_v16_slim.hex` |
| CHIMERA7 | `betaflight_2025.12.5_STM32F7X2_FLYWOOF722PROV2_CHIMERA7_custom_v16_slim.hex` |
| AOS_UL7_X8 | `betaflight_2025.12.5_STM32F7X2_MATEKF722HD_AOSUL7X8_custom_v16_slim.hex` |
| Explorer LR4 | `betaflight_2025.12.5_STM32F7X2_JHEF7DUAL_EXPLORERLR4_custom_v16_slim.hex` |
| Pavo25 V2 (CRSF, PINIO, **자력계 없음**, LED 없음) | `betaflight_2025.12.5_STM32F7X2_JHEF7DUAL_PAVO25V2_custom_v16_slim.hex` |
| X8_5INCH | `betaflight_2025.12.5_STM32H743_MATEKH743_X8_5INCH_custom_v16_slim.hex` |

보드가 다르면 잘못된 hex다. MARIO5와 AOS_UL7_O4는 같은 FC(SPEEDYBEEF405V4), Pavo25 V2와 Explorer LR4는 같은 FC(JHEF7DUAL)라서 파일명 라벨(MARIO5 / AOSUL7O4 / PAVO25V2 / EXPLORERLR4)까지 확인해야 한다. 파일명의 보드 이름이 기체 FC와 같은지 확인한 뒤 Betaflight Configurator의 **Load Firmware [Local]**로 올린다.

**F722 기체(TJRC_10, 8IN-KOPIS_X8, CHIMERA7, AOS_UL7_X8, Explorer LR4, Pavo25 V2)는 Alt Hold/Position Hold가 이번에 새로 추가됐다.** F722는 플래시가 512KB라 `TARGET_FLASH_SIZE >= 1024` 조건을 만족하지 못해 원래는 빠지는데, 빌드 스크립트에서 `-DUSE_ALTITUDE_HOLD -DUSE_GPS -DUSE_POSITION_HOLD`로 명시적으로 켰다(71~76% 사용, 여유 있음).
F405/H743 기체(MARIO5, AOS_UL7_O4, Mark4_6in, X8_5INCH)는 MCU 플래시가 1MB/2MB로 조건을 항상 만족해 원래도 포함되어 있었다. 4개 커스텀 CLI 파라미터는 전 기체 공통으로 포함되며, 그 외 안 쓰는 기능(VTX, 레인지파인더, SimonK, 안 쓰는 수신기 프로토콜 등)만 빠졌다.

## 2. 플래시 절차

1. 플래시 전에 CLI `diff all`을 저장한다(기존 백업).
2. Configurator → Firmware Flasher → **Load Firmware [Local]** → 위 표의 hex 선택.
3. **Full Chip Erase**를 켜고 플래시한다(설정 구조가 바뀔 수 있어 권장).
4. 플래시 후 CLI에서 저장해 둔 diff를 다시 붙여넣는다. VTX 등 빠진 기능 관련 줄은 오류가 나지만 무시해도 된다(아래 5절).
5. 이 문서의 3절 커스텀 CLI를 필요에 따라 추가하고 `save`한다.

## 3. 추가된 CLI 파라미터

| 파라미터 | 범위 | 기본값 | 설명 |
|---|---|---|---|
| `alt_hold_deadband_low` | 0–70 (%) | 20 | Alt Hold 하강 쪽 스틱 데드밴드. 기존 `alt_hold_deadband`(상승 쪽)와 독립적으로 설정. 기본값은 기존 동작과 같음 |
| `alt_hold_full_low_is_max_descend` | OFF / ON | OFF | ON이면 스로틀을 min_check 아래(완전 저)로 내렸을 때 최대 하강 속도로 내려간다. OFF면 기존처럼 호버 유지 |
| `alt_hold_hover_throttle` | 0–1700 (허용값: 0 또는 1100~1700) | 0 | 조종자가 스위치로 켠 Alt Hold / Position Hold 전용 호버 스로틀. 0이면 `ap_hover_throttle` 상속. 1100 미만/1700 초과의 0이 아닌 값은 무시(0으로 간주)된다. 페일세이프 착륙, AUTOPILOT, GPS Rescue는 이 값을 쓰지 않고 `ap_hover_throttle` > 진입 순간 스틱 캡처값 > 기본값 순으로 쓴다(v9/v10, 3-5절) |
| `landing_disarm_airmode_off_only` | OFF / ON | OFF | ON이면 EZ Disarm(`landing_disarm_threshold`)이 Airmode가 꺼진 상태에서만 동작. 착륙 시 AUX로 Airmode를 끄고 착륙하는 운용용. 프로파일별 설정 |

동작 요약
- 하강 임계 = `호버 − alt_hold_deadband_low × (호버 − 1000)`, 상승 임계 = `호버 + alt_hold_deadband × (2000 − 호버)`. 두 임계 사이는 고도 유지.
- `alt_hold_full_low_is_max_descend`는 Alt Hold 중 스로틀 완전 저에서만 적용된다.
- 이 값들은 블랙박스 헤더에도 기록된다.

### 사용 예

```
# Alt Hold 하강 데드밴드를 좁게(하강이 빨리 시작), 완전 저 = 최대 하강
set alt_hold_deadband_low = 10
set alt_hold_full_low_is_max_descend = ON

# Alt Hold 전용 호버 스로틀 (0 = 사용 안 함, 권장 기본값은 3-1절 참고: 1400)
set alt_hold_hover_throttle = 1400

# EZ Disarm을 Airmode OFF일 때만 (프로파일별)
profile 0
set landing_disarm_airmode_off_only = ON
save
```

설정값 확인: `get alt_hold` / `get landing_disarm`

## 3-1. 권장 기본 CLI 값 (GPS Rescue / EZ Landing / Alt Hold)

플래시·`diff all` 복원 후, 아래 값을 기본값으로 적용한다(전 기체 공통, 사용자 확정).

| 파라미터 | 값 | 설명 |
|---|---|---|
| `gps_ublox_flight_model` | `AIRBORNE_1G` | u-blox GPS 동적 모델. 저가속(≤1g) 항공기용 — GPS Rescue/Position Hold의 GPS 필터링 특성에 영향 |
| `mixer_type` | `EZLANDING` | EZ Landing 모터 믹서 사용(착륙 시 모터 출력 제한) |
| `ez_landing_limit` | `10` | EZ Landing 최대 모터 출력 제한(스틱 중앙·스로틀 0일 때) |
| `ez_landing_threshold` | `30` | 이 값 이하 스로틀에서 EZ Landing 제한이 걸리기 시작하는 임계값 |
| `min_check` | `1050` | 스로틀 로우엔드 체크 값(이 값 미만 = 스로틀 로우로 판정) |
| `alt_hold_climb_rate` | `70` | Alt Hold 상승 속도(cm/s 단위 스케일) |
| `alt_hold_deadband` | `25` | Alt Hold 상승 쪽(HIGH) 데드밴드. 이 저장소가 추가한 `alt_hold_deadband_low`(하강 쪽)와 쌍을 이룸 |
| `alt_hold_hover_throttle` | `1400` | 이 저장소가 추가한 Alt Hold/Position Hold 전용 호버 스로틀(3절 참고). 유효값(1100~1700)이라 스위치로 켠 Alt Hold/Position Hold에서만 1400을 사용(페일세이프/GPS Rescue는 `ap_hover_throttle`) |
| `gps_rescue_descend_rate` | `135` | GPS Rescue 하강 속도 |
| `gps_rescue_disarm_threshold` | `60` | GPS Rescue 착지 판정 후 디스암 임계값(가속도 저크) |
| `gps_rescue_use_mag` | `ON` | GPS Rescue 시 자력계 헤딩 사용(자력계 없는 Pavo25 V2는 해당 없음 — 아래 주 참고) |
| `pos_hold_without_mag` | `OFF` | Position Hold를 자력계 없이 쓰는 것을 허용할지 여부. OFF = 자력계 필수(자력계 없는 기체는 Position Hold 시 안전을 위해 비활성) |
| `landing_disarm_threshold` | `45` | EZ Disarm(착지 충격 자동 디스암) 활성. 값이 낮을수록 민감(코드 주석 안전값 약 100). 프로파일별 값 |
| `alt_hold_deadband_low` | `0` | Alt Hold 하강 쪽 데드밴드 없음 |
| `alt_hold_full_low_is_max_descend` | `ON` | 스로틀이 min_check 미만이면 최대 하강 속도로 하강 |
| `landing_disarm_airmode_off_only` | `ON` | airmode가 켜져 있으면 EZ Disarm 호출 안 함. 프로파일별 값 |

```
set gps_ublox_flight_model = AIRBORNE_1G
set mixer_type = EZLANDING
set ez_landing_limit = 10
set ez_landing_threshold = 30
set min_check = 1050
set alt_hold_climb_rate = 70
set gps_rescue_disarm_threshold = 60
set gps_rescue_use_mag = ON
set pos_hold_without_mag = OFF
set landing_disarm_threshold = 45
set gps_rescue_descend_rate = 135
set alt_hold_deadband = 25
set alt_hold_deadband_low = 0
set alt_hold_full_low_is_max_descend = ON
set alt_hold_hover_throttle = 1400
set landing_disarm_airmode_off_only = ON
save
```

- **자력계가 없는 Pavo25 V2**는 `gps_rescue_use_mag = ON`이어도 자력계 자체가 없어 실질적으로 무자력계 헤딩 추정으로 동작한다(`pos_hold_without_mag`가 Position Hold에 별도로 적용됨). 벤치에서 헤딩 추정 정확도를 먼저 확인한다.
- `landing_disarm_threshold`(45)와 `landing_disarm_airmode_off_only`(ON)는 PID 프로파일 값이므로 비행에 쓰는 프로파일마다 적용한다. airmode ON이면 EZ Disarm이 호출되지 않고 airmode OFF 착륙 때만 작동한다(2026-10-02 사용자 확정).
- `mixer_type = EZLANDING`은 6절 주의사항의 Alt Hold 하강 제동 상호작용과 함께 벤치에서 확인한다.

## 3-2. Alt Hold 진입 스틱 래치 (Entry Stick Latch, v5)

CLI 파라미터가 아니라 코드에 고정된 동작이다(PG 버전 변경 없음). 원본: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `665f62a9e`를 이 저장소(2025.12.5) 코드 구조에 맞게 이식.

- Alt Hold 진입(모드 켜는 순간)의 스로틀 스틱 위치를 저장한다.
- 스틱이 그 위치에서 5%(PWM 50, 1000~2000 범위) 이상 움직이기 전까지는 스틱에 의한 고도 조절을 완전히 무시하고 진입 시점 고도를 그대로 유지한다. 이 구간에서는 `alt_hold_full_low_is_max_descend`도 적용되지 않는다.
- 5%를 넘기면 그 순간부터 3절의 기존 커스텀 로직(`alt_hold_hover_throttle`, `alt_hold_deadband`, `alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`)이 호버 대비 스틱의 절대 위치 기준으로 적용된다.
- 한 번 래치가 풀리면 Alt Hold를 껐다가 다시 켜기 전까지 재적용되지 않는다.
- 페일세이프/GPS Rescue의 하강 오버라이드는 래치 상태와 무관하게 항상 최우선 적용된다.
- Position Hold의 좌우/전후 스틱 로직은 이 패치의 영향을 받지 않는다.

목적: Alt Hold 진입 순간 스틱이 정확히 호버 위치가 아니어도(조종자가 미세하게 어긋난 상태로 모드를 켜도) 의도치 않은 상승/하강이 시작되지 않도록 한다.

## 3-3. Alt Hold 해제 대기 (Exit Hold, v6/v7)

CLI 파라미터가 아니라 코드에 고정된 동작이다(PG 버전 변경 없음). 원본: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `b83985b`(해제 대기 + OSD 표시), `e4593eb`(해제 기준을 `ap_hover_throttle`로 변경), `415e8c4`(호버 구간을 가로지르면 해제 + 시뮬레이션 테스트, v7)를 이 저장소(2025.12.5) 코드 구조에 맞게 이식. 진입 스틱 래치(3-2절, 커밋 `665f62a`)는 이번 이식 대상이 아니다.

- Alt Hold 스위치를 끈 순간에 Alt Hold가 활성 상태였다면 바로 해제하지 않고 고도를 유지한다(해제 대기). 대기 중에는 스틱에 의한 고도 조절을 완전히 무시하고 목표 고도를 그대로 유지한다.
- 스로틀 스틱이 `ap_hover_throttle` ±5%(±PWM 50, 1000~2000 범위) 안에 들어오면 그때 Alt Hold를 해제한다. (v15) `ap_hover_throttle`은 `min_check` 재매핑을 거친 `rcCommand` 단위로 변환해 비교한다(예: 1300 → 1263 @ `min_check` 1050). 기준은 `ap_hover_throttle`만 쓴다 — `alt_hold_hover_throttle`, `thr_mid`는 기준이 아니다. `ap_hover_throttle`이 0이면 Alt Hold의 실효 호버 값(`alt_hold_hover_throttle`이 설정돼 있으면 그 값, 아니면 `ap_hover_throttle` 상속분)으로 대체한다.
- 스위치를 끈 순간 스틱이 이미 그 구간 안에 있으면 바로 해제된다.
- **(v7) 스틱을 빠르게 움직여 한 사이클(10ms) 사이에 ±5% 구간을 통째로 건너뛰어도 해제된다.** 매 사이클 직전 스틱 값을 저장해뒀다가, 이번 값과 직전 값 사이에서 `스틱 - ap_hover_throttle`의 부호가 바뀌었으면(=호버 값을 가로질렀으면) 구간 안에 정확히 들어오지 않았어도 그 자리에서 해제한다. 위(높은 쪽)에서 아래로, 아래에서 위로 넘어가는 경우 모두 해당. 해제 조건 = `|스틱 - ap_hover_throttle| <= 50` **이거나** 직전 사이클 대비 호버 값을 가로지른 경우.
- 대기 중 스위치를 다시 켜면 대기를 취소하고 일반 Alt Hold로 복귀한다(이때 3-2절의 진입 스틱 래치가 현재 스틱 위치로 새로 걸린다).
- 페일세이프/GPS Rescue의 하강 오버라이드는 대기 상태와 무관하게 항상 최우선 적용된다.
- (v11부터) 해제 대기 중이면 OSD 경고창에 "ALT WAIT"가 경고색으로 깜박이며 뜬다(비행모드 칸은 `ALTH`). 경고 우선순위는 `POSHOLD FAIL` 다음, `HEADFREE`/배터리 경고보다 앞이다.
- Position Hold는 변경되지 않았다.
- **해제 대기 중에도 `ALT_HOLD_MODE` 자체는 계속 켜져 있는 상태다.** 즉 자세 제어는 앵글(자동 수평) 모드 그대로 유지되고(스위치를 껐다고 즉시 레이트/매뉴얼 자세로 바뀌지 않음), 대기 중 스로틀 스틱을 아무리 내려도 고도가 내려가지 않는다(목표 고도를 그대로 붙잡고 있음). 이는 설계된 동작이다 — 스틱이 호버 구간에 들어오거나 호버 값을 가로지르기 전까지는 의도적으로 "완전히 무시"한다.

목적: Alt Hold 스위치를 끄는 순간 스틱이 호버 위치와 다르면 스로틀이 갑자기 바뀌던 문제를 막는다 — 스틱을 호버 근처로 가져와야만(혹은 빠르게 그 값을 가로질러야만) 수동 스로틀로 정상 전환된다.

검증: 호스트(PC)에서 돌리는 구글테스트 기반 시뮬레이션(`src/test/unit/althold_unittest.cc`, `AltholdCustomSim` 스위트)으로 진입 래치, 해제 대기 유지/해제, `ap_hover_throttle` 기준(다른 파라미터 아님) 확인, 즉시 해제, 위/아래 양방향 빠른 스틱 이동으로 구간을 건너뛰는 경우, 대기 중 스위치 재투입, 디스암 시 대기 삭제, 진입한 적 없는 상태에서 스위치를 끄면 대기가 시작되지 않는 경우까지 13개 테스트로 확인(실행: `cd src/test && make test_althold_unittest`).

**알아둘 동작 (v15 재분석 시뮬레이션에서 확인)**
- 스무딩 OFF에서 스위치를 끄면서 같은 프레임에 스틱을 끝까지 내리면, 스틱이 호버를 다시 지나갈 때까지 고도를 유지한다(ALT WAIT). 스무딩 ON(기본)에서는 필터 출력이 내려가며 호버를 지나는 순간 해제된다. 두 경우 모두 설계된 동작이다.
- 낮은 고도에서 빠르게 하강(약 3.7 m/s, 4 m)하다 복행(스틱 최대 상승)하면, 모델에서는 약 1.7초 뒤에야 상승으로 바뀌어 지면에 닿을 수 있다. v11은 아예 회복하지 못했다. 낮은 고도 복행은 여유를 두고 일찍 시작한다.
- 펌웨어를 업데이트하면 PID 프로파일(`pidProfiles` PG 11→12)과 Alt Hold 설정(`altHoldConfig` PG 4→6)이 초기화된다. 플래시 후 기체별 CLI(`diff all`)를 반드시 다시 적용한다.

## 3-4. Alt Hold 착륙 보조 (Landing Assist, v8)

CLI 파라미터가 아니라 코드에 고정된 동작이다(PG 버전 변경 없음). 원본: `jsungho/betaflight2026.6.x_custom` 브랜치 `custom-patch/alt-hold-throttle-range-2026.6.2` 커밋 `90d5d9c`(최초), `f29a459`(OSD 고도 조건), `0f78e2e`(5.0m/5.5m 히스테리시스), `e2c4e83`(1.8m/2.2m 히스테리시스)를 이 저장소(2025.12.5) 코드 구조에 맞게 이식. 2026.6.2의 `getAltitudeCmControl()`/`altHold.maxClimbRate`는 2025.12.5에 없어 각각 `getAltitudeCm()`/`altHold.maxVelocity`로 대체했다.

**동작 조건** (모두 만족해야 "착륙 보조" 적용):

| 조건 | 값 |
|---|---|
| 비행 모드 | Alt Hold(`ALT_HOLD_MODE`) |
| Airmode | OFF (`isAirmodeEnabled() == false`) |
| 시동 | ARMED |

- 위 세 조건을 하나라도 벗어나면 아래 히스테리시스 래치가 즉시 모두 풀린다. `altHoldInit()`과 Alt Hold 종료 전환 시에도 풀린다.
- 페일세이프 자동 착륙(`failsafeIsActive()`)과 GPS Rescue 자체의 속도 처리는 기존 로직 그대로이며 착륙 보조가 끼어들지 않는다.
- `USE_GPS_RESCUE`가 없는 빌드에서는 착륙 보조 속도 변경 코드 자체가 컴파일되지 않는다(이 저장소의 10개 기체는 전부 GPS/GPS Rescue를 포함하므로 전 기체 적용).

**수직 속도 상한** — Alt Hold의 기존 상한(`alt_hold_climb_rate × 10` cm/s)을 아래처럼 교체한다(상승에도 동일 적용, 스틱 비율은 그대로):

| 고도(제어용, `getAltitudeCm()`) | 속도 상한 |
|---|---|
| 5m 초과 (또는 Airmode ON / Alt Hold 아님) | `alt_hold_climb_rate` 그대로 |
| 5m 이하 | `gps_rescue_descend_rate × 2` |
| 2m 이하 | `gps_rescue_descend_rate × 1` |

**히스테리시스** (경계에서 상태가 떨리지 않도록 ON/OFF 임계값을 다르게 둠):

| 구간 | 켜짐(ON) | 꺼짐(OFF) |
|---|---|---|
| 5m 구간 (×2, OSD 문구 공통) | 고도 ≤ 5.0m (500cm) | 고도 > 5.5m (550cm) |
| 2m 구간 (×1) | 고도 ≤ 1.8m (180cm) | 고도 > 2.2m (220cm) |

2m 구간이 켜져 있으면 5m 구간 안에서도 ×1이 우선한다. 구현상 `isAltHoldLandingMode()` 호출마다 5m 래치가 갱신되고, 속도 상한 계산 함수가 이 함수를 호출한 뒤 2m 래치를 갱신한다.

**OSD 표시**: `isAltHoldLandingMode()`가 true면 경고창에 `"ALTHOLD : LANDING"`(색상: INFO, 깜박임 없음)을 표시한다. 배터리·RSSI·페일세이프 경고보다 우선순위가 낮고 비주얼 비퍼(`* * * *`) 바로 앞에 뜬다. Airmode가 켜지거나, 디스암되거나, 고도가 5.5m를 넘으면 문구가 사라진다. 새 OSD 경고 비트/CLI 항목을 추가하지 않았다.

기존 커스텀 CLI 값(`alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only`)과 진입 스틱 래치(3-2절)/해제 대기(3-3절)는 원래 로직대로 동작하며, 착륙 보조는 수직 속도 상한 계산 함수만 바꾼다.

목적: 조종자가 Airmode를 끄고 착륙 조작을 하는(이 저장소 3-1절의 운용 전제) 상황에서, 지면 근처의 과도한 상승/하강 속도를 GPS Rescue의 하강 속도 설정을 재사용해 자동으로 제한한다.

검증: 호스트(PC) 시뮬레이션에 `AltholdLandingAssist` 10개 테스트를 추가(총 23개). 5m 초과 시 `alt_hold_climb_rate` 그대로, 5m/2m 이하 구간별 배율, Airmode ON 시 미적용, 스틱 부분 입력 비율 유지, 상승 속도도 동일 상한 적용, 5.0m/5.5m 및 1.8m/2.2m 히스테리시스(경계 사이에서 상태 유지), `isAltHoldLandingMode()`가 Airmode/시동 상태를 따르는지까지 확인(실행: `cd src/test && make test_althold_unittest`).

## 3-5. alt_hold_hover_throttle 검증과 적용 범위 (v9, v10 보완)

| 항목 | 동작 |
|---|---|
| 허용값 | 0 또는 1100~1700 (경계 포함). CLI 범위는 0~1700 그대로 |
| 범위 밖 값(예 1, 500, 1099, 1701) | 저장은 되지만 사용 시 0으로 간주 → `ap_hover_throttle` 사용 |
| 0 | `ap_hover_throttle` 상속(기존 동작) |
| 변수 분리(v10) | `altHoldOverrideHoverPwm`(검증된 전용 값) / `altHoldCapturedHoverPwm`(`ap_hover_throttle`=0일 때의 진입 스틱 값). 진입 시 채우고 종료/초기화 시 0. **참고: `ap_hover_throttle`의 CLI 허용 범위는 1100~1700이라 CLI로는 0을 설정할 수 없다.** 0일 때의 캡처/폴백은 CLI 밖 경로(MSP 등)로 0이 저장된 경우를 위한 방어 코드이며 일반 운용에서는 쓰이지 않는다(기본값 1275). 표의 "0" 관련 설명은 이 내부 예외 처리를 가리킨다 |
| 스위치로 켠 Alt Hold / Position Hold | 유효한 `alt_hold_hover_throttle`을 호버 기준(고도 제어 호버 + 스틱 데드밴드 중심)으로 사용 |
| 페일세이프 착륙 | `failsafeIsActive()` 동안 `ap_hover_throttle` > 진입 순간 스틱 캡처값(`ap_hover_throttle`=0일 때) > 기본값(1275). 해제되면 다시 `alt_hold_hover_throttle` |
| AUTOPILOT 모드 | 2025.12.5에는 `AUTOPILOT_MODE`가 없어 해당 없음(추가되면 같은 조건에 포함) |
| GPS Rescue | 기존대로 Alt Hold가 꺼지고 `ap_hover_throttle` 사용. v10: `GPS_RESCUE_MODE` 플래그가 켜지는 즉시 전용 값을 건너뜀 |
| 해제 대기(3-3절) 기준 | 기존대로 `ap_hover_throttle` (0이면 유효한 `alt_hold_hover_throttle`로 폴백) |

구현: `alt_hold_multirotor.c`에 `ALT_HOLD_HOVER_THROTTLE_VALID_MIN/MAX`(1100/1700)와 `altHoldGetHoverThrottle()`을 추가했다. v9에서는 2026.6.2의 `autopilotCaptureHoverThrottleForAltHold`에 해당하는 "진입 순간 호버 캡처"가 없어 기존에 `altHoldInit()`에서 한 번 고정하던 `altHold.hoverThrottle`을, 사용 시점마다 `failsafeIsActive()`를 보고 정하는 함수로 바꿨다. v10부터는 진입 순간 캡처(`altHoldCapturedHoverPwm`)가 있으나, 이는 `ap_hover_throttle`이 0인 비정상 상태(CLI로는 설정 불가)에서만 쓰이는 폴백이다.

## 4. 플래시 후 알려진 오류 줄 (무시 가능)

- VTX 관련: `osd_vtx_channel_pos`, `osd_sys_vtx_temp_pos` 등 (VTX 제어 기능 제거)
- 8IN-KOPIS_X8, Pavo25 V2: `feature LED_STRIP`, `resource LED_STRIP 1 NONE`
- PINIO를 뺀 기체(Mark4_6in, AOS_UL7_X8, Explorer LR4, X8_5INCH): 해당 CLI에 PINIO 줄이 있으면 오류
- 랜지파인더/트랜스폰더/GPS 랩타이머 등 CLI에 원래 없던 설정이면 해당 사항 없음
- 서보(`servo`, `smix`), 배터리-컨티뉴(`battery_continue`), 아날로그 OSD 관련 CLI 줄(있었다면): v4부터 전 기체 공통 제거

## 5. 빌드에서 제거한 기능과 유지한 기능

- 제거: VTX 제어(common/control/table/SmartAudio/Tramp/MSP/RTC6705), 트랜스폰더, 레인지파인더·옵티컬플로우, OLED 대시보드, SimonK, GPS 랩타이머·Plus Codes, 런치 컨트롤, 안 쓰는 수신기·텔레메트리 프로토콜, PINIO(미사용 기체), LED 스트립(미사용 기체), **서보(전 기체), 배터리-컨티뉴(전 기체), 아날로그 OSD/MAX7456(전 기체)**
- 유지: **자력계(Pavo25 V2 제외)**, GPS / GPS Rescue, **Alt Hold / Position Hold(전 기체)**, **Alt Hold 진입 스틱 래치(전 기체, v5, 3-2절)**, **Alt Hold 해제 대기 + OSD "ALT WAIT"(전 기체, v6, 3-3절)**, **Alt Hold 착륙 보조 + OSD "ALTHOLD : LANDING"(전 기체, v8, 3-4절)**, MSP DisplayPort OSD(디지털 전용, Walksnail 등), 블랙박스, ESC 센서, BLHeli 4way(Bluejay / AM32), 커스텀 CLI 파라미터 4종
- 시리얼 포트의 `131073`(MSP + VTX_MSP) 설정은 그대로 두어도 되지만 VTX_MSP는 빌드에 없어 VTX 제어만 빠진다. OSD 표시는 MSP DisplayPort(디지털)로 유지된다.
- 자세한 기체별 표는 [BUILD_OPTIONS.md](BUILD_OPTIONS.md).

## 6. 주의

- F722 보드는 플래시 사용률이 73.6~79.0%다(v16 기준). 기능 추가 시 다시 확인한다.
- `mixer_type = EZLANDING`이 켜져 있으면 Alt Hold 하강 제동에 영향을 줄 수 있으니 시험 전에 확인한다.
- Pavo25 V2는 자력계가 없다 — Position Hold 동작(자력계 없이 헤딩 추정)을 벤치에서 먼저 확인한다. 이 기체의 CLI 덤프는 Betaflight 4.5.5 기준(오래됨)이니 플래시 전 최신 `diff all`로 재확인한다.
- **MARIO5(CRSF)/AOS_UL7_O4(FPort)/X8_5INCH(FPort)는 수신기 프로토콜이 CLI로 확정되지 않는다** — CLI에 `serialrx_provider`가 없어 가정한 값이다(자세한 내용: [BUILD_OPTIONS.md](BUILD_OPTIONS.md) 주석 2). 틀리면 플래시 후 수신기가 바인드되지 않으니 벤치에서 먼저 확인한다.
- **Alt Hold 해제 대기(v6/v7, 3-3절)**: 스위치를 꺼도 스틱이 `ap_hover_throttle` ±5% 안에 들어오거나(또는 빠르게 그 값을 가로지르거나) 하기 전까지는 고도가 계속 유지된다 — 스위치만 끄면 즉시 수동 스로틀로 넘어가던 이전 동작과 다르다. **대기 중에는 앵글(자동 수평) 자세 그대로이고, 스로틀을 아무리 내려도 하강하지 않는다** — 설계된 동작이지만 처음 접하면 "스로틀을 내렸는데 안 떨어진다"고 당황할 수 있으니 반드시 미리 숙지한다. OSD의 "ALT WAIT" 표시를 보고 스틱을 호버 근처로 가져와야(또는 반대편으로 빠르게 넘겨야) 해제된다는 점을 비행 전 벤치에서 먼저 확인한다(3-3절 "검증" 참고).
- **Alt Hold 착륙 보조(v8, 3-4절)**: Airmode를 끈 상태로 Alt Hold 중이면 지면 근처(5m/2m 이하)에서 수직 속도 상한이 `gps_rescue_descend_rate` 기반으로 자동으로 낮아진다 — `alt_hold_climb_rate`를 높게 설정해 둔 기체라면 착지 직전 체감 하강/상승 속도가 평소보다 느려질 수 있으니 비행 전 벤치에서 OSD "ALTHOLD : LANDING" 표시와 함께 먼저 확인한다. `gps_rescue_descend_rate`가 너무 낮으면 착륙 보조 구간의 속도도 같이 낮아진다는 점에 유의한다.
