# Betaflight 2025.12.5 커스텀 빌드 옵션 (기체별, 보드별 빌드)

베이스: `jsungho/betaflight2025.12.5_custom` / 브랜치 `custom-patch/alt-hold-throttle-range`
커스텀 패치(4종): `alt_hold_full_low_is_max_descend`, `alt_hold_deadband_low`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only` (참고: betaflight/betaflight#15775)

이 저장소의 펌웨어는 **보드별(`make <보드이름>`)** 로 빌드해서, 그 보드/기체가 실제로 쓰지 않는 기능을 빼 플래시 사용량을 줄였다(이전에 있던 통합 타겟(MCU 단위) hex는 제거되었다).
결과 파일은 기체 이름이 들어간 hex(`..._custom_v3_slim.hex`)이며, **각 기체에 맞는 파일 하나만** 올려야 한다.

## 기체별 빌드옵션 표

기체 CLI(2025.12.5 diff all)에서 실제로 쓰는 기능만 남기고, 쓰지 않는 기능은 빌드에서 뺐다.

| 기체 | 보드 (빌드 타깃) | MCU | 자력계 | PINIO | LED 스트립 | 수신기 프로토콜 | 텔레메트리 | F722 추가로 켠 옵션 | Flash |
|---|---|---|---|---|---|---|---|---|---|
| MARIO5 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 (사용) | 제거 | **CRSF** | CRSF | 해당 없음 | 40.5% |
| AOS_UL7_O4 | SPEEDYBEEF405V4 | F405 | 포함 | 포함 (사용) | 제거 | **FPort** | SmartPort (FPort 텔레메트리) | 해당 없음 | 40.2% |
| Mark4_6in | JHEF405PRO | F405 | 포함 | **제거** | 제거 | **SBUS** | 없음 | 해당 없음 | 40.8% |
| TJRC_10 | MATEKF722SE | F722 | 포함 | 포함 (사용) | 포함 | CRSF | CRSF | ALT / GPS / POS | 81.1% |
| 8IN-KOPIS_X8 | SPEEDYBEEF7V3 | F722 | 포함 | 포함 (사용) | 제거 (1) | CRSF | CRSF | ALT / GPS / POS | 78.6% |
| CHIMERA7 | FLYWOOF722PROV2 | F722 | 포함 | 포함 (사용) | 제거 | CRSF | CRSF | ALT / GPS / POS | 76.5% |
| AOS_UL7_X8 | MATEKF722HD | F722 | 포함 | **제거** | 포함 | CRSF | CRSF | ALT / GPS / POS | 78.3% |
| Explorer LR4 | JHEF7DUAL | F722 | 포함 | **제거** | 포함 | CRSF | CRSF | ALT / GPS / POS | 79.2% |
| Pavo25 V2 | JHEF7DUAL | F722 | **제거** (센서 없음) | 포함 (사용) | 제거 (1) | **CRSF** | CRSF | ALT / GPS / POS | 75.5% |
| X8_5INCH | MATEKH743 | H743 | 포함 | **제거** | 포함 | **FPort** | SmartPort (FPort 텔레메트리) | 해당 없음 | 25.0% |

- 자력계 = `USE_MAG` (드라이버 자동 포함). 보드 config 빌드는 이 옵션을 자동으로 켜지 않으므로 `build_custom.sh`가 Pavo25 V2를 제외한 모든 기체에 `-DUSE_MAG`를 명시한다.
- ALT = `USE_ALTITUDE_HOLD`, GPS = `USE_GPS`, POS = `USE_POSITION_HOLD`
- **모든 기체에서 Alt Hold / Position Hold / GPS·GPS Rescue, OSD(MSP DisplayPort), 블랙박스, 커스텀 파라미터 4종은 포함된다.**
- **Pavo25 V2만 자력계(USE_MAG) 제외** — 사용자 확인: 이 기체는 자력계 센서 자체가 없음.

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

## 빌드 방법

```bash
# 사전 준비
git submodule update --init --depth 1 src/config

# 전체 10개 기체
custom-patch/build_custom.sh

# 특정 보드 또는 기체 라벨만
custom-patch/build_custom.sh MATEKF722SE JHEF7DUAL
```

결과는 `custom-patch/firmware/2025.12.5/`에 `betaflight_2025.12.5_<MCU>_<보드>_<기체>_custom_v3_slim.hex` 형식으로 생성된다.

## 검증한 내용

- 전 기체 빌드/링크 성공 (플래시 오버플로 없음, 25~81% 사용)
- 4개 커스텀 CLI 파라미터(`alt_hold_deadband_low`, `alt_hold_full_low_is_max_descend`, `alt_hold_hover_throttle`, `landing_disarm_airmode_off_only`) 문자열이 전 기체 바이너리에 포함됨을 확인
- `altHoldInit`, `updatePosHold` 심볼로 Alt Hold / Position Hold가 전 기체(F405/F722/H743)에 실제로 링크됨을 확인
- 자력계 심볼(`compassConfig` 등) 존재 여부로 Pavo25 V2만 자력계가 빠졌고 나머지는 포함됨을 확인 (Explorer LR4: 5개 심볼 존재 vs Pavo25 V2 재현 빌드: 0개)

## 사용상 주의

- 보드가 다르면 잘못된 hex다. 같은 MCU라도 보드별 파일이 다르므로 기체와 파일명을 확인한 뒤 플래시한다.
- 플래시 후 기존 CLI diff(프로젝트 문서 보관분)를 재적용해야 한다.
- 컴파일·링크·심볼 확인까지만 했고 기체 부팅과 비행은 검증하지 않았다. 프롭 제거 벤치 테스트를 먼저 한다.
