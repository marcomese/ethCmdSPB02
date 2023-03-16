#include <math.h>

#define deltat 1.0f // sampling period in seconds (shown as 1 ms)
#define gyroMeasError 3.14159265358979f * (5.0f / 180.0f) // gyroscope measurement error in rad/s (shown as 5 deg/s)
#define beta sqrt(3.0f / 4.0f) * gyroMeasError // compute beta

void filterUpdate(float w_x, float w_y, float w_z, float a_x, float a_y, float a_z, float* SEq_1, float* SEq_2, float* SEq_3, float* SEq_4);
void eulerAngles(float q1, float q2, float q3, float q4, float* roll, float* pitch, float* yaw);