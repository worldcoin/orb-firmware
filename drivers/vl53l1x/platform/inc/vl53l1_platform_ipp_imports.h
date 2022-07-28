
#ifdef VL53L1_NEEDS_IPP
#  undef VL53L1_IPP_API
#  define VL53L1_IPP_API  __declspec(dllimport)
#  pragma comment (lib, "EwokPlus25API_IPP")
#endif
