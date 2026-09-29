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
| STM32H743 | `betaflight_2025.12.5_STM32H743_X8_5INCH_custom_v3.hex` | X8 5INCH |

같은 MCU를 쓰는 기체는 동일한 hex 파일을 사용하며, 보드별 리소스 매핑은 각 기체의 CLI DIFF ALL(`BTFL_cli_*.txt`, 프로젝트 문서 참고)로 런타임에 적용됩니다.

> 커스텀 빌드는 Betaflight 공식 지원 대상이 아니며, 사용자 자체 책임 하에 시험·적용합니다.
