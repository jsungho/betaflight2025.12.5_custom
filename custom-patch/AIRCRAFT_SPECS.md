# 기체별 최종 CLI 반영 내역 (실기체 diff all 백업 기준)

기준일: 2026-10-03 (백업 파일 시각 10-02 20:46 ~ 10-03 00:27 KST)
출처: 사용자가 업로드한 기체별 `diff all` 백업 9건. Mark4_6in은 미제출.
기능 매트릭스(빌드 포함 기능)는 [FEATURE_MATRIX.md](FEATURE_MATRIX.md) 참고.

## 0. 먼저 확인할 사항 (불일치)

| # | 내용 | 영향 |
|---|------|------|
| 1 | **X8_5INCH 백업은 펌웨어 2026.6.2** (MSP API 1.48, 빌드 09-30). 파일명은 2025.12.5지만 실제 기체는 2026.6.2 | 2025.12.5 v10 hex가 아닌 별도 대화(2026.6.2) 쪽 빌드가 올라가 있음. `landing_disarm_threshold 60`, `trust_mag ON`, `max_check 1950`도 이 때문 |
| 2 | **Pavo25 V2 백업은 펌웨어 4.5.5** (MSP API 1.46) | 커스텀 기능(alt_hold, landing_disarm, ap_*, 자력계) 없음. 구형 `gps_rescue_throttle_*` 파라미터 사용 |
| 3 | **Mark4_6in 백업 미제출** | 아래 표는 이전(09-25) 데이터 유지, "CLI 미제출"로 표시 |
| 4 | **자력계 설정이 권장 CLI와 다름**: AOS_UL7_O4, AOS_UL7_X8, EXPLORER_LR4, 8IN-KOPIS_X8, MARIO5, TJRC_10 6기는 `mag_hardware = NONE`, `gps_rescue_use_mag = OFF`, `pos_hold_without_mag = ON` | 권장 CLI(`gps_rescue_use_mag ON`, `pos_hold_without_mag OFF`)와 반대. FEATURE_MATRIX의 "자력계 포함"은 빌드에 드라이버가 들어있다는 뜻이며 실제 설정은 비활성 |
| 5 | CHIMERA7, X8_5INCH는 mag 관련 줄이 없음(기본값 사용) | X8_5INCH는 `trust_mag ON` |

## 1. 공통 값 (2025.12.5 커스텀, 7기 공통)

대상: AOS_UL7_O4, AOS_UL7_X8, CHIMERA7, EXPLORER_LR4, 8IN-KOPIS_X8, MARIO5, TJRC_10

```
mixer_type = EZLANDING          gps_ublox_flight_model = AIRBORNE_1G
ez_landing_limit = 10           ez_landing_threshold = 30
landing_disarm_threshold = 45   landing_disarm_airmode_off_only = ON
gps_rescue_descend_rate = 135   gps_rescue_disarm_threshold = 60
alt_hold_deadband = 25          alt_hold_climb_rate = 70
alt_hold_deadband_low = 0       alt_hold_full_low_is_max_descend = ON
alt_hold_hover_throttle = 1400
```

(권장 CLI와 일치. 단 `gps_rescue_use_mag`, `pos_hold_without_mag`는 위 4번 참고.)

## 2. 기체별 최종 값

| 기체 | 보드 | 펌웨어 | 믹서 | motor_kv | ap_hover_throttle | ap_throttle_min | ap_altitude_d | failsafe_throttle | 기타 |
|------|------|--------|------|---------|------------------|----------------|--------------|------------------|------|
| MARIO5 | SPEEDYBEEF405V4 | 2025.12.5 | 기본 | 2050 | 1300 | 1200 | 12 | 1280 | `ap_max_angle 45`, 프로파일 5S_PID95 / 6S_PID78 (각 landing_disarm 45) |
| AOS_UL7_O4 | SPEEDYBEEF405V4 | 2025.12.5 | 기본 | 1350 | 1300 | 1200 | 12 | 1250 | ESC_SENSOR, UART1=디지털VTX(131073), UART6=GPS |
| AOS_UL7_X8 | MATEKF722HD | 2025.12.5 | OCTOX8 | 1500 | 1300 | 1200 | (기본) | 1165 | `max_check 2000`, UART1=RX, UART2=GPS, UART5=디지털VTX |
| CHIMERA7 | FLYWOOF722PROV2 | 2025.12.5 | 기본 | 1300 | 1400 | 1250 | 12 | 1210 | `max_check 2000`, `gps_rescue_velocity_p 10` |
| EXPLORER_LR4 | JHEF7DUAL | 2025.12.5 | 기본 | - | **미설정** | 1170 | (기본) | 1200 | 프로파일 4S_PID95 / 5S_PID76 / 6S_PID63 (각 landing_disarm 45) |
| 8IN-KOPIS_X8 | SPEEDYBEEF7V3 | 2025.12.5 | OCTOX8 | 1150 | 1370 | 1230 | 13 | 1210 | `ap_throttle_max 1750`, `gps_rescue_min_start_dist 20`, UART3/4=ESC_SENSOR |
| TJRC_10 | MATEKF722SE | 2025.12.5 | 기본 | 900 | 1360 | 1230 | 12 | 1250 | |
| X8_5INCH | MATEKH743 | **2026.6.2** (!) | OCTOX8 | 1850 | 1300 | 1150 | 12 | 1200 | `ap_max_angle 45`, `max_check 1950`, `trust_mag ON`, `landing_disarm_threshold 60`, UART3/8=ESC_SENSOR, 배터리 프로파일 Li-Po/Li-Ion/LiHV |
| Pavo25 V2 | JHEF7DUAL | **4.5.5** (!) | 기본 | - | 없음 | 없음 | 없음 | 1250 | 구형 `gps_rescue_throttle_min 1250 / hover 1350 / d 18`, `landing_alt 3`, `max_angle 40`, `ez_landing 30/10`, 프로파일 4S_PID00 / 5S_PID80 |
| Mark4_6in | JHEF405PRO | - | - | - | - | - | - | - | **CLI 미제출 - 이전(09-25) 값 유지** |

## 3. 자력계 설정 (백업 기준)

| 기체 | mag_hardware | gps_rescue_use_mag | pos_hold_without_mag | align_mag | mag_align (r/p/y) | mag_calibration |
|------|-------------|-------------------|---------------------|-----------|-------------------|-----------------|
| AOS_UL7_O4 | NONE | OFF | ON | CW180FLIP | 0/1800/1800 | -16,798,-119 |
| AOS_UL7_X8 | NONE | OFF | ON | CUSTOM | 0/1800/0 | -569,241,-225 |
| CHIMERA7 | (기본) | (기본) | (기본) | - | - | - |
| EXPLORER_LR4 | NONE | OFF | ON | - | - | - |
| 8IN-KOPIS_X8 | NONE | OFF | ON | CW0FLIP | 0/1800/0 | -192,-711,86 |
| MARIO5 | NONE | OFF | ON | - | - | - |
| TJRC_10 | NONE | OFF | ON | - | - | - |
| X8_5INCH | (기본, trust_mag ON) | (기본) | (기본) | CW90 | 0/0/900 | 410,282,141 |
| Pavo25 V2 | 없음 (4.5.5, 센서 제거) | - | - | - | - | - |

`mag_declination = -90` (-9.0도)은 확인된 기체 공통.

## 4. 프로파일 / 하드웨어 참고

- AOS_UL7_X8: `rxfail 3 s 1325`, 프로파일 6S_PID90, `dyn_notch 80-450`
- 8IN-KOPIS_X8: `dshot_bidir ON`, `motor_output_reordering 3,2,6,7,4,5,1,0`, `rxfail 3 s 1400`
- AOS_UL7_O4: `motor_output_reordering 3,2,1,0,4,5,6,7`, `yaw_motors_reversed ON`
- X8_5INCH: `serialrx_inverted ON`, `align_board_roll 180`, `motor_output_reordering 7,6,5,4,3,2,1,0`
- Pavo25 V2: `align_board_roll 180`, `yaw_motors_reversed ON`, `motor_output_reordering 2,3,0,1,4,5,6,7`, gyro 2개 정렬 설정

## 5. 확인 필요

1. X8_5INCH를 2025.12.5 v10 hex로 다시 올릴지, 2026.6.2 유지인지.
2. Pavo25 V2는 4.5.5 유지인지(커스텀 hex 미적용).
3. 6기의 `mag_hardware NONE` / `gps_rescue_use_mag OFF` / `pos_hold_without_mag ON`이 의도된 운용 값인지, 권장 CLI(ON/OFF)로 맞출지.
4. Mark4_6in `diff all` 제출.
