/**
 * @file test_state_machine.cpp
 * @brief Unit tests for the StateMachine template
 */

#include <gtest/gtest.h>

#include "lopcore/state_machine/state_machine.hpp"

using namespace lopcore;

// Test enum for state machine
enum class TestState
{
    STATE_A,
    STATE_B,
    STATE_C
};

// Mock state for testing
class MockState : public IState<TestState>
{
public:
    explicit MockState(TestState id) : id_(id)
    {
    }

    void onEnter() override
    {
        enterCount_++;
        lastAction_ = "enter";
    }

    void update() override
    {
        updateCount_++;
        lastAction_ = "update";
    }

    void onExit() override
    {
        exitCount_++;
        lastAction_ = "exit";
    }

    TestState getStateId() const override
    {
        return id_;
    }

    // Test helpers
    int enterCount_{0};
    int updateCount_{0};
    int exitCount_{0};
    std::string lastAction_;

private:
    TestState id_;
};

class StateMachineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Fresh state machine for each test
    }
};

TEST_F(StateMachineTest, InitialStateIsSet)
{
    StateMachine<TestState> sm(TestState::STATE_A);
    EXPECT_EQ(sm.getCurrentState(), TestState::STATE_A);
    EXPECT_EQ(sm.getPreviousState(), TestState::STATE_A);
}

TEST_F(StateMachineTest, RegisterStateSucceeds)
{
    StateMachine<TestState> sm(TestState::STATE_A);
    auto state = std::make_unique<MockState>(TestState::STATE_A);

    EXPECT_TRUE(sm.registerState(TestState::STATE_A, std::move(state)));
}

TEST_F(StateMachineTest, CannotRegisterNullState)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    EXPECT_FALSE(sm.registerState(TestState::STATE_A, nullptr));
}

TEST_F(StateMachineTest, TransitionCallsExitAndEnter)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    auto stateA = std::make_unique<MockState>(TestState::STATE_A);
    auto stateB = std::make_unique<MockState>(TestState::STATE_B);

    auto *stateAPtr = stateA.get();
    auto *stateBPtr = stateB.get();

    sm.registerState(TestState::STATE_A, std::move(stateA));
    sm.registerState(TestState::STATE_B, std::move(stateB));

    // Transition from A to B
    EXPECT_TRUE(sm.transition(TestState::STATE_B));

    EXPECT_EQ(stateAPtr->exitCount_, 1);
    EXPECT_EQ(stateAPtr->lastAction_, "exit");
    EXPECT_EQ(stateBPtr->enterCount_, 1);
    EXPECT_EQ(stateBPtr->lastAction_, "enter");
    EXPECT_EQ(sm.getCurrentState(), TestState::STATE_B);
    EXPECT_EQ(sm.getPreviousState(), TestState::STATE_A);
}

TEST_F(StateMachineTest, UpdateCallsCurrentStateUpdate)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    auto state = std::make_unique<MockState>(TestState::STATE_A);
    auto *statePtr = state.get();

    sm.registerState(TestState::STATE_A, std::move(state));

    sm.update();
    EXPECT_EQ(statePtr->updateCount_, 1);

    sm.update();
    EXPECT_EQ(statePtr->updateCount_, 2);
}

TEST_F(StateMachineTest, TransitionToSameStateIsIgnored)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    auto state = std::make_unique<MockState>(TestState::STATE_A);
    auto *statePtr = state.get();

    sm.registerState(TestState::STATE_A, std::move(state));

    EXPECT_TRUE(sm.transition(TestState::STATE_A));
    EXPECT_EQ(statePtr->enterCount_, 0);
    EXPECT_EQ(statePtr->exitCount_, 0);
}

TEST_F(StateMachineTest, TransitionRulesAreEnforced)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    // Only allow A->B, not A->C
    sm.addTransitionRule(TestState::STATE_A, TestState::STATE_B);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));
    sm.registerState(TestState::STATE_C, std::make_unique<MockState>(TestState::STATE_C));

    EXPECT_TRUE(sm.transition(TestState::STATE_B));  // Allowed
    EXPECT_FALSE(sm.transition(TestState::STATE_C)); // Not allowed

    EXPECT_EQ(sm.getCurrentState(), TestState::STATE_B);
}

TEST_F(StateMachineTest, IsTransitionAllowedWorks)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.addTransitionRule(TestState::STATE_A, TestState::STATE_B);
    sm.addTransitionRule(TestState::STATE_B, TestState::STATE_C);

    EXPECT_TRUE(sm.isTransitionAllowed(TestState::STATE_A, TestState::STATE_B));
    EXPECT_FALSE(sm.isTransitionAllowed(TestState::STATE_A, TestState::STATE_C));
    EXPECT_TRUE(sm.isTransitionAllowed(TestState::STATE_B, TestState::STATE_C));
}

TEST_F(StateMachineTest, ObserversAreNotified)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));

    bool notified = false;
    TestState observedFrom = TestState::STATE_A;
    TestState observedTo = TestState::STATE_A;

    sm.addObserver([&](TestState from, TestState to) {
        notified = true;
        observedFrom = from;
        observedTo = to;
    });

    sm.transition(TestState::STATE_B);

    EXPECT_TRUE(notified);
    EXPECT_EQ(observedFrom, TestState::STATE_A);
    EXPECT_EQ(observedTo, TestState::STATE_B);
}

TEST_F(StateMachineTest, MultipleObserversWork)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));

    int observer1Count = 0;
    int observer2Count = 0;

    sm.addObserver([&](TestState, TestState) { observer1Count++; });
    sm.addObserver([&](TestState, TestState) { observer2Count++; });

    sm.transition(TestState::STATE_B);

    EXPECT_EQ(observer1Count, 1);
    EXPECT_EQ(observer2Count, 1);
}

TEST_F(StateMachineTest, HistoryIsTracked)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));
    sm.registerState(TestState::STATE_C, std::make_unique<MockState>(TestState::STATE_C));

    sm.transition(TestState::STATE_B);
    sm.transition(TestState::STATE_C);
    sm.transition(TestState::STATE_A);

    auto history = sm.getHistory();
    ASSERT_EQ(history.size(), 4);
    EXPECT_EQ(history[0], TestState::STATE_A); // Initial
    EXPECT_EQ(history[1], TestState::STATE_B);
    EXPECT_EQ(history[2], TestState::STATE_C);
    EXPECT_EQ(history[3], TestState::STATE_A);
}

TEST_F(StateMachineTest, HistoryRespectMaxSize)
{
    StateMachine<TestState> sm(TestState::STATE_A);
    sm.setMaxHistorySize(3);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));

    // Make many transitions
    sm.transition(TestState::STATE_B);
    sm.transition(TestState::STATE_A);
    sm.transition(TestState::STATE_B);
    sm.transition(TestState::STATE_A);

    auto history = sm.getHistory();
    EXPECT_LE(history.size(), 3);
}

TEST_F(StateMachineTest, ClearHistoryWorks)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));

    sm.transition(TestState::STATE_B);
    sm.clearHistory();

    auto history = sm.getHistory();
    EXPECT_EQ(history.size(), 1);
    EXPECT_EQ(history[0], TestState::STATE_B); // Current state
}

TEST_F(StateMachineTest, ClearTransitionRulesWorks)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.addTransitionRule(TestState::STATE_A, TestState::STATE_B);
    sm.clearTransitionRules();

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_C, std::make_unique<MockState>(TestState::STATE_C));

    // After clearing rules, all transitions allowed
    EXPECT_TRUE(sm.transition(TestState::STATE_C));
}

TEST_F(StateMachineTest, ClearObserversWorks)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));

    int observerCount = 0;
    sm.addObserver([&](TestState, TestState) { observerCount++; });

    sm.transition(TestState::STATE_B);
    EXPECT_EQ(observerCount, 1);

    sm.clearObservers();
    sm.transition(TestState::STATE_A);
    EXPECT_EQ(observerCount, 1); // Not incremented
}

TEST_F(StateMachineTest, NoRulesAllowsAllTransitions)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    sm.registerState(TestState::STATE_A, std::make_unique<MockState>(TestState::STATE_A));
    sm.registerState(TestState::STATE_B, std::make_unique<MockState>(TestState::STATE_B));
    sm.registerState(TestState::STATE_C, std::make_unique<MockState>(TestState::STATE_C));

    // No rules added, all transitions should work
    EXPECT_TRUE(sm.transition(TestState::STATE_B));
    EXPECT_TRUE(sm.transition(TestState::STATE_C));
    EXPECT_TRUE(sm.transition(TestState::STATE_A));
}

TEST_F(StateMachineTest, ComplexTransitionFlow)
{
    StateMachine<TestState> sm(TestState::STATE_A);

    auto stateA = std::make_unique<MockState>(TestState::STATE_A);
    auto stateB = std::make_unique<MockState>(TestState::STATE_B);
    auto stateC = std::make_unique<MockState>(TestState::STATE_C);

    auto *stateAPtr = stateA.get();
    auto *stateBPtr = stateB.get();
    auto *stateCPtr = stateC.get();

    sm.registerState(TestState::STATE_A, std::move(stateA));
    sm.registerState(TestState::STATE_B, std::move(stateB));
    sm.registerState(TestState::STATE_C, std::move(stateC));

    // A -> B -> C -> A
    sm.transition(TestState::STATE_B);
    sm.update();
    sm.transition(TestState::STATE_C);
    sm.update();
    sm.transition(TestState::STATE_A);
    sm.update();

    EXPECT_EQ(stateAPtr->enterCount_, 1);
    EXPECT_EQ(stateAPtr->exitCount_, 1);
    EXPECT_EQ(stateAPtr->updateCount_, 1);

    EXPECT_EQ(stateBPtr->enterCount_, 1);
    EXPECT_EQ(stateBPtr->exitCount_, 1);
    EXPECT_EQ(stateBPtr->updateCount_, 1);

    EXPECT_EQ(stateCPtr->enterCount_, 1);
    EXPECT_EQ(stateCPtr->exitCount_, 1);
    EXPECT_EQ(stateCPtr->updateCount_, 1);
}
