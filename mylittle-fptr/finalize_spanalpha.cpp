#include "z64.h"
#include "commonrdp.h"
#include "externfunc.h"


int finalize_spanalpha_generic();

int finalize_spanalpha_clamp_fb_noir();
int finalize_spanalpha_clamp_fb_ir();
int finalize_spanalpha_clamp_nofb_ae_noir();
int finalize_spanalpha_clamp_nofb_ae_ir();
int finalize_spanalpha_clamp_nofb_noae();
int finalize_spanalpha_wrap_noir();
int finalize_spanalpha_wrap_ir();
int finalize_spanalpha_zap();
int finalize_spanalpha_save_noir();
int finalize_spanalpha_save_ir();


int (*finalize_spanalpha_func[0x20])() =
{
	finalize_spanalpha_clamp_nofb_noae,
	finalize_spanalpha_wrap_noir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_noir,
	finalize_spanalpha_clamp_fb_noir,
	finalize_spanalpha_wrap_noir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_noir,
	finalize_spanalpha_clamp_nofb_ae_noir,
	finalize_spanalpha_wrap_noir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_noir,
	finalize_spanalpha_clamp_fb_noir,
	finalize_spanalpha_wrap_noir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_noir,
	finalize_spanalpha_clamp_nofb_noae,
	finalize_spanalpha_wrap_ir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_ir,
	finalize_spanalpha_clamp_fb_ir,
	finalize_spanalpha_wrap_ir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_ir,
	finalize_spanalpha_clamp_nofb_ae_ir,
	finalize_spanalpha_wrap_ir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_ir,
	finalize_spanalpha_clamp_fb_ir,
	finalize_spanalpha_wrap_ir,
	finalize_spanalpha_zap,
    finalize_spanalpha_save_ir
};

extern OTHER_MODES other_modes;
#define CVG_CLAMP				0
#define CVG_WRAP				1
#define CVG_ZAP					2
#define CVG_SAVE				3


extern UINT32 blend_en;
extern UINT32 curpixel_cvg, curpixel_memcvg;

int finalize_spanalpha_generic()
{
	int finalcvg;

	
	
	switch(other_modes.cvg_dest)
	{
	case CVG_CLAMP: 
		if (!blend_en)
		{
			finalcvg = curpixel_cvg - 1;
		}
		else
		{
			finalcvg = curpixel_cvg + curpixel_memcvg;
		}
		if (finalcvg & 8)
			finalcvg = 7;
		else
			finalcvg &= 7;
		break;
	case CVG_WRAP:
		finalcvg = (curpixel_cvg + curpixel_memcvg) & 7;
		break;
	case CVG_ZAP: 
		finalcvg = 7;
		break;
	case CVG_SAVE: 
		finalcvg = curpixel_memcvg;
		break;
	}

	return finalcvg;
}


int finalize_spanalpha_clamp_nofb_ae_ir()
{
	int finalcvg;
	if (!blend_en)
	{
		finalcvg = curpixel_cvg - 1;
	}
	else
	{
		finalcvg = curpixel_cvg + curpixel_memcvg;
	}
	if (finalcvg & 8)
		finalcvg = 7;
	else
		finalcvg &= 7;
	return finalcvg;
}

int finalize_spanalpha_clamp_nofb_ae_noir()
{
	int finalcvg;
	if (!blend_en)
	{
		finalcvg = curpixel_cvg - 1;
	}
	else
	{
		finalcvg = curpixel_cvg + 7;
	}
	if (finalcvg & 8)
		finalcvg = 7;
	else
		finalcvg &= 7;
	return finalcvg;
}

int finalize_spanalpha_clamp_nofb_noae()
{
	int finalcvg = curpixel_cvg - 1;
	if (finalcvg & 8)
		finalcvg = 7;
	else
		finalcvg &= 7;
	return finalcvg;
}

int finalize_spanalpha_clamp_fb_ir()
{
	int finalcvg;
	finalcvg = curpixel_cvg + curpixel_memcvg;
	if (finalcvg & 8)
		finalcvg = 7;
	else
		finalcvg &= 7;
	return finalcvg;
}

int finalize_spanalpha_clamp_fb_noir()
{
	int finalcvg;
	finalcvg = curpixel_cvg + 7;
	if (finalcvg & 8)
		finalcvg = 7;
	else
		finalcvg &= 7;
	return finalcvg;
}

int finalize_spanalpha_wrap_ir()
{
	return ((curpixel_cvg + curpixel_memcvg) & 7);
}

int finalize_spanalpha_wrap_noir()
{
	return ((curpixel_cvg + 7) & 7);
}

int finalize_spanalpha_zap()
{
	return 7;
}

int finalize_spanalpha_save_ir()
{
	return curpixel_memcvg;
}

int finalize_spanalpha_save_noir()
{
	return 7;
}