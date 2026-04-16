/******************************************************************************
 * Poseidon Embedded Software Ltd
 *
 * @file    tank_level.c
 * @brief   Tank level measurement functions
 * @author  Poseidon Embedded Software Ltd
 * @date    16 Apr 2026
 *
 * @details
 * Allows a user to enter the parameters of any given water/fuel tank and when
 * called with a level measurement, will convert the raw distance measurement
 * into liquid volume readings. Some filtering functions are also added to
 * remove spurious noise from measurements.
 *
 * MIT License Copyright (c) 2026 Poseidon Embedded Software Ltd
 *
 * @note
 * Target      :
 * Layer       :
 * Dependencies:
 ******************************************************************************/

/* Includes -----------------------------------------------------------------*/

#include "tank_level.h"
#include <math.h>
#include <stdint.h>

/* Private Macros -----------------------------------------------------------*/

/* Private Types ------------------------------------------------------------*/

//Moving average struct
typedef struct {
    float buffer[MA_WINDOW_SIZE];
    uint8_t index;
    uint8_t count;
} MovingAverageFilter;

//Exponential moving average struct
typedef struct {
    float alpha;   // 0.0 - 1.0
    float value;
    uint8_t initialized;
} EMAFilter;

//Median filter struct
typedef struct {
    float buffer[MEDIAN_SIZE];
    uint8_t index;
    uint8_t filled;
} MedianFilter;

//Rate limiter struct
typedef struct {
    float max_rise_per_sec;
    float max_fall_per_sec;
    float last_value;
    uint8_t initialized;
} RateLimiter;




/* Private Constants --------------------------------------------------------*/

/* Private Variables --------------------------------------------------------*/

/* Private Function Prototypes ----------------------------------------------*/

/* Public Functions ---------------------------------------------------------*/

/* Private Functions --------------------------------------------------------*/

/*
 * Rectangular tank
 *
 * length_mm = internal tank length in mm
 * width_mm = internal tank width in mm
 * level_mm = measured liquid level in mm
 *
 * Returns volume in litres
 */
float tank_volume_rectangular_litres(float length_mm, float width_mm,
		float level_mm)
{
	if (length_mm <= 0.0f || width_mm <= 0.0f || level_mm <= 0.0f)
	{
		return 0.0f;
	}
	/* Volume in cubic mm */
	float volume_mm3 = length_mm * width_mm * level_mm;
	/* 1 litre = 1,000,000 cubic mm */
	return volume_mm3 / 1000000.0f;
}

/*
 * Vertical cylindrical tank
 *
 * diameter_mm = internal tank diameter in mm
 * level_mm = measured liquid level in mm
 *
 * Returns volume in litres
 */
float tank_volume_vertical_cylinder_litres(float diameter_mm, float level_mm)
{
	if (diameter_mm <= 0.0f || level_mm <= 0.0f)
	{
		return 0.0f;
	}
	float radius_mm = diameter_mm / 2.0f;
	/* Volume in cubic mm: pi * r^2 * h */
	float volume_mm3 = (float) M_PI * radius_mm * radius_mm * level_mm;
	return volume_mm3 / 1000000.0f;
}

/*
 * Horizontal cylindrical tank
 *
 * diameter_mm = internal tank diameter in mm
 * length_mm = internal tank straight length in mm
 * level_mm = measured liquid level in mm
 *
 * Returns volume in litres
 *
 * Formula uses circular segment area:
 * A = r^2 * acos((r-h)/r) - (r-h) * sqrt(2rh - h^2)
 * Volume = A * length
 */
float tank_volume_horizontal_cylinder_litres(float diameter_mm, float length_mm,
		float level_mm)
{
	if (diameter_mm <= 0.0f || length_mm <= 0.0f || level_mm <= 0.0f)
	{
		return 0.0f;
	}
	float radius_mm = diameter_mm / 2.0f;
	/* Clamp level to valid range */
	if (level_mm > diameter_mm)
	{
		level_mm = diameter_mm;
	}
	float h = level_mm;
	float r = radius_mm;
	float term1 = r * r * acosf((r - h) / r);
	float term2 = (r - h) * sqrtf((2.0f * r * h) - (h * h));
	float segment_area_mm2 = term1 - term2;
	float volume_mm3 = segment_area_mm2 * length_mm;
	return volume_mm3 / 1000000.0f;
}

/*-----------------------------------FILTERS-----------------------------------*/

/*
 * Initializes the moving average filter structure.
 *
 * f - pointer to the MovingAverageFilter struct to initialize
 *
 * Returns void
 */
void ma_init(MovingAverageFilter *f)
{
    f->index = 0;
    f->count = 0;
    for (int i = 0; i < MA_WINDOW_SIZE; i++)
        f->buffer[i] = 0.0f;
}


/*
 * Updates the moving average filter with a new value and returns the filtered result.
 *
 * f - pointer to the MovingAverageFilter struct
 * new_value - the new measurement to add to the filter
 *
 * Returns the current moving average after adding the new value
 */
float ma_update(MovingAverageFilter *f, float new_value)
{
    f->buffer[f->index] = new_value;
    f->index = (f->index + 1) % MA_WINDOW_SIZE;

    if (f->count < MA_WINDOW_SIZE)
        f->count++;

    float sum = 0.0f;
    for (int i = 0; i < f->count; i++)
        sum += f->buffer[i];

    return sum / f->count;
}

/*
 * Initializes the exponential moving average filter structure.
 *
 * f - pointer to the EMAFilter struct to initialize
 * alpha - smoothing factor (0.0 - 1.0)
 *
 * Returns void
 */
void ema_init(EMAFilter *f, float alpha)
{
    f->alpha = alpha;
    f->value = 0.0f;
    f->initialized = 0;
}

/*
 * Updates the exponential moving average filter with a new value and returns the filtered result.
 *
 * f - pointer to the EMAFilter struct
 * new_value - the new measurement to add to the filter
 *
 * Returns the current exponential moving average after adding the new value
 */
float ema_update(EMAFilter *f, float new_value)
{
    if (!f->initialized)
    {
        f->value = new_value;
        f->initialized = 1;
        return new_value;
    }

    f->value = (f->alpha * new_value) + ((1.0f - f->alpha) * f->value);
    return f->value;
}

/*
 * Initializes the median filter structure.
 *
 * f - pointer to the MedianFilter struct to initialize
 *
 * Returns void
 */
void median_init(MedianFilter *f)
{
    f->index = 0;
    f->filled = 0;
}

/*
 * Sorts the input array and returns the median value.
 *
 * arr - pointer to the array of float values
 * n - number of elements in the array
 *
 * Returns the median value from the array
 */
static float sort_and_get_median(float *arr, int n)
{
    float temp[n];
    for (int i = 0; i < n; i++)
        temp[i] = arr[i];

    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (temp[j] < temp[i])
            {
                float t = temp[i];
                temp[i] = temp[j];
                temp[j] = t;
            }

    return temp[n / 2];
}

/*
 * Updates the median filter with a new value and returns the filtered result.
 *
 * f - pointer to the MedianFilter struct
 * new_value - the new measurement to add to the filter
 *
 * Returns the current median after adding the new value
 */
float median_update(MedianFilter *f, float new_value)
{
    f->buffer[f->index] = new_value;
    f->index = (f->index + 1) % MEDIAN_SIZE;

    if (f->filled < MEDIAN_SIZE)
        f->filled++;

    return sort_and_get_median(f->buffer, f->filled);
}

/*
 * Initializes the rate limiter structure.
 *
 * r - pointer to the RateLimiter struct to initialize
 * rise - maximum allowed rise rate in units per second
 * fall - maximum allowed fall rate in units per second
 *
 * Returns void
 */
void rate_init(RateLimiter *r, float rise, float fall)
{
    r->max_rise_per_sec = rise;
    r->max_fall_per_sec = fall;
    r->initialized = 0;
}

/*
 * Updates the rate limiter with a new value and returns the filtered result.
 *
 * r - pointer to the RateLimiter struct
 * new_value - the new measurement to add to the filter
 * dt_sec - time elapsed since last update in seconds
 *
 * Returns the current value after applying the rate limit
 */
float rate_update(RateLimiter *r, float new_value, float dt_sec)
{
    if (!r->initialized)
    {
        r->last_value = new_value;
        r->initialized = 1;
        return new_value;
    }

    float max_rise = r->max_rise_per_sec * dt_sec;
    float max_fall = r->max_fall_per_sec * dt_sec;

    float delta = new_value - r->last_value;

    if (delta > max_rise)
        delta = max_rise;
    else if (delta < -max_fall)
        delta = -max_fall;

    r->last_value += delta;
    return r->last_value;
}
