Native small-font assets (12px cells), added for 0.2.88-p8-fontcrisp-r2.

Chinese and other covered non-ASCII characters:
WenQuanYi Bitmap Song 9pt/12px medium, version 0.9.9.8.
Copyright 2004-2010 WenQuanYi Board of Trustees and Qianqian Fang.
The BDF header specifies GPL v2 with font embedding exception.
Complete BDF and its attribution comments are retained in wenquanyi_9pt.bdf.gz.
Mirror: https://github.com/larryli/u8g2_wqy
Commit: 34d1bf7e054ef5a331c45248cd238edc49498897
Path: bdf/wenquanyi_9pt.bdf
Original BDF SHA256: dcf916124ab0d3d520b5994482ebc84d93dcd7cd19299b83f8fb696855cdb201
WQY-COPYING.txt and WQY-AUTHORS.txt are complete members read from the
official SourceForge release archive. The archive transfer was truncated;
it is not used as the font input or represented as a complete download.

ASCII:
X11 Misc Fixed 6x12; its BDF declares:
"Public domain terminal emulator font. Share and enjoy."
https://raw.githubusercontent.com/olikraus/u8g2/master/tools/font/bdf/6x12.bdf
Original BDF SHA256: dc6da890a65bcdc1daa0a84db09c25256bcb6ae382c968cb62c1d71e3a17e933

tools/rebuild_small12.py validates the compressed input hashes and packs
complete ink only. Characters with ink outside the existing 12px cell are
left to the existing coverage fallback, rather than clipped or removed.
This changes small glyph outlines only. Font metrics, advances, anchors,
style transforms, snapshot schema and medium/large rendering are unchanged.
