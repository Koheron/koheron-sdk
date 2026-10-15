#pragma once
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "server/drivers/dpll/board.hpp"
using DpllBoard = dpll::ClockedBoard<ClockGenerator>;
