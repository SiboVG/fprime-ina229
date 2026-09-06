#include "ManagerTester.hpp"
TEST(Manager,Nominal){Ina229::ManagerTester t;t.nominal();}
TEST(Manager,Failures){Ina229::ManagerTester t;t.failures();}
TEST(Manager,Commands){Ina229::ManagerTester t;t.commands();}
int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}
