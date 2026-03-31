#include "exceptions.h"
#include "uart.h"

void extern "C" lower_el2_sync(ExceptionContext& ex);
