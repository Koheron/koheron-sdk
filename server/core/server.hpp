/// Server main class
///
/// (c) Koheron

#ifndef __KOHERON_HPP__
#define __KOHERON_HPP__

#include <atomic>

namespace koheron {

class Server
{
  public:
    Server();

    int run();
    std::atomic<bool> exit_all;
};

} // namespace koheron

#endif // __KOHERON_HPP__
