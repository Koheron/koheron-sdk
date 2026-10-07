# Shared native compiler settings; source from the instrument test runners.
pna_test_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$pna_test_repo"
PNA_TEST_CXX=${PNA_TEST_CXX:-${CXX:-g++-13}}
flags=(-std=c++20 -g -pthread -fsanitize=address,undefined -fno-omit-frame-pointer
       -no-pie -I. -Iserver/external_libs -I/usr/include/eigen3)
if [[ -n ${PNA_TEST_CXXFLAGS:-} ]]; then
    read -r -a pna_test_extra_flags <<< "$PNA_TEST_CXXFLAGS"
    flags+=("${pna_test_extra_flags[@]}")
fi
