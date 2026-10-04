#!/usr/bin/env bash
# Betaflight 2025.12.5 custom (alt-hold-throttle-range / landing_disarm_airmode_off_only)
# 기체(보드)별 커스텀 HEX 빌드 스크립트 - 통합 타겟(MCU 단위) 대신 보드별 빌드로 플래시 용량을 줄인다.
#
# 사용법:  custom-patch/build_custom.sh [보드이름 또는 기체라벨 ...]   (인자 없으면 전체 10개 기체)
# 결과:    custom-patch/out/ 아래에 기체별 이름의 .hex 생성 (OUT 환경변수로 변경 가능)
#
# 사전 준비:
#   - arm-none-eabi-gcc 설치 (apt 13.2.1 사용 시 mk/local.mk 에 GCC_REQUIRED_VERSION := 13.2.1)
#   - git submodule update --init --depth 1 src/config   (보드별 config 필요)
set -euo pipefail
cd "$(dirname "$0")/.."

OUT="${OUT:-$PWD/custom-patch/firmware/2025.12.5}"
VER="2025.12.5"
SUFFIX="custom_v15_slim"

# ---- 모든 기체 공통으로 뺄 기능 (CLI에서 사용하지 않음, VTX 는 사용자 지시) -------------------------------
# -DCUSTOM_NO_xxx 는 src/main/target/common_post.h 끝의 custom-patch 블록이 처리한다.
COMMON="-DCUSTOM_NO_VTX -DCUSTOM_NO_TRANSPONDER -DCUSTOM_NO_RANGEFINDER -DCUSTOM_NO_DASHBOARD"
COMMON="$COMMON -DCUSTOM_NO_SIMONK -DCUSTOM_NO_GPS_EXTRAS -DCUSTOM_NO_LAUNCH_CONTROL"
COMMON="$COMMON -DCUSTOM_NO_RX_OTHER -DCUSTOM_NO_TELEM_OTHER"
# v4: 전 기체 서보(USE_SERVOS)/배터리-컨티뉴(USE_BATTERY_CONTINUE) 미사용 - 사용자 지시로 제거.
# OSD는 디지털(MSP DisplayPort, Walksnail 등)만 쓰므로 -DUSE_OSD_HD 를 명시해 아날로그(USE_OSD_SD, MAX7456)를
# common_pre.h 의 "둘 다 안 정해지면 둘 다 켠다" 분기에서 원천적으로 제외한다(USE_MAX7456도 함께 자동 제외됨).
COMMON="$COMMON -DCUSTOM_NO_SERVOS -DCUSTOM_NO_BATTERY_CONTINUE -DUSE_OSD_HD"
# 보드 config 로 빌드하면 USE_MAG 가 자동으로 정의되지 않는다(common_pre.h 의 !USE_CONFIG 블록에서만 정의).
# 전 기체 CLI 에 mag_calibration / align_mag 설정이 있으므로 자력계를 명시적으로 켠다(드라이버는 자동 포함).
# 자력계가 없는 기체(NO_MAG_LABELS)는 루프에서 -DUSE_MAG 를 붙이지 않는다.
NO_MAG_LABELS="PAVO25V2"

# ---- 기체 종류별 조합 ---------------------------------------------------------------------------------
# 수신기: CRSF 기본. SBUS / FPort / SmartPort 텔레메트리는 필요한 기체만 남긴다.
NO_LEGACY_RX="-DCUSTOM_NO_RX_SBUS -DCUSTOM_NO_RX_FPORT -DCUSTOM_NO_TELEM_SMARTPORT"

# 2025.12.5 업스트림도 플래시 1MB 미만(F722=512KB) 보드별 빌드에서는 Alt Hold / Position Hold / GPS / LED 스트립을
# 컴파일에서 제외한다(common_pre.h: TARGET_FLASH_SIZE >= 1024 블록, USE_CONFIG 경로에서는 자동 상속되지 않음).
# 통합(MCU 단위) 빌드에서는 이 값이 충분히 커서 이미 포함되어 있었지만, 보드별 빌드로 바꾸면 다시 켜줘야 한다.
F722_ON="-DUSE_ALTITUDE_HOLD -DUSE_GPS -DUSE_POSITION_HOLD"
LED_ON="-DUSE_LED_STRIP"            # F722 에서 LED 스트립을 쓰는 기체만
LED_OFF="-DCUSTOM_NO_LED_STRIP"     # F405 등 기본 포함인 MCU 에서 LED 스트립을 안 쓰는 기체
NO_PINIO="-DCUSTOM_NO_PINIO"        # PINIO(resource PINIO / pinio_box)를 쓰지 않는 기체

# 형식: 보드이름|MCU|기체 라벨|EXTRA_FLAGS
TARGETS=(
  # 같은 보드(SPEEDYBEEF405V4)를 쓰는 두 기체를 수신기/기능별로 분리해서 빌드한다
  "SPEEDYBEEF405V4|STM32F405|MARIO5|$COMMON $LED_OFF $NO_LEGACY_RX"
  "SPEEDYBEEF405V4|STM32F405|AOSUL7O4|$COMMON $LED_OFF -DCUSTOM_NO_RX_CRSF -DCUSTOM_NO_RX_SBUS"
  "JHEF405PRO|STM32F405|MARK4_6IN|$COMMON $LED_OFF $NO_PINIO -DCUSTOM_NO_RX_CRSF -DCUSTOM_NO_RX_FPORT -DCUSTOM_NO_TELEM_SMARTPORT"
  "MATEKF722SE|STM32F7X2|TJRC10|$COMMON $F722_ON $LED_ON $NO_LEGACY_RX"
  "SPEEDYBEEF7V3|STM32F7X2|8INKOPISX8|$COMMON $F722_ON $NO_LEGACY_RX"
  "FLYWOOF722PROV2|STM32F7X2|CHIMERA7|$COMMON $F722_ON $NO_LEGACY_RX"
  "MATEKF722HD|STM32F7X2|AOSUL7X8|$COMMON $F722_ON $LED_ON $NO_PINIO $NO_LEGACY_RX"
  "JHEF7DUAL|STM32F7X2|EXPLORERLR4|$COMMON $F722_ON $LED_ON $NO_PINIO $NO_LEGACY_RX"
  # Pavo25 V2: 수신기 CRSF, PINIO 사용, LED 스트립 핀 없음(제거), 자력계 없음
  "JHEF7DUAL|STM32F7X2|PAVO25V2|$COMMON $F722_ON $NO_LEGACY_RX"
  "MATEKH743|STM32H743|X8_5INCH|$COMMON $NO_PINIO -DCUSTOM_NO_RX_CRSF -DCUSTOM_NO_RX_SBUS"
)

mkdir -p "$OUT"
WANT=("$@")

for entry in "${TARGETS[@]}"; do
  IFS='|' read -r board mcu label flags <<<"$entry"
  if [ ${#WANT[@]} -gt 0 ]; then
    match=0; for w in "${WANT[@]}"; do [ "$w" = "$board" ] || [ "$w" = "$label" ] && match=1; done
    [ $match -eq 1 ] || continue
  fi

  case " $NO_MAG_LABELS " in *" $label "*) ;; *) flags="$flags -DUSE_MAG" ;; esac
  echo "==> $board ($label)"
  make "clean_$board" >/dev/null 2>&1 || rm -rf "obj/main/$board" "obj/betaflight_${VER}_${mcu}_${board}".*
  make "$board" EXTRA_FLAGS="$flags" -j"$(nproc)"

  src="obj/betaflight_${VER}_${mcu}_${board}.hex"
  dst="$OUT/betaflight_${VER}_${mcu}_${board}_${label}_${SUFFIX}.hex"
  cp "$src" "$dst"
  echo "    -> $dst"
done

echo "done. md5:"
( cd "$OUT" && md5sum ./*.hex )
