#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AUR_DIR="$SCRIPT_DIR/aur"
WORK_DIR="${TMPDIR:-/tmp}/aur-deploy-$$"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] <tag>

Deploy quadrate to AUR.

Arguments:
    tag               Git tag to deploy (e.g., 2.0.0.alpha1)

Options:
    -n, --dry-run     Show what would be done without making changes
    -t, --test        Build package locally to test PKGBUILD
    -h, --help        Show this help message

Examples:
    $(basename "$0") 2.0.0.alpha1           # Deploy tag 2.0.0.alpha1
    $(basename "$0") --test 2.0.0.alpha1    # Test build locally first
    $(basename "$0") -n 2.0.0.alpha1        # Dry-run to see changes
EOF
}

log() {
    echo "==> $*"
}

error() {
    echo "ERROR: $*" >&2
    exit 1
}

cleanup() {
    if [[ -d "$WORK_DIR" ]]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

# Convert git tag to AUR-compatible pkgver
# e.g., "2.0.0-alpha.1" -> "2.0.0.alpha1"
tag_to_pkgver() {
    local tag="$1"
    echo "$tag" | sed 's/-/./g' | sed 's/\.$//'
}

# Generate PKGBUILD from template
generate_pkgbuild() {
    local tag="$1"
    local pkgver
    pkgver=$(tag_to_pkgver "$tag")

    sed -e "s/_PKGVER_/$pkgver/" \
        -e "s/_GITTAG_/$tag/" \
        "$AUR_DIR/quadrate/PKGBUILD"
}

test_build() {
    local tag="$1"

    log "Testing build for quadrate $tag"

    local test_dir="$WORK_DIR/test-quadrate"
    mkdir -p "$test_dir"

    generate_pkgbuild "$tag" > "$test_dir/PKGBUILD"

    cd "$test_dir"
    makepkg -sf --noconfirm
    log "Build successful for quadrate $tag"
}

deploy_package() {
    local tag="$1"
    local dry_run="${2:-false}"
    local aur_repo="ssh://aur@aur.archlinux.org/quadrate.git"
    local pkgver
    pkgver=$(tag_to_pkgver "$tag")

    log "Deploying quadrate $tag (pkgver: $pkgver) to AUR"

    local clone_dir="$WORK_DIR/quadrate"
    mkdir -p "$WORK_DIR"

    # Clone or initialize AUR repo
    if git ls-remote "$aur_repo" &>/dev/null; then
        log "Cloning existing AUR repo"
        git clone "$aur_repo" "$clone_dir"
    else
        log "Initializing new AUR repo"
        mkdir -p "$clone_dir"
        cd "$clone_dir"
        git init
        git remote add origin "$aur_repo"
    fi

    cd "$clone_dir"

    # Generate PKGBUILD
    generate_pkgbuild "$tag" > PKGBUILD

    # Generate .SRCINFO
    log "Generating .SRCINFO"
    makepkg --printsrcinfo > .SRCINFO

    # Check for changes
    if git diff --quiet HEAD -- PKGBUILD .SRCINFO 2>/dev/null; then
        log "No changes for quadrate $tag, skipping"
        return 0
    fi

    # Stage files
    git add PKGBUILD .SRCINFO

    local commit_msg="Update to $pkgver"

    if [[ "$dry_run" == "true" ]]; then
        log "[DRY-RUN] Would commit: $commit_msg"
        log "[DRY-RUN] Would push to $aur_repo"
        echo ""
        echo "Generated PKGBUILD:"
        echo "---"
        cat PKGBUILD
        echo "---"
        git diff --cached
    else
        git commit -m "$commit_msg"
        git push origin master
        log "Successfully deployed quadrate $pkgver to AUR"
    fi
}

main() {
    local dry_run=false
    local test_mode=false
    local tag=""

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -n|--dry-run)
                dry_run=true
                shift
                ;;
            -t|--test)
                test_mode=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            -*)
                error "Unknown option: $1"
                ;;
            *)
                if [[ -z "$tag" ]]; then
                    tag="$1"
                else
                    error "Unexpected argument: $1"
                fi
                shift
                ;;
        esac
    done

    if [[ -z "$tag" ]]; then
        error "Missing required argument: tag"
    fi

    if [[ "$test_mode" == "true" ]]; then
        test_build "$tag"
    else
        deploy_package "$tag" "$dry_run"
    fi

    log "Done!"
}

main "$@"
