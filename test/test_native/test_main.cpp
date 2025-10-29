#include <unity.h>
#include "shared_logic.h"

/**
 * @brief Test the rpm2sps function for various inputs.
 */
void test_rpm2sps_conversion() {
    TEST_ASSERT_EQUAL_DOUBLE(0.0, rpm2sps(0.0));
    TEST_ASSERT_EQUAL_DOUBLE(SPR_CMD / 60.0, rpm2sps(1.0));
    TEST_ASSERT_EQUAL_DOUBLE(SPR_CMD, rpm2sps(60.0));
}

/**
 * @brief Test the rpm2sps function with negative input.
 */
void test_rpm2sps_negative_input() {
    TEST_ASSERT_EQUAL_DOUBLE(-(SPR_CMD / 60.0), rpm2sps(-1.0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rpm2sps_conversion);
    RUN_TEST(test_rpm2sps_negative_input);
    return UNITY_END();
}
