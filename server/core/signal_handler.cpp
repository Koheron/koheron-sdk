/// Implementation of signal_handler.hpp
///
/// (c) Koheron

#include "signal_handler.hpp"

#include <iostream>
#include <cxxabi.h>
#include <csignal>

extern "C" {
  #include <execinfo.h>
}

#include "server.hpp"

namespace koheron {

Server *SignalHandler::server = nullptr;

int SignalHandler::init(Server *server_)
{
    server = server_;

    if (set_interrupt_signals() < 0 ||
        set_ignore_signals()   < 0 ||
        set_crash_signals()    < 0)
        return -1;

    return 0;
}

// Interrupt signals

int volatile SignalHandler::s_interrupted = 0;

static void exit_signal_handler(int)
{
    SignalHandler::s_interrupted = 1;
}

int SignalHandler::set_interrupt_signals()
{
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = exit_signal_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    if (sigaction(SIGINT, &sig_int_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot set SIGINT handler\n");
        return -1;
    }

    if (sigaction(SIGTERM, &sig_int_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot set SIGTERM handler\n");
        return -1;
    }

    return 0;
}

// Ignored signals

int SignalHandler::set_ignore_signals()
{
    struct sigaction sig_ign_handler;

    sig_ign_handler.sa_handler = SIG_IGN;
    sig_ign_handler.sa_flags = 0;
    sigemptyset(&sig_ign_handler.sa_mask);

    // Disable SIGPIPE which is call by the socket write function
    // when client closes its connection during writing.
    // Results in an unwanted server shutdown
    if (sigaction(SIGPIPE, &sig_ign_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot disable SIGPIPE\n");
        return -1;
    }

    // XXX TV
    // According to this:
    // http://stackoverflow.com/questions/7296923/different-signal-handler-for-thread-and-process-is-it-possible
    //
    // SIGPIPE is delivered to the thread generating it.
    // It might thus be possible to stop the session emitting it.

    if (sigaction(SIGTSTP, &sig_ign_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot disable SIGTSTP\n");
        return -1;
    }

    return 0;
}

// Crashed signals
//
// Write backtrace to the syslog. See:
// http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes

static void crash_signal_handler(int sig)
{
    constexpr uint32_t backtrace_buff_size = 100;

    // The signal handler is called several times
    // on a segmentation fault (WHY ?).
    // So only display the backtrace the first time
    if (SignalHandler::s_interrupted)
        return;

    const char *sig_name;

    switch (sig) {
      case SIGBUS:
        sig_name = "(Bus Error)";
        break;
      case SIGSEGV:
        sig_name = "(Segmentation Fault)";
        break;
      case SIGABRT:
        sig_name = "(Abort)";
        break;
      default:
        sig_name = "(Unidentify signal)";
    }

    SignalHandler::server->syslog.print<PANIC>(
                              "CRASH: signal %d %s\n", sig, sig_name);

    void *buffer[backtrace_buff_size];
    auto size = backtrace(buffer, backtrace_buff_size);
    char **messages = backtrace_symbols(buffer, size);

    if (messages == nullptr) {
        SignalHandler::server->syslog.print<ERROR>(
                                             "No backtrace_symbols");
        goto exit;
    }

    for (int i = 0; i < size && messages != nullptr; i++) {
        char *mangled_name = nullptr;
        char *offset_begin = nullptr;
        char *offset_end = nullptr;

        // Find parentheses and +address offset surrounding mangled name
        for (char *p = messages[i]; *p; ++p) {
            if (*p == '(') {
                mangled_name = p;
            }
            else if (*p == '+') {
                offset_begin = p;
            }
            else if (*p == ')') {
                offset_end = p;
                break;
            }
        }

        // If the line could be processed, attempt to demangle the symbol
        if (mangled_name && offset_begin && offset_end
            && mangled_name < offset_begin) {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            char *real_name = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

            // If demangling is successful, output the demangled function name
            if (status == 0) {
                SignalHandler::server->syslog.print<INFO>(
                        "[bt]: (%d) %s : %s+%s%s\n",
                        i, messages[i], real_name, offset_begin, offset_end);
            } else { // Otherwise, output the mangled function name
                SignalHandler::server->syslog.print<INFO>(
                        "[bt]: (%d) %s : %s+%s%s\n",
                        i, messages[i], mangled_name, offset_begin, offset_end);
            }

            free(real_name);
        }
    }

    free(messages);

exit: // Exit Server
    SignalHandler::s_interrupted = 1;
}

int SignalHandler::set_crash_signals()
{
    struct sigaction sig_crash_handler{};

    sig_crash_handler.sa_handler = crash_signal_handler;
    sigemptyset(&sig_crash_handler.sa_mask);
    sig_crash_handler.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(SIGSEGV, &sig_crash_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot set SIGSEGV handler\n");
        return -1;
    }

    if (sigaction(SIGBUS, &sig_crash_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot set SIGBUS handler\n");
        return -1;
    }

    if (sigaction(SIGABRT, &sig_crash_handler, nullptr) < 0) {
        server->syslog.print<CRITICAL>("Cannot set SIGABRT handler\n");
        return -1;
    }

    return 0;
}

} // namespace koheron
