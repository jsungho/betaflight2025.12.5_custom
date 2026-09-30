# Betaflight 2025.12.5 커스텀 펌웨어 (jsungho)

기체별로 필요한 기능만 넣은 Betaflight 2025.12.5 커스텀 hex 모음과 사용 설명서. **보드별(`make <보드이름>`) 빌드로 플래시 용량을 줄이고, F722 기체에는 Alt Hold/Position Hold를 새로 추가한** 버전이다. (이전에 있던 통합 타겟(MCU 단위) hex `firmware/v3/`는 제거되었다 — 이 폴더가 유일한 배포본이다.)

**v4(현재): 전 기체에서 서보(USE_SERVOS)와 배터리-컨티뉴(USE_BATTERY_CONTINUE)를 제거했고, OSD는 디지털(MSP DisplayPort 등)만 남기고 아날로그 OSD/MAX7456 드라이버를 제거했다.** 이 저장소의 모든 기체가 디지털 VTX만 쓰고 서보를 쓰지 않기 때문. 파일명 접미사가 `_v3_slim` → `_v4_slim`으로 바뀌었다.

- 브랜치: `custom-patch/alt-hold-throttle-range`
- 참고 이슈: betaflight/betaflight#15775
- 상세 빌드 옵션 표: [BUILD_OPTIONS.md](BUILD_OPTIONS.md)
- 재빌드: [build_custom.sh](build_custom.sh)
- 펌웨어: [`firmware/2025.12.5/`](firmware/2025.12.5/) (무결성: `SHA256SUMS.txt`)

> 컴파일·링크·바이너리 심볼 확인까지만 했고 실기체 비행은 검증하지 않았다. **프롭 제거 벤치 테스트 후** 사용한다.

## 1. 어떤 hex를 올리나 (기체별 1개만, 총 10개)

| 기체 | 파일 |
|---|---|
| MARIO5 (CRSF, PINIO 유지) | `betaflight_2025.12.5_STM32F405_SPEEDYBEEF405V4_MARIO5_custom_v4_slim.hex` |
| AOS_UL7_O4 (FPort, LED 스트립 제거) | `betaflight_2025.12.5_STM32F405_SPEEDYBEEF405V4_AOSUL7O4_custom_v4_slim.hex` |
| Mark4_6in | `betaflight_2025.12.5_STM32F405_JHEF405PRO_MARK4_6IN_custom_v4_slim.hex` |
| TJRC_10 | `betaflight_2025.12.5_STM32F7X2_MATEKF722SE_TJRC10_custom_v4_slim.hex` |
| 8IN-KOPIS_X8 | `betaflight_2025.12.5_STM32F7X2_SPEEDYBEEF7V3_8INKOPISX8_custom_v4_slim.hex` |
| CHIMERA7 | `betaflight_2025.12.5_STM32F7X2_FLYWOOF722PROV2_CHIMERA7_custom_v4_slim.hex` |
| AOS_UL7_X8 | `betaflight_2025.12.5_STM32F7X2_MATEKF722HD_AOSUL7X8_custom_v4_slim.hex` |
| Explorer LR4 | `betaflight_2025.12.5_STM32F7X2_JHEF7DUAL_EXPLORERLR4_custom_v4_slim.hex` |
| Pavo25 V2 (CRSF, PINIO, **자력계 없음**, LED 없음) | `betaflight_2025.12.5_STM32F7X2_JHEF7DUAL_PAVO25V2_custom_v4_slim.hex` |
| X8_5INCH | `betaflight_2025.12.5_STM32H743_MATEKH743_X8_5INCH_custom_v4_slim.hex` |

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
| `alt_hold_hover_throttle` | 0–1700 | 0 | Alt Hold / Position Hold 전용 호버 스로틀. 0이면 기존 동작(`ap_hover_throttle` 상속). 0이 아니면 GPS Rescue와 별개로 우선 적용된다. GPS Rescue는 영향을 받지 않는다 |
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

# Alt Hold 전용 호버 스로틀 (0 = 사용 안 함)
set alt_hold_hover_throttle = 1300

# EZ Disarm을 Airmode OFF일 때만 (프로파일별)
profile 0
set landing_disarm_airmode_off_only = ON
save
```

설정값 확인: `get alt_hold` / `get landing_disarm`

## 4. 플래시 후 알려진 오류 줄 (무시 가능)

- VTX 관련: `osd_vtx_channel_pos`, `osd_sys_vtx_temp_pos` 등 (VTX 제어 기능 제거)
- 8IN-KOPIS_X8, Pavo25 V2: `feature LED_STRIP`, `resource LED_STRIP 1 NONE`
- PINIO를 뺀 기체(Mark4_6in, AOS_UL7_X8, Explorer LR4, X8_5INCH): 해당 CLI에 PINIO 줄이 있으면 오류
- 랜지파인더/트랜스폰더/GPS 랩타이머 등 CLI에 원래 없던 설정이면 해당 사항 없음
- 서보(`servo`, `smix`), 배터리-컨티뉴(`battery_continue`), 아날로그 OSD 관련 CLI 줄(있었다면): v4부터 전 기체 공통 제거

## 5. 빌드에서 제거한 기능과 유지한 기능

- 제거: VTX 제어(common/control/table/SmartAudio/Tramp/MSP/RTC6705), 트랜스폰더, 레인지파인더·옵티컬플로우, OLED 대시보드, SimonK, GPS 랩타이머·Plus Codes, 런치 컨트롤, 안 쓰는 수신기·텔레메트리 프로토콜, PINIO(미사용 기체), LED 스트립(미사용 기체), **서보(전 기체), 배터리-컨티뉴(전 기체), 아날로그 OSD/MAX7456(전 기체)**
- 유지: **자력계(Pavo25 V2 제외)**, GPS / GPS Rescue, **Alt Hold / Position Hold(전 기체)**, MSP DisplayPort OSD(디지털 전용, Walksnail 등), 블랙박스, ESC 센서, BLHeli 4way(Bluejay / AM32), 커스텀 CLI 파라미터 4종
- 시리얼 포트의 `131073`(MSP + VTX_MSP) 설정은 그대로 두어도 되지만 VTX_MSP는 빌드에 없어 VTX 제어만 빠진다. OSD 표시는 MSP DisplayPort(디지털)로 유지된다.
- 자세한 기체별 표는 [BUILD_OPTIONS.md](BUILD_OPTIONS.md).

## 6. 주의

- F722 보드는 플래시 사용률이 71~76%다. 기능 추가 시 다시 확인한다.
- `mixer_type = EZLANDING`이 켜져 있으면 Alt Hold 하강 제동에 영향을 줄 수 있으니 시험 전에 확인한다.
- Pavo25 V2는 자력계가 없다 — Position Hold 동작(자력계 없이 헤딩 추정)을 벤치에서 먼저 확인한다. 이 기체의 CLI 덤프는 Betaflight 4.5.5 기준(오래됨)이니 플래시 전 최신 `diff all`로 재확인한다.
- **MARIO5(CRSF)/AOS_UL7_O4(FPort)/X8_5INCH(FPort)는 수신기 프로토콜이 CLI로 확정되지 않는다** — CLI에 `serialrx_provider`가 없어 가정한 값이다(자세한 내용: [BUILD_OPTIONS.md](BUILD_OPTIONS.md) 주석 2). 틀리면 플래시 후 수신기가 바인드되지 않으니 벤치에서 먼저 확인한다.
