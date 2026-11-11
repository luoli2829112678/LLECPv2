#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace LL {
// One-sided peak-amplitude spectrum, mean removed and Hann coherent-gain corrected.
inline std::vector<std::pair<double, double>> ScopeFFT(const std::vector<double> &values, double rate) {
    if (values.size() < 16 || !std::isfinite(rate) || rate <= 0)
        throw std::runtime_error("FFT需要至少16个有效采样点");
    std::size_t n = 1;
    while (n * 2 <= values.size() && n < 8192)
        n *= 2;
    std::vector<std::complex<double>> a(n);
    double mean = 0, gain = 0;
    for (std::size_t i = 0; i < n; ++i)
        mean += values[i];
    mean /= n;
    for (std::size_t i = 0; i < n; ++i) {
        double win = .5 - .5 * std::cos(6.283185307179586 * i / (n - 1));
        gain += win;
        a[i] = (values[i] - mean) * win;
    }
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len *= 2) {
        auto root = std::polar(1.0, -6.283185307179586 / len);
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w = 1;
            for (std::size_t j = 0; j < len / 2; ++j) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= root;
            }
        }
    }
    std::vector<std::pair<double, double>> result;
    for (std::size_t i = 0; i <= n / 2; ++i)
        result.push_back({i * rate / n, std::abs(a[i]) / gain * (i && i < n / 2 ? 2 : 1)});
    return result;
}
} // namespace LL
