#include <ocf_test_support.h>

void setUp() { ocf_test::reset(); }
void tearDown() {}

void test_parse_direction_known_values()
{
    TEST_ASSERT_EQUAL(OCFDirection::INWARD, parseDirection("in"));
    TEST_ASSERT_EQUAL(OCFDirection::OUTWARD, parseDirection("out"));
    TEST_ASSERT_EQUAL(OCFDirection::BOTH, parseDirection("both"));
}

void test_parse_direction_is_case_sensitive_and_rejects_unknown()
{
    TEST_ASSERT_EQUAL(OCFDirection::NONE, parseDirection("IN"));
    TEST_ASSERT_EQUAL(OCFDirection::NONE, parseDirection(""));
    TEST_ASSERT_EQUAL(OCFDirection::NONE, parseDirection("sideways"));
}

void test_write_direction_round_trips()
{
    TEST_ASSERT_EQUAL_STRING("in", writeDirection(OCFDirection::INWARD));
    TEST_ASSERT_EQUAL_STRING("out", writeDirection(OCFDirection::OUTWARD));
    TEST_ASSERT_EQUAL_STRING("both", writeDirection(OCFDirection::BOTH));
    TEST_ASSERT_EQUAL_STRING("none", writeDirection(OCFDirection::NONE));
    for (OCFDirection d : {OCFDirection::INWARD, OCFDirection::OUTWARD, OCFDirection::BOTH})
        TEST_ASSERT_EQUAL(d, parseDirection(writeDirection(d)));
}

void test_direction_enum_values_are_stable()
{
    TEST_ASSERT_EQUAL(0, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(1, OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(2, OCFDirection::NONE);
    TEST_ASSERT_EQUAL(3, OCFDirection::BOTH);
}

void test_copy_string_fits()
{
    char buf[8] = "xxxxxxx";
    TEST_ASSERT_EQUAL(3, copyString(buf, sizeof(buf), "Mia"));
    TEST_ASSERT_EQUAL_STRING("Mia", buf);
}

void test_copy_string_truncates_and_terminates()
{
    char buf[5];
    memset(buf, 'x', sizeof(buf));
    TEST_ASSERT_EQUAL(4, copyString(buf, sizeof(buf), "Garfield"));
    TEST_ASSERT_EQUAL_STRING("Garf", buf);
}

void test_copy_string_exact_fit_minus_terminator()
{
    char buf[4];
    TEST_ASSERT_EQUAL(3, copyString(buf, sizeof(buf), "abcd"));
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    TEST_ASSERT_EQUAL(3, copyString(buf, sizeof(buf), "abc"));
    TEST_ASSERT_EQUAL_STRING("abc", buf);
}

void test_copy_string_null_source_clears_destination()
{
    char buf[4] = "abc";
    TEST_ASSERT_EQUAL(0, copyString(buf, sizeof(buf), nullptr));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_copy_string_ignores_unusable_destination()
{
    char buf[2] = "a";
    TEST_ASSERT_EQUAL(0, copyString(nullptr, 10, "abc"));
    TEST_ASSERT_EQUAL(0, copyString(buf, 0, "abc"));
    TEST_ASSERT_EQUAL_STRING("a", buf);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_direction_known_values);
    RUN_TEST(test_parse_direction_is_case_sensitive_and_rejects_unknown);
    RUN_TEST(test_write_direction_round_trips);
    RUN_TEST(test_direction_enum_values_are_stable);
    RUN_TEST(test_copy_string_fits);
    RUN_TEST(test_copy_string_truncates_and_terminates);
    RUN_TEST(test_copy_string_exact_fit_minus_terminator);
    RUN_TEST(test_copy_string_null_source_clears_destination);
    RUN_TEST(test_copy_string_ignores_unusable_destination);
    return UNITY_END();
}
