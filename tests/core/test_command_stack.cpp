#include <kinetiqra/core/Command.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using kinetiqra::core::Command;
using kinetiqra::core::CommandStack;

namespace {

// Writes a value into somewhere the test can see, remembering what was there.
class SetValue final : public Command {
public:
    SetValue(int& target, int value, std::vector<std::string>* log = nullptr)
        : target_(target), value_(value), previous_(target), log_(log) {}

    void apply() override {
        previous_ = target_;
        target_ = value_;
        if (log_ != nullptr) {
            log_->push_back("apply " + std::to_string(value_));
        }
    }

    void revert() override {
        target_ = previous_;
        if (log_ != nullptr) {
            log_->push_back("revert " + std::to_string(value_));
        }
    }

    [[nodiscard]] std::string_view name() const override { return "set value"; }

private:
    int& target_;
    int value_;
    int previous_;
    std::vector<std::string>* log_;
};

}  // namespace

TEST_CASE("executing applies the command") {
    CommandStack stack;
    int value = 0;

    stack.execute(std::make_unique<SetValue>(value, 42));

    CHECK(value == 42);
    CHECK(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
    CHECK(stack.undo_name() == "set value");
}

TEST_CASE("undo restores the previous value and redo puts it back") {
    CommandStack stack;
    int value = 1;

    stack.execute(std::make_unique<SetValue>(value, 2));
    REQUIRE(value == 2);

    REQUIRE(stack.undo());
    CHECK(value == 1);
    CHECK_FALSE(stack.can_undo());
    CHECK(stack.can_redo());

    REQUIRE(stack.redo());
    CHECK(value == 2);
    CHECK(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
}

TEST_CASE("undo walks back through several commands in order") {
    CommandStack stack;
    int value = 0;

    stack.execute(std::make_unique<SetValue>(value, 1));
    stack.execute(std::make_unique<SetValue>(value, 2));
    stack.execute(std::make_unique<SetValue>(value, 3));
    REQUIRE(value == 3);
    CHECK(stack.depth() == 3);

    stack.undo();
    CHECK(value == 2);
    stack.undo();
    CHECK(value == 1);
    stack.undo();
    CHECK(value == 0);
    CHECK_FALSE(stack.undo());
}

TEST_CASE("executing after an undo discards what was undone") {
    CommandStack stack;
    int value = 0;

    stack.execute(std::make_unique<SetValue>(value, 1));
    stack.execute(std::make_unique<SetValue>(value, 2));
    stack.undo();
    REQUIRE(value == 1);
    REQUIRE(stack.can_redo());

    // A new edit replaces the future that was undone, which is what every
    // editor does and what people expect when they change their mind.
    stack.execute(std::make_unique<SetValue>(value, 99));

    CHECK(value == 99);
    CHECK_FALSE(stack.can_redo());
    CHECK(stack.depth() == 2);
}

TEST_CASE("undo and redo report nothing to do on an empty stack") {
    CommandStack stack;

    CHECK_FALSE(stack.undo());
    CHECK_FALSE(stack.redo());
    CHECK_FALSE(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
    CHECK(stack.undo_name().empty());
}

TEST_CASE("apply and revert are called in the right order") {
    CommandStack stack;
    int value = 0;
    std::vector<std::string> log;

    stack.execute(std::make_unique<SetValue>(value, 1, &log));
    stack.execute(std::make_unique<SetValue>(value, 2, &log));
    stack.undo();
    stack.redo();

    const std::vector<std::string> expected{"apply 1", "apply 2", "revert 2", "apply 2"};
    CHECK(log == expected);
}

TEST_CASE("clearing forgets the history without touching the value") {
    CommandStack stack;
    int value = 0;

    stack.execute(std::make_unique<SetValue>(value, 7));
    stack.clear();

    CHECK(value == 7);
    CHECK_FALSE(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
}

TEST_CASE("a null command is ignored rather than crashing") {
    CommandStack stack;

    stack.execute(nullptr);

    CHECK_FALSE(stack.can_undo());
}
