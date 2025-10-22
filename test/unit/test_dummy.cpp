/**
 * @file test_dummy.cpp
 * @brief Dummy test to verify Google Test setup
 *
 * This test should pass and confirms that the test infrastructure is working.
 * Once this passes, we can add real component tests.
 *
 * Week 1 - Foundation & Testing Setup
 */

#include <gtest/gtest.h>

/**
 * @brief Basic sanity test
 */
TEST(DummyTest, BasicAssertion)
{
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}

/**
 * @brief Test that will verify test failure detection
 */
TEST(DummyTest, StringComparison)
{
    std::string expected = "Hello, GrowBorg!";
    std::string actual = "Hello, GrowBorg!";
    EXPECT_EQ(expected, actual);
}

/**
 * @brief Test floating point comparison
 */
TEST(DummyTest, FloatingPoint)
{
    double a = 0.1 + 0.2;
    double b = 0.3;
    EXPECT_NEAR(a, b, 0.0001);
}

/**
 * @brief Test exception handling
 */
TEST(DummyTest, ExceptionHandling)
{
    EXPECT_NO_THROW({
        int x = 42;
        (void) x; // Suppress unused variable warning
    });
}

/**
 * @brief Main function (handled by gtest_main)
 *
 * Note: We link against GTest::gtest_main, so we don't need to write main()
 */
