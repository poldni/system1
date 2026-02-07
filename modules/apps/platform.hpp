#ifndef APP_PLATFORM_HPP
#define APP_PLATFORM_HPP

// This header acts as the central point for platform-specific configuration.
// It includes the correct Board Support Package (BSP) implementation
// based on the compile-time definition set by CMake.

#if defined(IDF_TARGET)
// ESP32 Target


namespace app
{
   

} // namespace app

#else


namespace app
{
    

} // namespace app

#endif

#endif // APP_PLATFORM_HPP