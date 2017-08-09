#include "audiotrack.h"

#include "cosine.h"

AudioTrack::AudioTrack()
    : initialized_(false),
      channels_(0),
      level_(UNIT_LEVEL),
      fade_mode_(Fade::None),
      fade_length_(0),
      fade_progress_(0),
      initial_level_(0),
      final_level_(0),
      reader_(nullptr, nullptr, nullptr),
      file_context_(nullptr),
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
      level_(UNIT_LEVEL),
      fade_mode_(Fade::None),
      fade_length_(0),
      fade_progress_(0),
      initial_level_(0),
      final_level_(0),
      reader_(tell_callback, seek_callback, read_callback),
      file_context_(nullptr),
      running_(false),
      stopping_(false)
{
}

bool AudioTrack::start(void *file_context, Mode mode, int32_t level, AudioTrack::Fade fade_mode, uint32_t fade_length)
{
    if (!initialized_) {
        return false;
    }

    running_ = false;
    stopping_ = false;

    if (!reader_.open(file_context, mode)) {
        return false;
    }

    file_context_ = file_context;

    if (reader_.channels() > MAX_TRACK_CHANNELS) {
        reader_.close();
        return false;
    }

    if (reader_.channels() != channels_) {
        if ((channels_ != 1) && (reader_.channels() != 1)) {
            reader_.close();
            return false;
        }
    }

    level_ = 0;

    running_ = true;

    fade(level, fade_mode, fade_length);

    return true;
}

void AudioTrack::fade(int32_t level, AudioTrack::Fade fade_mode, uint32_t fade_length)
{
    if (!initialized_) {
        return;
    }

    if (!running_) {
        return;
    }

    fade_mode_ = fade_mode;

    if (fade_mode_ != Fade::None) {
        fade_length_ = fade_length;
        fade_progress_ = 0;

        initial_level_ = level_;
        final_level_ = level;
    } else {
        fade_length_ = 0;
        fade_progress_ = 0;

        initial_level_ = level;
        level_ = level;
        final_level_ = level;
    }
}

void AudioTrack::stop(AudioTrack::Fade fade_mode, uint32_t fade_length)
{
    if (!initialized_) {
        return;
    }

    fade(0, fade_mode, fade_length);

    if (fade_mode_ != Fade::None) {
        stopping_ = true;
    } else {
        reader_.close();

        stopping_ = false;
        running_ = false;
    }
}

size_t AudioTrack::play(int16_t *buffer, size_t frames)
{
    if (!initialized_) {
        return 0;
    }

    if (!running_) {
        return 0;
    }

    int16_t input_frame[MAX_TRACK_CHANNELS];

    for (size_t frame_index = 0; frame_index < frames; frame_index++) {
        if (reader_.decodeToI16(input_frame, 1) < 1) {
            return frame_index;
        }

        if (channels_ == reader_.channels()) {
            for (unsigned int channel = 0; channel < channels_; channel++) {
                int32_t sample = (input_frame[channel] * level_) / UNIT_LEVEL;
                buffer[channels_ * frame_index + channel] = static_cast<int16_t>(sample);
            }
        } else if (reader_.channels() == 1) {
            int32_t sample = (input_frame[0] * level_) / UNIT_LEVEL;
            for (unsigned int channel = 0; channel < channels_; channel++) {
                buffer[channels_ * frame_index + channel] = static_cast<int16_t>(sample);
            }
        } else if (channels_ == 1) {
            int32_t sample = 0;
            for (unsigned int channel = 0; channel < reader_.channels(); channel++) {
                sample += input_frame[channel];
            }
            sample /= static_cast<int32_t>(reader_.channels());
            sample *= level_;
            sample /= UNIT_LEVEL;
            buffer[channels_ * frame_index] = static_cast<int16_t>(sample);
        } else {
            return 0;
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

            int64_t level_offset = final_level_ - initial_level_;

            switch (fade_mode_) {
            case Fade::LinearIn:
            case Fade::LinearOut:
                level_offset *= fade_progress_;
                level_offset /= fade_length_;
                level_ = initial_level_ + static_cast<int32_t>(level_offset);
                break;
            case Fade::CosineIn:
                level_offset *= cosineFromZeroToHalfPi(fade_length_ - fade_progress_, fade_length_);
                level_offset /= 32768;
                level_ = initial_level_ + static_cast<int32_t>(level_offset);
                break;
            case Fade::CosineOut:
                level_offset *= 32768 - cosineFromZeroToHalfPi(fade_progress_, fade_length_);
                level_offset /= 32768;
                level_ = initial_level_ + static_cast<int32_t>(level_offset);
                break;
            case Fade::SCurveIn:
            case Fade::SCurveOut:
                level_offset *= 32768 - cosineFromZeroToHalfPi(fade_progress_ * 2, fade_length_);
                level_offset /= 65536;
                level_ = initial_level_ + static_cast<int32_t>(level_offset);
                break;
            case Fade::None:
                break;
            }
        }
    }

    return frames;
}
