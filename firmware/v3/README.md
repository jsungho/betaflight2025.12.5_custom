# v3 커스텀 펌웨어 (.hex) — 기체별 빌드

기준 소스: `custom-patch/alt-hold-throttle-range` 브랜치 (base tag `2025.12.5`)

포함된 커스텀 CLI 파라미터:
- `alt_hold_full_low_is_max_descend` (v1)
- `alt_hold_deadband_low` (v1)
- `alt_hold_hover_throttle` (v2)
- `landing_disarm_airmode_off_only` (v3)

## MCU별 파일 매핑

| MCU | 파일 | 대상 기체 |
|---|---|---|
| STM32F405 | `betaflight_2025.12.5_STM32F405_MARIO5_custom_v3.hex` | MARIO5 |
| STM32F405 | `betaflight_2025.12.5_STM32F405_MARK4_6IN_custom_v3.hex` | MARK4 6IN |
| STM32F405 | `betaflight_2025.12.5_STM32F405_AOSUL7O4_custom_v3.hex` | AOS UL7 O4 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_TJRC10_custom_v3.hex` | TJRC 10 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_8INKOPISX8_custom_v3.hex` | 8IN KOPIS X8 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_CHIMERA7_custom_v3.hex` | CHIMERA7 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_AOSUL7X8_custom_v3.hex` | AOS UL7 X8 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_EXPLORERLR4_custom_v3.hex` | EXPLORER LR4 |
| STM32F7X2 | `betaflight_2025.12.5_STM32F7X2_PAVO25V2_custom_v3.hex` | Pavo25 V2 (JHEF7DUAL, 듀얼자이로) |
| STM32H743 | `betaflight_2025.12.5_STM32H743_X8_5INCH_custom_v3.hex` | X8 5INCH |

같은 MCU를 쓰는 기체는 동일한 hex 파일을 사용하며, 보드별 리소스 매핑은 각 기체의 CLI DIFF ALL(`BTFL_cli_*.txt`, 프로젝트 문서 참고)로 런타임에 적용됩니다.

> 커스텀 빌드는 Betaflight 공식 지원 대상이 아니며, 사용자 자체 책임 하에 시험·적용합니다.

---

## 커스텀 CLI 사용법

### 1) `alt_hold_full_low_is_max_descend` (OFF/ON, 기본 OFF) — v1

Alt Hold/Position Hold 중 스로틀 스틱이 `min_check` 미만(THROTTLE_LOW 구간)일 때의 동작을 결정합니다.

- **OFF (기본)**: 스틱 위치와 무관하게 강제 호버 유지 (공식 Betaflight와 동일)
- **ON**: THROTTLE_LOW 상태를 "최대 하강률" 커맨드로 해석 — 스틱을 끝까지 내리면 최대 속도로 하강

```
set alt_hold_full_low_is_max_descend = ON
save
```

⚠️ 반드시 **프롭 제거 벤치 테스트 선행** 후 비행 시험할 것.

### 2) `alt_hold_deadband_low` (0~70, 기본 20) — v1

기존 `alt_hold_deadband`는 상승(HIGH)측 데드밴드만 제어했는데, 이 파라미터로 하강(LOW)측 데드밴드를 독립적으로 설정할 수 있습니다.

```
set alt_hold_deadband_low = 10
save
```

값이 작을수록 스틱을 조금만 내려도 하강이 시작됩니다.

### 3) `alt_hold_hover_throttle` (0 또는 1100~1700, 기본 0) — v2

Alt Hold/Position Hold가 사용하는 호버 기준 스로틀 값을 GPS Rescue와 독립적으로 지정합니다.

- **0 (기본)**: 기존과 동일하게 `ap_hover_throttle` 값을 그대로 상속 (동작 변화 없음)
- **1100~1700**: 지정한 값을 Alt Hold/Position Hold 전용 호버 기준값으로 사용

```
set alt_hold_hover_throttle = 1450
save
```

처음엔 `0`(상속)으로 두고 검증한 뒤, 필요 시 기체의 실제 호버 스로틀 값으로 조정 권장.

### 4) `landing_disarm_airmode_off_only` (OFF/ON, 기본 OFF) — v3

기존 `landing_disarm_threshold`(EZ Disarm, 착지 충격 감지 시 자동 디스암)는 airmode 상태와 무관하게 항상 작동합니다. 이 파라미터는 EZ Disarm이 **airmode가 꺼져 있을 때만** 작동하도록 제한합니다.

- **OFF (기본)**: 공식 Betaflight와 100% 동일 — airmode 상태 무관하게 EZ Disarm 작동
- **ON**: airmode가 켜져 있으면 EZ Disarm 완전 비활성화. airmode를 꺼야만 (예: 착륙을 위해 AUX 스위치로 airmode OFF) EZ Disarm이 작동

```
set landing_disarm_threshold = 50            # 착지 충격 판정 임계값 (0=비활성, 안전값은 대략 100 부근)
set landing_disarm_airmode_off_only = ON     # airmode ON일 땐 비활성, OFF(착륙 시)에만 작동
save
```

**AUX 스위치로 airmode를 켜고 끄며 비행하고, 착륙 시에만 airmode를 끄는 운용 습관**을 가진 조종자에게 유용합니다 — 평상시 비행 중 급격한 저크로 인한 오작동 디스암 위험을 없애면서, 착륙 순간에는 자동 디스암 편의를 얻을 수 있습니다.

⚠️ **페일세이프/GPS Rescue 관련 주의**: 본 기체들은 페일세이프 시 airmode용 AUX 채널이 `rxfail`로 airmode ON 범위 값으로 강제 고정되어, `isAirmodeEnabled()`가 `true`로 평가됩니다. 즉 `ON`으로 설정하면 **페일세이프/GPS Rescue 자동복귀·착륙 중에도 EZ Disarm이 함께 비활성화**됩니다. 이는 자동복귀 중 거친 기동으로 인한 오작동 디스암을 막기 위해 의도적으로 선택한 동작이며, 별도의 "페일세이프 중에는 예외적으로 작동" 옵션은 추가하지 않았습니다.

PROFILE_VALUE로 등록되어 있어 PID 프로파일별로 다르게 설정할 수 있습니다 (예: 프로파일1=OFF, 프로파일2=ON).

---

## 시험 순서 권장

1. 프롭 제거 벤치 테스트: 각 파라미터를 하나씩 켜고 응답 확인
2. `landing_disarm_airmode_off_only = ON` 시험 시: 평상시 비행(airmode ON) 중 오작동 디스암이 없는지, 착륙 시(airmode OFF) 원하는 임계값에서 정상 디스암되는지 순서대로 확인
3. 블랙박스 로깅 필수 — v1~v3 신규 파라미터 4개 모두 블랙박스 헤더에 기록됨
4. 개활지·저고도부터 단계적으로 비행 시험
5. 문제 시 즉시 원복할 수 있도록 기존 공식 2025.12.5 `.hex` 백업 유지
