@page guide_hw_codecs Аппаратное кодирование и декодирование

@tableofcontents

## 1. Назначение

Эта страница описывает API `av::hw` в avcpp. API даёт удобный и безопасный доступ к аппаратным кодекам FFmpeg.

Страница отвечает на вопросы:

- как создать устройство и пул кадров;
- как включить аппаратный декодер;
- как включить аппаратный кодировщик;
- как передать кадр между памятью системы и устройством;
- как построить путь без копирования;
- как проверить доступность устройства без сбоя тестов;
- какие ограничения есть на разных платформах.

Целевая аудитория: разработчики avcpp и авторы приложений на avcpp.

Справочник по типам — [группа hw](../group__hw.html).

## 2. Термины

| Термин | Значение |
|---|---|
| Устройство | Аппаратный узел: GPU, медиаблок, видеокарта. В FFmpeg это `AVHWDeviceContext`. |
| Пул кадров | Набор поверхностей одного размера и формата. В FFmpeg это `AVHWFramesContext`. |
| Поверхность | Кадр в памяти устройства. Приложение не читает его напрямую. |
| Память системы | Обычная оперативная память. Кадр доступен через указатели `data[]`. |
| Нулевое копирование | Путь, при котором кадр не покидает память устройства. |
| Загрузка | Перенос кадра из памяти системы в память устройства. |
| Чтение | Перенос кадра из памяти устройства в память системы. |
| `DeviceType` | Тип устройства: `vaapi`, `qsv`, `cuda`, `d3d11va` и другие. |
| `EncodingMode` | Режим кодировщика: `Auto`, `Frames`, `Device`. |

Правило: один термин означает одно понятие. Не используйте слово «буфер» для устройства, пула и кадра.

## 3. Слои FFmpeg

FFmpeg делит аппаратную работу на три слоя.

1. `AVHWDeviceContext` описывает устройство. Он создаётся один раз.
2. `AVHWFramesContext` описывает пул поверхностей. Он связан с устройством.
3. `AVCodecContext` использует устройство или пул. Поле `hw_device_ctx` задаёт устройство. Поле `hw_frames_ctx` задаёт пул.

Слой `AVFrame` хранит дескриптор кадра. Если поле `frame->hw_frames_ctx` не пусто, кадр находится в памяти устройства.

Схема слоёв и соответствие типам avcpp:

@image html hw-architecture.svg "Архитектура API"

## 4. Владение объектами

`av::hw::DeviceContext` и `av::hw::FramesContext` владеют объектами FFmpeg. Внутри лежит `BufferRef`. Копирование объекта добавляет ссылку. Уничтожение объекта снимает ссылку.

`VideoFrame` не владеет устройством. Кадр хранит ссылку на пул через `hw_frames_ctx`. Пул хранит ссылку на устройство через `device_ref`.

Схема владения:

@image html hw-ownership.svg "Владение объектами"

Правила:

- Создавайте устройство раньше пула. Уничтожайте пул раньше устройства. Счётчик ссылок допускает другой порядок, но явный порядок проще читать.
- Не освобождайте `AVBufferRef*` вручную. Передайте его в `wrap()`. `wrap()` добавляет ссылку.
- Не храните `AVHWDeviceContext*` после уничтожения `DeviceContext`.
- Все `*Range` типы не владеют памятью. Не сохраняйте диапазон дольше его владельца.

## 5. Типы API

### 5.1. `DeviceType`

`DeviceType` — тонкая обёртка над `AVHWDeviceType`.

```cpp
av::hw::DeviceType type = av::hw::DeviceType::fromName("vaapi");
if (type.isNone()) {
    // FFmpeg не знает такой тип или тип не собран
}
const char* name = type.name(); // "vaapi" или "none"
```

Поиск идёт по имени. Код не зависит от числовых значений перечисления. Это важно для старых версий FFmpeg.

### 5.2. `DeviceContext`

`DeviceContext` создаётся фабрикой. Конструктор не используется: создание устройства может завершиться ошибкой.

```cpp
std::error_code ec;
av::hw::DeviceContext device =
    av::hw::DeviceContext::create(type, "/dev/dri/renderD128", {}, ec);
if (!device.isValid()) {
    // ec содержит причину
}
```

Перегрузки:

```cpp
static DeviceContext create(DeviceType type,
                            const char* device = nullptr,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());

static DeviceContext create(DeviceType type,
                            const std::string& device,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());

static DeviceContext derive(const DeviceContext& source,
                            DeviceType type,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());
```

Метод `derive()` создаёт производное устройство. Пример: устройство OpenCL из устройства VAAPI. Для QSV производное устройство не используйте. Оно может привести к сбою.

### 5.3. Строка устройства, а не путь

Параметр `device` имеет тип `const char*` или `std::string`. Это сделано намеренно. Устройство не всегда является файлом.

Значение зависит от backend:

- VAAPI и DRM: путь к файлу, например `/dev/dri/renderD128`;
- CUDA: номер устройства, например `"1"`;
- D3D11VA: имя адаптера;
- VideoToolbox: имя или пустая строка;
- по умолчанию: `nullptr` или пустая строка.

`std::filesystem::path` подразумевает файл. Такой тип не может выразить номер или имя адаптера. Поэтому API использует строку.

Перегрузка с `const char*` не копирует строку. Вызывающий код держит строку живой. Перегрузка с `const std::string&` хранит копию внутри диапазона.

### 5.4. `FramesContext`

```cpp
av::hw::FramesContext frames =
    av::hw::FramesContext::create(device,
                                  av::hw::hwPixelFormat(type),
                                  AV_PIX_FMT_NV12,
                                  1920, 1080,
                                  0,
                                  ec);
```

Параметр `initialPoolSize = 0` означает размер пула по умолчанию. FFmpeg выбирает размер сам.

Методы:

```cpp
bool isValid() const;
DeviceType device() const; // копия владельца устройства
PixelFormat format() const;    // формат поверхности
PixelFormat swFormat() const;  // формат для загрузки и чтения
int width() const;
int height() const;
PixelFormatList transferFormats(AVHWFrameTransferDirection direction,
                                OptionalErrorCode ec = throws()) const;
```

Формат `swFormat` — это формат кадра в памяти системы. Не все устройства поддерживают `NV12`. Проверяйте `FrameConstraints::swFormats()`.

### 5.5. Диапазоны без выделения памяти

API возвращает диапазоны. Диапазон не выделяет память и не копирует массив.

| Функция | Тип | Владелец памяти |
|---|---|---|
| `allDeviceTypes()` | `DeviceTypeRange` | FFmpeg |
| `probeDevices(device)` | `DeviceProbeRange` | FFmpeg, проверка при обходе |
| `frameConstraints(device, ec)` | `FrameConstraints` | объект-владелец |
| `FrameConstraints::hwFormats()` | `PixelFormatRange` | `FrameConstraints` |
| `FrameConstraints::swFormats()` | `PixelFormatRange` | `FrameConstraints` |
| `codecConfigs(codec)` | `CodecConfigRange` | FFmpeg |
| `findDecoders(id, type)` | `CodecRange` | FFmpeg |
| `findEncoders(id, type)` | `CodecRange` | FFmpeg |
| `FramesContext::transferFormats()` | `PixelFormatList` | объект-владелец |

`PixelFormatList` владеет массивом `AVPixelFormat*`. Метод `range()` возвращает невладеющий вид. Сам список можно обходить напрямую.

Пример:

```cpp
for (av::hw::DeviceType type : av::hw::allDeviceTypes()) {
    std::cout << type.name() << '\n';
}

for (const av::hw::DeviceProbe& probe : av::hw::probeDevices()) {
    std::cout << probe.type.name() << ": "
              << (probe.available ? "available" : probe.error.message())
              << '\n';
}
```

`probeDevices()` создаёт временное устройство для каждого типа. Это единственная надёжная проверка. `av_hwdevice_iterate_types()` перечисляет собранные backend, а не присутствующее железо.

### 5.6. Конфигурации кодека

`codecConfigs(codec)` возвращает список `AVCodecHWConfig`. Каждая запись говорит, какой тип устройства и какой формат поддерживает кодек.

`findCodecConfig(codec, type, methodsMask)` возвращает первую подходящую запись.

`findDecoder(id, type)` и `findEncoder(id, type)` ищут кодек по идентификатору и типу устройства. Функция просматривает все кодеки с этим идентификатором. Это важно для QSV. Общий кодек `h264` не поддерживает QSV. Нужен `h264_qsv`.

`findDecoders()` и `findEncoders()` возвращают ленивый диапазон. Диапазон не выделяет память.

### 5.7. Ограничения кадров

```cpp
av::hw::FrameConstraints constraints =
    av::hw::FrameConstraints::query(device, ec);
if (constraints.isValid()) {
    for (av::PixelFormat format : constraints.swFormats())
        std::cout << format.name() << '\n';

    std::cout << constraints.maxWidth() << 'x' << constraints.maxHeight() << '\n';
}
```

`FrameConstraints` владеет структурой FFmpeg. Методы `hwFormats()` и `swFormats()` возвращают невладеющие виды.

Некоторые backend сообщают широкие границы: от `0x0` до `INT_MAX`. Не считайте эти числа точным ограничением железа.

## 6. Декодирование

### 6.1. Порядок вызовов

1. Создайте `DeviceContext`.
2. Создайте `VideoDecoderContext`. Передайте кодек, найденный через `findDecoder()`.
3. Вызовите `setHwDeviceContext(device)` до `open()`.
4. При необходимости вызовите `setHwTransferToSystem(true)`.
5. Вызовите `open()`.
6. Передавайте пакеты через `send()` и читайте кадры через `receive()`.
7. После конца потока вызовите `flush()` и дочитайте кадры.

Схема:

@image html hw-decode.svg "Последовательность декодирования"

Пример:

```cpp
av::Codec codec = av::hw::findDecoder(AV_CODEC_ID_H264, type);
av::VideoDecoderContext decoder(vst, codec);
decoder.setHwDeviceContext(device, ec);
decoder.setHwTransferToSystem(transferToSystem);
decoder.open(av::Codec(), ec);

while ((packet = ictx.readPacket(ec))) {
    if (packet.streamIndex() != vst.index())
        continue;

    for (;;) {
        const bool accepted = decoder.send(packet, ec);
        for (;;) {
            av::VideoFrame frame = decoder.receive(ec);
            if (!frame.isValid())
                break;
            consume(frame);
        }
        if (accepted)
            break;
    }
}
```

Не используйте устаревший `decode()` для аппаратного декодера, если нужны все кадры. Асинхронные backend, например QSV, могут вернуть кадр позже. Путь `send()` и `receive()` забирает все кадры. Не забудьте `flush()`.

### 6.2. Автоматическое чтение в память системы

По умолчанию аппаратный декодер возвращает кадры на устройстве. Это включает нулевое копирование.

Если приложению нужны указатели `data[]`, включите автоперенос:

```cpp
decoder.setHwTransferToSystem(true);
```

Тогда `receive()` вернёт кадр в памяти системы. Один и тот же декодер нельзя переключать между режимами на середине потока. Выберите режим до `open()`.

### 6.3. `get_format`

Декодер задаёт только `hw_device_ctx`. Статический `get_format` читает это поле и выводит тип устройства. Поле `AVCodecContext::opaque` не используется. Приложение может свободно применять `opaque` для своих целей.

Некоторые декодеры, например QSV в FFmpeg 4.x, объявляют только метод `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX`. avcpp создаёт для них пул кадров до `open()`. Размер берётся из `coded_width` и `coded_height`, формат — из ограничений устройства. Без пула FFmpeg 4.x тихо уходит в память системы.

Тихого перехода на программный декодер нет. Если аппаратный путь невозможен, API возвращает ошибку. Так приложение узнаёт о проблеме сразу.

## 7. Кодирование

### 7.1. Режимы

`EncodingMode` задаёт способ передачи кадров кодировщику.

| Режим | Значение |
|---|---|
| `Auto` | Сначала пул кадров. Если пул создать нельзя, используется устройство. |
| `Frames` | Только пул кадров. Требуется `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX`. |
| `Device` | Только устройство. Требуется `AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX`. |

`Auto` — рекомендуемый режим для нового кода.

### 7.2. Порядок вызовов

1. Создайте кодировщик.
2. Задайте ширину, высоту, время, частоту кадров и битрейт.
3. Вызовите `configureHwEncoding()`.
4. Вызовите `open()`.
5. Передавайте кадры через `encode()`.
6. Вызовите `encode()` без кадра для слива.

Схема:

@image html hw-encode.svg "Последовательность кодирования"

Пример:

```cpp
av::Codec codec = av::hw::findEncoder(AV_CODEC_ID_H264, type);
av::VideoEncoderContext encoder(codec);
encoder.setWidth(1920);
encoder.setHeight(1080);
encoder.setTimeBase(av::Rational{1, 25});
encoder.setFrameRate(av::Rational{25, 1});
encoder.setBitRate(4'000'000);

encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                            AV_PIX_FMT_NONE, ec);
encoder.open(av::Codec(), ec);
```

Параметр `swFormat = AV_PIX_FMT_NONE` включает выбор формата. Порядок выбора:

1. запрошенный формат;
2. текущий `pix_fmt` кодека;
3. `NV12` из ограничений устройства;
4. `YUV420P` из ограничений;
5. первый формат из ограничений.

### 7.3. Автоматическая загрузка

Автозагрузка включена по умолчанию. Если на вход приходит кадр в памяти системы, `encode()` сначала загружает его в пул. Затем кадр уходит в кодировщик.

Отключить автозагрузку:

```cpp
encoder.setHwAutoUpload(false);
```

Отключайте автозагрузку только для явного пути без копирования. Иначе кодировщик получит кадр неверного типа.

### 7.4. Частота кадров

Некоторым кодировщикам нужна частота кадров до `open()`. Пример: QSV. Без неё QSV возвращает ошибку «Current frame rate is unsupported».

```cpp
encoder.setFrameRate(av::Rational{25, 1});
```

## 8. Нулевое копирование

### 8.1. Идея

Декодер создаёт пул поверхностей. Кодировщик использует тот же пул. Кадр не покидает устройство.

Этот подход применяет официальный пример FFmpeg `vaapi_transcode.c`. Он работает для VAAPI, QSV и CUDA.

```cpp
av::VideoFrame first = receiveFirstHardwareFrame();
av::hw::FramesContext frames = first.hwFramesContext();

encoder.configureHwEncoding(frames, ec); // автозагрузка выключена
encoder.open(av::Codec(), ec);

// Далее каждый кадр передаётся в encode() без переноса.
```

Схема:

@image html hw-transcode-zero-copy.svg "Нулевое копирование"

### 8.2. Почему не `av_hwframe_map`

`av_hwframe_map()` не является надёжным путём между разными контекстами. Некоторые backend возвращают `ENOSYS`. Производный пул QSV может привести к сбою.

Правильный путь: взять пул из первого кадра декодера и передать его кодировщику.

### 8.3. Запасной путь

Если пул недоступен, используйте режим с копированием:

1. `frame.toSystemMemory()`;
2. `encoder.configureHwEncoding(device, Auto)`;
3. `encoder.encode(softwareFrame)` с включённой автозагрузкой.

Пример `api2-hw-transcode` делает этот выбор автоматически.

## 9. Масштабирование

`VideoRescaler` работает с памятью системы. Полный аппаратный конвейер с изменением размера требует промежуточного чтения.

Путь:

1. аппаратное декодирование;
2. `toSystemMemory()`;
3. `VideoRescaler::rescale()`;
4. `toHwFrame()` или автозагрузка;
5. аппаратное кодирование.

Схема:

@image html hw-scale-pipeline.svg "Конвейер масштабирования"

Пример: `api2-hw-scale`.

Важно: перенос между памятью системы и устройством стоит времени. Измеряйте оба пути на целевом железе. Иногда программный конвейер быстрее из-за малого размера кадра.

## 10. Передача кадров

`VideoFrame` предоставляет четыре операции.

| Метод | Направление | Примечание |
|---|---|---|
| `toSystemMemory(format, ec)` | устройство → система | `format = AV_PIX_FMT_NONE` выбирает формат автоматически |
| `toHwFrame(frames, ec)` | система → устройство | Сначала вызывает `av_hwframe_get_buffer()` |
| `mapTo(frames, flags, ec)` | отображение | Зависит от backend, может вернуть `ENOSYS` |
| `hwFramesContext()` | чтение | Возвращает пул кадра |
| `hwDeviceContext()` | чтение | Возвращает устройство пула |

`toHwFrame()` сначала выделяет поверхность в пуле. Затем вызывает `av_hwframe_transfer_data()`. Прямой вызов `av_hwframe_transfer_data()` без поверхности возвращает `EINVAL` на некоторых backend.

`VideoFrame::clone()` не клонирует аппаратный кадр. Метод возвращает `HardwareTransferUnsupported`. Копирование поверхности не является дешёвой операцией. Если нужен второй владелец, используйте копирование объекта `VideoFrame`. Копия добавляет ссылку на `AVFrame`.

После переноса или масштабирования назначьте время и индекс потока явно. Новый объект не всегда наследует `m_timeBase` обёртки avcpp.

## 11. Выбор устройства

### 11.1. Порядок выбора

1. Получите список собранных типов: `allDeviceTypes()`.
2. Проверьте наличие: `isDeviceAvailable(type)` или `probeDevices()`.
3. Найдите декодер: `findDecoder(codecId, type)`.
4. Найдите кодировщик: `findEncoder(codecId, type)`.
5. Проверьте конфигурации: `findCodecConfig()`.
6. Создайте устройство: `DeviceContext::create()`.
7. Проверьте форматы: `frameConstraints()`.

### 11.2. Платформы

| Платформа | Основные типы | Только проверка |
|---|---|---|
| Linux | `vaapi`, `qsv`, `cuda`, `vdpau` | `opencl`, `vulkan`, `drm` |
| Windows | `d3d11va`, `dxva2`, `qsv`, `cuda`, `amf` | `opencl`, `vulkan` |
| macOS | `videotoolbox` | — |
| Android | `mediacodec` | — |

«Только проверка» означает, что тип полезен для проверки устройства или пула. Аппаратный кодек для него не тестируется.

### 11.3. QSV

QSV требует отдельный кодек. Общий `h264` не подходит.

```cpp
av::Codec decoder = av::hw::findDecoder(AV_CODEC_ID_H264, qsvType);
```

Кодировщик QSV требует `setFrameRate()` до `open()`. Декодер QSV может отдать первые кадры позже. Используйте `send()` и `receive()`.

QSV с FFmpeg 4.x работает через libmfx. В `ubuntu:22.04` эта связка неполная:

- декодер работает, но кадры остаются в памяти системы; кадры на устройстве отдаёт только устаревший путь `-hwaccel qsv`;
- `av_hwframe_transfer_data()` к пулу QSV не возвращает управление; драйвер бесконечно создаёт потоки;
- `MFXVideoENCODE_Init()` отклоняет любые параметры, `open()` возвращает `AVERROR(ENOSYS)`.

Те же команды в `ffmpeg(1)` ведут себя так же. Поэтому это ограничение среды, а не avcpp. Тесты пропускают такие шаги и печатают предупреждение. Для проверки Intel на FFmpeg 4.x используйте VAAPI. Начиная с FFmpeg 5.0 QSV работает через oneVPL и этих ограничений не имеет.

## 12. Ошибки

| Код | Причина | Действие |
|---|---|---|
| `HardwareDeviceUnavailable` | Устройство не создаётся | Проверить драйвер и имя устройства |
| `HardwareDeviceInvalid` | Пустой или неверный `DeviceContext` | Создать устройство заново |
| `HardwareConfigNotFound` | Кодек не поддерживает устройство | Выбрать другой кодек или режим |
| `HardwareFramesInvalid` | Пул не создан | Проверить формат, размер и ограничения |
| `HardwareTransferUnsupported` | Перенос невозможен | Использовать память системы |
| `HardwareCodecNotFound` | Нет аппаратного кодека | Использовать программный кодек |
| `CodecAlreadyOpened` | Настройка после `open()` | Настроить кодек до `open()` |

Все функции принимают `OptionalErrorCode`. По умолчанию ошибка вызывает исключение. Передайте `std::error_code`, если нужен возврат кода.

## 13. Соответствие FFmpeg и avcpp

| FFmpeg | avcpp |
|---|---|
| `AVHWDeviceType` | `av::hw::DeviceType` |
| `AVHWDeviceContext` | `av::hw::DeviceContext` |
| `AVHWFramesContext` | `av::hw::FramesContext` |
| `AVHWFramesConstraints` | `av::hw::FrameConstraints` |
| `AVCodecHWConfig` | `av::hw::codecConfigs()` |
| `av_hwdevice_iterate_types()` | `av::hw::allDeviceTypes()` |
| `av_hwdevice_ctx_create()` | `DeviceContext::create()` |
| `av_hwdevice_ctx_create_derived_opts()` | `DeviceContext::derive()` |
| `av_hwframe_ctx_alloc()` + `av_hwframe_ctx_init()` | `FramesContext::create()` |
| `av_hwframe_transfer_get_formats()` | `FramesContext::transferFormats()` |
| `av_hwframe_get_buffer()` + `av_hwframe_transfer_data()` | `VideoFrame::toHwFrame()` |
| `av_hwframe_transfer_data()` в систему | `VideoFrame::toSystemMemory()` |
| `av_hwframe_map()` | `VideoFrame::mapTo()` |
| `avcodec_get_hw_config()` | `av::hw::codecConfigs()` |

## 14. Совместимость FFmpeg

Минимальная версия — FFmpeg 4.0.

- `AVCPP_HAS_HWCONTEXT` показывает, доступен ли API.
- `av_hwdevice_ctx_create_derived_opts()` появился в FFmpeg 4.3. Для старых версий есть запасной путь.
- Декодеры, которые объявляют только `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX`, получают пул кадров от avcpp. Это касается QSV в FFmpeg 4.x.
- Типы устройств и форматы ищутся по имени. Код не зависит от новых значений перечисления.
- Сборка `AV_DISABLE_AVFORMAT=ON` не содержит аппаратных тестов и примеров с демультиплексором. Библиотека при этом собирается.

## 15. Проверка

Тесты находятся в `tests/Hardware.cpp` и `tests/Fixtures.cpp`. Тесты не должны падать на машине без железа.

Правило:

- нет устройства или кодека → `SKIP`;
- устройство есть, но не создаётся → `FAIL`.

Переменные окружения:

```sh
AVCPP_HW_DEVICES=vaapi,qsv   # ограничить набор, включая фикстуры
AVCPP_HW_SKIP=cuda           # исключить backend
```

Переменные действуют и на тесты с фикстурами.

Набор по умолчанию:

- Linux: VAAPI, QSV, CUDA, VDPAU;
- Windows: D3D11VA, DXVA2, QSV, CUDA, AMF;
- macOS: VideoToolbox;
- Android: MediaCodec.

Команды:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure

./build/tests/test_executor '[hw]'
./build/tests/test_executor '*Hardware fixtures*'
```

Проверено:

- FFmpeg 9.x на Linux: VAAPI, QSV, CUDA — все аппаратные тесты и фикстуры проходят;
- FFmpeg 4.4 в `ubuntu:22.04` с проброшенным Intel (VAAPI): все 91 утверждение `[hw]` и обе фикстуры с переносом кадров;
- FFmpeg 4.4 в `ubuntu:22.04` с проброшенным Intel (QSV): 60 утверждений `[hw]`; QSV пропускает перенос, кодировщик и кадры на устройстве;
- FFmpeg 4.4 в `ubuntu:22.04` с проброшенной NVIDIA (CUDA): 96 утверждений `[hw]` и обе фикстуры, включая нулевое копирование;
- сборка без `libavformat`: 49 тестов;
- сборка библиотеки на C++17;
- Meson: сборка и тесты;
- Valgrind: нет ошибок памяти в коде avcpp. Отчёты содержат только кэши драйверов NVIDIA, CUDA, OpenCL и DBus.

Проверка в Docker:

```sh
# Intel (VAAPI и QSV)
docker run --rm --memory=3g --device /dev/dri:/dev/dri \
    -e LIBVA_DRIVER_NAME=iHD -e AVCPP_HW_DEVICES=vaapi,qsv \
    -v "$PWD":/src:ro -w /src IMAGE bash -lc \
    'cmake -B /tmp/b -DAVCPP_TEST_DATA=ON ... && cmake --build /tmp/b -j && /tmp/b/tests/test_executor "[hw]"'

# NVIDIA (CUDA): проброс устройств и библиотек драйвера
docker run --rm --memory=4g \
    --device /dev/nvidia0 --device /dev/nvidiactl --device /dev/nvidia-uvm \
    --device /dev/nvidia-uvm-tools --device /dev/nvidia-modeset \
    -v /dev/nvidia-caps:/dev/nvidia-caps -v /path/to/nvidia-libs:/nvidia:ro \
    -e LD_LIBRARY_PATH=/nvidia -e AVCPP_HW_DEVICES=cuda \
    -v "$PWD":/src:ro -w /src IMAGE bash -lc \
    'cmake -B /tmp/b ... && /tmp/b/tests/test_executor "*Hardware fixtures*"'
```

Без `nvidia-container-toolkit` флаг `--gpus all` не работает. Пробросьте устройства и библиотеки драйвера вручную.

Ограничьте память контейнера и задайте `timeout`. Тест QSV на FFmpeg 4.x без этих мер исчерпывает память.

## 16. Примеры

В каталоге `example/api2-samples`:

| Пример | Назначение |
|---|---|
| `api2-hw-probe` | Список устройств, ограничений и конфигураций кодеков |
| `api2-hw-decode` | Аппаратное декодирование, режим с переносом и без |
| `api2-hw-encode` | Синтетические кадры и аппаратный кодировщик |
| `api2-hw-transcode` | Транскодирование без копирования и запасной путь |
| `api2-hw-scale` | Аппаратное декодирование, масштабирование, кодирование |

Запуск:

```sh
./build/example/api2-samples/api2-hw-probe
./build/example/api2-samples/api2-hw-decode vaapi input.mp4 --to-system
./build/example/api2-samples/api2-hw-transcode vaapi input.mp4 output.mkv
./build/example/api2-samples/api2-hw-scale vaapi input.mp4 scaled.mkv
```

## 17. Ограничения

- API не выбирает устройство автоматически. Приложение задаёт тип и имя.
- API не делает тихий переход на программный кодек. Это решение приложения.
- `VideoRescaler` не работает с поверхностями напрямую.
- `VideoFrame::clone()` не копирует аппаратный кадр.
- `mapTo()` зависит от backend.
- Производный пул QSV не используется.
- VideoToolbox в FFmpeg 4.x не создаёт пул для ручного frames-контекста и не отдаёт ограничения. Работают только пути через кодек: декодер и кодировщик создают пул сами.
- QSV в FFmpeg 4.x: нет переноса кадров, нет кодировщика, декодер отдаёт кадры в памяти системы. Подробности в разделе 11.3.
- Устройство CUDA может стать недоступным после сна или смены драйвера. Проверяйте его заново.
- Набор форматов и ограничений зависит от драйвера.
- Аппаратный путь не всегда быстрее. Измеряйте результат.

## 18. Чек-лист

- [ ] Вызван `av::init()` до работы с библиотекой.
- [ ] Тип устройства получен через `DeviceType::fromName()`.
- [ ] Устройство создано через `DeviceContext::create()`.
- [ ] Ошибки проверяются через `std::error_code`.
- [ ] Декодер настроен до `open()`.
- [ ] Для QSV найден отдельный кодек через `findDecoder()`.
- [ ] Для QSV задана частота кадров до `open()`.
- [ ] Режим переноса выбран до `open()`.
- [ ] Для нулевого копирования используется пул из первого кадра.
- [ ] Автозагрузка выключена только для нулевого копирования.
- [ ] Время и индекс потока заданы после переноса.
- [ ] Пул уничтожается раньше устройства.
- [ ] Тесты без железа дают `SKIP`, а не `FAIL`.
- [ ] `docs/` и `PLAN.md` не добавлены в Git.
