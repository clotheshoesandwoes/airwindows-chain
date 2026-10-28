#pragma once

#include <array>
#include <cmath>

namespace awchain
{
// How loud a stereo signal sounds, K-weighted the way LUFS meters weigh it
// (ITU BS.1770: a high shelf for the head, a high-pass for the lows), averaged
// over about the last three seconds. Audio thread; allocates nothing.
class Loudness
{
  public:
    void prepare(double sampleRate)
    {
        // The filters for any sample rate, as libebur128 derives them.
        constexpr double pi = 3.14159265358979323846;
        double k = std::tan(pi * 1681.974450955533 / sampleRate);
        const double q = 0.7071752369554196;
        const double vh = std::pow(10.0, 3.999843853973347 / 20.0), vb = std::pow(vh, 0.4996667741545416);
        double a0 = 1.0 + k / q + k * k;
        shelf = {(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0};

        k = std::tan(pi * 38.13547087602444 / sampleRate);
        const double qh = 0.5003270373238773;
        a0 = 1.0 + k / qh + k * k;
        highPass = {1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / qh + k * k) / a0};

        smoothing = 1.0 - std::exp(-1.0 / (3.0 * sampleRate));
        state = {};
        meanSquare = 0.0;
    }

    void measure(const float *left, const float *right, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double l = weigh(0, left[i]), r = weigh(1, right[i]);
            meanSquare += smoothing * (l * l + r * r - meanSquare);
        }
        // One bad sample from an effect would otherwise stick here for good.
        if (!std::isfinite(meanSquare))
        {
            state = {};
            meanSquare = 0.0;
        }
    }

    double power() const { return meanSquare; } // LUFS = -0.691 + 10 log10(power)

  private:
    struct Biquad
    {
        double b0, b1, b2, a1, a2;
    };
    struct State
    {
        double x1, x2, y1, y2;
    };

    Biquad shelf{}, highPass{};
    std::array<State, 4> state{}; // shelf then high-pass, per channel
    double smoothing{0.0}, meanSquare{0.0};

    static double run(const Biquad &f, State &s, double x)
    {
        const double y = f.b0 * x + f.b1 * s.x1 + f.b2 * s.x2 - f.a1 * s.y1 - f.a2 * s.y2;
        s = {x, s.x1, y, s.y1};
        return y;
    }

    double weigh(int channel, double x)
    {
        return run(highPass, state[(size_t)channel * 2 + 1], run(shelf, state[(size_t)channel * 2], x));
    }
};
} // namespace awchain
