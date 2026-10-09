#include <sys/mman.h>
#include "server/drivers/uio.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <termios.h>
using namespace std::chrono_literals;
void check(bool ok, const char* text) {
    if (!ok) { std::cerr << text << '\n'; std::abort(); }
}
template<class F> void wait_until(F f) {
    const auto end = std::chrono::steady_clock::now()+2s;
    while (!f()) { check(std::chrono::steady_clock::now()<end,"timeout"); std::this_thread::sleep_for(1ms); }
}
struct Owned {
    std::atomic<int>& destroyed;
    ~Owned() { ++destroyed; }
};
int main(int argc, char** argv) {
    check(argc==2,"fixture root");
    const std::filesystem::path root{argv[1]};
    const int master=posix_openpt(O_RDWR|O_NOCTTY);
    check(master>=0 && grantpt(master)==0 && unlockpt(master)==0,"pty");
    const char* slave_name=ptsname(master);
    const int slave=::open(slave_name,O_RDWR|O_NOCTTY);
    termios term{}; check(tcgetattr(slave,&term)==0,"get termios");
    cfmakeraw(&term); check(tcsetattr(slave,TCSANOW,&term)==0,"raw pty"); ::close(slave);
    std::filesystem::create_symlink(slave_name,root/"dev/uio0");
    std::atomic<int> calls{0},destroyed{0};
    {
        Uio<0> uio;
        check(uio.open()>=0,"open fake UIO");
        check(!uio.listen(std::move_only_function<void(int)>{},1ms),"empty callback accepted");
        check(!uio.listen(std::function<void(int)>{},1ms),"empty legacy callback accepted");
        // A timeout completes the worker before unlisten/destruction. Restart
        // must join it before replacing std::thread.
        for (int i=0;i<3;++i) {
            check(uio.listen([&, owner=std::unique_ptr<Owned>(new Owned{destroyed})](int rc) {
                check(owner!=nullptr && rc==0,"timeout callback"); ++calls;
            },5ms),"listen move-only callback");
            wait_until([&]{return !uio.is_running();});
        }
        check(calls==3,"timeout count");
        uio.unlisten();
        check(destroyed==3,"callback resource lifetime");
        // Drain the three IRQ-arm words already sent to the fake device.
        uint32_t words[3]; check(::read(master,words,sizeof(words))==sizeof(words),"arm words");
        check(uio.listen([&](int rc) {
            check(rc==7,"IRQ counter"); uio.unlisten(); ++calls;
        },500ms),"IRQ listen");
        uint32_t word; check(::read(master,&word,sizeof(word))==sizeof(word),"initial arm");
        word=7; check(::write(master,&word,sizeof(word))==sizeof(word),"interrupt");
        wait_until([&]{return calls==4;});
        uio.unlisten();
    }
    ::close(master);
}
