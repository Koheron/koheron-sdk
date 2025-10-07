
#ifndef __SERVER_RUNTIME_SYSTEMD_HPP__
#define __SERVER_RUNTIME_SYSTEMD_HPP__

#include <string_view>

namespace systemd {

void notify_ready(std::string_view status_msg = {});

} // namespace systemd

#endif //__SERVER_RUNTIME_SYSTEMD_HPP__