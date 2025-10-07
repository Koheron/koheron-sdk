/// Signal handler
///
/// (c) Koheron

#ifndef __SERVER_RUNTIME_SIGNAL_HANDLER_HPP__
#define __SERVER_RUNTIME_SIGNAL_HANDLER_HPP__

namespace koheron {

class SignalHandler
{
  public:
    int init();

    bool interrupt() const {return s_interrupted != 0;}

    static int volatile s_interrupted;

  private:
    int set_interrupt_signals();
    int set_ignore_signals();
    int set_crash_signals();
};

} // namespace koheron

#endif // __SERVER_RUNTIME_SIGNAL_HANDLER_HPP__
