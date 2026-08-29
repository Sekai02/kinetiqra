#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace kinetiqra::anim {

// Where playback has got to.
//
// Deliberately free of clips, scenes and windows: it is a clock with a duration
// and a couple of switches, which makes the behaviour at the ends of a clip
// testable without any of the rest.
class Player {
public:
    void advance(float delta_seconds) {
        if (!playing_ || duration_ <= 0.0F) {
            return;
        }

        time_ += delta_seconds * speed_;

        if (looping_) {
            // Wrapped rather than clamped, and with fmod-like behaviour in both
            // directions so that a negative speed runs backwards off the start
            // and reappears at the end.
            time_ = std::fmod(time_, duration_);
            if (time_ < 0.0F) {
                time_ += duration_;
            }
            return;
        }

        if (time_ >= duration_) {
            time_ = duration_;
            playing_ = false;
        } else if (time_ <= 0.0F) {
            time_ = 0.0F;
            playing_ = false;
        }
    }

    void play() {
        // Starting again at the very end rewinds, which is what pressing play on
        // a finished clip is asking for.
        if (!looping_ && time_ >= duration_) {
            time_ = 0.0F;
        }
        playing_ = true;
    }

    void pause() { playing_ = false; }

    void stop() {
        playing_ = false;
        time_ = 0.0F;
    }

    void set_time(float seconds) { time_ = std::clamp(seconds, 0.0F, duration_); }

    void set_duration(float seconds) {
        duration_ = std::max(seconds, 0.0F);
        time_ = std::clamp(time_, 0.0F, duration_);
    }

    void set_looping(bool looping) { looping_ = looping; }

    void set_speed(float speed) { speed_ = speed; }

    [[nodiscard]] float time() const { return time_; }

    [[nodiscard]] float duration() const { return duration_; }

    [[nodiscard]] bool playing() const { return playing_; }

    [[nodiscard]] bool looping() const { return looping_; }

    [[nodiscard]] float speed() const { return speed_; }

private:
    float time_{0.0F};
    float duration_{0.0F};
    float speed_{1.0F};
    bool playing_{false};
    bool looping_{true};
};

}  // namespace kinetiqra::anim
