#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kinetiqra::core {

// One reversible change.
//
// Every mutation of the scene is expressed this way. That is not a style
// preference: undo cannot be added to code that writes to the scene directly,
// because by then there is no record of what the previous value was and no
// single place to intercept. See docs/INVARIANTS.md.
class Command {
public:
    Command() = default;
    virtual ~Command() = default;

    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(Command&&) = delete;

    virtual void apply() = 0;
    virtual void revert() = 0;

    // Shown in the history, so it reads as what the user did rather than as a
    // class name.
    [[nodiscard]] virtual std::string_view name() const = 0;
};

// The history: what has been done, and what has been undone and could be done
// again.
class CommandStack {
public:
    // Applies the command and takes ownership of it. Anything undone is
    // discarded, because the future it belonged to no longer exists.
    void execute(std::unique_ptr<Command> command) {
        if (command == nullptr) {
            return;
        }

        command->apply();
        done_.push_back(std::move(command));
        undone_.clear();
    }

    bool undo() {
        if (done_.empty()) {
            return false;
        }

        std::unique_ptr<Command> command = std::move(done_.back());
        done_.pop_back();
        command->revert();
        undone_.push_back(std::move(command));
        return true;
    }

    bool redo() {
        if (undone_.empty()) {
            return false;
        }

        std::unique_ptr<Command> command = std::move(undone_.back());
        undone_.pop_back();
        command->apply();
        done_.push_back(std::move(command));
        return true;
    }

    [[nodiscard]] bool can_undo() const { return !done_.empty(); }

    [[nodiscard]] bool can_redo() const { return !undone_.empty(); }

    [[nodiscard]] std::string_view undo_name() const {
        return done_.empty() ? std::string_view{} : done_.back()->name();
    }

    [[nodiscard]] std::string_view redo_name() const {
        return undone_.empty() ? std::string_view{} : undone_.back()->name();
    }

    [[nodiscard]] std::size_t depth() const { return done_.size(); }

    void clear() {
        done_.clear();
        undone_.clear();
    }

private:
    std::vector<std::unique_ptr<Command>> done_;
    std::vector<std::unique_ptr<Command>> undone_;
};

}  // namespace kinetiqra::core
