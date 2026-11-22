@page guide_building Сборка и установка

@tableofcontents

## Требования

- FFmpeg >= 4.0:
  - libavformat >= 58.x.x,
  - libavcodec >= 58.x.x,
  - libavutil >= 56.x.x,
  - libswscale >= 5.x.x,
  - libswresample >= 3.x.x,
  - libavdevice >= 58.x.x (необязательно),
  - libavfilter >= 7.x.x (необязательно),
  - libpostproc >= 55.x.x (устаревшая и необязательная; удалена в FFmpeg 8);
- GCC >= 9.0, Clang или MSVC. Требуется C++17;
- CMake >= 3.19 для рекомендуемой сборки.

Более старые FFmpeg (вплоть до 2.0) и GCC (вплоть до 6.0) могут работать, но не проверялись.
Самая старая версия Ubuntu в CI — 22.04.

## Установка FFmpeg

На Debian, Ubuntu 19.10+ или Linux Mint 20.x и новее:

```sh
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev \
                     libavfilter-dev libswscale-dev libswresample-dev \
                     libavdevice-dev
```

`libpostproc` — устаревшая библиотека: avcpp её не использует, а FFmpeg 8 удалил
её из апстрима, поэтому в новых дистрибутивах пакета может не быть вовсе.

На Ubuntu 18.04 и Linux Mint 19.x сначала добавьте PPA с FFmpeg 4:

```sh
sudo add-apt-repository ppa:jonathonf/ffmpeg-4 -y
sudo apt update && sudo apt upgrade
```

Затем установите те же пакеты.

## Сборка с CMake

```sh
git clone https://github.com/h4tr3d/avcpp.git avcpp-git
cd avcpp-git
cmake -B build
cmake --build build -j
```

Тесты включены по умолчанию; Catch2 скачивается на этапе конфигурации, поэтому подмодули не нужны.
Если FFmpeg установлен в нестандартное место:

```sh
cmake -B build -DPC_FFMPEG_LIBRARY_DIRS=<path> -DPC_FFMPEG_INCLUDE_DIRS=<path>
```

Установка в префикс:

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
```

Для пакетирования используйте `DESTDIR`:

```sh
sudo cmake --install build --prefix /usr --strip
```

## Параметры конфигурации

| параметр | по умолчанию | значение |
| --- | --- | --- |
| `AV_ENABLE_STATIC` | `On` | собирать статическую библиотеку |
| `AV_ENABLE_SHARED` | `On` | собирать разделяемую библиотеку |
| `AV_BUILD_EXAMPLES` | `On` | собирать `example/api2-samples` |
| `AVCPP_ENABLE_TESTS` | `On` | собирать набор тестов Catch2 |
| `AVCPP_TEST_DATA` | `Off` | включить тесты, которым нужны бинарные фикстуры из `avcpp-test-data` |
| `AV_DISABLE_AVFORMAT` | `Off` | не использовать libavformat; также отключает libavfilter и libavdevice |
| `AV_DISABLE_AVFILTER` | `Off` | не использовать libavfilter; также отключает libavdevice |
| `AV_DISABLE_AVDEVICE` | `Off` | не использовать libavdevice |
| `AVCPP_ENABLE_DOCS` | `Off` | добавить цели `docs`, `docs-check`, `docs-translations` и `docs-serve` |
| `CMAKE_CXX_STANDARD` | новейший поддерживаемый | переопределить стандарт C++; документированный минимум — C++17 |

Устаревшее: `AV_BUILD_TESTS` — псевдоним `AVCPP_ENABLE_TESTS`.

Параметры поиска FFmpeg:

- `FFMPEG_PKG_CONFIG_SUFFIX` — суффикс, добавляемый к именам пакетов pkg-config;
- `PC_<component>_LIBRARY_DIRS`, `PC_<component>_INCLUDE_DIRS` — подсказки для одного компонента;
- `PC_FFMPEG_LIBRARY_DIRS`, `PC_FFMPEG_INCLUDE_DIRS` — подсказки для всех компонентов.

`<component>` — один из `AVCODEC`, `AVDEVICE`, `AVFORMAT`, `AVFILTER`, `AVUTIL`, `POSTPROC`,
`SWSCALE`, `SWRESAMPLE`.

Чтобы конфигурировать без сети, укажите `FETCHCONTENT_SOURCE_DIR_CATCH2` с локальной копией Catch2.

## Интеграция как git-подмодуль

```sh
git submodule add https://github.com/h4tr3d/avcpp.git 3rdparty/avcpp
```

```cmake
add_subdirectory(3rdparty/avcpp)
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

avcpp определяет, что он не корневой проект, и пропускает установку.

## Интеграция через FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    avcpp
    GIT_REPOSITORY https://github.com/h4tr3d/avcpp.git
    GIT_TAG        v3.1.0
)
set(AV_ENABLE_STATIC On)
set(AV_ENABLE_SHARED Off)
FetchContent_MakeAvailable(avcpp)

target_link_libraries(prog PRIVATE avcpp::avcpp)
```

## Установка через vcpkg

```json
{
    "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
    "dependencies": [
        "avcpp"
    ],
    "name": "your-app-name",
    "version-string": "0.0.1"
}
```

```cmake
find_package(avcpp CONFIG REQUIRED)
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

## Сборка с Meson

@warning Поддерживается CMake. У сборки на Meson нет мейнтейнера, и она не входит в CI.

```sh
meson setup build
meson compile -C build
meson test -C build
meson install -C build
```

`-Dbuild_tests=false` и `-Dbuild_samples=false` соответствуют `AVCPP_ENABLE_TESTS` и
`AV_BUILD_EXAMPLES`; `-Dtest_data=true` соответствует `AVCPP_TEST_DATA`.

## Сборка документации

```sh
cmake -B build -DAVCPP_ENABLE_DOCS=ON
cmake --build build --target docs        # английская и русская версии в build/docs/site
cmake --build build --target docs-serve  # то же плюс сервер на localhost:8000
```

`docs-check` собирает справочник и сравнивает предупреждения с `docs/doxygen-baseline.txt`; сейчас
база пуста, поэтому любое новое предупреждение останавливает сборку. Те же команды без CMake —
`docs/build.sh` и `docs/check-warnings.sh`.
