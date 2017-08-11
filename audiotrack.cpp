#include "audiotrack.h"

#ifdef HAS_COSINE_TABLE
#include "cosine.h"
#endif

AudioTrack::AudioTrack()
    : initialized_(false),
      channels_(0),
      upmixing_(1),
      frames_per_ms_(0),
      level_(UNIT_LEVEL),
      fade_mode_(Fade::None),
      fade_length_ms_(0),
      fade_length_(0),
      fade_progress_(0),
      initial_level_(0),
      final_level_(0),
      reader_(nullptr, nullptr, nullptr),
      file_(nullptr),
      running_(false),
      stopping_(false)
{
}

AudioTrack::AudioTrack(WavReader::TellCallback tell_callback,
                       WavReader::SeekCallback seek_callback,
                       WavReader::ReadCallback read_callback,
                       unsigned int channels)
    : initialized_(true),
      channels_(channels),
      upmixing_(1),
      frames_per_ms_(0),
      level_(UNIT_LEVEL),
      fade_mode_(Fade::None),
      fade_length_ms_(0),
      fade_length_(0),
      fade_progress_(0),
      initial_level_(0),
      final_level_(0),
      reader_(tell_callback, seek_callback, read_callback),
      file_(nullptr),
      running_(false),
      stopping_(false)
{
}

bool AudioTrack::start(void *file, Mode mode, uint16_t level, AudioTrack::Fade fade_mode, uint16_t fade_length_ms)
{
    if (!initialized_) {
        return false;
    }

    running_ = false;
    stopping_ = false;

    if (!reader_.open(file, mode)) {
        return false;
    }

    file_ = file;

    if (reader_.channels() > MAX_TRACK_CHANNELS) {
        reader_.close();
        return false;
    }

    if (reader_.channels() != channels_) {
        if (channels_ % reader_.channels() != 0) {
            reader_.close();
            return false;
        }

        upmixing_ = channels_ / reader_.channels();
    } else {
        upmixing_ = 1;
    }

    // Not exactly precise, but good for overflow-free conversions
    frames_per_ms_ = static_cast<uint16_t>(reader_.samplingRate() / 1000);

    level_ = 0;

    running_ = true;

    fade(level, fade_mode, fade_length_ms);

    return true;
}

void AudioTrack::fade(uint16_t level, AudioTrack::Fade fade_mode, uint16_t fade_length_ms)
{
    if (!initialized_) {
        return;
    }

    if (!running_) {
        return;
    }

    if (level > MAX_LEVEL) {
        level = MAX_LEVEL;
    }

    fade_mode_ = fade_mode;

    if (fade_mode_ != Fade::None) {
        fade_length_ms_ = fade_length_ms;
        fade_length_ = fade_length_ms * frames_per_ms_;
        fade_progress_ = 0;

        initial_level_ = level_;
        final_level_ = level;
    } else {
        fade_length_ms_ = 0;
        fade_length_ = 0;
        fade_progress_ = 0;

        initial_level_ = level;
        level_ = level;
        final_level_ = level;
    }
}

void AudioTrack::stop(AudioTrack::Fade fade_mode, uint16_t fade_length_ms)
{
    if (!initialized_) {
        return;
    }

    fade(0, fade_mode, fade_length_ms);

    if (fade_mode_ != Fade::None) {
        stopping_ = true;
    } else {
        reader_.close();

        stopping_ = false;
        running_ = false;
    }
}

void AudioTrack::rewind()
{
    if (!initialized_) {
        return;
    }

    if (!running_) {
        return;
    }

    reader_.rewind();
}

size_t AudioTrack::play(int16_t *buffer, size_t frames)
{
    if (!initialized_) {
        return 0;
    }

    if (!running_) {
        return 0;
    }

    frames = reader_.decodeToI16(buffer, frames, upmixing_);

    for (size_t frame_index = 0; frame_index < frames; frame_index++) {
        if (level_ != UNIT_LEVEL) {
            for (unsigned int channel = 0; channel < channels_; channel++) {
                int32_t sample = (buffer[channels_ * frame_index + channel] * level_) / UNIT_LEVEL;
                buffer[channels_ * frame_index + channel] = static_cast<int16_t>(sample);
            }
        }

        if (fade_mode_ != Fade::None) {
            if (fade_progress_ == fade_length_) {
                if (stopping_) {
                    stop(Fade::None, 0);
                } else {
                    fade(final_level_, Fade::None, 0);
                }
            }

            fade_progress_++;

            int32_t level_offset = static_cast<int32_t>(final_level_) - static_cast<int32_t>(initial_level_);
            uint16_t fade_progress_ms;

            switch (fade_mode_) {
            case Fade::LinearIn:
            case Fade::LinearOut:
                fade_progress_ms = static_cast<uint16_t>(fade_progress_ / frames_per_ms_);
                level_offset *= fade_progress_ms;
                level_offset /= fade_length_ms_;
                level_ = initial_level_ + static_cast<uint16_t>(level_offset);
                break;
#ifdef HAS_COSINE_TABLE
            case Fade::CosineIn:
                fade_progress_ms = static_cast<uint16_t>(fade_progress_ / frames_per_ms_);
                level_offset *= cosineFromZeroToHalfPi(fade_length_ms_ - fade_progress_ms, fade_length_ms_);
                level_offset /= 32768;
                level_ = initial_level_ + static_cast<uint16_t>(level_offset);
                break;
            case Fade::CosineOut:
                fade_progress_ms = static_cast<uint16_t>(fade_progress_ / frames_per_ms_);
                level_offset *= 32768 - cosineFromZeroToHalfPi(fade_progress_ms, fade_length_ms_);
                level_offset /= 32768;
                level_ = initial_level_ + static_cast<uint16_t>(level_offset);
                break;
            case Fade::SCurveIn:
            case Fade::SCurveOut:
                fade_progress_ms = static_cast<uint16_t>(fade_progress_ / frames_per_ms_);
                level_offset *= 32768 - cosineFromZeroToHalfPi(fade_progress_ms * 2, fade_length_ms_);
                level_offset /= 65536;
                level_ = initial_level_ + static_cast<uint16_t>(level_offset);
                break;
#endif
            case Fade::None:
                break;
            }
        }
    }

    return frames;
}
