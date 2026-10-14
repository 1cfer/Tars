from pathlib import Path
import gzip
root = Path(__file__).resolve().parents[1] / "firmware" / "TARS"
source = (root / "TarsPage.h").read_text(encoding="utf-8")
html = source.split('R"HTML(', 1)[1].split(')HTML";', 1)[0].encode("utf-8")
blob = gzip.compress(html, mtime=0)
header = "#pragma once\n#include <Arduino.h>\n// Generado por tools/pack_panel.py; no contiene credenciales.\nstatic const uint8_t TARS_PAGE_GZIP[] PROGMEM={\n"
header += ",\n".join(",".join(f"0x{x:02x}" for x in blob[i:i+24]) for i in range(0, len(blob), 24)) + "\n};\n"
(root / "TarsPageGzip.h").write_text(header, encoding="utf-8")
print(f"Panel {len(html)} bytes -> {len(blob)} gzip")
