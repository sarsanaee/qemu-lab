#!/bin/bash
#
# Sync QEMU or kernel sources to a build host, compile there, and pull the
# generated artifacts back to the local machine. The build host can either be a
# real SSH target or a local directory that acts like one.
#
# This script is intended for local development workflows where editing stays on
# the workstation while heavy builds move elsewhere.

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)
env_file="${REMOTE_BUILD_ENV_FILE:-$repo_root/.remote-build.env}"

log() {
    printf '[remote-build] %s\n' "$*"
}

die() {
    printf '[remote-build] error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: scripts/remote-build.sh <command> [options]

Commands:
  doctor
      Show the active configuration and basic tool checks.

  qemu-sync
      Sync this QEMU tree to the build host.

  qemu-build [--reconfigure] [make-target ...]
      Sync, optionally configure, build QEMU remotely, and pull the build
      directory back to the local machine.

  qemu-pull
      Pull the remote QEMU build directory back locally.

  kernel-sync
      Sync the configured kernel tree to the build host.

  kernel-build [--reconfigure] [make-target ...]
      Sync, optionally configure, build the kernel remotely, and pull the build
      directory back locally.

  kernel-pull
      Pull the remote kernel build directory back locally.

  shell
      Open an interactive shell inside the build host root.

  help
      Show this help text.

Configuration is loaded from .remote-build.env when present.
Use .remote-build.env.example as the starting point for local customization.
EOF
}

if [ -f "$env_file" ]; then
    # shellcheck disable=SC1090
    source "$env_file"
fi

BUILD_TRANSPORT=${BUILD_TRANSPORT:-local}
BUILD_HOST=${BUILD_HOST:-}
BUILD_PORT=${BUILD_PORT:-22}
BUILD_USER=${BUILD_USER:-$USER}
REMOTE_ROOT=${REMOTE_ROOT:-$HOME/.cache/qemu-lab-build-server}
MAKE_JOBS=${MAKE_JOBS:-}

QEMU_LOCAL_SRC_DIR=${QEMU_LOCAL_SRC_DIR:-$repo_root}
QEMU_REMOTE_SRC_DIR=${QEMU_REMOTE_SRC_DIR:-$REMOTE_ROOT/qemu-src}
QEMU_REMOTE_BUILD_DIR=${QEMU_REMOTE_BUILD_DIR:-$REMOTE_ROOT/qemu-build}
QEMU_LOCAL_BUILD_DIR=${QEMU_LOCAL_BUILD_DIR:-$repo_root/build/remote-qemu}

KERNEL_LOCAL_SRC_DIR=${KERNEL_LOCAL_SRC_DIR:-}
KERNEL_REMOTE_SRC_DIR=${KERNEL_REMOTE_SRC_DIR:-$REMOTE_ROOT/linux-src}
KERNEL_REMOTE_BUILD_DIR=${KERNEL_REMOTE_BUILD_DIR:-$REMOTE_ROOT/linux-build}
KERNEL_LOCAL_BUILD_DIR=${KERNEL_LOCAL_BUILD_DIR:-$repo_root/build/remote-kernel}
KERNEL_ARCH=${KERNEL_ARCH:-$(uname -m)}
KERNEL_CONFIG_TARGET=${KERNEL_CONFIG_TARGET:-defconfig}
KERNEL_CROSS_COMPILE=${KERNEL_CROSS_COMPILE:-}

default_kernel_targets() {
    case "$KERNEL_ARCH" in
    arm64|aarch64)
        printf '%s\n' Image modules dtbs
        ;;
    x86_64|x86)
        printf '%s\n' bzImage modules
        ;;
    *)
        printf '%s\n' all
        ;;
    esac
}

if ! declare -p QEMU_CONFIGURE_ARGS >/dev/null 2>&1; then
    QEMU_CONFIGURE_ARGS=(--enable-debug)
fi

if ! declare -p QEMU_EXTRA_MAKE_ARGS >/dev/null 2>&1; then
    QEMU_EXTRA_MAKE_ARGS=()
fi

if ! declare -p KERNEL_BUILD_TARGETS >/dev/null 2>&1; then
    mapfile -t KERNEL_BUILD_TARGETS < <(default_kernel_targets)
fi

if ! declare -p KERNEL_EXTRA_MAKE_ARGS >/dev/null 2>&1; then
    KERNEL_EXTRA_MAKE_ARGS=()
fi

if ! declare -p SOURCE_SYNC_EXCLUDES >/dev/null 2>&1; then
    SOURCE_SYNC_EXCLUDES=(
        .git/
        build/
        .cache/
        .vscode/
        compile_commands.json
        '*.cover'
        '*.mbx'
        '*.patch'
    )
fi

require_local_tool() {
    command -v "$1" >/dev/null 2>&1 || die "Required local tool '$1' is missing"
}

quote_words() {
    local quoted=()
    local word

    for word in "$@"; do
        printf -v word '%q' "$word"
        quoted+=("$word")
    done

    local IFS=' '
    printf '%s' "${quoted[*]}"
}

ssh_destination() {
    if [ -n "$BUILD_USER" ]; then
        printf '%s@%s' "$BUILD_USER" "$BUILD_HOST"
    else
        printf '%s' "$BUILD_HOST"
    fi
}

run_remote() {
    local command=$1

    case "$BUILD_TRANSPORT" in
    local)
        bash -lc "$command"
        ;;
    ssh)
        [ -n "$BUILD_HOST" ] || die "BUILD_HOST must be set when BUILD_TRANSPORT=ssh"
        ssh -p "$BUILD_PORT" "$(ssh_destination)" \
            "bash -lc $(printf '%q' "$command")"
        ;;
    *)
        die "Unsupported BUILD_TRANSPORT '$BUILD_TRANSPORT' (expected local or ssh)"
        ;;
    esac
}

remote_exists() {
    run_remote "test -e $(quote_words "$1")" >/dev/null 2>&1
}

ensure_remote_dir() {
    run_remote "mkdir -p $(quote_words "$1")"
}

detect_jobs() {
    if [ -n "$MAKE_JOBS" ]; then
        printf '%s\n' "$MAKE_JOBS"
        return
    fi

    run_remote 'getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 8' |
        tail -n 1 | tr -d '[:space:]'
}

sync_tree_to_remote() {
    local src_dir=$1
    local dst_dir=$2
    local exclude
    local -a rsync_args=(
        --archive
        --delete
        --delete-excluded
        --filter=':- .gitignore'
    )

    for exclude in "${SOURCE_SYNC_EXCLUDES[@]+"${SOURCE_SYNC_EXCLUDES[@]}"}"; do
        rsync_args+=(--exclude="$exclude")
    done

    case "$BUILD_TRANSPORT" in
    local)
        mkdir -p "$dst_dir"
        rsync "${rsync_args[@]}" "$src_dir/" "$dst_dir/"
        ;;
    ssh)
        ensure_remote_dir "$dst_dir"
        rsync "${rsync_args[@]}" -e "ssh -p $BUILD_PORT" \
            "$src_dir/" "$(ssh_destination):$dst_dir/"
        ;;
    esac
}

sync_tree_from_remote() {
    local src_dir=$1
    local dst_dir=$2
    local -a rsync_args=(
        --archive
        --delete
    )

    mkdir -p "$dst_dir"

    case "$BUILD_TRANSPORT" in
    local)
        rsync "${rsync_args[@]}" "$src_dir/" "$dst_dir/"
        ;;
    ssh)
        rsync "${rsync_args[@]}" -e "ssh -p $BUILD_PORT" \
            "$(ssh_destination):$src_dir/" "$dst_dir/"
        ;;
    esac
}

sync_file_to_remote() {
    local src_file=$1
    local dst_file=$2
    local dst_dir

    dst_dir=$(dirname -- "$dst_file")

    case "$BUILD_TRANSPORT" in
    local)
        mkdir -p "$dst_dir"
        rsync --archive "$src_file" "$dst_file"
        ;;
    ssh)
        ensure_remote_dir "$dst_dir"
        rsync --archive -e "ssh -p $BUILD_PORT" \
            "$src_file" "$(ssh_destination):$dst_file"
        ;;
    esac
}

ensure_qemu_source() {
    [ -f "$QEMU_LOCAL_SRC_DIR/configure" ] || \
        die "QEMU_LOCAL_SRC_DIR does not look like a QEMU checkout: $QEMU_LOCAL_SRC_DIR"
}

ensure_kernel_source() {
    [ -n "$KERNEL_LOCAL_SRC_DIR" ] || \
        die "KERNEL_LOCAL_SRC_DIR is not set. Update .remote-build.env first."
    [ -f "$KERNEL_LOCAL_SRC_DIR/Makefile" ] || \
        die "KERNEL_LOCAL_SRC_DIR does not look like a kernel checkout: $KERNEL_LOCAL_SRC_DIR"
}

qemu_sync() {
    ensure_qemu_source
    log "Syncing QEMU sources to $QEMU_REMOTE_SRC_DIR"
    sync_tree_to_remote "$QEMU_LOCAL_SRC_DIR" "$QEMU_REMOTE_SRC_DIR"
}

qemu_pull() {
    if ! remote_exists "$QEMU_REMOTE_BUILD_DIR"; then
        die "Remote QEMU build directory does not exist yet: $QEMU_REMOTE_BUILD_DIR"
    fi

    log "Pulling QEMU build artifacts to $QEMU_LOCAL_BUILD_DIR"
    sync_tree_from_remote "$QEMU_REMOTE_BUILD_DIR" "$QEMU_LOCAL_BUILD_DIR"
}

qemu_configure() {
    local -a cmd=("$QEMU_REMOTE_SRC_DIR/configure")
    cmd+=("${QEMU_CONFIGURE_ARGS[@]+"${QEMU_CONFIGURE_ARGS[@]}"}")

    log "Configuring QEMU in $QEMU_REMOTE_BUILD_DIR"
    run_remote "mkdir -p $(quote_words "$QEMU_REMOTE_BUILD_DIR") && \
        cd $(quote_words "$QEMU_REMOTE_BUILD_DIR") && \
        $(quote_words "${cmd[@]}")"
}

qemu_build() {
    local reconfigure=0
    local jobs
    local -a cmd

    if [ "${1:-}" = "--reconfigure" ]; then
        reconfigure=1
        shift
    fi

    qemu_sync

    if [ "$reconfigure" -eq 1 ] || ! remote_exists "$QEMU_REMOTE_BUILD_DIR/Makefile"; then
        qemu_configure
    fi

    jobs=$(detect_jobs)
    cmd=(make -C "$QEMU_REMOTE_BUILD_DIR" "-j$jobs")
    cmd+=("${QEMU_EXTRA_MAKE_ARGS[@]+"${QEMU_EXTRA_MAKE_ARGS[@]}"}")
    cmd+=("$@")

    log "Building QEMU with $jobs parallel jobs"
    run_remote "$(quote_words "${cmd[@]}")"
    qemu_pull
}

kernel_sync() {
    ensure_kernel_source
    log "Syncing kernel sources to $KERNEL_REMOTE_SRC_DIR"
    sync_tree_to_remote "$KERNEL_LOCAL_SRC_DIR" "$KERNEL_REMOTE_SRC_DIR"
}

seed_remote_kernel_config() {
    local local_config=

    if [ -f "$KERNEL_LOCAL_BUILD_DIR/.config" ]; then
        local_config="$KERNEL_LOCAL_BUILD_DIR/.config"
    elif [ -f "$KERNEL_LOCAL_SRC_DIR/.config" ]; then
        local_config="$KERNEL_LOCAL_SRC_DIR/.config"
    fi

    if [ -n "$local_config" ]; then
        ensure_remote_dir "$KERNEL_REMOTE_BUILD_DIR"
        sync_file_to_remote "$local_config" "$KERNEL_REMOTE_BUILD_DIR/.config"
    fi
}

kernel_configure() {
    local config_target=$KERNEL_CONFIG_TARGET
    local -a cmd=(
        make
        -C "$KERNEL_REMOTE_SRC_DIR"
        O="$KERNEL_REMOTE_BUILD_DIR"
        ARCH="$KERNEL_ARCH"
    )

    if [ -n "$KERNEL_CROSS_COMPILE" ]; then
        cmd+=("CROSS_COMPILE=$KERNEL_CROSS_COMPILE")
    fi

    seed_remote_kernel_config

    if remote_exists "$KERNEL_REMOTE_BUILD_DIR/.config" &&
        [ "$config_target" = "defconfig" ]; then
        config_target=olddefconfig
    fi

    cmd+=("${KERNEL_EXTRA_MAKE_ARGS[@]+"${KERNEL_EXTRA_MAKE_ARGS[@]}"}")
    cmd+=("$config_target")

    log "Configuring kernel build directory with target $config_target"
    run_remote "mkdir -p $(quote_words "$KERNEL_REMOTE_BUILD_DIR") && \
        $(quote_words "${cmd[@]}")"
}

kernel_pull() {
    if ! remote_exists "$KERNEL_REMOTE_BUILD_DIR"; then
        die "Remote kernel build directory does not exist yet: $KERNEL_REMOTE_BUILD_DIR"
    fi

    log "Pulling kernel build artifacts to $KERNEL_LOCAL_BUILD_DIR"
    sync_tree_from_remote "$KERNEL_REMOTE_BUILD_DIR" "$KERNEL_LOCAL_BUILD_DIR"
}

kernel_build() {
    local reconfigure=0
    local jobs
    local -a targets
    local -a cmd=(
        make
        -C "$KERNEL_REMOTE_SRC_DIR"
        O="$KERNEL_REMOTE_BUILD_DIR"
        ARCH="$KERNEL_ARCH"
    )

    if [ "${1:-}" = "--reconfigure" ]; then
        reconfigure=1
        shift
    fi

    kernel_sync

    if [ "$reconfigure" -eq 1 ] || ! remote_exists "$KERNEL_REMOTE_BUILD_DIR/.config"; then
        kernel_configure
    fi

    if [ -n "$KERNEL_CROSS_COMPILE" ]; then
        cmd+=("CROSS_COMPILE=$KERNEL_CROSS_COMPILE")
    fi

    jobs=$(detect_jobs)
    cmd+=("-j$jobs")
    cmd+=("${KERNEL_EXTRA_MAKE_ARGS[@]+"${KERNEL_EXTRA_MAKE_ARGS[@]}"}")

    if [ "$#" -gt 0 ]; then
        targets=("$@")
    else
        targets=("${KERNEL_BUILD_TARGETS[@]+"${KERNEL_BUILD_TARGETS[@]}"}")
    fi

    cmd+=("${targets[@]}")

    log "Building kernel targets: ${targets[*]}"
    run_remote "$(quote_words "${cmd[@]}")"
    kernel_pull
}

open_shell() {
    local shell_cmd

    case "$BUILD_TRANSPORT" in
    local)
        mkdir -p "$REMOTE_ROOT"
        cd "$REMOTE_ROOT"
        exec "${SHELL:-/bin/bash}" -l
        ;;
    ssh)
        [ -n "$BUILD_HOST" ] || die "BUILD_HOST must be set when BUILD_TRANSPORT=ssh"
        shell_cmd="mkdir -p $(quote_words "$REMOTE_ROOT") && \
            cd $(quote_words "$REMOTE_ROOT") && \
            exec \${SHELL:-/bin/bash} -l"
        exec ssh -t -p "$BUILD_PORT" "$(ssh_destination)" \
            "bash -lc $(printf '%q' "$shell_cmd")"
        ;;
    esac
}

doctor() {
    require_local_tool bash
    require_local_tool rsync
    require_local_tool make
    require_local_tool tail
    require_local_tool tr

    if [ "$BUILD_TRANSPORT" = "ssh" ]; then
        require_local_tool ssh
    fi

    printf 'transport: %s\n' "$BUILD_TRANSPORT"
    printf 'env file: %s\n' "$env_file"
    printf 'remote root: %s\n' "$REMOTE_ROOT"
    printf 'qemu source: %s\n' "$QEMU_LOCAL_SRC_DIR"
    printf 'qemu local build: %s\n' "$QEMU_LOCAL_BUILD_DIR"
    printf 'qemu remote source: %s\n' "$QEMU_REMOTE_SRC_DIR"
    printf 'qemu remote build: %s\n' "$QEMU_REMOTE_BUILD_DIR"
    printf 'kernel source: %s\n' "${KERNEL_LOCAL_SRC_DIR:-<unset>}"
    printf 'kernel local build: %s\n' "$KERNEL_LOCAL_BUILD_DIR"
    printf 'kernel remote source: %s\n' "$KERNEL_REMOTE_SRC_DIR"
    printf 'kernel remote build: %s\n' "$KERNEL_REMOTE_BUILD_DIR"
    printf 'kernel arch: %s\n' "$KERNEL_ARCH"
    printf 'jobs: %s\n' "$(detect_jobs)"

    if [ "$BUILD_TRANSPORT" = "ssh" ]; then
        printf 'ssh target: %s\n' "$(ssh_destination)"
        if run_remote 'command -v bash >/dev/null 2>&1 && command -v rsync >/dev/null 2>&1 && command -v make >/dev/null 2>&1'; then
            printf 'remote tools: ok\n'
        else
            printf 'remote tools: missing one of bash/rsync/make\n'
        fi
    fi
}

command_name=${1:-help}
shift || true

case "$command_name" in
doctor)
    doctor
    ;;
qemu-sync)
    qemu_sync
    ;;
qemu-build)
    qemu_build "$@"
    ;;
qemu-pull)
    qemu_pull
    ;;
kernel-sync)
    kernel_sync
    ;;
kernel-build)
    kernel_build "$@"
    ;;
kernel-pull)
    kernel_pull
    ;;
shell)
    open_shell
    ;;
help|-h|--help)
    usage
    ;;
*)
    usage >&2
    die "Unknown command '$command_name'"
    ;;
esac
