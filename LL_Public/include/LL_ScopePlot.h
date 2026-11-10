#pragma once
#include "LL_CpsData.h"
#include <algorithm>
#include <limits>
namespace LL {
inline constexpr std::size_t ScopeGap = std::numeric_limits<std::size_t>::max();
// Retain first/min/max/last per screen column, in original time order.
// Never connect across a missing channel or a sample sequence gap.
inline std::vector<std::size_t> ScopePlotIndices(const std::vector<ScopeSample> &samples,
                                                 const std::string &channel, std::size_t first,
                                                 std::size_t columns, std::size_t end = ScopeGap) {
    std::vector<std::size_t> result, bucket;
    end = std::min(end, samples.size());
    if (first >= end)
        return result;
    const auto start = samples[first].timestampUs;
    const double span = std::max<double>(1, samples[end - 1].timestampUs - start);
    std::size_t previous = ScopeGap, column = ScopeGap;
    auto flush = [&] {
        if (bucket.empty())
            return;
        const auto less = [&](auto a, auto b) {
            return samples[a].values.at(channel) < samples[b].values.at(channel);
        };
        std::vector<std::size_t> points = {
            bucket.front(), *std::min_element(bucket.begin(), bucket.end(), less),
            *std::max_element(bucket.begin(), bucket.end(), less), bucket.back()};
        std::sort(points.begin(), points.end());
        points.erase(std::unique(points.begin(), points.end()), points.end());
        result.insert(result.end(), points.begin(), points.end());
        bucket.clear();
    };
    for (std::size_t i = first; i < end; ++i) {
        if (!samples[i].values.count(channel)) {
            flush();
            if (previous != ScopeGap)
                result.push_back(ScopeGap);
            previous = ScopeGap;
            continue;
        }
        if (previous != ScopeGap && samples[i].sequence > samples[previous].sequence + 1) {
            flush();
            result.push_back(ScopeGap);
        }
        const auto x = static_cast<std::size_t>((samples[i].timestampUs - start) / span * columns);
        if (x != column) {
            flush();
            column = x;
        }
        bucket.push_back(i);
        previous = i;
    }
    flush();
    return result;
}
} // namespace LL
