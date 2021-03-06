#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "sensors_model.h"
#include "quad_parameters.h"

// Magnetometer constants and functions:
// Adapted from Gazebo PX4 SITL:
// https://github.com/PX4/sitl_gazebo/blob/2e80474653bcb7808a7dfced4cf403ef607d69da/src/gazebo_mavlink_interface.cpp
// https://github.com/PX4/sitl_gazebo/blob/2e80474653bcb7808a7dfced4cf403ef607d69da/src/geo_mag_declination.cpp
// Set this always to the sampling in degrees for the table below
#define SAMPLING_RES		10.0
#define SAMPLING_MIN_LAT	-60.0
#define SAMPLING_MAX_LAT	60.0
#define SAMPLING_MIN_LON	-180.0
#define SAMPLING_MAX_LON	180.0

int get_lookup_table_index(float *val, float min, float max);
float get_table_data(float lat, float lon, int table[13][37]);
float get_mag_declination(float lat, float lon);
float get_mag_inclination(float lat, float lon);
float get_mag_strength(float lat, float lon);

void calc_mag_field(double lat, double lon, double dcm_be[3][3], double mag_field[3]);
void calc_baro(BaroSensor *baro, double alt, double vel_b_x, double noise);

/*
 * Calculation / lookup table for Earth's magnetic field declination (deg), inclination (deg) and strength (centi-Tesla).
 * Data generated by https://www.ngdc.noaa.gov/geomag-web/#igrfgrid IGRF calculator on 22 Jan 2018
 */

// Declination data in degrees
int declination_table[13][37] = {
	{ 47,46,45,43,42,41,39,37,33,29,23,16,10,4,-1,-6,-10,-15,-20,-27,-34,-42,-49,-56,-62,-67,-72,-74,-75,-73,-61,-22,26,42,47,48,47 },
	{ 31,31,31,30,30,30,30,29,27,24,18,11,3,-4,-9,-13,-15,-18,-21,-27,-33,-40,-47,-52,-56,-57,-56,-52,-44,-30,-14,2,14,22,27,30,31 },
	{ 22,23,23,23,22,22,22,23,22,19,13,5,-4,-12,-17,-20,-22,-22,-23,-25,-30,-36,-41,-45,-46,-44,-39,-31,-21,-11,-3,4,10,15,19,21,22 },
	{ 17,17,17,18,17,17,17,17,16,13,8,-1,-10,-18,-22,-25,-26,-25,-22,-20,-21,-25,-29,-32,-31,-28,-23,-16,-9,-3,0,4,7,11,14,16,17 },
	{ 13,13,14,14,14,13,13,12,11,9,3,-5,-14,-20,-24,-25,-24,-21,-17,-12,-9,-11,-14,-17,-18,-16,-12,-8,-3,-0,1,3,6,8,11,12,13 },
	{ 11,11,11,11,11,10,10,10,9,6,-0,-8,-15,-21,-23,-22,-19,-15,-10,-5,-2,-2,-4,-7,-9,-8,-7,-4,-1,1,1,2,4,7,9,10,11 },
	{ 10,9,9,9,9,9,9,8,7,3,-3,-10,-16,-20,-20,-18,-14,-9,-5,-2,1,2,0,-2,-4,-4,-3,-2,-0,0,0,1,3,5,7,9,10 },
	{ 9,9,9,9,9,9,9,8,6,1,-4,-11,-16,-18,-17,-14,-10,-5,-2,-0,2,3,2,0,-1,-2,-2,-1,-0,-1,-1,-1,1,3,6,8,9 },
	{ 8,9,9,10,10,10,10,8,5,0,-6,-12,-15,-16,-15,-11,-7,-4,-1,1,3,4,3,2,1,0,-0,-0,-1,-2,-3,-4,-2,0,3,6,8 },
	{ 7,9,10,11,12,12,12,9,5,-1,-7,-13,-15,-15,-13,-10,-6,-3,0,2,3,4,4,4,3,2,1,0,-1,-3,-5,-6,-6,-3,0,4,7 },
	{ 5,8,11,13,14,15,14,11,5,-2,-9,-15,-17,-16,-13,-10,-6,-3,0,3,4,5,6,6,6,5,4,2,-1,-5,-8,-9,-9,-6,-3,1,5 },
	{ 3,8,11,15,17,17,16,12,5,-4,-12,-18,-19,-18,-16,-12,-8,-4,-0,3,5,7,9,10,10,9,7,4,-1,-6,-10,-12,-12,-9,-5,-1,3 },
	{ 3,8,12,16,19,20,18,13,4,-8,-18,-24,-25,-23,-20,-16,-11,-6,-1,3,7,11,14,16,17,17,14,8,-0,-8,-13,-15,-14,-11,-7,-2,3 },	
};

// Inclination data in degrees
int inclination_table[13][37] = {
	{ -78,-76,-74,-72,-70,-68,-65,-63,-60,-57,-55,-54,-54,-55,-56,-57,-58,-59,-59,-59,-59,-60,-61,-63,-66,-69,-73,-76,-79,-83,-86,-87,-86,-84,-82,-80,-78 },
	{ -72,-70,-68,-66,-64,-62,-60,-57,-54,-51,-49,-48,-49,-51,-55,-58,-60,-61,-61,-61,-60,-60,-61,-63,-66,-69,-72,-76,-78,-80,-81,-80,-79,-77,-76,-74,-72 },
	{ -64,-62,-60,-59,-57,-55,-53,-50,-47,-44,-41,-41,-43,-47,-53,-58,-62,-65,-66,-65,-63,-62,-61,-63,-65,-68,-71,-73,-74,-74,-73,-72,-71,-70,-68,-66,-64 },
	{ -55,-53,-51,-49,-46,-44,-42,-40,-37,-33,-30,-30,-34,-41,-48,-55,-60,-65,-67,-68,-66,-63,-61,-61,-62,-64,-65,-66,-66,-65,-64,-63,-62,-61,-59,-57,-55 },
	{ -42,-40,-37,-35,-33,-30,-28,-25,-22,-18,-15,-16,-22,-31,-40,-48,-55,-59,-62,-63,-61,-58,-55,-53,-53,-54,-55,-55,-54,-53,-51,-51,-50,-49,-47,-45,-42 },
	{ -25,-22,-20,-17,-15,-12,-10,-7,-3,1,3,2,-5,-16,-27,-37,-44,-48,-50,-50,-48,-44,-41,-38,-38,-38,-39,-39,-38,-37,-36,-35,-35,-34,-31,-28,-25 },
	{ -5,-2,1,3,5,8,10,13,16,20,21,19,12,2,-10,-20,-27,-30,-30,-29,-27,-23,-19,-17,-17,-17,-18,-18,-17,-16,-16,-16,-16,-15,-12,-9,-5 },
	{ 15,18,21,22,24,26,29,31,34,36,37,34,28,20,10,2,-3,-5,-5,-4,-2,2,5,7,8,7,7,6,7,7,7,6,5,6,8,11,15 },
	{ 31,34,36,38,39,41,43,46,48,49,49,46,42,36,29,24,20,19,20,21,23,25,28,30,30,30,29,29,29,29,28,27,25,25,26,28,31 },
	{ 43,45,47,49,51,53,55,57,58,59,59,56,53,49,45,42,40,40,40,41,43,44,46,47,47,47,47,47,47,47,46,44,42,41,40,42,43 },
	{ 53,54,56,57,59,61,64,66,67,68,67,65,62,60,57,55,55,54,55,56,57,58,59,59,60,60,60,60,60,60,59,57,55,53,52,52,53 },
	{ 62,63,64,65,67,69,71,73,75,75,74,73,70,68,67,66,65,65,65,66,66,67,68,68,69,70,70,71,71,70,69,67,65,63,62,62,62 },
	{ 71,71,72,73,75,77,78,80,81,81,80,79,77,76,74,73,73,73,73,73,73,74,74,75,76,77,78,78,78,78,77,75,73,72,71,71,71 },
};

// Strength data in centi-Tesla
int strength_table[13][37] = {
	{ 62,60,58,56,54,52,49,46,43,41,38,36,34,32,31,31,30,30,30,31,33,35,38,42,46,51,55,59,62,64,66,67,67,66,65,64,62 },
	{ 59,56,54,52,50,47,44,41,38,35,32,29,28,27,26,26,26,25,25,26,28,30,34,39,44,49,54,58,61,64,65,66,65,64,63,61,59 },
	{ 54,52,49,47,45,42,40,37,34,30,27,25,24,24,24,24,24,24,24,24,25,28,32,37,42,48,52,56,59,61,62,62,62,60,59,56,54 },
	{ 49,47,44,42,40,37,35,33,30,28,25,23,22,23,23,24,25,25,26,26,26,28,31,36,41,46,51,54,56,57,57,57,56,55,53,51,49 },
	{ 43,41,39,37,35,33,32,30,28,26,25,23,23,23,24,25,26,28,29,29,29,30,32,36,40,44,48,51,52,52,51,51,50,49,47,45,43 },
	{ 38,36,35,33,32,31,30,29,28,27,26,25,24,24,25,26,28,30,31,32,32,32,33,35,38,42,44,46,47,46,45,45,44,43,41,40,38 },
	{ 34,33,32,32,31,31,31,30,30,30,29,28,27,27,27,28,29,31,32,33,33,33,34,35,37,39,41,42,43,42,41,40,39,38,36,35,34 },
	{ 33,33,32,32,33,33,34,34,35,35,34,33,32,31,30,30,31,32,33,34,35,35,36,37,38,40,41,42,42,41,40,39,37,36,34,33,33 },
	{ 34,34,34,35,36,37,39,40,41,41,40,39,37,35,35,34,35,35,36,37,38,39,40,41,42,43,44,45,45,45,43,41,39,37,35,34,34 },
	{ 37,37,38,39,41,42,44,46,47,47,46,45,43,41,40,39,39,40,41,41,42,43,45,46,47,48,49,50,50,50,48,46,43,41,39,38,37 },
	{ 42,42,43,44,46,48,50,52,53,53,52,51,49,47,45,45,44,44,45,46,46,47,48,50,51,53,54,55,56,55,54,52,49,46,44,43,42 },
	{ 48,48,49,50,52,53,55,56,57,57,56,55,53,51,50,49,48,48,48,49,49,50,51,53,55,56,58,59,60,60,58,56,54,52,50,49,48 },
	{ 54,54,54,55,56,57,58,58,59,58,58,57,56,54,53,52,51,51,51,51,52,53,54,55,57,58,60,61,62,61,61,59,58,56,55,54,54 },
};

int get_lookup_table_index(float *val, float min, float max)
{
	// Limit to table bounds: required for maxima even when table spans full globe range
	// Limit to (table bounds - 1) because bilinear interpolation requires checking (index + 1)
	*val = constrain(*val, min, max - SAMPLING_RES);

	return ((-(min) + *val) / SAMPLING_RES);
}

float get_table_data(float lat, float lon, int table[13][37])
{
	/*
	 * If the values exceed valid ranges, return zero as default
	 * as we have no way of knowing what the closest real value
	 * would be.
	 */
	if (lat < -90.0 || lat > 90.0 ||
	    lon < -180.0 || lon > 180.0) {
		return 0.0;
	}

	// Round down to nearest sampling resolution
	float min_lat = (int)(lat / SAMPLING_RES) * SAMPLING_RES;
	float min_lon = (int)(lon / SAMPLING_RES) * SAMPLING_RES;

	// Find index of nearest low sampling point
	unsigned min_lat_index = get_lookup_table_index(&min_lat, SAMPLING_MIN_LAT, SAMPLING_MAX_LAT);
	unsigned min_lon_index = get_lookup_table_index(&min_lon, SAMPLING_MIN_LON, SAMPLING_MAX_LON);

	const float data_sw = table[min_lat_index][min_lon_index];
	const float data_se = table[min_lat_index][min_lon_index + 1];
	const float data_ne = table[min_lat_index + 1][min_lon_index + 1];
	const float data_nw = table[min_lat_index + 1][min_lon_index];

	// Perform bilinear interpolation on the four grid corners
	const float lat_scale = constrain((lat - min_lat) / SAMPLING_RES, 0.0, 1.0f);
	const float lon_scale = constrain((lon - min_lon) / SAMPLING_RES, 0.0, 1.0f);

	const float data_min = lon_scale * (data_se - data_sw) + data_sw;
	const float data_max = lon_scale * (data_ne - data_nw) + data_nw;

	return lat_scale * (data_max - data_min) + data_min;
}

float get_mag_declination(float lat, float lon)
{
	return get_table_data(lat, lon, declination_table);
}

float get_mag_inclination(float lat, float lon)
{
	return get_table_data(lat, lon, inclination_table);
}

float get_mag_strength(float lat, float lon)
{
	return get_table_data(lat, lon, strength_table);
}

// GPS

void init_gps(GpsSensor *gps, double eph, double epv, double fix, double visible_sats, double lat_lon_noise_std_dev, double alt_noise_std_dev, double speed_noise_std_dev)
{
    gps->eph = eph;
    gps->epv = epv;
    gps->fix = fix;
    gps->visible_sats = visible_sats;

    gps->lat_lon_noise_std_dev = lat_lon_noise_std_dev;
    gps->alt_noise_std_dev = alt_noise_std_dev;
    gps->speed_noise_std_dev = speed_noise_std_dev;

    gps->lat_lon_alt[0] = HOME_LAT;
    gps->lat_lon_alt[1] = HOME_LON;
    gps->lat_lon_alt[2] = HOME_ALT;
}

void update_gps(GpsSensor *gps, double pos_e[3], double vel_e[3])
{
    int i;
    
    // Convert NED to lat lon alt
    ned_to_latlonalt(pos_e, gps->lat_lon_alt, HOME_LAT, HOME_LON, HOME_ALT);

    // Add noise
    gps->lat_lon_alt[0] = gps->lat_lon_alt[0] + ((NOISE != 0 ) ? zero_mean_noise(gps->lat_lon_noise_std_dev) : 0);
    gps->lat_lon_alt[1] = gps->lat_lon_alt[1] + ((NOISE != 0 ) ? zero_mean_noise(gps->lat_lon_noise_std_dev) : 0);
    gps->lat_lon_alt[2] = gps->lat_lon_alt[2] + ((NOISE != 0 ) ? zero_mean_noise(gps->alt_noise_std_dev) : 0);

    for(i = 0 ; i < 3 ; i++)
        gps->gps_speed[i] = vel_e[i] + ((NOISE != 0 ) ? zero_mean_noise(gps->speed_noise_std_dev) : 0);

    gps->ground_speed = sqrt(pow(gps->gps_speed[0], 2) + pow(gps->gps_speed[1], 2));
    gps->cog = rad2deg(wrap_angle_2pi(atan2(gps->gps_speed[1], gps->gps_speed[0])));
}

// IMU

void init_imu(ImuSensor *imu, double acc_noise_std_dev, double gyro_noise_std_dev)
{
    int i;

    imu->acc_noise_std_dev = acc_noise_std_dev;
    imu->gyro_noise_std_dev = gyro_noise_std_dev;

    for(i = 0 ; i < 3 ; i++)
    {
        imu->acc[i] = 0.0;
        imu->gyro[i] = 0.0;
    }
    imu->acc[2] = -GRAVITY;
}

void update_imu(ImuSensor *imu, double acc_b[3], double omega_b[3], double dcm_be[3][3])
{
    int i;

    for(i = 0 ; i < 3 ; i++)
    {
        imu->acc[i] = acc_b[i] + (dcm_be[i][2] * (-GRAVITY)) + ((NOISE != 0 ) ? zero_mean_noise(imu->acc_noise_std_dev) : 0);
        imu->gyro[i] = omega_b[i] + ((NOISE != 0 ) ? zero_mean_noise(imu->gyro_noise_std_dev) : 0);
    }
}

// Magnetometer

void init_mag(MagSensor *mag, double mag_noise_std_dev)
{
    double dcm_be[3][3];

    // Setup DCM with initial yaw rotation, roll and pitch are zero at start of sim.
    dcm_be[0][0] = cos(HOME_YAW);
    dcm_be[0][1] = sin(HOME_YAW);
    dcm_be[0][2] = 0;
    dcm_be[1][0] = -sin(HOME_YAW);
    dcm_be[1][1] = cos(HOME_YAW);
    dcm_be[1][2] = 0;
    dcm_be[2][0] = 0;
    dcm_be[2][1] = 0;
    dcm_be[2][2] = 1;

    mag->mag_noise_std_dev = mag_noise_std_dev;
    calc_mag_field(HOME_LAT, HOME_LON, dcm_be, mag->mag_field);
}

void update_mag(MagSensor *mag, double lat, double lon, double dcm_be[3][3])
{   
    calc_mag_field(lat, lon, dcm_be, mag->mag_field);

    mag->mag_field[0] = mag->mag_field[0] + ((NOISE != 0 ) ? zero_mean_noise(mag->mag_noise_std_dev) : 0);
    mag->mag_field[1] = mag->mag_field[1] + ((NOISE != 0 ) ? zero_mean_noise(mag->mag_noise_std_dev) : 0);
    mag->mag_field[2] = mag->mag_field[2] + ((NOISE != 0 ) ? zero_mean_noise(mag->mag_noise_std_dev) : 0);
}

void calc_mag_field(double lat, double lon, double dcm_be[3][3], double mag_field[3]) {
    double declination_rad, inclination_rad, strength_ga;
    double H;
    double mag_e[3];
    
    // Magnetic declination and inclination (radians)
    declination_rad = deg2rad(get_mag_declination(lat, lon));
    inclination_rad = deg2rad(get_mag_inclination(lat, lon));

    // Magnetic strength (10^5xnanoTesla)
    strength_ga = 0.01 * get_mag_strength(lat, lon);

    // Magnetic filed components are calculated by http://geomag.nrcan.gc.ca/mag_fld/comp-en.php
    H = strength_ga * cos(inclination_rad);
    mag_e[0] = H * cos(declination_rad);
    mag_e[1] = H * sin(declination_rad);
    mag_e[2] = H * tan(inclination_rad);

    // Magnetic field data from WMM2018 (10^5xnanoTesla)
    earth_to_body_rotation(dcm_be, mag_e, mag_field);
}

// Barometer

void init_baro(BaroSensor *baro, double baro_noise_std_dev)
{
    baro->baro_noise_std_dev = baro_noise_std_dev;
    calc_baro(baro, HOME_ALT, 0, 0);
}

void update_baro(BaroSensor *baro, double alt, double vel_b_x)
{
    double noise = ((NOISE != 0 ) ? zero_mean_noise(baro->baro_noise_std_dev) : 0);
    calc_baro(baro, alt, vel_b_x, noise);
}

void calc_baro(BaroSensor *baro, double alt, double vel_b_x, double noise)
{
    double lapse_rate, temperature_msl, pressure_msl;
    double temperature_local, pressure_ratio;
    double density_ratio;
    double rho;
    
    // Calculate abs_pressure using an ISA model for the tropsphere (valid up to 11km above MSL)
    lapse_rate = -Lb;
    temperature_msl = Tb;
    pressure_msl = Pb;

    temperature_local = temperature_msl - lapse_rate * alt;
    pressure_ratio = pow((temperature_msl/temperature_local) , -((GRAVITY * M) / (R * Lb)));

    // Calculate density using an ISA model for the tropsphere (valid up to 11km above MSL)
    density_ratio = powf((temperature_msl/temperature_local) , 4.256f);
    rho = RHO / density_ratio;

    baro->pressure = 0.01 * ((pressure_msl / pressure_ratio) + noise);
    baro->pressure_alt = alt - noise / (GRAVITY * rho);
    baro->diff_pressure = 0.005 * rho * pow(vel_b_x, 2);
    baro->temperature = temperature_local - 273.0;
}