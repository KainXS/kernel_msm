/* modify for 1.7232 baseline upgrade */
#include <linux/mfd/pm8xxx/batterydata-lib.h>

static struct single_row_lut fcc_temp = {
	.x		= {-20, 0, 25, 40, 60},
	.y		= {1954, 1975, 1983, 1983, 1979},
	.cols		= 5
};

static struct single_row_lut fcc_sf = {
	.x		= {0},
	.y		= {100},
	.cols		= 1
};

static struct sf_lut rbatt_sf = { 
	.rows		= 28, 
	.cols			= 5,
	/* row_entries are temperature */
	.row_entries	= {-20, 0, 25, 40, 60},
	.percent		= {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
	.sf 			= {
					{1068,337,100,74,64},
					{1047,341,102,75,65},
					{1019,341,104,77,66},
					{997,339,108,79,67},
					{984,332,113,81,69},
					{983,324,120,86,72},
					{996,319,126,92,75},
					{1015,316,114,95,81},
					{1032,315,101,77,66},
					{1054,316,100,75,65},
					{1092,322,101,77,67},
					{1151,338,103,80,70},
					{1224,360,105,81,73},
					{1309,380,110,80,67},
					{1516,397,116,79,67},
					{1981,411,122,80,68},
					{2732,439,122,81,67},
					{4209,479,138,92,74},
					{3367,405,125,88,73},
					{3796,411,129,90,74},
					{4394,419,135,92,74},
					{5212,429,135,94,80},
					{6289,442,136,102,85},
					{7855,471,137,107,90},
					{10236,526,140,112,95},
					{14033,633,153,119,102},
					{20801,984,183,136,119},
					{34851,2602,271,200,278},   
	}
};

static struct pc_temp_ocv_lut pc_temp_ocv = {
	.rows	= 29,
	.cols		= 5,
	.temp	= {-20, 0, 25, 40, 60},
	.percent	= {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
	.ocv		= {
				{4322, 4324, 4319, 4314, 4306},
				{4175, 4235, 4247, 4247, 4243},
				{4081, 4172, 4188, 4189, 4185},
				{4003, 4115, 4133, 4134, 4130},
				{3935, 4061, 4081, 4081, 4078},
				{3878, 4006, 4032, 4032, 4030},
				{3832, 3954, 3987, 3988, 3986},
				{3799, 3908, 3944, 3947, 3945},
				{3771, 3868, 3894, 3903, 3903},
				{3737, 3834, 3851, 3855, 3855},
				{3699, 3805, 3823, 3826, 3826},
				{3664, 3784, 3801, 3805, 3804},
				{3635, 3770, 3784, 3787, 3786},
				{3610, 3757, 3771, 3772, 3770},
				{3586, 3739, 3760, 3758, 3747},
				{3559, 3715, 3744, 3740, 3727},
				{3526, 3683, 3714, 3712, 3700},
				{3485, 3638, 3675, 3676, 3665},
				{3426, 3574, 3648, 3657, 3649},
				{3411, 3559, 3640, 3648, 3637},
				{3394, 3537, 3623, 3628, 3614},
				{3375, 3508, 3591, 3598, 3580},
				{3352, 3469, 3548, 3558, 3539},
				{3325, 3422, 3494, 3509, 3489},
				{3291, 3366, 3430, 3446, 3426},
				{3247, 3298, 3356, 3371, 3350},
				{3185, 3218, 3271, 3279, 3254},
				{3098, 3124, 3159, 3153, 3127},
				{3000, 3000, 3000, 3000, 3000}
	}
};


/* modify for 1.7232 baseline upgrade */
struct bms_battery_data Huawei_HB5V1HV_data = {
	.fcc				= 1950,
	.fcc_temp_lut		= &fcc_temp,
	.fcc_sf_lut		= &fcc_sf,
	.pc_temp_ocv_lut	= &pc_temp_ocv,
        .rbatt_sf_lut		= &rbatt_sf,
	.default_rbatt_mohm	=157,

};
