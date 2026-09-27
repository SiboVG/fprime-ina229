#include "ManagerTester.hpp"
TEST(Manager, Nominal) {
    Ina229::ManagerTester t;
    t.nominal();
}
TEST(Manager, Failures) {
    Ina229::ManagerTester t;
    t.failures();
}
TEST(Manager, Commands) {
    Ina229::ManagerTester t;
    t.commands();
}
TEST(Manager, AlertConfigurationKeepsReadsValid) {
    Ina229::ManagerTester t;
    t.alertConfigurationKeepsReadsValid();
}
TEST(Manager, IntermittentFaultsAreRateLimited) {
    Ina229::ManagerTester t;
    t.intermittentFaultsAreRateLimited();
}
TEST(Manager, ValidationErrorsAreNotDeviceFaults) {
    Ina229::ManagerTester t;
    t.validationErrorsAreNotDeviceFaults();
}
TEST(Manager, OverflowDegradesAndRestores) {
    Ina229::ManagerTester t;
    t.overflowDegradesAndRestores();
}
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
