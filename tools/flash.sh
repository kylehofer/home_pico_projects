#!/usr/bin/env sh
# Build and flash one of the sub-projects in projects/ onto a Pico.
#
# Put the Pico in BOOTSEL mode, or let picotool reboot it over USB if the
# firmware already running on it has stdio_usb enabled.
#
#   tools/flash.sh --list              # show the available projects
#   tools/flash.sh garage_door         # build + flash projects/garage_door over USB
#   tools/flash.sh -b garden_shed      # build only, don't touch the device
#   tools/flash.sh --ota 10.0.0.5 garage_door   # build + push over WiFi
#
# When the OTA bootloader is built (PICO_OTA, on by default), the image flashed
# over USB is the combined bootloader + application, and the application on its
# own is deliberately not bootable: it is linked to run from 0x1005b000 with no
# second stage of its own, so only the combined image is ever written here.
#
# --ota sends the application to a device already running the bootloader, which
# it enters either on the 'enterOta' Sparkplug metric or with GPIO15 pulled low
# at boot. It needs serial-flash on PATH:
#
#   go install github.com/usedbytes/serial-flash@latest
#
# The build tree defaults to ./build, which is the same one the VS Code
# Pico/CMake extensions use, so this shares its cache (and its build type)
# rather than recompiling the world. Override with -d/--build-dir or BUILD_DIR.
#
# The WiFi/broker settings are baked into the firmware at configure time and
# cached, so they only need to be in the environment the first time a build
# tree is configured:
#
#   WIFI_SSID=... WIFI_PASSWORD=... BROKER_ADDRESS=tcp://host:1883 \
#     NTP_ADDRESS=host tools/flash.sh garage_door
#
# The board defaults to pico_w and can be changed with --board/PICO_BOARD, but
# note that the root CMakeLists.txt calls pico_sdk_init() before it does
# `set(PICO_BOARD pico_w)`, so for anything other than pico_w the SDK and the
# project sources end up disagreeing about the board until that is untangled.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECTS_DIR="$ROOT/projects"

# Keep the toolchain versions in step with the root CMakeLists.txt.
cmake_var() {
    sed -n "s/^[[:space:]]*set([[:space:]]*$1[[:space:]]\+\([^) ]*\).*/\1/p" "$ROOT/CMakeLists.txt" | head -n1
}
SDK_VERSION="$(cmake_var sdkVersion)"
TOOLCHAIN_VERSION="$(cmake_var toolchainVersion)"
PICOTOOL_VERSION="$(cmake_var picotoolVersion)"

PICO_INSTALL="$HOME/.pico-sdk"
: "${PICO_SDK_PATH:=$PICO_INSTALL/sdk/$SDK_VERSION}"
# The ARM toolchain is not on PATH on this machine; point the SDK at it
# directly so this works from a bare shell.
: "${PICO_TOOLCHAIN_PATH:=$PICO_INSTALL/toolchain/$TOOLCHAIN_VERSION}"
export PICO_SDK_PATH PICO_TOOLCHAIN_PATH
PATH="$PICO_TOOLCHAIN_PATH/bin:$PICO_INSTALL/picotool/$PICOTOOL_VERSION/picotool:$PICO_INSTALL/cmake/v3.31.5/bin:$PICO_INSTALL/ninja/v1.12.1:$PATH"
export PATH

die() { printf 'flash.sh: %s\n' "$*" >&2; exit 1; }

# Every projects/<name>/CMakeLists.txt that declares an executable. Prints
# "<name> <target> <enabled|disabled>", where disabled means projects/
# CMakeLists.txt does not add_subdirectory() it, so it is not part of a build.
each_project() {
    for dir in "$PROJECTS_DIR"/*/; do
        name="$(basename "$dir")"
        [ -f "$dir/CMakeLists.txt" ] || continue
        target="$(sed -n 's/^[[:space:]]*add_executable([[:space:]]*\([A-Za-z0-9_]\+\).*/\1/p' "$dir/CMakeLists.txt" | head -n1)"
        [ -n "$target" ] || continue
        if grep -qE "^[[:space:]]*add_subdirectory\([[:space:]]*\.?/?$name[[:space:]]*\)" "$PROJECTS_DIR/CMakeLists.txt"; then
            printf '%s %s enabled\n' "$name" "$target"
        else
            printf '%s %s disabled\n' "$name" "$target"
        fi
    done
}

list_projects() {
    printf 'Projects in %s:\n' "$PROJECTS_DIR"
    each_project | while read -r name target state; do
        if [ "$state" = enabled ]; then
            printf '  %-16s -> %s\n' "$name" "$target"
        else
            printf '  %-16s -> %s  (not in projects/CMakeLists.txt)\n' "$name" "$target"
        fi
    done
}

usage() {
    cat <<USAGE
usage: tools/flash.sh [options] <project>

  -l, --list             list the available projects and exit
  -b, --build-only       configure and build, but do not flash
      --ota HOST         send the build to a device in the bootloader over WiFi
                         instead of flashing over USB
      --board BOARD      PICO_BOARD to build for (default: \$PICO_BOARD or pico_w)
  -d, --build-dir DIR    build tree to use (default: \$BUILD_DIR or $ROOT/build)
  -h, --help             this message
USAGE
}

BUILD="${BUILD_DIR:-$ROOT/build}"
BUILD_ONLY=0
OTA_HOST=""
PROJECT=""
BOARD="${PICO_BOARD:-pico_w}"
BOARD_GIVEN=0
[ -n "${PICO_BOARD:-}" ] && BOARD_GIVEN=1

while [ $# -gt 0 ]; do
    case "$1" in
        -l|--list) list_projects; exit 0 ;;
        -b|--build-only) BUILD_ONLY=1 ;;
        -d|--build-dir) [ $# -ge 2 ] || die "$1 needs a directory"; BUILD="$2"; shift ;;
        --board) [ $# -ge 2 ] || die "$1 needs a board name"; BOARD="$2"; BOARD_GIVEN=1; shift ;;
        --ota) [ $# -ge 2 ] || die "$1 needs a host name or address"; OTA_HOST="$2"; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; die "unknown option: $1" ;;
        *) [ -z "$PROJECT" ] || die "only one project at a time (got '$PROJECT' and '$1')"; PROJECT="$1" ;;
    esac
    shift
done

if [ -n "$OTA_HOST" ] && [ "$BUILD_ONLY" -eq 1 ]; then
    die "--ota and --build-only are mutually exclusive"
fi

if [ -z "$PROJECT" ]; then
    usage >&2
    printf '\n' >&2
    list_projects >&2
    exit 2
fi

# Accept either the directory name (garage_door) or the CMake target
# (pico_garage_door).
TARGET=""
STATE=""
for candidate in name target; do
    [ -z "$TARGET" ] || break
    while read -r name target state; do
        case "$candidate" in
            name) [ "$name" = "$PROJECT" ] || continue ;;
            target) [ "$target" = "$PROJECT" ] || continue ;;
        esac
        PROJECT="$name"; TARGET="$target"; STATE="$state"
        break
    done <<EOF
$(each_project)
EOF
done

if [ -z "$TARGET" ]; then
    list_projects >&2
    printf '\n' >&2
    die "no such project: $PROJECT"
fi
if [ "$STATE" = disabled ]; then
    die "$PROJECT is not built: add 'add_subdirectory($PROJECT)' to projects/CMakeLists.txt"
fi

command -v cmake >/dev/null 2>&1 || die "cmake not found on PATH"
[ -d "$PICO_SDK_PATH" ] || die "PICO_SDK_PATH does not exist: $PICO_SDK_PATH"

if [ "$BOARD" != pico_w ]; then
    printf 'flash.sh: warning: building for %s, but the root CMakeLists.txt forces\n' "$BOARD" >&2
    printf 'flash.sh:          PICO_BOARD=pico_w for the project sources.\n' >&2
fi

CONFIGURE_ARGS=""
# pico_sdk_init() runs before the root CMakeLists.txt sets PICO_BOARD, so the
# SDK reads it out of the cache and silently falls back to a plain 'pico'
# unless it is passed in here.
if [ "$BOARD_GIVEN" -eq 1 ] || [ ! -f "$BUILD/CMakeCache.txt" ]; then
    CONFIGURE_ARGS="-DPICO_BOARD=$BOARD"
fi

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    # Fresh tree: pick a build type and bake in whatever settings are in the
    # environment. On later runs these all come back out of the CMake cache,
    # and CMAKE_BUILD_TYPE is left alone so this does not fight the IDE.
    CONFIGURE_ARGS="$CONFIGURE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release}"
    # Match the generator the IDE uses, so a tree made here behaves the same.
    command -v ninja >/dev/null 2>&1 && CONFIGURE_ARGS="$CONFIGURE_ARGS -G Ninja"
    for var in WIFI_SSID WIFI_PASSWORD BROKER_ADDRESS NTP_ADDRESS; do
        eval "value=\${$var:-}"
        [ -n "$value" ] && CONFIGURE_ARGS="$CONFIGURE_ARGS -D$var=$value"
    done
    if [ -z "${WIFI_SSID:-}" ]; then
        printf 'flash.sh: warning: WIFI_SSID is not set and %s is a fresh build\n' "$BUILD" >&2
        printf 'flash.sh:          tree, so the firmware will have no WiFi credentials.\n' >&2
    fi
fi

FRESH=0
[ -f "$BUILD/CMakeCache.txt" ] || FRESH=1

# shellcheck disable=SC2086
cmake -S "$ROOT" -B "$BUILD" $CONFIGURE_ARGS
if [ "$FRESH" -eq 1 ]; then
    # The root CMakeLists.txt appends to CMAKE_CXX_FLAGS before the Pico
    # toolchain has populated it, so on the very first configure of a tree the
    # C++ flags come out as just "-Wall -Wextra" and every .cpp fails to
    # assemble ("selected processor does not support ... in ARM mode"). By the
    # second configure the toolchain's -mcpu/-mthumb are in the cache and get
    # picked up. Cheaper to configure twice than to explain that error.
    printf 'flash.sh: re-configuring (first pass on a new tree loses the C++ -mcpu flags)\n'
    # shellcheck disable=SC2086
    cmake -S "$ROOT" -B "$BUILD" $CONFIGURE_ARGS
fi
# Mirror the condition the root CMakeLists.txt uses to decide whether the
# bootloader is part of the build, so we know which image to flash. Reading it
# back out of the cache keeps this honest for a tree configured by the IDE.
OTA_BUILD=0
if grep -q '^PICO_OTA:BOOL=ON' "$BUILD/CMakeCache.txt" 2>/dev/null &&
   grep -qE '^WIFI_SSID:[A-Z]+=.+' "$BUILD/CMakeCache.txt" 2>/dev/null; then
    OTA_BUILD=1
fi

if [ -n "$OTA_HOST" ] && [ "$OTA_BUILD" -eq 0 ]; then
    die "this build tree has no OTA bootloader, so there is nothing to update over the air"
fi

OUT="$BUILD/projects/$PROJECT"
APP_ELF="$OUT/$TARGET.elf"
COMBINED_UF2="$OUT/picowota_$TARGET.uf2"

if [ "$OTA_BUILD" -eq 1 ]; then
    # Building the combined target pulls in the application and the bootloader.
    cmake --build "$BUILD" --target "picowota_$TARGET" -j"$(nproc)"
    [ -f "$COMBINED_UF2" ] || die "built picowota_$TARGET but found no $COMBINED_UF2"
    [ -f "$APP_ELF" ] || die "built $TARGET but found no $APP_ELF"
    USB_IMAGE="$COMBINED_UF2"
else
    cmake --build "$BUILD" --target "$TARGET" -j"$(nproc)"
    USB_IMAGE="$OUT/$TARGET.uf2"
    if [ ! -f "$USB_IMAGE" ]; then
        USB_IMAGE="$(find "$BUILD" -name "$TARGET.uf2" -print -quit)"
        [ -n "$USB_IMAGE" ] || die "built $TARGET but found no $TARGET.uf2 under $BUILD"
    fi
fi

if [ "$BUILD_ONLY" -eq 1 ]; then
    printf 'flash.sh: built %s\n' "$USB_IMAGE"
    [ "$OTA_BUILD" -eq 1 ] && printf 'flash.sh:       and %s (for --ota)\n' "$APP_ELF"
    exit 0
fi

if [ -n "$OTA_HOST" ]; then
    command -v serial-flash >/dev/null 2>&1 ||
        die "serial-flash not found on PATH; install it with:
         go install github.com/usedbytes/serial-flash@latest"
    printf 'flash.sh: sending %s to %s:4242\n' "$APP_ELF" "$OTA_HOST"
    printf 'flash.sh: (the device must already be in the bootloader)\n'
    serial-flash "tcp:$OTA_HOST:4242" "$APP_ELF"
    exit 0
fi

command -v picotool >/dev/null 2>&1 || die "picotool not found on PATH"
printf 'flash.sh: flashing %s\n' "$USB_IMAGE"
picotool load -x "$USB_IMAGE" -f
