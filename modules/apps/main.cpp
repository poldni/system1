#include "platform.hpp"

#include <array>
#include <cstdint>

static void run_application()
{
    
}

#if defined(IDF_TARGET)
// Entry point for ESP-IDF
extern "C" void app_main()
{
    run_application();
}
#else
// Entry point for Host (Windows, Linux)
int main()
{
    run_application();
    return 0;
}
#endif
