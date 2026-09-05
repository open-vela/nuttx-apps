# face_detection — Frontal Face Detection (TFLite Micro)

Detects whether a face is **squarely facing the camera**, on the R528 board,
using MediaPipe **BlazeFace short-range**. Derived from `person_detection`:
the camera / JPEG-decode / LCD-blit paths are identical; only the model,
pre-processing and post-processing differ.

```
person_detection : 96x96 gray  INT8    -> [1,2]                    (binary classifier)
face_detection   : 128x128 RGB FLOAT32 -> [1,896,16]+[1,896,1]     (SSD + keypoints)
```

The bundled model is **PINTO_model_zoo #030 BlazeFace**
(`face_detection_front_128_integer_quant.tflite`): int8 weights internally,
but **float32 input/output** tensors (a QUANTIZE op wraps the input,
DEQUANTIZE ops wrap the two outputs). Already embedded as
`face_detect_model_data.inc` — no extra download needed to build.

## Pipeline

1. **Capture** 320x240 MJPEG from `/dev/video` (same as person_detection).
2. **Decode** with tjpgd; the decode callback fills two buffers in one pass:
   - RGB565 for the LCD preview,
   - 128x128 RGB **float32** for the model (pixel → `[-1,1]`; the model does
     its own int8 QUANTIZE internally).
3. **Invoke** BlazeFace.
4. **Post-process**: decode 896 anchors → sigmoid score → threshold → NMS →
   6 face keypoints per surviving face.
5. **Frontal test** from the eye + nose keypoints:
   - `roll  = |eyeL.y - eyeR.y| / eyeDist  < 0.40`   (head not tilted)
   - `yaw   = |nose.x - eyeMid.x| / eyeDist < 0.35`   (not turned left/right)
   - nose below eyes, mouth below nose (vertical sanity).
6. **Overlay** box (green=frontal, yellow=turned) + keypoints, and a banner:
   `FRONTAL FACE` / `FACE (TURNED)` / `NO FACE`.

Thresholds are `#define`s at the top of `face_detection_main.cc`
(`SCORE_THRESH`, `NMS_IOU`, `ROLL_THRESH`, `YAW_THRESH`) — tune on-device.

## The model (already embedded)

`face_detect_model_data.inc` is generated and committed, so the app builds and
runs as-is. It expects:
- input  `[1,128,128,3]` **float32**
- outputs `[1,896,16]` (boxes+keypoints) and `[1,896,1]` (raw scores)

### Regenerating it from the source .tflite

The source model `face_detection_front_128_integer_quant.tflite` is kept in
this directory for provenance. To rebuild the `.inc`:

```sh
cd apps/examples/face_detection
python3 tools/tflite_to_inc.py face_detection_front_128_integer_quant.tflite \
        face_detect_model_data.inc
```

To fetch the original from PINTO_model_zoo #030:
```sh
curl "https://s3.ap-northeast-2.wasabisys.com/pinto-model-zoo/030_BlazeFace/resources.tar.gz" \
     -o resources.tar.gz && tar xzf resources.tar.gz
# -> 030_BlazeFace/face_detection_front_128_integer_quant.tflite
```

### Using a different BlazeFace variant

If you swap in a model with a different anchor count or coordinate layout
(e.g. full-range BlazeFace = 2016 anchors), adjust `NUM_ANCHORS`,
`generate_anchors()` and `decode_detections()` accordingly. The current
anchors were verified byte-for-byte against MediaPipe's SsdAnchorsCalculator
for the front/short-range config.

## Build & config

Add to your board defconfig (same TFLite deps as person_detection):

```
CONFIG_SYSTEM_FLATBUFFERS=y
CONFIG_MATH_GEMMLOWP=y
CONFIG_MATH_KISSFFT=y
CONFIG_MATH_RUY=y
CONFIG_TFLITEMICRO=y
CONFIG_MLEARNING_CMSIS_NN=y      # optional HW accel
CONFIG_ARM_NEON=y                # optional HW accel
CONFIG_EXAMPLES_FACE_DETECTION=y
CONFIG_EXAMPLES_FACE_DETECTION_STACKSIZE=32768
CONFIG_EXAMPLES_FACE_DETECTION_ARENA_SIZE=524288
```

Or select **Application Configuration → Examples → Frontal Face Detection**
in `menuconfig`. The parent `apps/examples/Kconfig` is auto-generated and
`Make.defs` is wildcard-included, so no manual registration is needed.

## Run (NSH)

```
nsh> face_detection            # blank image (sanity check)
nsh> face_detection -t face.ppm  # 128x128 binary PPM (P6), maxval 255
nsh> face_detection -c           # camera, one shot
nsh> face_detection -c -r 0      # camera, live preview forever
```

On first run the app prints the arena usage and anchor count:
```
Model loaded, arena: NNNNNN/524288 bytes, anchors: 896
```
Round `arena_used_bytes` up and set `CONFIG_EXAMPLES_FACE_DETECTION_ARENA_SIZE`
to reclaim RAM.

## Op resolver

`face_detection_main.cc` registers a superset of ops for the short-range
graph (Conv2D, DepthwiseConv2D, Max/AveragePool2D, Add, Pad, Reshape,
Concatenation, Logistic, Quantize, Dequantize). If `AllocateTensors()` or
`Invoke()` reports a missing builtin opcode, add the matching `AddXxx()`
call — the error message names the opcode.

## Notes / known approximations

- 320x240 is squished into 128x128 (independent x/y scaling), matching
  person_detection. For best accuracy consider letterboxing to a square and
  mapping keypoints back accordingly.
- The left/right eye labels are a convention; the frontal test is symmetric,
  so it is correct regardless of which physical eye is keypoint 0 vs 1.
