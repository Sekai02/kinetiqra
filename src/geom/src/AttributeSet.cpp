#include <kinetiqra/geom/AttributeSet.hpp>

#include <algorithm>

namespace kinetiqra::geom {

std::vector<std::string> AttributeSet::names(Domain domain) const {
    std::vector<std::string> result;
    result.reserve(channels_[index_of(domain)].size());

    for (const auto& [name, channel] : channels_[index_of(domain)]) {
        result.push_back(name);
    }

    // The map is unordered, so sort to give callers something reproducible.
    std::sort(result.begin(), result.end());
    return result;
}

void AttributeSet::resize(Domain domain, std::size_t count) {
    counts_[index_of(domain)] = count;

    for (auto& [name, channel] : channels_[index_of(domain)]) {
        channel->resize(count);
    }
}

}  // namespace kinetiqra::geom
