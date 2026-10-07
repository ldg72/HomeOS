#!/bin/sh
# HomeOS — genera un'immagine SYS exFAT minima per il Core.
#
# Il Core decide il profilo di esecuzione di Init leggendo /Prefs/default.prefs
# dalla risorsa SYS in formato exFAT (vedi SDK/docs/core/exec64_core_boot_contract.md).
# Questo script produce un'immagine grezza exFAT contenente solo quel file, da
# passare a tools/mkbundle.py come terzo argomento.
#
# Uso:
#   tools/mksys.sh OUTPUT.img [SIZE_MB] [PROFILE]
#
# Richiede macOS (hdiutil + newfs_exfat). Gli artifact sono rigenerabili.

set -eu

OUT=${1:?uso: mksys.sh OUTPUT.img [SIZE_MB] [PROFILE]}
SIZE_MB=${2:-2}
PROFILE=${3:-performance}

case "$PROFILE" in
    performance|protect) ;;
    *) echo "mksys.sh: profilo non valido '$PROFILE' (performance|protect)" >&2; exit 2 ;;
esac

WORK=$(mktemp -d "${TMPDIR:-/tmp}/homeos-sys.XXXXXX")
IMG="$WORK/sys.img"
MNT="$WORK/mnt"
DEV=""

cleanup() {
    if [ -n "$DEV" ]; then
        hdiutil detach -quiet "$DEV" >/dev/null 2>&1 || true
    fi
    if mount | grep -q " on $MNT "; then
        hdiutil detach -quiet "$MNT" >/dev/null 2>&1 || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

echo "mksys: creo immagine grezza da ${SIZE_MB} MiB"
mkdir -p "$MNT"
dd if=/dev/zero of="$IMG" bs=1m count="$SIZE_MB" 2>/dev/null

# Attacco come immagine grezza (nessun wrapper UDIF): il file resta exFAT puro.
DEV=$(hdiutil attach -nomount -nobrowse -imagekey diskimage-class=CRawDiskImage "$IMG" \
      | head -1 | awk '{print $1}')
if [ -z "$DEV" ]; then
    echo "mksys: attach del device fallito" >&2
    exit 1
fi
echo "mksys: device $DEV"

newfs_exfat -v HOMEOS "$DEV" >/dev/null
hdiutil detach -quiet "$DEV" >/dev/null
DEV=""

hdiutil attach -quiet -nobrowse -mountpoint "$MNT" "$IMG"
mkdir -p "$MNT/Prefs"
printf 'system-profile=%s\n' "$PROFILE" > "$MNT/Prefs/default.prefs"
sync
hdiutil detach -quiet "$MNT" >/dev/null

mkdir -p "$(dirname "$OUT")"
cp "$IMG" "$OUT"
echo "mksys: $OUT ($(wc -c < "$OUT" | tr -d ' ') byte, profilo=$PROFILE)"
