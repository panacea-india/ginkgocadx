#!/bin/sh
# Package a build made by ci/build.sh as an AppImage. Run from the repository
# root inside the container described by ci/Dockerfile.bookworm.
#
# Usage: ci/appimage.sh [build-dir] [output-dir]
#
# Downloads linuxdeploy and its GTK plugin into <build-dir>/appimage-tools the
# first time, at pinned versions, and refuses to run them unless their SHA-256
# matches. linuxdeploy is an AppImage itself; APPIMAGE_EXTRACT_AND_RUN lets it
# run without FUSE, which containers usually don't have.
#
# To update a tool, change its URL, download it, check it and put the new hash
# here. The hashes were recorded when the versions were pinned (upstream
# doesn't publish checksums; the linuxdeploy size matched the release asset).
set -eu

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"
ROOT="$(pwd)"
APPDIR="$ROOT/$BUILD_DIR/AppDir"
TOOLS="$ROOT/$BUILD_DIR/appimage-tools"

# linuxdeploy release 1-alpha-20251107-1
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
# linuxdeploy-plugin-gtk commit 7a3fbc31 (2026-07-02)
GTK_PLUGIN_URL="https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/7a3fbc31a9e5075073ff8790f26effbac5f84453/linuxdeploy-plugin-gtk.sh"
GTK_PLUGIN_SHA256="b0f4cbc684a0103a9651f0955b635eaea0096b3a66c0f5a2c2aa337960375171"

# fetch <url> <file> <sha256>
fetch()
{
        if [ ! -f "$2" ]; then
                wget -q -O "$2.part" "$1"
                mv "$2.part" "$2"
        fi
        if ! echo "$3  $2" | sha256sum -c --status; then
                echo "SHA-256 mismatch for $2; refusing to run it" >&2
                rm -f "$2"
                exit 1
        fi
        chmod +x "$2"
}

mkdir -p "$TOOLS"
fetch "$LINUXDEPLOY_URL" "$TOOLS/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY_SHA256"
fetch "$GTK_PLUGIN_URL" "$TOOLS/linuxdeploy-plugin-gtk.sh" "$GTK_PLUGIN_SHA256"
export PATH="$TOOLS:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1

# Install into the AppDir. Ginkgo finds its resources relative to the
# executable (<prefix>/bin -> <prefix>/share/ginkgocadx), so the usual /usr
# layout works inside the AppImage as well.
rm -rf "$APPDIR"
make -C "$BUILD_DIR" install DESTDIR="$APPDIR" > /dev/null

# Debian's DCMTK reads its data dictionary from a path compiled into the
# library (/usr/share/libdcmtk17). Bundle it and point DCMTK at the copy;
# without it tags such as Modality can't be decoded and images don't open.
# linuxdeploy's AppRun sources the scripts in apprun-hooks/ at start-up.
DCMTK_DATA=$(ls -d /usr/share/libdcmtk* | head -1)
mkdir -p "$APPDIR/usr/share/dcmtk" "$APPDIR/apprun-hooks"
cp "$DCMTK_DATA/dicom.dic" "$DCMTK_DATA/private.dic" "$APPDIR/usr/share/dcmtk/"
cat > "$APPDIR/apprun-hooks/dcmtk-dictionary.sh" <<'EOF'
export DCMDICTPATH="$APPDIR/usr/share/dcmtk/dicom.dic:$APPDIR/usr/share/dcmtk/private.dic"
EOF

# GTK3 apps need their theme, pixbuf loaders and schemas bundled
export DEPLOY_GTK_VERSION=3

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"
linuxdeploy-x86_64.AppImage \
        --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/ginkgocadx" \
        --desktop-file "$APPDIR/usr/share/applications/ginkgocadx.desktop" \
        --icon-file "$APPDIR/usr/share/icons/hicolor/48x48/apps/ginkgocadx.png" \
        --plugin gtk \
        --output appimage

echo "AppImage written to $OUT_DIR:"
ls -1 ./*.AppImage
