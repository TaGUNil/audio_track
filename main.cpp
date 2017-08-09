#include <cstdio>
#include <cstdlib>
#include <vector>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "audiotrack.h"

size_t tell_callback(void *file_context)
{
    long result = ftell(reinterpret_cast<FILE *>(file_context));

    if (result >= 0) {
        return static_cast<size_t>(result);
    }

    return 0;
}

bool seek_callback(void *file_context, size_t offset)
{
    int result = fseek(reinterpret_cast<FILE *>(file_context),
                       static_cast<long>(offset),
                       SEEK_SET);

    if (result >= 0) {
        return true;
    }

    return false;
}

size_t read_callback(void *file_context, uint8_t *buffer, size_t length)
{
    return fread(buffer,
                 1,
                 length,
                 reinterpret_cast<FILE *>(file_context));
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file> <level> [mode]\n", argv[0]);
        return 1;
    }

    FILE *wav_file = fopen(argv[1], "r");
    if (wav_file == nullptr) {
        fprintf(stderr, "Cannot open file \"%s\"\n", argv[1]);
        return 1;
    }

    AudioTrack track(&tell_callback,
                     &seek_callback,
                     &read_callback,
                     2);

    AudioTrack::Mode mode = AudioTrack::Mode::Single;

    if (argc >= 4) {
        switch (argv[3][0]) {
        case 'c':
            mode = AudioTrack::Mode::Continuous;
            break;
        case 's':
        default:
            mode = AudioTrack::Mode::Single;
        }
    }

    switch (mode) {
    case AudioTrack::Mode::Single:
        printf("Single mode\n");
        break;
    case AudioTrack::Mode::Continuous:
        printf("Continuous mode\n");
        break;
    }

    int32_t level = static_cast<int32_t>(atof(argv[2]) * AudioTrack::UNIT_LEVEL);

    if (!track.start(wav_file, mode, level)) {
        fprintf(stderr, "Cannot play file\n");
        return 1;
    }

    pa_sample_spec sample_format = {
        .format = PA_SAMPLE_S16NE,
        .rate = static_cast<uint32_t>(track.samplingRate()),
        .channels = static_cast<uint8_t>(track.channels())
    };

    pa_simple *stream = nullptr;
    int error;

    stream = pa_simple_new(nullptr,
                           argv[0],
                           PA_STREAM_PLAYBACK,
                           nullptr,
                           "playback",
                           &sample_format,
                           nullptr,
                           nullptr,
                           &error);
    if (stream == nullptr) {
        fprintf(stderr,
                "pa_simple_new() failed: %s\n",
                pa_strerror(error));
        return 1;
    }

    static const size_t MAX_BUFFER_FRAMES = 1024;
    std::vector<int16_t> buffer(MAX_BUFFER_FRAMES * track.channels());

    for (;;) {
        size_t frames = track.play(buffer.data(), MAX_BUFFER_FRAMES);
        if (frames == 0) {
            break;
        }

        if (pa_simple_write(stream,
                            buffer.data(),
                            frames * track.channels() * sizeof(int16_t),
                            &error) < 0) {
            fprintf(stderr,
                    "pa_simple_write() failed: %s\n",
                    pa_strerror(error));
            pa_simple_free(stream);
            return 1;
        }
    }

    if (pa_simple_drain(stream, &error) < 0) {
        fprintf(stderr,
                "pa_simple_drain() failed: %s\n",
                pa_strerror(error));
        pa_simple_free(stream);
        return 1;
    }

    pa_simple_free(stream);

    return 0;
}
