#pragma once

#include <cstddef>
#include <cstdint>

#include "wavreader.h"

class AudioTrack
{
public:
    typedef WavReader::Mode Mode;

    enum class Fade
    {
        None,
        LinearIn,
        LinearOut,
        CosineIn,
        CosineOut,
        SCurveIn,
        SCurveOut
    };

    static const int32_t UNIT_LEVEL = 65536;

    static const unsigned int MAX_TRACK_CHANNELS = 2;

public:
    AudioTrack();

    AudioTrack(WavReader::TellCallback tell_callback,
               WavReader::SeekCallback seek_callback,
               WavReader::ReadCallback read_callback,
               unsigned int channels);

    bool start(void *file_context, Mode mode, int32_t level, Fade fade_mode = Fade::None, uint32_t fade_length = 0);

    void fade(int32_t level, Fade fade_mode = Fade::None, uint32_t fade_length = 0);

    void stop(Fade fade_mode = Fade::None, uint32_t fade_length = 0);

    size_t play(int16_t *buffer, size_t frames);

    bool running()
    {
        return running_;
    }

    void *playingNow()
    {
        return running_ ? file_context_ : nullptr;
    }

    unsigned int channels()
    {
        return channels_;
    }

    unsigned long samplingRate()
    {
        return reader_.samplingRate();
    }

private:
    bool initialized_;

    unsigned int channels_;

    int32_t level_;

    Fade fade_mode_;

    uint32_t fade_length_;
    uint32_t fade_progress_;

    int32_t initial_level_;
    int32_t final_level_;

    WavReader reader_;
    void *file_context_;

    bool running_;
    bool stopping_;
};
