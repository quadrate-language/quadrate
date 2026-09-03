#!/usr/bin/env bash

set -euo pipefail

tag="${TAG:?TAG must be set}"

git config --global --add safe.directory "$PWD"
git config --global user.email "klahr@r8.rs"
git config --global user.name "Quadrate Release Bot"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

git clone ssh://aur@aur.archlinux.org/quadrate.git "$work/aur"

cp pkg/aur/quadrate/PKGBUILD "$work/aur/PKGBUILD"
cd "$work/aur"
sed -i "s/^pkgver=.*/pkgver=$tag/" PKGBUILD
sed -i "s/^_gittag=.*/_gittag=$tag/" PKGBUILD
sed -i "s/^pkgrel=.*/pkgrel=1/" PKGBUILD
makepkg --printsrcinfo >.SRCINFO

git add PKGBUILD .SRCINFO
if git diff --cached --quiet HEAD -- PKGBUILD .SRCINFO; then
	echo "AUR PKGBUILD already at $tag — nothing to push."
else
	git commit -m "Bump to $tag"
	git push origin HEAD:master
fi
