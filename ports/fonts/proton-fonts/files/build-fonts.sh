#!/bin/sh
# Generate Proton's replacement Windows fonts, following the "Fonts" rules
# of ValveSoftware/Proton Makefile.in (proton-11.0-2e).
#
# Environment: FONTSRC (Proton fonts/), LIBSRC (liberation-fonts src/),
# FONTOBJ (output directory), PYTHON (python3 with fontTools).
set -eu

: "${FONTSRC:?}" "${LIBSRC:?}" "${FONTOBJ:?}" "${PYTHON:?}"
mkdir -p "$FONTOBJ"
cd "$FONTOBJ"

cp "$LIBSRC"/*.sfd .
patch -s LiberationMono-Regular.sfd "$FONTSRC/patches/LiberationMono-Regular.patch"

# output, PostScript name, family, full name, Liberation source (Proton's
# *_NAMES and *_ORIG).  The use of "Arial", "Times New Roman" etc. is for
# compatibility with programs that require those exact strings; these
# fonts are not those fonts.
while IFS='|' read -r out ps family full src; do
	fontforge -quiet -script "$FONTSRC/scripts/generatefont.pe" \
	    "$src.sfd" "$ps" "$family" "$full" "$out.ttf"
done <<'EOF'
arial|Arial|Arial|Arial|LiberationSans-Regular
arialbd|Arial-Bold|Arial|Arial Bold|LiberationSans-Bold
times|TimesNewRoman|Times New Roman|Times New Roman|LiberationSerif-Regular
georgia|Georgia|Georgia|Georgia|LiberationSerif-Regular
cour|CourierNew|Courier New|Courier New|LiberationMono-Regular
courbd|CourierNewPS-BoldMT|Courier New|Courier New Bold|LiberationMono-Bold
EOF

noto() {
	for n; do printf '%s ' "$FONTSRC/noto/$n-Regular.ttf"; done
}

# shellcheck disable=SC2046
"$PYTHON" "$FONTSRC/scripts/merge.py" $(noto NotoSans NotoSansArabic \
    NotoSansArmenian NotoSansBengali NotoSansCoptic NotoSansGeorgian \
    NotoSansGujarati NotoSansHebrew NotoSansKhmer NotoSansTamil \
    NotoSansThaana NotoSansThai NotoSansMath NotoSansSymbols NotoSansMono) \
    "MicrosoftSansSerif" "Microsoft Sans Serif" "Regular" \
    "$FONTSRC/ranges/micross" micross.ttf

# shellcheck disable=SC2046
"$PYTHON" "$FONTSRC/scripts/merge.py" $(noto NotoSans NotoSansChakma \
    NotoSansBengaliUI NotoSansDevanagariUI NotoSansGujaratiUI \
    NotoSansGurmukhiUI NotoSansKannadaUI NotoSansMalayalamUI \
    NotoSansMeeteiMayek NotoSansOlChiki NotoSansOriyaUI NotoSansSinhalaUI \
    NotoSansSoraSompeng NotoSansTamilUI NotoSansTeluguUI NotoSerifTibetan \
    NotoSansMath NotoSansSymbols2) \
    "NirmalaUI" "Nirmala UI" "Regular" "$FONTSRC/ranges/nirmala" nirmala.ttf

rm -f ./*.sfd ./*.sfd.orig
