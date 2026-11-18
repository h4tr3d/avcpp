@page guide_getting_started Начало работы

@tableofcontents

Здесь собирается минимальная полезная программа: открыть файл, найти видеопоток, декодировать его
и напечатать по строке на кадр. Это основа, на которую опираются все остальные руководства.

Что нужно: FFmpeg 4.0 или новее и компилятор с C++17. Если библиотека еще не установлена, см.
[руководство по сборке](../guide_building.html).

## Программа

```cpp
#include <iostream>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input file>\n";
        return 1;
    }

    av::init();

    std::error_code ec;
    av::FormatContext ictx;
    ictx.openInput(argv[1], ec);
    if (ec) {
        std::cerr << "Can't open input: " << ec.message() << '\n';
        return 1;
    }

    ictx.findStreamInfo(ec);
    if (ec) {
        std::cerr << "Can't find streams: " << ec.message() << '\n';
        return 1;
    }

    av::Stream vst;
    for (auto &&st : ictx.streams()) {
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
            vst = st;
            break;
        }
    }

    if (!vst.isValid()) {
        std::cerr << "Video stream not found\n";
        return 1;
    }

    av::VideoDecoderContext dec{vst};
    dec.open(ec);
    if (ec) {
        std::cerr << "Can't open codec: " << ec.message() << '\n';
        return 1;
    }

    for (;;) {
        av::Packet pkt = ictx.readPacket(ec);
        if (ec) {
            std::cerr << "Read error: " << ec.message() << '\n';
            return 1;
        }
        if (!pkt)
            break; // конец файла

        if (pkt.streamIndex() != vst.index())
            continue;

        if (!dec.send(pkt, ec)) {
            std::cerr << "Can't send a packet: " << ec.message() << '\n';
            return 1;
        }

        for (;;) {
            av::VideoFrame frame = dec.receive(ec);
            if (ec) {
                std::cerr << "Can't receive a frame: " << ec.message() << '\n';
                return 1;
            }
            if (!frame)
                break;

            std::cout << frame.pts().seconds() << " s, "
                      << frame.width() << 'x' << frame.height()
                      << ", format " << frame.pixelFormat().name() << '\n';
        }
    }
}
```

## Что делает каждый шаг

**`av::init()`** регистрирует кодеки и форматы FFmpeg. Вызывайте ее один раз, до любого другого
вызова avcpp или FFmpeg. До нее все остальное небезопасно.

**`av::FormatContext`** владеет входом. `openInput()` определяет контейнер, `findStreamInfo()`
читает параметры потоков. Второй вызов пропускать нельзя: параметры кодека часто неизвестны, пока
файл не прочитан.

**`av::Stream`** — дескриптор одного потока внутри контекста. Его можно копировать, это дешево;
уничтожение копии не уничтожает поток. `mediaType()`, `index()` и `timeBase()` — те методы,
которые нужны чаще всего.

**`av::VideoDecoderContext`** создается из потока и берет кодек из параметров потока. `open()` без
аргументов — обычный случай; остальные аргументы нужны для явного выбора кодека и опций.

**`send()` / `receive()`** — современный API кодеков FFmpeg и единственный корректный для
асинхронных декодеров. Пакет обычно дает ноль, один или несколько кадров; кадр может появиться
через много пакетов, а отклоненный `send()` означает «осушите декодер и повторите». Подробно
контракт разобран в @ref guide_decode.

## Ошибки

Любой вызов, который может завершиться ошибкой, принимает `OptionalErrorCode`. По умолчанию это
`throws()`, то есть исключение `av::Exception` (наследник `std::system_error`). Если передать
`std::error_code&`, вызов перестает бросать и записывает ошибку туда. Поэтому в программе выше
объявлен один `std::error_code ec` и переиспользуется.

Внутри одного вызова стили не смешиваются: либо вы получаете код, либо исключение. Категории и
полный список кодов — в @ref guide_errors.

## Линковка

@code{.cmake}
add_executable(probe main.cpp)
target_link_libraries(probe PRIVATE avcpp::avcpp)
@endcode

## Дальше

- @ref guide_decode — время кадров, слив и почему `receive()` возвращает пустые значения.
- @ref guide_encode — запись кадров обратно в контейнер.
- @ref guide_errors — модель ошибок целиком.
- @ref guide_hw_codecs — тот же сценарий на GPU.
