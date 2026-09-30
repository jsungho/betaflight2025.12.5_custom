# 기체별 업로드 기능 정리표 (2025.12.5 보드별 슬림 빌드)

대상 hex: [`firmware/2025.12.5/`](firmware/2025.12.5/) (10개, 보드별 빌드)
기존 통합 타겟(MCU 단위) hex는 [`../firmware/v3/`](../firmware/v3/)에 그대로 남아 있다 — 이 표는 새 보드별 빌드 기준.

| 기체 | 보드(FC) | MCU | 자력계 | PINIO | LED 스트립 | 수신기 | 텔레메트리 | Alt/Pos Hold | Flash |
|---|---|---|---|---|---|---|---|---|---|
| MARIO5 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 40.5% |
| AOS_UL7_O4 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 | 제거 | FPort | SmartPort | 포함 | 40.2% |
| Mark4_6in | JHEF405PRO | F405 | 포함 | 제거 | 제거 | SBUS | 없음 | 포함 | 40.8% |
| TJRC_10 | MATEKF722SE | F722 | 포함 | 포함 | 포함 | CRSF | CRSF | 포함 | 81.1% |
| 8IN-KOPIS_X8 | SPEEDYBEEF7V3 | F722 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 78.6% |
| CHIMERA7 | FLYWOOF722PROV2 | F722 | 포함 | 포함 | 제거 | CRSF | CRSF | 포함 | 76.5% |
| AOS_UL7_X8 | MATEKF722HD | F722 | 포함 | 제거 | 포함 | CRSF | CRSF | 포함 | 78.3% |
| Explorer LR4 | JHEF7DUAL | F722 | 포함 | 제거 | 포함 | CRSF | CRSF | 포함 | 79.2% |
| **Pavo25 V2** | JHEF7DUAL | F722 | **제거 (센서 없음)** | 포함 | 제거 | CRSF | CRSF | 포함 | 75.5% |
| X8_5INCH | MATEKH743 | H743 | 포함 | 제거 | 포함 | FPort | SmartPort | 포함 | 25.0% |

## 공통 사항 (전 기체)

- 커스텀 CLI 4종 전부 포함: `alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only`
- Alt Hold / Position Hold / GPS·GPS Rescue / MSP DisplayPort OSD / 블랙박스 / ESC 센서 / BLHeli 4way: 전 기체 포함
- 제거(공통): VTX 제어, 트랜스폰더, 레인지파인더, SimonK, GPS 랩타이머·Plus Codes, 런치 컨트롤

## 기체별 차이

- **자력계**: Pavo25 V2만 제외 (센서 미장착)
- **PINIO**: Mark4_6in, AOS_UL7_X8, Explorer LR4, X8_5INCH는 미사용이라 제거
- **LED 스트립**: F405 3종 + 8IN-KOPIS_X8 + Pavo25 V2는 핀/기능 없어 제거, 나머지(TJRC_10, AOS_UL7_X8, Explorer LR4, X8_5INCH)는 유지
- **수신기/텔레메트리**: 기체마다 실제 쓰는 프로토콜만 남김 — CRSF가 대부분, Mark4_6in만 SBUS(텔레메트리 없음), AOS_UL7_O4·X8_5INCH는 FPort+SmartPort

상세 근거·검증 내역은 [BUILD_OPTIONS.md](BUILD_OPTIONS.md), 플래시 절차·CLI 사용법은 [README.md](README.md) 참고.
