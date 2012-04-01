
#ifndef __COMMONRDP_H__
#define __COMMONRDP_H__


typedef struct
{
	int blshiftersused;
	int cvgneededafterzinterp;
} FAKEOTHERMODES;

typedef struct
{
	int cycle_type;
	int persp_tex_en;
	int detail_tex_en;
	int sharpen_tex_en;
	int tex_lod_en;
	int en_tlut;
	int tlut_type;
	int sample_type;
	int mid_texel;
	int bi_lerp0;
	int bi_lerp1;
	int convert_one;
	int key_en;
	int rgb_dither_sel;
	int alpha_dither_sel;
	int blend_m1a_0;
	int blend_m1a_1;
	int blend_m1b_0;
	int blend_m1b_1;
	int blend_m2a_0;
	int blend_m2a_1;
	int blend_m2b_0;
	int blend_m2b_1;
	int force_blend;
	int alpha_cvg_select;
	int cvg_times_alpha;
	int z_mode;
	int cvg_dest;
	int color_on_cvg;
	int image_read_en;
	int z_update_en;
	int z_compare_en;
	int antialias_en;
	int z_source_sel;
	int dither_alpha_en;
	int alpha_compare_en;
	FAKEOTHERMODES f;
} OTHER_MODES;

#endif