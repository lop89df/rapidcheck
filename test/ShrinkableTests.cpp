#include <catch.hpp>
#include <rapidcheck-catch.h>

#include "rapidcheck/Shrinkable.h"
#include "rapidcheck/shrinkable/Create.h"

#include "util/TemplateProps.h"
#include "util/Logger.h"
#include "util/Generators.h"

using namespace rc;
using namespace rc::test;

namespace {

template<typename ValueCallable, typename ShrinksCallable>
class MockShrinkableImpl
{
public:
    MockShrinkableImpl(ValueCallable value, ShrinksCallable shrinks)
        : m_value(value)
        , m_shrinks(shrinks) {}

    typename std::result_of<ValueCallable()>::type
    value() const { return m_value(); }

    typename std::result_of<ShrinksCallable()>::type
    shrinks() const { return m_shrinks(); }

private:
    ValueCallable m_value;
    ShrinksCallable m_shrinks;
};

template<typename ValueCallable, typename ShrinksCallable>
Shrinkable<Decay<typename std::result_of<ValueCallable()>::type>>
makeMockShrinkable(ValueCallable value, ShrinksCallable shrinks)
{
    return makeShrinkable<MockShrinkableImpl<ValueCallable, ShrinksCallable>>(
        value, shrinks);
}

class LoggingShrinkableImpl : public Logger
{
public:
    typedef std::pair<std::string, std::vector<std::string>> IdLogPair;

    LoggingShrinkableImpl() : Logger() {}
    LoggingShrinkableImpl(std::string theId) : Logger(std::move(theId)) {}

    IdLogPair value() const { return { id, log }; }

    Seq<Shrinkable<IdLogPair>> shrinks() const
    { return Seq<Shrinkable<IdLogPair>>(); }
};

typedef Shrinkable<std::pair<std::string, std::vector<std::string>>> LoggingShrinkable;

}

TEST_CASE("Shrinkable") {
    SECTION("calls value() of the implementation object") {
        bool valueCalled = false;
        Shrinkable<int> shrinkable = makeMockShrinkable(
            [&] {
                valueCalled = true;
                return 1337;
            },
            [] { return Seq<Shrinkable<int>>(); });

        REQUIRE(shrinkable.value() == 1337);
        REQUIRE(valueCalled);
    }

    SECTION("calls shrinks() of the implementation object") {
        Shrinkable<int> shrink = makeMockShrinkable(
            [] { return 123; },
            [] { return Seq<Shrinkable<int>>(); });
        auto shrinks = seq::just(shrink);

        bool shrinksCalled = false;
        Shrinkable<int> shrinkable = makeMockShrinkable(
            [] { return 0; },
            [&] {
                shrinksCalled = true;
                return shrinks;
            });

        REQUIRE(shrinkable.shrinks() == shrinks);
        REQUIRE(shrinksCalled);
    }

    SECTION("copies implementation if constructed from lvalue") {
        LoggingShrinkableImpl impl("foobar");
        LoggingShrinkable shrinkable(impl);

        const auto value = shrinkable.value();
        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "copy constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("moves implementation if constructed from rvalue") {
        LoggingShrinkable shrinkable(LoggingShrinkableImpl("foobar"));
        const auto value = shrinkable.value();

        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "move constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("copy construction copies the implementation object") {
        LoggingShrinkable original(LoggingShrinkableImpl("foobar"));
        auto copy(original);

        const auto value = copy.value();
        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "move constructed",
            "copy constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("copy assignment copies the implementation object") {
        LoggingShrinkable original(LoggingShrinkableImpl("foobar"));
        LoggingShrinkable copy(LoggingShrinkableImpl("blah"));
        copy = original;

        const auto value = copy.value();
        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "move constructed",
            "copy constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("move construction neither moves nor copies") {
        LoggingShrinkable original(LoggingShrinkableImpl("foobar"));
        LoggingShrinkable moved(std::move(original));

        const auto value = moved.value();
        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "move constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("move assignment neither moves nor copies") {
        LoggingShrinkable original(LoggingShrinkableImpl("foobar"));
        LoggingShrinkable moved(LoggingShrinkableImpl("blah"));
        moved = std::move(original);

        const auto value = moved.value();
        std::vector<std::string> expectedLog{
            "constructed as foobar",
            "move constructed"};
        REQUIRE(value.first == "foobar");
        REQUIRE(value.second == expectedLog);
    }

    SECTION("operator==/operator!=") {
        propConformsToEquals<Shrinkable<int>>();

        prop("different values yield inequal shrinkables",
             [](Seq<Shrinkable<int>> shrinks, int v1) {
                 int v2 = *gen::distinctFrom(v1);
                 RC_ASSERT(shrinkable::just(v1, shrinks) !=
                           shrinkable::just(v2, shrinks));
             });

        prop("different shrinks yield inequal shrinkables",
             [](int value, Seq<Shrinkable<int>> shrinks1) {
                 Seq<Shrinkable<int>> shrinks2 = *gen::distinctFrom(shrinks1);
                 RC_ASSERT(shrinkable::just(value, shrinks1) !=
                           shrinkable::just(value, shrinks2));
             });
    }

    SECTION("makeShrinkable") {
        SECTION("constructs implementation object in place") {
            auto shrinkable = makeShrinkable<LoggingShrinkableImpl>("foobar");
            const auto value = shrinkable.value();
            REQUIRE(value.first == "foobar");
            std::vector<std::string> expectedLog{"constructed as foobar"};
            REQUIRE(value.second == expectedLog);
        }
    }
}