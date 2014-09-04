#include "oakintro.h"
#include "engine/video.h"
#include "engine/callback.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "images/lucas.h"
#include "images/dawn.h"

/* What BLDALPHA will be set to when half faded */
#define PARTIAL_FADE 0xA03

/* How long fade ins of the big sprites will take */
#define FADE_STEPS 0x5

/* How long each running frame will be visible for */
#define FRAME_DURATION 0xA

/* Define how much the sprites will slide each frame */
#define SLIDE_STEP 2

/* Individual animations. Pair for each half */
frame anim[] = { { 0x0, FRAME_DURATION }, { 0x100, FRAME_DURATION }, { 0x80, FRAME_DURATION }, { 0x180, FRAME_DURATION }, { 0xFFFE, 0 } };
frame anim2[] = { { 0x40, FRAME_DURATION }, { 0x140, FRAME_DURATION }, { 0xC0, FRAME_DURATION }, { 0x1C0, FRAME_DURATION }, { 0xFFFE, 0 } };
frame still[] = { { 0x0, FRAME_DURATION } , { 0xFFFF, 0x0 } };
frame still2[] = { { 0x40, FRAME_DURATION } , { 0xFFFF, 0x0 } };

/* Animation table */
frame *pAnimTop[] = { anim, still };
frame *pAnimBottom[] = { anim2, still2 };

/* OAM Template */
sprite oam = { 0x400, 0xC000, 0x800, 0x0 };

typedef struct genderState {
	u32 dawnAnimate : 1;
	u32 dawnOpaque : 1;
	u32 dawnCenter : 1;
	u32 lucasAnimate : 1;
	u32 lucasOpaque : 1;
	u32 lucasCenter : 1;
	u32 choice : 1;
	u8 lucasFrame;
	u8 dawnFrame;
} genderState;

genderState *state = (genderState*) 0x0203B108;

void lucas_callback(object *self) {
	/* Check if lucas must be semi-transparent */

	if (state->lucasOpaque) {
		self->final_oam.attr0 &= 0xFBFF;
	} else {
		self->final_oam.attr0 = (self->final_oam.attr0 & 0xFBFF) | 0x400;
	}

	if (state->lucasAnimate) {
		/* While we're animating, store the current frame */
		state->lucasFrame = self->anim_frame;
	} else {
		/* Pause on the last known frame */
		self->anim_frame = state->lucasFrame;
	}

	if (state->lucasCenter) {
		/* We must centre Lucas */
		if (self->x > 0x77) {
			self->x -= SLIDE_STEP;
		} else {
			/* He's centered; clear the bit */
			state->lucasCenter = 0;
		}
	}
}

void dawn_callback(object *self) {
	/* Check if Dawn must be semi-transparent */

	if (state->dawnOpaque) {
		self->final_oam.attr0 &= 0xFBFF;
	} else {
		self->final_oam.attr0 = (self->final_oam.attr0 & 0xFBFF) | 0x400;
	}

	if (state->dawnAnimate) {
		state->dawnFrame = self->anim_frame;
	} else {
		self->anim_frame = state->dawnFrame;
	}

	if (state->dawnCenter) {
			/* We must centre Lucas */
			if (self->x < 0x77) {
				self->x += SLIDE_STEP;
			} else {
				/* He's centered; clear the bit */
				state->dawnCenter = 0;
			}
	}
}

/* Lucas */
resource lucasGraphics = { lucasTiles, 0x4000, 0x1000 };
resource lucasPalette = { lucasPal, 0x1000, 0x1000  };
template lucasTop = { 0x1000, 0x1000, &oam, &pAnimTop, 0, 0x8231CFC, lucas_callback };
template lucasBottom = { 0x1000, 0x1000, &oam, &pAnimBottom, 0, 0x8231CFC, lucas_callback };

/* Dawn */
resource dawnGraphics = { dawnTiles, 0x4000, 0x200 };
resource dawnPalette = { dawnPal, 0x200, 0x200  };
template dawnTop = { 0x200, 0x200, &oam, &pAnimTop, 0, 0x8231CFC, dawn_callback };
template dawnBottom = { 0x200, 0x200, &oam, &pAnimBottom, 0, 0x8231CFC, dawn_callback };

u16 loadBigSprite(resource *graphics, resource *palette, template *top, template *bottom, u16 x, u16 y) {
	u16 out;

	object_load_compressed_graphics((u32*) graphics);
	object_load_palette((u32*) palette);

	// Set BLDCNT to allow alpha blending of OAM onto bg
	//display_ioreg_set(0x50, 0x2F50);

	// Set opacity of the sprites
	//display_ioreg_set(0x52, 0x40C);

	// Now we can toggle the one objects opacity bit to set which objects are blended

	out = object_display((u32*) top, x, y, 1) >> 8;
	out |= object_display((u32*) bottom, x, y + 0x40, 1);

	// Should store object ID somewhere, we may need it for later
	return out;
}

void blank(u8 i) {

}

void fadeInLucas(u8 index);
void fadeInDawn(u8 index);
void genderChooser(u8 index);
void confirmGenderChoice(u8 index);
void slideDawn(u8 index);
void slideLucas(u8 index);

void chooseGender(u8 index) {
	//loadBigSprite(&dawnGraphics, &dawnPalette, &dawnTop, &dawnBottom, 0x30, 0x20);
	u16 ids = loadBigSprite(&lucasGraphics, &lucasPalette, &lucasTop, &lucasBottom, 0xC0, 0x20);

	//*state = ids;

	/* We use the semi-transparent OAM mode to select 1st target */
	display_ioreg_set(0x50, 0x2F00);
	display_ioreg_set(0x52, 0x1F00);

	tasks[index].args[6] = FADE_STEPS;
	tasks[index].args[7] = 0x10;
	tasks[index].args[8] = 0x0;
	tasks[index].function = (u32) fadeInLucas;
}

void fadeInLucas(u8 index) {
	u16 bg = tasks[index].args[7];
	u16 fg = tasks[index].args[8];

	if (bg) {
		if (!tasks[index].args[6]) {
			tasks[index].args[7] = bg - 1;
			tasks[index].args[8] = fg + 1;
			display_ioreg_set(0x52, (bg << 8) | fg);

			tasks[index].args[6] = FADE_STEPS;
		} else {
			tasks[index].args[6] -= 1;
		}

	} else {
		/* Set Lucas to be opaque */
		state->lucasOpaque = 1;
		//state->lucasAnimate = 1;

		/* Fade out again */
		display_ioreg_set(0x52, 0x1F00);
		tasks[index].args[6] = FADE_STEPS;
		tasks[index].args[7] = 0x10;
		tasks[index].args[8] = 0x0;

		loadBigSprite(&dawnGraphics, &dawnPalette, &dawnTop, &dawnBottom, 0x30, 0x20);
		tasks[index].function = (u32) fadeInDawn;
	}
}

void fadeInDawn(u8 index) {
	u16 bg = tasks[index].args[7];
	u16 fg = tasks[index].args[8];

	if (bg) {
		if (!tasks[index].args[6]) {
			tasks[index].args[7] = bg - 1;
			tasks[index].args[8] = fg + 1;
			display_ioreg_set(0x52, (bg << 8) | fg);

			tasks[index].args[6] = FADE_STEPS;
		} else {
			tasks[index].args[6] -= 1;
		}
	} else {
		/* Do a half fade */
		display_ioreg_set(0x52, PARTIAL_FADE);
		tasks[index].function = (u32) genderChooser;
	}
}

void genderChooser(u8 index) {
	/*
	 * This task will run until the user presses 'A'.
	 * It will check for button presses and set the appropriate state struct bits.
	 */

	if (buttons->left || buttons->right) {
		/* Bundle into a single case, we only have two options anyway */
		state->choice = !state->choice;
		audio_play(SOUND_CLINK);
	}

	if (state->choice) {
		/* Girl picked */
		state->lucasAnimate = 0;
		state->lucasOpaque = 0;
		state->dawnAnimate = 1;
		state->dawnOpaque = 1;
	} else {
		/* Boy picked */
		state->lucasAnimate = 1;
		state->lucasOpaque = 1;
		state->dawnAnimate = 0;
		state->dawnOpaque = 0;
	}

	if (buttons->A) {
		/* Player has accepted the choice, confirm their decision */
		audio_play(SOUND_CLINK);
		if (state->choice) {
			/* Request the callback to centre Dawn */
			state->dawnCenter = 1;
			state->dawnAnimate = 0;
			tasks[index].function = (u32) slideDawn;
		} else {
			/* Request the callback to centre Dawn */
			state->lucasCenter = 1;
			state->lucasAnimate = 0;
			tasks[index].function = (u32) slideLucas;
		}
	}
}

void slideDawn(u8 index) {
	if (state->dawnCenter) {
		/* Dawn is still being centred. Wait for callback to clear bit */
		return;
	}
	tasks[index].function = (u32) confirmGenderChoice;
}

void slideLucas(u8 index) {
	if (state->lucasCenter) {
		/* Lucas is still being centred */
		return;
	}

	tasks[index].function = (u32) confirmGenderChoice;
}

void confirmGenderChoice(u8 index) {

}