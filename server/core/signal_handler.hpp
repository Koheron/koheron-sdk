/// Signal handler
///
/// (c) Koheron

#ifndef __SIGNAL_HANDLER_HPP__
#define __SIGNAL_HANDLER_HPP__

namespace koheron {

class Server;

class SignalHandler
{
  public:
    int init(Server *server_);

    bool interrupt() const {return s_interrupted != 0;}

    static int volatile s_interrupted;
    static Server *server;

  private:
    int set_interrupt_signals();
    int set_ignore_signals();
    int set_crash_signals();
};

} // namespace koheron

#endif // __SIGNAL_HANDLER_HPP__
