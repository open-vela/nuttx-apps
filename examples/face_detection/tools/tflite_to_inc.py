#!/usr/bin/env python3
# ==========================================================================
# tflite_to_inc.py — embed a .tflite model as a C array for face_detection.
#
# Usage:
#   python3 tflite_to_inc.py <model.tflite> [output.inc]
#
# Produces an .inc that defines:
#   alignas(16) static const unsigned char g_face_detect_model_data[];
#   static const unsigned int g_face_detect_model_data_len;
#
# matching what face_detection_main.cc #includes. Equivalent to `xxd -i`
# but with the exact symbol names, 16-byte alignment, and a size constant.
# ==========================================================================

import sys
import os

SYMBOL = "g_face_detect_model_data"


def main():
    if len(sys.argv) < 2:
        print("Usage: tflite_to_inc.py <model.tflite> [output.inc]")
        return 1

    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else "face_detect_model_data.inc"

    with open(src, "rb") as f:
        data = f.read()

    # Sanity check: TFLite flatbuffers carry the "TFL3" file identifier at
    # byte offset 4. Warn (do not fail) if it is missing.
    if len(data) < 8 or data[4:8] != b"TFL3":
        print("WARNING: %s does not look like a TFLite file "
              "(missing 'TFL3' identifier)" % src)

    with open(dst, "w") as out:
        out.write("// Auto-generated from %s (%d bytes)\n"
                  % (os.path.basename(src), len(data)))
        out.write("alignas(16) static const unsigned char %s[] = {\n" % SYMBOL)
        for i in range(0, len(data), 12):
            row = data[i:i + 12]
            out.write("  " + ", ".join("0x%02x" % b for b in row) + ",\n")
        out.write("};\n")
        out.write("static const unsigned int %s_len = %d;\n"
                  % (SYMBOL, len(data)))

    print("Wrote %s (%d bytes model, symbol %s)" % (dst, len(data), SYMBOL))
    return 0


if __name__ == "__main__":
    sys.exit(main())
