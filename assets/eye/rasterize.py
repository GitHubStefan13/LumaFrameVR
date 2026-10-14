from pathlib import Path
import io
import cairosvg
from PIL import Image

root = Path(__file__).resolve().parent
source = (root / "eye.svg").read_bytes()
image = Image.open(io.BytesIO(cairosvg.svg2png(bytestring=source, output_width=160, output_height=160)))
alpha = image.convert("RGBA").resize((40, 40), Image.Resampling.LANCZOS).getchannel("A").tobytes()
header = root.parents[1] / "cpp/ui/assets/passthrough_eye.hpp"
rows = [", ".join(str(value) for value in alpha[index:index + 40]) for index in range(0, len(alpha), 40)]
header.write_text("#pragma once\n#include <array>\n#include <cstdint>\nnamespace lumaframevr::ui::assets {\ninline constexpr std::array<std::uint8_t, 1600> passthrough_eye{\n" + ",\n".join(rows) + "\n};\n}\n")
image.save(root / "eye-raster.png")
print(f"Rasterized upstream Lucide eye into {header}")
