#ifndef __SERVER_RUNTIME_SYSTEMD_HPP__
#define __SERVER_RUNTIME_SYSTEMD_HPP__

#include <string_view>

namespace rt::systemd {

void notify_ready(std::string_view status_msg = {});

} // namespace rt::systemd

#endif //__SERVER_RUNTIME_SYSTEMD_HPP__

