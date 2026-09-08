#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ember
{
    // Global organic-motion generator: a Lorenz attractor (x, y, z) plus a smooth fractal Perlin
    // source and a two-layer fractal sample & hold. Runs once per audio block; voices interpolate
    // between the block's start and end values.
    class ChaosEngine
    {
    public:
        static constexpr int numOutputs = 5;   // lorenz x, y, z, perlin, s&h
        using Frame = std::array<float, numOutputs>;

        void prepare (double sr)
        {
            sampleRate = sr;
            reset();
        }

        void reset()
        {
            x = 1.0; y = 1.0; z = 1.05;
            for (int i = 0; i < 3000; ++i)   // settle onto the attractor
                step (0.01);

            perlinT = 0.0;
            phase1 = phase2 = 0.0;
            held1 = nextRandom();
            held2 = nextRandom();
            current = outputs();
        }

        // Advances by numSamples. `prev` receives the outputs at the block start, `next` at its end.
        void process (int numSamples, float lorenzRate, float fractalRate, Frame& prev, Frame& next)
        {
            prev = current;

            const double dt = (double) lorenzRate * numSamples / sampleRate;
            const int steps = std::max (1, (int) std::ceil (dt / 0.01));
            for (int i = 0; i < steps; ++i)
                step (dt / steps);

            const double fr = (double) fractalRate * numSamples / sampleRate;
            perlinT += fr;

            phase1 += fr;
            while (phase1 >= 1.0) { phase1 -= 1.0; held1 = nextRandom(); }
            phase2 += fr * 3.7;
            while (phase2 >= 1.0) { phase2 -= 1.0; held2 = nextRandom(); }

            current = outputs();
            next = current;
        }

        // normalised Lorenz position for the display (x, z in -1..1)
        float displayX() const noexcept { return current[0]; }
        float displayY() const noexcept { return current[1]; }
        float displayZ() const noexcept { return current[2]; }

    private:
        static constexpr double sigma = 10.0, rho = 28.0, beta = 8.0 / 3.0;

        void step (double h) noexcept
        {
            auto f = [] (double px, double py, double pz, double& dx, double& dy, double& dz)
            {
                dx = sigma * (py - px);
                dy = px * (rho - pz) - py;
                dz = px * py - beta * pz;
            };

            double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
            f (x, y, z, k1x, k1y, k1z);
            f (x + 0.5 * h * k1x, y + 0.5 * h * k1y, z + 0.5 * h * k1z, k2x, k2y, k2z);
            f (x + 0.5 * h * k2x, y + 0.5 * h * k2y, z + 0.5 * h * k2z, k3x, k3y, k3z);
            f (x + h * k3x, y + h * k3y, z + h * k3z, k4x, k4y, k4z);

            x += h / 6.0 * (k1x + 2 * k2x + 2 * k3x + k4x);
            y += h / 6.0 * (k1y + 2 * k2y + 2 * k3y + k4y);
            z += h / 6.0 * (k1z + 2 * k2z + 2 * k3z + k4z);
        }

        Frame outputs() const noexcept
        {
            auto clamp1 = [] (double v) { return (float) std::clamp (v, -1.0, 1.0); };
            const float perlin = std::clamp (1.4f * (noise (perlinT) + 0.5f * noise (perlinT * 2.0 + 17.3)
                                                      + 0.25f * noise (perlinT * 4.0 + 31.7)) / 1.75f, -1.0f, 1.0f);
            return { clamp1 (x / 22.0), clamp1 (y / 28.0), clamp1 ((z - 25.0) / 25.0), perlin,
                     (float) (0.7 * held1 + 0.3 * held2) };
        }

        // 1D gradient noise in about [-1, 1]
        static float noise (double t) noexcept
        {
            const double fl = std::floor (t);
            const float f = (float) (t - fl);
            const auto i = (int32_t) fl;
            const float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
            const float g0 = gradient (i), g1 = gradient (i + 1);
            const float a = g0 * f, b = g1 * (f - 1.0f);
            return 2.0f * (a + (b - a) * u);
        }

        static float gradient (int32_t i) noexcept
        {
            uint32_t h = (uint32_t) i * 374761393u + 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return (float) (h & 0xffffu) / 32767.5f - 1.0f;
        }

        float nextRandom() noexcept
        {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            return (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }

        double sampleRate = 44100.0;
        double x = 1.0, y = 1.0, z = 1.05, perlinT = 0.0, phase1 = 0.0, phase2 = 0.0;
        float held1 = 0.0f, held2 = 0.0f;
        uint32_t rng = 0x2545f491u;
        Frame current {};
    };
}
