#pragma once

#include <kinetiqra/geom/Domain.hpp>

#include <array>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace kinetiqra::geom {

// Named, typed channels attached to a domain.
//
// Positions, UVs and normals are ordinary channels here rather than fields of
// the mesh. That is what lets skin weights, colours and selection masks arrive
// later without touching the mesh, the bake or the importer, which is the part
// that would otherwise have to be rewritten every time.
//
// Channels are kept in step with the element count of their domain, so a
// channel is always as long as the thing it describes.
class AttributeSet {
public:
    // Adds a channel, or returns the existing one if the name and type match.
    // Returns nullptr if a channel of that name exists with a different type,
    // which is a programming error rather than something to paper over.
    template <typename T>
    std::vector<T>* add(const std::string& name, Domain domain, T fill = T{}) {
        auto& channels = channels_[index_of(domain)];

        if (auto found = channels.find(name); found != channels.end()) {
            return typed<T>(found->second.get());
        }

        auto channel = std::make_unique<TypedChannel<T>>(fill);
        channel->data.resize(counts_[index_of(domain)], fill);
        auto* raw = channel.get();
        channels.emplace(name, std::move(channel));
        return &raw->data;
    }

    template <typename T>
    [[nodiscard]] std::vector<T>* find(const std::string& name, Domain domain) {
        auto& channels = channels_[index_of(domain)];
        auto found = channels.find(name);
        return found == channels.end() ? nullptr : typed<T>(found->second.get());
    }

    template <typename T>
    [[nodiscard]] const std::vector<T>* find(const std::string& name, Domain domain) const {
        const auto& channels = channels_[index_of(domain)];
        auto found = channels.find(name);
        return found == channels.end() ? nullptr : typed<T>(found->second.get());
    }

    [[nodiscard]] bool has(const std::string& name, Domain domain) const {
        return channels_[index_of(domain)].count(name) != 0;
    }

    bool remove(const std::string& name, Domain domain) {
        return channels_[index_of(domain)].erase(name) != 0;
    }

    [[nodiscard]] std::vector<std::string> names(Domain domain) const;

    // A deep copy, channels and their fill values included.
    //
    // Named rather than offered as a copy constructor: the channels are held
    // behind unique pointers, so copying one is a real cost, and it should be
    // something a caller asked for rather than something that happens by
    // passing a mesh by value.
    [[nodiscard]] AttributeSet clone() const;

    // Grows or shrinks every channel of the domain. New elements take the fill
    // value the channel was created with.
    void resize(Domain domain, std::size_t count);

    [[nodiscard]] std::size_t count(Domain domain) const { return counts_[index_of(domain)]; }

    [[nodiscard]] std::size_t channel_count(Domain domain) const {
        return channels_[index_of(domain)].size();
    }

private:
    class Channel {
    public:
        virtual ~Channel() = default;
        Channel() = default;
        Channel(const Channel&) = delete;
        Channel& operator=(const Channel&) = delete;
        Channel(Channel&&) = delete;
        Channel& operator=(Channel&&) = delete;

        virtual void resize(std::size_t count) = 0;
        [[nodiscard]] virtual std::size_t size() const = 0;
        [[nodiscard]] virtual std::type_index type() const = 0;

        // Copying through the base, which is the only way to duplicate a
        // channel whose element type has been erased.
        [[nodiscard]] virtual std::unique_ptr<Channel> clone() const = 0;
    };

    template <typename T>
    class TypedChannel final : public Channel {
    public:
        explicit TypedChannel(T fill_value) : fill(fill_value) {}

        void resize(std::size_t count) override { data.resize(count, fill); }

        [[nodiscard]] std::size_t size() const override { return data.size(); }

        [[nodiscard]] std::type_index type() const override { return typeid(T); }

        [[nodiscard]] std::unique_ptr<Channel> clone() const override {
            auto copy = std::make_unique<TypedChannel<T>>(fill);
            copy->data = data;
            return copy;
        }

        std::vector<T> data;
        T fill;
    };

    template <typename T>
    static std::vector<T>* typed(Channel* channel) {
        if (channel == nullptr || channel->type() != std::type_index(typeid(T))) {
            return nullptr;
        }
        return &static_cast<TypedChannel<T>*>(channel)->data;
    }

    template <typename T>
    static const std::vector<T>* typed(const Channel* channel) {
        if (channel == nullptr || channel->type() != std::type_index(typeid(T))) {
            return nullptr;
        }
        return &static_cast<const TypedChannel<T>*>(channel)->data;
    }

    static constexpr std::size_t index_of(Domain domain) {
        return static_cast<std::size_t>(domain);
    }

    std::array<std::unordered_map<std::string, std::unique_ptr<Channel>>, kDomainCount> channels_;
    std::array<std::size_t, kDomainCount> counts_{};
};

}  // namespace kinetiqra::geom
