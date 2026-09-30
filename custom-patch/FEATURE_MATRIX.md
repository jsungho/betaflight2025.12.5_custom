# 기체별 업로드 기능 정리표 (2025.12.5 보드별 슬림 빌드)

대상 hex: [`firmware/2025.12.5/`](firmware/2025.12.5/) (10개, 보드별 빌드, `_v7_slim`) — 이 저장소의 유일한 배포 펌웨어다. 이전에 있던 통합 타겟(MCU 단위) hex(`firmware/v3/`)는 제거되었다.

**v4: 서보(USE_SERVOS)·배터리-컨티뉴(USE_BATTERY_CONTINUE)·아날로그 OSD(USE_OSD_SD/MAX7456) 전 기체 공통 제거, OSD는 디지털(MSP DisplayPort 등)만 유지.**
**v5: Alt Hold 진입 스틱 래치(Entry Stick Latch) 추가 — `alt_hold_multirotor.c`. CLI 항목 없음(코드 고정), PG 버전 변경 없음. 전 기체 동일 적용. 자세한 동작은 [README.md](README.md) 3-2절, [BUILD_OPTIONS.md](BUILD_OPTIONS.md) 참고.**
**v6: Alt Hold 해제 대기(Exit Hold) + OSD "ALT WAIT" 표시 추가 — `alt_hold_multirotor.{c,h}`, `fc/core.c`, `osd/osd_elements.c`. CLI 항목 없음(코드 고정), PG 버전 변경 없음. 전 기체 동일 적용. 자세한 동작은 [README.md](README.md) 3-3절, [BUILD_OPTIONS.md](BUILD_OPTIONS.md) 참고.**
**v7: 해제 대기의 "호버 구간 건너뜀" 결함 수정(빠른 스틱 이동 시 위/아래 양방향 모두 해제) + 호스트 시뮬레이션 유닛테스트 13개 추가 — `alt_hold_multirotor.c`, `src/test/unit/althold_unittest.cc`. CLI 항목 없음(코드 고정), PG 버전 변경 없음. 전 기체 동일 적용.**

| 기체 | 보드(FC) | MCU | 자력계 | PINIO | LED 스트립 | 수신기 | 텔레메트리 | Alt/Pos Hold | 진입 래치 | 해제 대기 | 서보 | 배터리-컨티뉴 | OSD | Flash |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| MARIO5 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 39.38% |
| AOS_UL7_O4 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 | 제거 | FPort | SmartPort | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 39.17% |
| Mark4_6in | JHEF405PRO | F405 | 포함 | 제거 | 제거 | SBUS | 없음 | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 39.70% |
| TJRC_10 | MATEKF722SE | F722 | 포함 | 포함 | 포함 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 78.92% |
| 8IN-KOPIS_X8 | SPEEDYBEEF7V3 | F722 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 76.39% |
| CHIMERA7 | FLYWOOF722PROV2 | F722 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 74.10% |
| AOS_UL7_X8 | MATEKF722HD | F722 | 포함 | 제거 | 포함 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 76.69% |
| Explorer LR4 | JHEF7DUAL | F722 | 포함 | 제거 | 포함 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 76.75% |
| **Pavo25 V2** | JHEF7DUAL | F722 | **제거 (센서 없음)** | 포함 | 제거 | CRSF | CRSF | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 73.55% |
| X8_5INCH | MATEKH743 | H743 | 포함 | 제거 | 포함 | FPort | SmartPort | 포함 | 포함 | 포함 | 제거 | 제거 | 디지털만 | 24.35% |

> **참고**: F722 기체(TJRC_10 ~ Pavo25 V2)는 512KB 플래시라 `TARGET_FLASH_SIZE >= 1024` 조건을 만족하지 못해 Alt Hold/Position Hold가 기본적으로 빠진다. 이 표의 F722 기체용 "Alt/Pos Hold 포함"은 빌드에서 `-DUSE_ALTITUDE_HOLD -DUSE_GPS -DUSE_POSITION_HOLD`로 명시적으로 켠 것이다. 자세한 근거는 [BUILD_OPTIONS.md](BUILD_OPTIONS.md)의 "F722에서 추가 옵션이 필요한 이유" 참고.

## 공통 사항 (전 기체)

- 커스텀 CLI 4종 전부 포함: `alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only`
- Alt Hold / Position Hold / GPS·GPS Rescue / MSP DisplayPort OSD(디지털) / 블랙박스 / ESC 센서 / BLHeli 4way: 전 기체 포함
- **Alt Hold 진입 스틱 래치(Entry Stick Latch, v5)**: 전 기체 포함. Alt Hold 진입 순간 스로틀 스틱을 래치해 5%(PWM 50) 이상 움직이기 전까지 고도를 그대로 유지. CLI 항목 없음(코드 고정)
- **Alt Hold 해제 대기(Exit Hold, v6/v7)**: 전 기체 포함. Alt Hold 스위치를 끈 순간에도 즉시 해제하지 않고, 스로틀 스틱이 `ap_hover_throttle` ±5%(PWM 50) 안에 들어오거나(v7: 빠른 스틱 이동으로 그 구간을 가로질러도) 해제. 대기 중에는 OSD 비행모드에 "ALT WAIT"(경고색)를 표시하고, 자세는 앵글 그대로·스로틀을 내려도 하강하지 않음(설계된 동작). CLI 항목 없음(코드 고정)
- 제거(공통): VTX 제어, 트랜스폰더, 레인지파인더, SimonK, GPS 랩타이머·Plus Codes, 런치 컨트롤, **서보, 배터리-컨티뉴, 아날로그 OSD/MAX7456**(v4)

## 기체별 차이

- **자력계**: Pavo25 V2만 제외 (센서 미장착)
- **PINIO**: Mark4_6in, AOS_UL7_X8, Explorer LR4, X8_5INCH는 미사용이라 제거
- **LED 스트립**: F405 3종 + 8IN-KOPIS_X8 + Pavo25 V2는 핀/기능 없어 제거, 나머지(TJRC_10, AOS_UL7_X8, Explorer LR4, X8_5INCH)는 유지
- **수신기/텔레메트리**: 기체마다 실제 쓰는 프로토콜만 남김 — CRSF가 대부분, Mark4_6in만 SBUS(텔레메트리 없음), AOS_UL7_O4·X8_5INCH는 FPort+SmartPort

상세 근거·검증 내역은 [BUILD_OPTIONS.md](BUILD_OPTIONS.md), 플래시 절차·CLI 사용법은 [README.md](README.md) 참고.
