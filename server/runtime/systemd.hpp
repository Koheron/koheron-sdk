
#ifndef __KOHERON_SYSTEMD_HPP__
#define __KOHERON_SYSTEMD_HPP__

#include <string_view>

namespace systemd {

void notify_ready(std::string_view status_msg = {});

} // namespace systemd

#endif //__KOHERON_SYSTEMD_HPP__