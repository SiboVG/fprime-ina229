#include "TestHarness.hpp"
#include "examples/Ft232h/UnixSpi.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
INA229_TEST(BridgeTransfersBytesAndPreservesOutputOnFailure) {
 for(int kind=0;kind<3;++kind) {
  int fds[2];EXPECT_EQ(0,socketpair(AF_UNIX,SOCK_STREAM,0,fds));
  Ina229Bench::UnixSpi spi(fds[0],100);
  std::thread peer([&] {
   std::uint8_t request[4]={0};size_t count=0;
   while(count<4){auto n=::read(fds[1],request+count,4-count);if(n<=0)break;count+=n;}
   if(count!=4){::close(fds[1]);return;}
   const std::uint8_t reply[4]={static_cast<std::uint8_t>(kind==1),0,0x54,0x49};
   const auto size=kind==2?2U:4U;
   for(unsigned i=0;i<size;++i) ::write(fds[1],reply+i,1);
   ::close(fds[1]);
  });
  std::uint8_t tx[3]={0xf9,0,0},rx[3]={42,42,42};
  const bool success=spi.transfer(tx,rx,3);if(!success) ::shutdown(fds[0],SHUT_RDWR);peer.join();
  EXPECT_EQ(kind==0,success);EXPECT_EQ(kind==0?0x54:42,rx[1]);
 }
}
INA229_TEST(BridgeTimesOutAndDisconnects) {
 int fds[2];EXPECT_EQ(0,socketpair(AF_UNIX,SOCK_STREAM,0,fds));
 Ina229Bench::UnixSpi spi(fds[0],20);
 std::uint8_t tx[3]={0},rx[3]={42,42,42};
 auto start=std::chrono::steady_clock::now();
 EXPECT_FALSE(spi.transfer(tx,rx,3));EXPECT_EQ(42,rx[0]);
 EXPECT_TRUE(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(500));
 EXPECT_FALSE(spi.transfer(tx,rx,3));::close(fds[1]);
}
INA229_TEST(BridgeValidatesBoundsAndHandlesClosedPeer) {
 Ina229Bench::UnixSpi empty;std::uint8_t bytes[7]={0};
 EXPECT_FALSE(empty.open("/this/socket/does/not/exist",10));
 EXPECT_FALSE(empty.transfer(bytes,bytes,3));
 int fds[2];EXPECT_EQ(0,socketpair(AF_UNIX,SOCK_STREAM,0,fds));
 Ina229Bench::UnixSpi spi(fds[0],20);
 EXPECT_FALSE(spi.transfer(nullptr,bytes,3));EXPECT_FALSE(spi.transfer(bytes,bytes,0));
 EXPECT_FALSE(spi.transfer(bytes,bytes,7));::close(fds[1]);
 EXPECT_FALSE(spi.transfer(bytes,bytes,3));
}
int main(){return Ina229Test::runAll();}
