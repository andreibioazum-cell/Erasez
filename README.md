# Erasez

Мини-«Майнкрафт» для Android на **чистом C** и **WebGPU**
(C API `webgpu.h` + WGSL-шейдеры, без движков и без C++).

Рендер выполняет **wgpu-native** (реализация WebGPU, бэкенд **Vulkan**).
Вид от первого лица: ходите по процедурно сгенерированному миру из блоков
(холмы, биомы: снежная тундра / леса / пустыня, деревья, каменные выступы).
Каждый мир генерируется заново из детерминированного шума при старте.

## Управление

| Действие | Как |
|---|---|
| Движение | Левый нижний угол — виртуальный джойстик |
| Поворот камеры | Тащить пальцем по свободной области экрана |
| Прыжок / вверх в полёте | Круглая кнопка справа внизу (стрелка вверх) |
| Включить/выключить полёт | Кнопка со стрелками «вверх-вниз» над кнопкой прыжка |
| Спуск в полёте | Кнопка со стрелкой вниз (появляется левее прыжка в полёте) |

## Состав

- `main.c` — жизненный цикл NativeActivity и игровой цикл;
- `engine.h` — состояние игры и мир (размеры, типы блоков);
- `world.h` — генерация мира (fBm-шум, биомы, деревья);
- `physics.h` — коллизии игрока с вокселями, гравитация, полёт;
- `render.h` — **WebGPU**: WGSL-шейдеры, пайплайны, меш мира, небо, UI;
- `math_utils.h` — матричная математика (перспектива под WebGPU/Vulkan);
- `include/webgpu/` — заголовки `webgpu.h` + `wgpu.h`
  (зафиксированы под релиз wgpu-native **v29.0.1.1**);
- `input.h` — сенсорное управление.

## Требования к устройству

WebGPU-рендер работает через **Vulkan** (Android 7+ / Vulkan 1.0+).
Если адаптер не нашёлся, в logcat будет сообщение `no adapter (нужен Vulkan)`.

## Как собрать APK

⚠️ Старый CI-воркфлоу собирает OpenGL ES и для этого кода **не подойдёт**.
Готовый новый воркфлоу лежит в **`.github/workflows/main.yml.wgpu`** —
его нужно скопировать поверх `.github/workflows/main.yml` (сам файл
воркфлоу в этом репозитории менять нельзя, т.к. токен без права
`workflows`). Новый воркфлоу сам скачивает готовые бинарники
wgpu-native под Android (arm64-v8a и armeabi-v7a) из релиза
[gfx-rs/wgpu-native v29.0.1.1](https://github.com/gfx-rs/wgpu-native/releases/tag/v29.0.1.1),
собирает `.so` и пакует APK с `libwgpu_native.so` внутри.

Локально (нужен Android NDK + скачанный архив wgpu-native):

```bash
# например, для arm64:
curl -fL -o wgpu.zip https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-android-aarch64-release.zip
unzip -q wgpu.zip -d wgpu   # внутри: include/webgpu/... и lib/libwgpu_native.so

TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin
GLUE=$ANDROID_NDK_ROOT/sources/android/native_app_glue
$TOOLCHAIN/aarch64-linux-android21-clang main.c $GLUE/android_native_app_glue.c \
  -O2 -s -fPIC -shared -I. -Iinclude/webgpu -I$GLUE \
  -Lwgpu/lib -lwgpu_native -landroid -llog -lm -o libmain.so
```
