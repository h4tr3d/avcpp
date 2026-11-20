@page api_nav Навигатор по API

@tableofcontents

Справочник API генерируется из комментариев в исходниках и существует только на английском: это
осознанное решение, комментарии пишутся один раз и не переводятся. Эта страница — карта: какая
задача какие типы требует и где именно они описаны.

Ссылки ведут в английский справочник; русские руководства объясняют последовательность вызовов.

| Нужно… | Типы | Справочник | Руководство |
|---|---|---|---|
| инициализировать библиотеку | `av::init()` | [core](../group__core.html) | @ref guide_getting_started |
| открыть файл или поток | `av::FormatContext`, `av::InputFormat` | [avformat](../group__avformat.html) | @ref guide_getting_started |
| перечислить потоки | `av::Stream` | [avformat](../group__avformat.html) | @ref guide_getting_started |
| читать сжатые данные | `av::Packet` | [avformat](../group__avformat.html) | @ref guide_decode |
| декодировать видео | `av::VideoDecoderContext`, `av::VideoFrame` | [codec](../group__codec.html), [frame](../group__frame.html) | @ref guide_decode |
| декодировать аудио | `av::AudioDecoderContext`, `av::AudioSamples` | [codec](../group__codec.html), [frame](../group__frame.html) | @ref guide_decode |
| декодировать субтитры | `av::SubtitleDecoderContext`, `av::SubtitleFrame` | [subtitle](../group__subtitle.html) | @ref guide_subtitles |
| кодировать видео | `av::VideoEncoderContext` | [codec](../group__codec.html) | @ref guide_encode |
| кодировать аудио | `av::AudioEncoderContext` | [codec](../group__codec.html) | @ref guide_encode |
| писать контейнер | `av::FormatContext`, `av::OutputFormat` | [avformat](../group__avformat.html) | @ref guide_encode |
| копировать потоки без декодирования | `av::CodecParameters` | [avformat](../group__avformat.html) | @ref guide_remux |
| фильтровать кадры | `av::FilterGraph`, `av::BufferSrcFilterContext`, `av::BufferSinkFilterContext` | [filter](../group__filter.html) | @ref guide_filters |
| масштабировать видео | `av::VideoRescaler` | [frame](../group__frame.html) | @ref guide_filters |
| ресемплировать аудио | `av::AudioResampler` | [frame](../group__frame.html) | @ref guide_filters |
| использовать GPU | `av::hw::DeviceContext`, `av::hw::FramesContext` | [hw](../group__hw.html) | @ref guide_hw_codecs |
| обрабатывать ошибки | `av::OptionalErrorCode`, `av::Errors`, `av::Exception` | [core](../group__core.html) | @ref guide_errors |
| работать со временем | `av::Timestamp`, `av::Rational` | [core](../group__core.html) | — |
| хранить опции | `av::Dictionary`, `av::DictionaryArray` | [core](../group__core.html) | — |
| владеть сырыми данными | `av::BufferRef`, `av::BufferRefView` | [core](../group__core.html) | — |

## Четыре объекта, вокруг которых все построено

Почти любая операция двигает данные между четырьмя типами:

- **`av::Packet`** — сжатые байты, один поток, одна метка времени.
- **`av::VideoFrame` / `av::AudioSamples`** — несжатые данные плюс описание формата.
- **`av::CodecContext2` и наследники** — состояние, превращающее одно в другое.
- **`av::FormatContext`** — контейнер, который хранит потоки, а значит и пакеты.

Если вы знаете, какой из этих четырех типов функция принимает и возвращает, вы знаете, где
находитесь в конвейере. Переходы разобраны в @ref guide_decode и @ref guide_encode, а
@ref guide_remux их вообще пропускает.

## Соглашения об именах

- Пары `setX()` / `x()` — прямой доступ к свойству; они не делают ввода-вывода и не падают.
- `create()` возвращает значение и может завершиться ошибкой; конструкторов, которые могут
  упасть, в API намеренно нет.
- Типы `*Range`, `*View`, `*List` — невладеющие виды на память, которой владеет кто-то другой.
- `raw()` возвращает указатель FFmpeg, когда нужно вызвать FFmpeg напрямую.
