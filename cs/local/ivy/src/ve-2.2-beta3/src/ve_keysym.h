#ifndef VE_KEYSYM_H
#define VE_KEYSYM_H

#include <ve_version.h>

#ifdef __cplusplus
extern "C" {
#endif

/** misc
    This module defines keycodes for keyboard devices.  All input
    keyboard devices
    should return these keycodes when the appropriate keys are pressed.
    They are all defined as macros, based upon the X11 keysym definitions.
    All macros begin with the <code>VEK_</code> prefix.  See the header
    file for the list of macros.
 */

/* Names for special keys, loosely based on the X11 definitions - notice
   that ASCII keys have their ASCII values here */
/* I've kept all X11 definitions including ones that are unlikely to
   ever be used here.  This does not mean that every keyboard implementation
   will map all of these characters - in fact, most keyboard implementations
   are likely to be downright lazy about it... */
/* Prefix for any keysym definition is "VEK_" */

/* Copyright from the original X11 file: */
/*
Copyright (c) 1987, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* You got that?  So cool those TORTIOUS ACTIONs already... */

/* The following keysym is returned whenever we get a key we do not
   recognize */
#define VEK_UNKNOWN            (-1)

/*
 * TTY Functions, cleverly chosen to map to ascii, for convenience of
 * programming, but could have been arbitrary (at the cost of lookup
 * tables in client code.
 */
/* VE note:  we are deliberately keeping the ASCII encoding so that
   on ASCII systems, you can match against either VEK_x or 'x' */

#define VEK_BackSpace		0xFF08	/* back space, back char */
#define VEK_Tab			0xFF09
#define VEK_Linefeed		0xFF0A	/* Linefeed, LF */
#define VEK_Clear		0xFF0B
#define VEK_Return		0xFF0D	/* Return, enter */
#define VEK_Pause		0xFF13	/* Pause, hold */
#define VEK_Scroll_Lock		0xFF14
#define VEK_Sys_Req		0xFF15
#define VEK_Escape		0xFF1B
#define VEK_Delete		0xFFFF	/* Delete, rubout */

/* International & multi-key character composition */

#define VEK_Multi_key		0xFF20  /* Multi-key character compose */
#define VEK_Codeinput		0xFF37
#define VEK_SingleCandidate	0xFF3C
#define VEK_MultipleCandidate	0xFF3D
#define VEK_PreviousCandidate	0xFF3E

/* Japanese keyboard support */

#define VEK_Kanji		0xFF21	/* Kanji, Kanji convert */
#define VEK_Muhenkan		0xFF22  /* Cancel Conversion */
#define VEK_Henkan_Mode		0xFF23  /* Start/Stop Conversion */
#define VEK_Henkan		0xFF23  /* Alias for Henkan_Mode */
#define VEK_Romaji		0xFF24  /* to Romaji */
#define VEK_Hiragana		0xFF25  /* to Hiragana */
#define VEK_Katakana		0xFF26  /* to Katakana */
#define VEK_Hiragana_Katakana	0xFF27  /* Hiragana/Katakana toggle */
#define VEK_Zenkaku		0xFF28  /* to Zenkaku */
#define VEK_Hankaku		0xFF29  /* to Hankaku */
#define VEK_Zenkaku_Hankaku	0xFF2A  /* Zenkaku/Hankaku toggle */
#define VEK_Touroku		0xFF2B  /* Add to Dictionary */
#define VEK_Massyo		0xFF2C  /* Delete from Dictionary */
#define VEK_Kana_Lock		0xFF2D  /* Kana Lock */
#define VEK_Kana_Shift		0xFF2E  /* Kana Shift */
#define VEK_Eisu_Shift		0xFF2F  /* Alphanumeric Shift */
#define VEK_Eisu_toggle		0xFF30  /* Alphanumeric toggle */
#define VEK_Kanji_Bangou		0xFF37  /* Codeinput */
#define VEK_Zen_Koho		0xFF3D	/* Multiple/All Candidate(s) */
#define VEK_Mae_Koho		0xFF3E	/* Previous Candidate */

/* 0xFF31 thru 0xFF3F are under VEK_KOREAN */

/* Cursor control & motion */

#define VEK_Home			0xFF50
#define VEK_Left			0xFF51	/* Move left, left arrow */
#define VEK_Up			0xFF52	/* Move up, up arrow */
#define VEK_Right		0xFF53	/* Move right, right arrow */
#define VEK_Down			0xFF54	/* Move down, down arrow */
#define VEK_Prior		0xFF55	/* Prior, previous */
#define VEK_Page_Up		0xFF55
#define VEK_Next			0xFF56	/* Next */
#define VEK_Page_Down		0xFF56
#define VEK_End			0xFF57	/* EOL */
#define VEK_Begin		0xFF58	/* BOL */


/* Misc Functions */

#define VEK_Select		0xFF60	/* Select, mark */
#define VEK_Print		0xFF61
#define VEK_Execute		0xFF62	/* Execute, run, do */
#define VEK_Insert		0xFF63	/* Insert, insert here */
#define VEK_Undo			0xFF65	/* Undo, oops */
#define VEK_Redo			0xFF66	/* redo, again */
#define VEK_Menu			0xFF67
#define VEK_Find			0xFF68	/* Find, search */
#define VEK_Cancel		0xFF69	/* Cancel, stop, abort, exit */
#define VEK_Help			0xFF6A	/* Help */
#define VEK_Break		0xFF6B
#define VEK_Mode_switch		0xFF7E	/* Character set switch */
#define VEK_script_switch        0xFF7E  /* Alias for mode_switch */
#define VEK_Num_Lock		0xFF7F

/* Keypad Functions, keypad numbers cleverly chosen to map to ascii */

#define VEK_KP_Space		0xFF80	/* space */
#define VEK_KP_Tab		0xFF89
#define VEK_KP_Enter		0xFF8D	/* enter */
#define VEK_KP_F1		0xFF91	/* PF1, KP_A, ... */
#define VEK_KP_F2		0xFF92
#define VEK_KP_F3		0xFF93
#define VEK_KP_F4		0xFF94
#define VEK_KP_Home		0xFF95
#define VEK_KP_Left		0xFF96
#define VEK_KP_Up		0xFF97
#define VEK_KP_Right		0xFF98
#define VEK_KP_Down		0xFF99
#define VEK_KP_Prior		0xFF9A
#define VEK_KP_Page_Up		0xFF9A
#define VEK_KP_Next		0xFF9B
#define VEK_KP_Page_Down		0xFF9B
#define VEK_KP_End		0xFF9C
#define VEK_KP_Begin		0xFF9D
#define VEK_KP_Insert		0xFF9E
#define VEK_KP_Delete		0xFF9F
#define VEK_KP_Equal		0xFFBD	/* equals */
#define VEK_KP_Multiply		0xFFAA
#define VEK_KP_Add		0xFFAB
#define VEK_KP_Separator		0xFFAC	/* separator, often comma */
#define VEK_KP_Subtract		0xFFAD
#define VEK_KP_Decimal		0xFFAE
#define VEK_KP_Divide		0xFFAF

#define VEK_KP_0			0xFFB0
#define VEK_KP_1			0xFFB1
#define VEK_KP_2			0xFFB2
#define VEK_KP_3			0xFFB3
#define VEK_KP_4			0xFFB4
#define VEK_KP_5			0xFFB5
#define VEK_KP_6			0xFFB6
#define VEK_KP_7			0xFFB7
#define VEK_KP_8			0xFFB8
#define VEK_KP_9			0xFFB9



/*
 * Auxilliary Functions; note the duplicate definitions for left and right
 * function keys;  Sun keyboards and a few other manufactures have such
 * function key groups on the left and/or right sides of the keyboard.
 * We've not found a keyboard with more than 35 function keys total.
 */

#define VEK_F1			0xFFBE
#define VEK_F2			0xFFBF
#define VEK_F3			0xFFC0
#define VEK_F4			0xFFC1
#define VEK_F5			0xFFC2
#define VEK_F6			0xFFC3
#define VEK_F7			0xFFC4
#define VEK_F8			0xFFC5
#define VEK_F9			0xFFC6
#define VEK_F10			0xFFC7
#define VEK_F11			0xFFC8
#define VEK_L1			0xFFC8
#define VEK_F12			0xFFC9
#define VEK_L2			0xFFC9
#define VEK_F13			0xFFCA
#define VEK_L3			0xFFCA
#define VEK_F14			0xFFCB
#define VEK_L4			0xFFCB
#define VEK_F15			0xFFCC
#define VEK_L5			0xFFCC
#define VEK_F16			0xFFCD
#define VEK_L6			0xFFCD
#define VEK_F17			0xFFCE
#define VEK_L7			0xFFCE
#define VEK_F18			0xFFCF
#define VEK_L8			0xFFCF
#define VEK_F19			0xFFD0
#define VEK_L9			0xFFD0
#define VEK_F20			0xFFD1
#define VEK_L10			0xFFD1
#define VEK_F21			0xFFD2
#define VEK_R1			0xFFD2
#define VEK_F22			0xFFD3
#define VEK_R2			0xFFD3
#define VEK_F23			0xFFD4
#define VEK_R3			0xFFD4
#define VEK_F24			0xFFD5
#define VEK_R4			0xFFD5
#define VEK_F25			0xFFD6
#define VEK_R5			0xFFD6
#define VEK_F26			0xFFD7
#define VEK_R6			0xFFD7
#define VEK_F27			0xFFD8
#define VEK_R7			0xFFD8
#define VEK_F28			0xFFD9
#define VEK_R8			0xFFD9
#define VEK_F29			0xFFDA
#define VEK_R9			0xFFDA
#define VEK_F30			0xFFDB
#define VEK_R10			0xFFDB
#define VEK_F31			0xFFDC
#define VEK_R11			0xFFDC
#define VEK_F32			0xFFDD
#define VEK_R12			0xFFDD
#define VEK_F33			0xFFDE
#define VEK_R13			0xFFDE
#define VEK_F34			0xFFDF
#define VEK_R14			0xFFDF
#define VEK_F35			0xFFE0
#define VEK_R15			0xFFE0

/* Modifiers */

#define VEK_Shift_L		0xFFE1	/* Left shift */
#define VEK_Shift_R		0xFFE2	/* Right shift */
#define VEK_Control_L		0xFFE3	/* Left control */
#define VEK_Control_R		0xFFE4	/* Right control */
#define VEK_Caps_Lock		0xFFE5	/* Caps lock */
#define VEK_Shift_Lock		0xFFE6	/* Shift lock */

#define VEK_Meta_L		0xFFE7	/* Left meta */
#define VEK_Meta_R		0xFFE8	/* Right meta */
#define VEK_Alt_L		0xFFE9	/* Left alt */
#define VEK_Alt_R		0xFFEA	/* Right alt */
#define VEK_Super_L		0xFFEB	/* Left super */
#define VEK_Super_R		0xFFEC	/* Right super */
#define VEK_Hyper_L		0xFFED	/* Left hyper */
#define VEK_Hyper_R		0xFFEE	/* Right hyper */

/*
 * ISO 9995 Function and Modifier Keys
 * Byte 3 = 0xFE
 */

#define	VEK_ISO_Lock					0xFE01
#define	VEK_ISO_Level2_Latch				0xFE02
#define	VEK_ISO_Level3_Shift				0xFE03
#define	VEK_ISO_Level3_Latch				0xFE04
#define	VEK_ISO_Level3_Lock				0xFE05
#define	VEK_ISO_Group_Shift		0xFF7E	/* Alias for mode_switch */
#define	VEK_ISO_Group_Latch				0xFE06
#define	VEK_ISO_Group_Lock				0xFE07
#define	VEK_ISO_Next_Group				0xFE08
#define	VEK_ISO_Next_Group_Lock				0xFE09
#define	VEK_ISO_Prev_Group				0xFE0A
#define	VEK_ISO_Prev_Group_Lock				0xFE0B
#define	VEK_ISO_First_Group				0xFE0C
#define	VEK_ISO_First_Group_Lock				0xFE0D
#define	VEK_ISO_Last_Group				0xFE0E
#define	VEK_ISO_Last_Group_Lock				0xFE0F

#define	VEK_ISO_Left_Tab					0xFE20
#define	VEK_ISO_Move_Line_Up				0xFE21
#define	VEK_ISO_Move_Line_Down				0xFE22
#define	VEK_ISO_Partial_Line_Up				0xFE23
#define	VEK_ISO_Partial_Line_Down			0xFE24
#define	VEK_ISO_Partial_Space_Left			0xFE25
#define	VEK_ISO_Partial_Space_Right			0xFE26
#define	VEK_ISO_Set_Margin_Left				0xFE27
#define	VEK_ISO_Set_Margin_Right				0xFE28
#define	VEK_ISO_Release_Margin_Left			0xFE29
#define	VEK_ISO_Release_Margin_Right			0xFE2A
#define	VEK_ISO_Release_Both_Margins			0xFE2B
#define	VEK_ISO_Fast_Cursor_Left				0xFE2C
#define	VEK_ISO_Fast_Cursor_Right			0xFE2D
#define	VEK_ISO_Fast_Cursor_Up				0xFE2E
#define	VEK_ISO_Fast_Cursor_Down				0xFE2F
#define	VEK_ISO_Continuous_Underline			0xFE30
#define	VEK_ISO_Discontinuous_Underline			0xFE31
#define	VEK_ISO_Emphasize				0xFE32
#define	VEK_ISO_Center_Object				0xFE33
#define	VEK_ISO_Enter					0xFE34

#define	VEK_dead_grave					0xFE50
#define	VEK_dead_acute					0xFE51
#define	VEK_dead_circumflex				0xFE52
#define	VEK_dead_tilde					0xFE53
#define	VEK_dead_macron					0xFE54
#define	VEK_dead_breve					0xFE55
#define	VEK_dead_abovedot				0xFE56
#define	VEK_dead_diaeresis				0xFE57
#define	VEK_dead_abovering				0xFE58
#define	VEK_dead_doubleacute				0xFE59
#define	VEK_dead_caron					0xFE5A
#define	VEK_dead_cedilla					0xFE5B
#define	VEK_dead_ogonek					0xFE5C
#define	VEK_dead_iota					0xFE5D
#define	VEK_dead_voiced_sound				0xFE5E
#define	VEK_dead_semivoiced_sound			0xFE5F
#define	VEK_dead_belowdot				0xFE60

#define	VEK_First_Virtual_Screen				0xFED0
#define	VEK_Prev_Virtual_Screen				0xFED1
#define	VEK_Next_Virtual_Screen				0xFED2
#define	VEK_Last_Virtual_Screen				0xFED4
#define	VEK_Terminate_Server				0xFED5

#define	VEK_AccessX_Enable				0xFE70
#define	VEK_AccessX_Feedback_Enable			0xFE71
#define	VEK_RepeatKeys_Enable				0xFE72
#define	VEK_SlowKeys_Enable				0xFE73
#define	VEK_BounceKeys_Enable				0xFE74
#define	VEK_StickyKeys_Enable				0xFE75
#define	VEK_MouseKeys_Enable				0xFE76
#define	VEK_MouseKeys_Accel_Enable			0xFE77
#define	VEK_Overlay1_Enable				0xFE78
#define	VEK_Overlay2_Enable				0xFE79
#define	VEK_AudibleBell_Enable				0xFE7A

#define	VEK_Pointer_Left					0xFEE0
#define	VEK_Pointer_Right				0xFEE1
#define	VEK_Pointer_Up					0xFEE2
#define	VEK_Pointer_Down					0xFEE3
#define	VEK_Pointer_UpLeft				0xFEE4
#define	VEK_Pointer_UpRight				0xFEE5
#define	VEK_Pointer_DownLeft				0xFEE6
#define	VEK_Pointer_DownRight				0xFEE7
#define	VEK_Pointer_Button_Dflt				0xFEE8
#define	VEK_Pointer_Button1				0xFEE9
#define	VEK_Pointer_Button2				0xFEEA
#define	VEK_Pointer_Button3				0xFEEB
#define	VEK_Pointer_Button4				0xFEEC
#define	VEK_Pointer_Button5				0xFEED
#define	VEK_Pointer_DblClick_Dflt			0xFEEE
#define	VEK_Pointer_DblClick1				0xFEEF
#define	VEK_Pointer_DblClick2				0xFEF0
#define	VEK_Pointer_DblClick3				0xFEF1
#define	VEK_Pointer_DblClick4				0xFEF2
#define	VEK_Pointer_DblClick5				0xFEF3
#define	VEK_Pointer_Drag_Dflt				0xFEF4
#define	VEK_Pointer_Drag1				0xFEF5
#define	VEK_Pointer_Drag2				0xFEF6
#define	VEK_Pointer_Drag3				0xFEF7
#define	VEK_Pointer_Drag4				0xFEF8
#define	VEK_Pointer_Drag5				0xFEFD

#define	VEK_Pointer_EnableKeys				0xFEF9
#define	VEK_Pointer_Accelerate				0xFEFA
#define	VEK_Pointer_DfltBtnNext				0xFEFB
#define	VEK_Pointer_DfltBtnPrev				0xFEFC

/*
 * 3270 Terminal Keys
 * Byte 3 = 0xFD
 */

#define VEK_3270_Duplicate      0xFD01
#define VEK_3270_FieldMark      0xFD02
#define VEK_3270_Right2         0xFD03
#define VEK_3270_Left2          0xFD04
#define VEK_3270_BackTab        0xFD05
#define VEK_3270_EraseEOF       0xFD06
#define VEK_3270_EraseInput     0xFD07
#define VEK_3270_Reset          0xFD08
#define VEK_3270_Quit           0xFD09
#define VEK_3270_PA1            0xFD0A
#define VEK_3270_PA2            0xFD0B
#define VEK_3270_PA3            0xFD0C
#define VEK_3270_Test           0xFD0D
#define VEK_3270_Attn           0xFD0E
#define VEK_3270_CursorBlink    0xFD0F
#define VEK_3270_AltCursor      0xFD10
#define VEK_3270_KeyClick       0xFD11
#define VEK_3270_Jump           0xFD12
#define VEK_3270_Ident          0xFD13
#define VEK_3270_Rule           0xFD14
#define VEK_3270_Copy           0xFD15
#define VEK_3270_Play           0xFD16
#define VEK_3270_Setup          0xFD17
#define VEK_3270_Record         0xFD18
#define VEK_3270_ChangeScreen   0xFD19
#define VEK_3270_DeleteWord     0xFD1A
#define VEK_3270_ExSelect       0xFD1B
#define VEK_3270_CursorSelect   0xFD1C
#define VEK_3270_PrintScreen    0xFD1D
#define VEK_3270_Enter          0xFD1E

/*
 *  Latin 1
 *  Byte 3 = 0
 */
#define VEK_space               0x020
#define VEK_exclam              0x021
#define VEK_quotedbl            0x022
#define VEK_numbersign          0x023
#define VEK_dollar              0x024
#define VEK_percent             0x025
#define VEK_ampersand           0x026
#define VEK_apostrophe          0x027
#define VEK_quoteright          0x027	/* deprecated */
#define VEK_parenleft           0x028
#define VEK_parenright          0x029
#define VEK_asterisk            0x02a
#define VEK_plus                0x02b
#define VEK_comma               0x02c
#define VEK_minus               0x02d
#define VEK_period              0x02e
#define VEK_slash               0x02f
#define VEK_0                   0x030
#define VEK_1                   0x031
#define VEK_2                   0x032
#define VEK_3                   0x033
#define VEK_4                   0x034
#define VEK_5                   0x035
#define VEK_6                   0x036
#define VEK_7                   0x037
#define VEK_8                   0x038
#define VEK_9                   0x039
#define VEK_colon               0x03a
#define VEK_semicolon           0x03b
#define VEK_less                0x03c
#define VEK_equal               0x03d
#define VEK_greater             0x03e
#define VEK_question            0x03f
#define VEK_at                  0x040
#define VEK_A                   0x041
#define VEK_B                   0x042
#define VEK_C                   0x043
#define VEK_D                   0x044
#define VEK_E                   0x045
#define VEK_F                   0x046
#define VEK_G                   0x047
#define VEK_H                   0x048
#define VEK_I                   0x049
#define VEK_J                   0x04a
#define VEK_K                   0x04b
#define VEK_L                   0x04c
#define VEK_M                   0x04d
#define VEK_N                   0x04e
#define VEK_O                   0x04f
#define VEK_P                   0x050
#define VEK_Q                   0x051
#define VEK_R                   0x052
#define VEK_S                   0x053
#define VEK_T                   0x054
#define VEK_U                   0x055
#define VEK_V                   0x056
#define VEK_W                   0x057
#define VEK_X                   0x058
#define VEK_Y                   0x059
#define VEK_Z                   0x05a
#define VEK_bracketleft         0x05b
#define VEK_backslash           0x05c
#define VEK_bracketright        0x05d
#define VEK_asciicircum         0x05e
#define VEK_underscore          0x05f
#define VEK_grave               0x060
#define VEK_quoteleft           0x060	/* deprecated */
#define VEK_a                   0x061
#define VEK_b                   0x062
#define VEK_c                   0x063
#define VEK_d                   0x064
#define VEK_e                   0x065
#define VEK_f                   0x066
#define VEK_g                   0x067
#define VEK_h                   0x068
#define VEK_i                   0x069
#define VEK_j                   0x06a
#define VEK_k                   0x06b
#define VEK_l                   0x06c
#define VEK_m                   0x06d
#define VEK_n                   0x06e
#define VEK_o                   0x06f
#define VEK_p                   0x070
#define VEK_q                   0x071
#define VEK_r                   0x072
#define VEK_s                   0x073
#define VEK_t                   0x074
#define VEK_u                   0x075
#define VEK_v                   0x076
#define VEK_w                   0x077
#define VEK_x                   0x078
#define VEK_y                   0x079
#define VEK_z                   0x07a
#define VEK_braceleft           0x07b
#define VEK_bar                 0x07c
#define VEK_braceright          0x07d
#define VEK_asciitilde          0x07e

#define VEK_nobreakspace        0x0a0
#define VEK_exclamdown          0x0a1
#define VEK_cent        	       0x0a2
#define VEK_sterling            0x0a3
#define VEK_currency            0x0a4
#define VEK_yen                 0x0a5
#define VEK_brokenbar           0x0a6
#define VEK_section             0x0a7
#define VEK_diaeresis           0x0a8
#define VEK_copyright           0x0a9
#define VEK_ordfeminine         0x0aa
#define VEK_guillemotleft       0x0ab	/* left angle quotation mark */
#define VEK_notsign             0x0ac
#define VEK_hyphen              0x0ad
#define VEK_registered          0x0ae
#define VEK_macron              0x0af
#define VEK_degree              0x0b0
#define VEK_plusminus           0x0b1
#define VEK_twosuperior         0x0b2
#define VEK_threesuperior       0x0b3
#define VEK_acute               0x0b4
#define VEK_mu                  0x0b5
#define VEK_paragraph           0x0b6
#define VEK_periodcentered      0x0b7
#define VEK_cedilla             0x0b8
#define VEK_onesuperior         0x0b9
#define VEK_masculine           0x0ba
#define VEK_guillemotright      0x0bb	/* right angle quotation mark */
#define VEK_onequarter          0x0bc
#define VEK_onehalf             0x0bd
#define VEK_threequarters       0x0be
#define VEK_questiondown        0x0bf
#define VEK_Agrave              0x0c0
#define VEK_Aacute              0x0c1
#define VEK_Acircumflex         0x0c2
#define VEK_Atilde              0x0c3
#define VEK_Adiaeresis          0x0c4
#define VEK_Aring               0x0c5
#define VEK_AE                  0x0c6
#define VEK_Ccedilla            0x0c7
#define VEK_Egrave              0x0c8
#define VEK_Eacute              0x0c9
#define VEK_Ecircumflex         0x0ca
#define VEK_Ediaeresis          0x0cb
#define VEK_Igrave              0x0cc
#define VEK_Iacute              0x0cd
#define VEK_Icircumflex         0x0ce
#define VEK_Idiaeresis          0x0cf
#define VEK_ETH                 0x0d0
#define VEK_Eth                 0x0d0	/* deprecated */
#define VEK_Ntilde              0x0d1
#define VEK_Ograve              0x0d2
#define VEK_Oacute              0x0d3
#define VEK_Ocircumflex         0x0d4
#define VEK_Otilde              0x0d5
#define VEK_Odiaeresis          0x0d6
#define VEK_multiply            0x0d7
#define VEK_Ooblique            0x0d8
#define VEK_Ugrave              0x0d9
#define VEK_Uacute              0x0da
#define VEK_Ucircumflex         0x0db
#define VEK_Udiaeresis          0x0dc
#define VEK_Yacute              0x0dd
#define VEK_THORN               0x0de
#define VEK_Thorn               0x0de	/* deprecated */
#define VEK_ssharp              0x0df
#define VEK_agrave              0x0e0
#define VEK_aacute              0x0e1
#define VEK_acircumflex         0x0e2
#define VEK_atilde              0x0e3
#define VEK_adiaeresis          0x0e4
#define VEK_aring               0x0e5
#define VEK_ae                  0x0e6
#define VEK_ccedilla            0x0e7
#define VEK_egrave              0x0e8
#define VEK_eacute              0x0e9
#define VEK_ecircumflex         0x0ea
#define VEK_ediaeresis          0x0eb
#define VEK_igrave              0x0ec
#define VEK_iacute              0x0ed
#define VEK_icircumflex         0x0ee
#define VEK_idiaeresis          0x0ef
#define VEK_eth                 0x0f0
#define VEK_ntilde              0x0f1
#define VEK_ograve              0x0f2
#define VEK_oacute              0x0f3
#define VEK_ocircumflex         0x0f4
#define VEK_otilde              0x0f5
#define VEK_odiaeresis          0x0f6
#define VEK_division            0x0f7
#define VEK_oslash              0x0f8
#define VEK_ugrave              0x0f9
#define VEK_uacute              0x0fa
#define VEK_ucircumflex         0x0fb
#define VEK_udiaeresis          0x0fc
#define VEK_yacute              0x0fd
#define VEK_thorn               0x0fe
#define VEK_ydiaeresis          0x0ff

/*
 *   Latin 2
 *   Byte 3 = 1
 */

#define VEK_Aogonek             0x1a1
#define VEK_breve               0x1a2
#define VEK_Lstroke             0x1a3
#define VEK_Lcaron              0x1a5
#define VEK_Sacute              0x1a6
#define VEK_Scaron              0x1a9
#define VEK_Scedilla            0x1aa
#define VEK_Tcaron              0x1ab
#define VEK_Zacute              0x1ac
#define VEK_Zcaron              0x1ae
#define VEK_Zabovedot           0x1af
#define VEK_aogonek             0x1b1
#define VEK_ogonek              0x1b2
#define VEK_lstroke             0x1b3
#define VEK_lcaron              0x1b5
#define VEK_sacute              0x1b6
#define VEK_caron               0x1b7
#define VEK_scaron              0x1b9
#define VEK_scedilla            0x1ba
#define VEK_tcaron              0x1bb
#define VEK_zacute              0x1bc
#define VEK_doubleacute         0x1bd
#define VEK_zcaron              0x1be
#define VEK_zabovedot           0x1bf
#define VEK_Racute              0x1c0
#define VEK_Abreve              0x1c3
#define VEK_Lacute              0x1c5
#define VEK_Cacute              0x1c6
#define VEK_Ccaron              0x1c8
#define VEK_Eogonek             0x1ca
#define VEK_Ecaron              0x1cc
#define VEK_Dcaron              0x1cf
#define VEK_Dstroke             0x1d0
#define VEK_Nacute              0x1d1
#define VEK_Ncaron              0x1d2
#define VEK_Odoubleacute        0x1d5
#define VEK_Rcaron              0x1d8
#define VEK_Uring               0x1d9
#define VEK_Udoubleacute        0x1db
#define VEK_Tcedilla            0x1de
#define VEK_racute              0x1e0
#define VEK_abreve              0x1e3
#define VEK_lacute              0x1e5
#define VEK_cacute              0x1e6
#define VEK_ccaron              0x1e8
#define VEK_eogonek             0x1ea
#define VEK_ecaron              0x1ec
#define VEK_dcaron              0x1ef
#define VEK_dstroke             0x1f0
#define VEK_nacute              0x1f1
#define VEK_ncaron              0x1f2
#define VEK_odoubleacute        0x1f5
#define VEK_udoubleacute        0x1fb
#define VEK_rcaron              0x1f8
#define VEK_uring               0x1f9
#define VEK_tcedilla            0x1fe
#define VEK_abovedot            0x1ff

/*
 *   Latin 3
 *   Byte 3 = 2
 */

#define VEK_Hstroke             0x2a1
#define VEK_Hcircumflex         0x2a6
#define VEK_Iabovedot           0x2a9
#define VEK_Gbreve              0x2ab
#define VEK_Jcircumflex         0x2ac
#define VEK_hstroke             0x2b1
#define VEK_hcircumflex         0x2b6
#define VEK_idotless            0x2b9
#define VEK_gbreve              0x2bb
#define VEK_jcircumflex         0x2bc
#define VEK_Cabovedot           0x2c5
#define VEK_Ccircumflex         0x2c6
#define VEK_Gabovedot           0x2d5
#define VEK_Gcircumflex         0x2d8
#define VEK_Ubreve              0x2dd
#define VEK_Scircumflex         0x2de
#define VEK_cabovedot           0x2e5
#define VEK_ccircumflex         0x2e6
#define VEK_gabovedot           0x2f5
#define VEK_gcircumflex         0x2f8
#define VEK_ubreve              0x2fd
#define VEK_scircumflex         0x2fe


/*
 *   Latin 4
 *   Byte 3 = 3
 */

#define VEK_kra                 0x3a2
#define VEK_kappa               0x3a2	/* deprecated */
#define VEK_Rcedilla            0x3a3
#define VEK_Itilde              0x3a5
#define VEK_Lcedilla            0x3a6
#define VEK_Emacron             0x3aa
#define VEK_Gcedilla            0x3ab
#define VEK_Tslash              0x3ac
#define VEK_rcedilla            0x3b3
#define VEK_itilde              0x3b5
#define VEK_lcedilla            0x3b6
#define VEK_emacron             0x3ba
#define VEK_gcedilla            0x3bb
#define VEK_tslash              0x3bc
#define VEK_ENG                 0x3bd
#define VEK_eng                 0x3bf
#define VEK_Amacron             0x3c0
#define VEK_Iogonek             0x3c7
#define VEK_Eabovedot           0x3cc
#define VEK_Imacron             0x3cf
#define VEK_Ncedilla            0x3d1
#define VEK_Omacron             0x3d2
#define VEK_Kcedilla            0x3d3
#define VEK_Uogonek             0x3d9
#define VEK_Utilde              0x3dd
#define VEK_Umacron             0x3de
#define VEK_amacron             0x3e0
#define VEK_iogonek             0x3e7
#define VEK_eabovedot           0x3ec
#define VEK_imacron             0x3ef
#define VEK_ncedilla            0x3f1
#define VEK_omacron             0x3f2
#define VEK_kcedilla            0x3f3
#define VEK_uogonek             0x3f9
#define VEK_utilde              0x3fd
#define VEK_umacron             0x3fe

/*
 * Katakana
 * Byte 3 = 4
 */

#define VEK_overline				       0x47e
#define VEK_kana_fullstop                               0x4a1
#define VEK_kana_openingbracket                         0x4a2
#define VEK_kana_closingbracket                         0x4a3
#define VEK_kana_comma                                  0x4a4
#define VEK_kana_conjunctive                            0x4a5
#define VEK_kana_middledot                              0x4a5  /* deprecated */
#define VEK_kana_WO                                     0x4a6
#define VEK_kana_a                                      0x4a7
#define VEK_kana_i                                      0x4a8
#define VEK_kana_u                                      0x4a9
#define VEK_kana_e                                      0x4aa
#define VEK_kana_o                                      0x4ab
#define VEK_kana_ya                                     0x4ac
#define VEK_kana_yu                                     0x4ad
#define VEK_kana_yo                                     0x4ae
#define VEK_kana_tsu                                    0x4af
#define VEK_kana_tu                                     0x4af  /* deprecated */
#define VEK_prolongedsound                              0x4b0
#define VEK_kana_A                                      0x4b1
#define VEK_kana_I                                      0x4b2
#define VEK_kana_U                                      0x4b3
#define VEK_kana_E                                      0x4b4
#define VEK_kana_O                                      0x4b5
#define VEK_kana_KA                                     0x4b6
#define VEK_kana_KI                                     0x4b7
#define VEK_kana_KU                                     0x4b8
#define VEK_kana_KE                                     0x4b9
#define VEK_kana_KO                                     0x4ba
#define VEK_kana_SA                                     0x4bb
#define VEK_kana_SHI                                    0x4bc
#define VEK_kana_SU                                     0x4bd
#define VEK_kana_SE                                     0x4be
#define VEK_kana_SO                                     0x4bf
#define VEK_kana_TA                                     0x4c0
#define VEK_kana_CHI                                    0x4c1
#define VEK_kana_TI                                     0x4c1  /* deprecated */
#define VEK_kana_TSU                                    0x4c2
#define VEK_kana_TU                                     0x4c2  /* deprecated */
#define VEK_kana_TE                                     0x4c3
#define VEK_kana_TO                                     0x4c4
#define VEK_kana_NA                                     0x4c5
#define VEK_kana_NI                                     0x4c6
#define VEK_kana_NU                                     0x4c7
#define VEK_kana_NE                                     0x4c8
#define VEK_kana_NO                                     0x4c9
#define VEK_kana_HA                                     0x4ca
#define VEK_kana_HI                                     0x4cb
#define VEK_kana_FU                                     0x4cc
#define VEK_kana_HU                                     0x4cc  /* deprecated */
#define VEK_kana_HE                                     0x4cd
#define VEK_kana_HO                                     0x4ce
#define VEK_kana_MA                                     0x4cf
#define VEK_kana_MI                                     0x4d0
#define VEK_kana_MU                                     0x4d1
#define VEK_kana_ME                                     0x4d2
#define VEK_kana_MO                                     0x4d3
#define VEK_kana_YA                                     0x4d4
#define VEK_kana_YU                                     0x4d5
#define VEK_kana_YO                                     0x4d6
#define VEK_kana_RA                                     0x4d7
#define VEK_kana_RI                                     0x4d8
#define VEK_kana_RU                                     0x4d9
#define VEK_kana_RE                                     0x4da
#define VEK_kana_RO                                     0x4db
#define VEK_kana_WA                                     0x4dc
#define VEK_kana_N                                      0x4dd
#define VEK_voicedsound                                 0x4de
#define VEK_semivoicedsound                             0x4df
#define VEK_kana_switch          0xFF7E  /* Alias for mode_switch */

/*
 *  Arabic
 *  Byte 3 = 5
 */

#define VEK_Arabic_comma                                0x5ac
#define VEK_Arabic_semicolon                            0x5bb
#define VEK_Arabic_question_mark                        0x5bf
#define VEK_Arabic_hamza                                0x5c1
#define VEK_Arabic_maddaonalef                          0x5c2
#define VEK_Arabic_hamzaonalef                          0x5c3
#define VEK_Arabic_hamzaonwaw                           0x5c4
#define VEK_Arabic_hamzaunderalef                       0x5c5
#define VEK_Arabic_hamzaonyeh                           0x5c6
#define VEK_Arabic_alef                                 0x5c7
#define VEK_Arabic_beh                                  0x5c8
#define VEK_Arabic_tehmarbuta                           0x5c9
#define VEK_Arabic_teh                                  0x5ca
#define VEK_Arabic_theh                                 0x5cb
#define VEK_Arabic_jeem                                 0x5cc
#define VEK_Arabic_hah                                  0x5cd
#define VEK_Arabic_khah                                 0x5ce
#define VEK_Arabic_dal                                  0x5cf
#define VEK_Arabic_thal                                 0x5d0
#define VEK_Arabic_ra                                   0x5d1
#define VEK_Arabic_zain                                 0x5d2
#define VEK_Arabic_seen                                 0x5d3
#define VEK_Arabic_sheen                                0x5d4
#define VEK_Arabic_sad                                  0x5d5
#define VEK_Arabic_dad                                  0x5d6
#define VEK_Arabic_tah                                  0x5d7
#define VEK_Arabic_zah                                  0x5d8
#define VEK_Arabic_ain                                  0x5d9
#define VEK_Arabic_ghain                                0x5da
#define VEK_Arabic_tatweel                              0x5e0
#define VEK_Arabic_feh                                  0x5e1
#define VEK_Arabic_qaf                                  0x5e2
#define VEK_Arabic_kaf                                  0x5e3
#define VEK_Arabic_lam                                  0x5e4
#define VEK_Arabic_meem                                 0x5e5
#define VEK_Arabic_noon                                 0x5e6
#define VEK_Arabic_ha                                   0x5e7
#define VEK_Arabic_heh                                  0x5e7  /* deprecated */
#define VEK_Arabic_waw                                  0x5e8
#define VEK_Arabic_alefmaksura                          0x5e9
#define VEK_Arabic_yeh                                  0x5ea
#define VEK_Arabic_fathatan                             0x5eb
#define VEK_Arabic_dammatan                             0x5ec
#define VEK_Arabic_kasratan                             0x5ed
#define VEK_Arabic_fatha                                0x5ee
#define VEK_Arabic_damma                                0x5ef
#define VEK_Arabic_kasra                                0x5f0
#define VEK_Arabic_shadda                               0x5f1
#define VEK_Arabic_sukun                                0x5f2
#define VEK_Arabic_switch        0xFF7E  /* Alias for mode_switch */

/*
 * Cyrillic
 * Byte 3 = 6
 */
#define VEK_Serbian_dje                                 0x6a1
#define VEK_Macedonia_gje                               0x6a2
#define VEK_Cyrillic_io                                 0x6a3
#define VEK_Ukrainian_ie                                0x6a4
#define VEK_Ukranian_je                                 0x6a4  /* deprecated */
#define VEK_Macedonia_dse                               0x6a5
#define VEK_Ukrainian_i                                 0x6a6
#define VEK_Ukranian_i                                  0x6a6  /* deprecated */
#define VEK_Ukrainian_yi                                0x6a7
#define VEK_Ukranian_yi                                 0x6a7  /* deprecated */
#define VEK_Cyrillic_je                                 0x6a8
#define VEK_Serbian_je                                  0x6a8  /* deprecated */
#define VEK_Cyrillic_lje                                0x6a9
#define VEK_Serbian_lje                                 0x6a9  /* deprecated */
#define VEK_Cyrillic_nje                                0x6aa
#define VEK_Serbian_nje                                 0x6aa  /* deprecated */
#define VEK_Serbian_tshe                                0x6ab
#define VEK_Macedonia_kje                               0x6ac
#define VEK_Byelorussian_shortu                         0x6ae
#define VEK_Cyrillic_dzhe                               0x6af
#define VEK_Serbian_dze                                 0x6af  /* deprecated */
#define VEK_numerosign                                  0x6b0
#define VEK_Serbian_DJE                                 0x6b1
#define VEK_Macedonia_GJE                               0x6b2
#define VEK_Cyrillic_IO                                 0x6b3
#define VEK_Ukrainian_IE                                0x6b4
#define VEK_Ukranian_JE                                 0x6b4  /* deprecated */
#define VEK_Macedonia_DSE                               0x6b5
#define VEK_Ukrainian_I                                 0x6b6
#define VEK_Ukranian_I                                  0x6b6  /* deprecated */
#define VEK_Ukrainian_YI                                0x6b7
#define VEK_Ukranian_YI                                 0x6b7  /* deprecated */
#define VEK_Cyrillic_JE                                 0x6b8
#define VEK_Serbian_JE                                  0x6b8  /* deprecated */
#define VEK_Cyrillic_LJE                                0x6b9
#define VEK_Serbian_LJE                                 0x6b9  /* deprecated */
#define VEK_Cyrillic_NJE                                0x6ba
#define VEK_Serbian_NJE                                 0x6ba  /* deprecated */
#define VEK_Serbian_TSHE                                0x6bb
#define VEK_Macedonia_KJE                               0x6bc
#define VEK_Byelorussian_SHORTU                         0x6be
#define VEK_Cyrillic_DZHE                               0x6bf
#define VEK_Serbian_DZE                                 0x6bf  /* deprecated */
#define VEK_Cyrillic_yu                                 0x6c0
#define VEK_Cyrillic_a                                  0x6c1
#define VEK_Cyrillic_be                                 0x6c2
#define VEK_Cyrillic_tse                                0x6c3
#define VEK_Cyrillic_de                                 0x6c4
#define VEK_Cyrillic_ie                                 0x6c5
#define VEK_Cyrillic_ef                                 0x6c6
#define VEK_Cyrillic_ghe                                0x6c7
#define VEK_Cyrillic_ha                                 0x6c8
#define VEK_Cyrillic_i                                  0x6c9
#define VEK_Cyrillic_shorti                             0x6ca
#define VEK_Cyrillic_ka                                 0x6cb
#define VEK_Cyrillic_el                                 0x6cc
#define VEK_Cyrillic_em                                 0x6cd
#define VEK_Cyrillic_en                                 0x6ce
#define VEK_Cyrillic_o                                  0x6cf
#define VEK_Cyrillic_pe                                 0x6d0
#define VEK_Cyrillic_ya                                 0x6d1
#define VEK_Cyrillic_er                                 0x6d2
#define VEK_Cyrillic_es                                 0x6d3
#define VEK_Cyrillic_te                                 0x6d4
#define VEK_Cyrillic_u                                  0x6d5
#define VEK_Cyrillic_zhe                                0x6d6
#define VEK_Cyrillic_ve                                 0x6d7
#define VEK_Cyrillic_softsign                           0x6d8
#define VEK_Cyrillic_yeru                               0x6d9
#define VEK_Cyrillic_ze                                 0x6da
#define VEK_Cyrillic_sha                                0x6db
#define VEK_Cyrillic_e                                  0x6dc
#define VEK_Cyrillic_shcha                              0x6dd
#define VEK_Cyrillic_che                                0x6de
#define VEK_Cyrillic_hardsign                           0x6df
#define VEK_Cyrillic_YU                                 0x6e0
#define VEK_Cyrillic_A                                  0x6e1
#define VEK_Cyrillic_BE                                 0x6e2
#define VEK_Cyrillic_TSE                                0x6e3
#define VEK_Cyrillic_DE                                 0x6e4
#define VEK_Cyrillic_IE                                 0x6e5
#define VEK_Cyrillic_EF                                 0x6e6
#define VEK_Cyrillic_GHE                                0x6e7
#define VEK_Cyrillic_HA                                 0x6e8
#define VEK_Cyrillic_I                                  0x6e9
#define VEK_Cyrillic_SHORTI                             0x6ea
#define VEK_Cyrillic_KA                                 0x6eb
#define VEK_Cyrillic_EL                                 0x6ec
#define VEK_Cyrillic_EM                                 0x6ed
#define VEK_Cyrillic_EN                                 0x6ee
#define VEK_Cyrillic_O                                  0x6ef
#define VEK_Cyrillic_PE                                 0x6f0
#define VEK_Cyrillic_YA                                 0x6f1
#define VEK_Cyrillic_ER                                 0x6f2
#define VEK_Cyrillic_ES                                 0x6f3
#define VEK_Cyrillic_TE                                 0x6f4
#define VEK_Cyrillic_U                                  0x6f5
#define VEK_Cyrillic_ZHE                                0x6f6
#define VEK_Cyrillic_VE                                 0x6f7
#define VEK_Cyrillic_SOFTSIGN                           0x6f8
#define VEK_Cyrillic_YERU                               0x6f9
#define VEK_Cyrillic_ZE                                 0x6fa
#define VEK_Cyrillic_SHA                                0x6fb
#define VEK_Cyrillic_E                                  0x6fc
#define VEK_Cyrillic_SHCHA                              0x6fd
#define VEK_Cyrillic_CHE                                0x6fe
#define VEK_Cyrillic_HARDSIGN                           0x6ff

/*
 * Greek
 * Byte 3 = 7
 */

#define VEK_Greek_ALPHAaccent                           0x7a1
#define VEK_Greek_EPSILONaccent                         0x7a2
#define VEK_Greek_ETAaccent                             0x7a3
#define VEK_Greek_IOTAaccent                            0x7a4
#define VEK_Greek_IOTAdiaeresis                         0x7a5
#define VEK_Greek_OMICRONaccent                         0x7a7
#define VEK_Greek_UPSILONaccent                         0x7a8
#define VEK_Greek_UPSILONdieresis                       0x7a9
#define VEK_Greek_OMEGAaccent                           0x7ab
#define VEK_Greek_accentdieresis                        0x7ae
#define VEK_Greek_horizbar                              0x7af
#define VEK_Greek_alphaaccent                           0x7b1
#define VEK_Greek_epsilonaccent                         0x7b2
#define VEK_Greek_etaaccent                             0x7b3
#define VEK_Greek_iotaaccent                            0x7b4
#define VEK_Greek_iotadieresis                          0x7b5
#define VEK_Greek_iotaaccentdieresis                    0x7b6
#define VEK_Greek_omicronaccent                         0x7b7
#define VEK_Greek_upsilonaccent                         0x7b8
#define VEK_Greek_upsilondieresis                       0x7b9
#define VEK_Greek_upsilonaccentdieresis                 0x7ba
#define VEK_Greek_omegaaccent                           0x7bb
#define VEK_Greek_ALPHA                                 0x7c1
#define VEK_Greek_BETA                                  0x7c2
#define VEK_Greek_GAMMA                                 0x7c3
#define VEK_Greek_DELTA                                 0x7c4
#define VEK_Greek_EPSILON                               0x7c5
#define VEK_Greek_ZETA                                  0x7c6
#define VEK_Greek_ETA                                   0x7c7
#define VEK_Greek_THETA                                 0x7c8
#define VEK_Greek_IOTA                                  0x7c9
#define VEK_Greek_KAPPA                                 0x7ca
#define VEK_Greek_LAMDA                                 0x7cb
#define VEK_Greek_LAMBDA                                0x7cb
#define VEK_Greek_MU                                    0x7cc
#define VEK_Greek_NU                                    0x7cd
#define VEK_Greek_XI                                    0x7ce
#define VEK_Greek_OMICRON                               0x7cf
#define VEK_Greek_PI                                    0x7d0
#define VEK_Greek_RHO                                   0x7d1
#define VEK_Greek_SIGMA                                 0x7d2
#define VEK_Greek_TAU                                   0x7d4
#define VEK_Greek_UPSILON                               0x7d5
#define VEK_Greek_PHI                                   0x7d6
#define VEK_Greek_CHI                                   0x7d7
#define VEK_Greek_PSI                                   0x7d8
#define VEK_Greek_OMEGA                                 0x7d9
#define VEK_Greek_alpha                                 0x7e1
#define VEK_Greek_beta                                  0x7e2
#define VEK_Greek_gamma                                 0x7e3
#define VEK_Greek_delta                                 0x7e4
#define VEK_Greek_epsilon                               0x7e5
#define VEK_Greek_zeta                                  0x7e6
#define VEK_Greek_eta                                   0x7e7
#define VEK_Greek_theta                                 0x7e8
#define VEK_Greek_iota                                  0x7e9
#define VEK_Greek_kappa                                 0x7ea
#define VEK_Greek_lamda                                 0x7eb
#define VEK_Greek_lambda                                0x7eb
#define VEK_Greek_mu                                    0x7ec
#define VEK_Greek_nu                                    0x7ed
#define VEK_Greek_xi                                    0x7ee
#define VEK_Greek_omicron                               0x7ef
#define VEK_Greek_pi                                    0x7f0
#define VEK_Greek_rho                                   0x7f1
#define VEK_Greek_sigma                                 0x7f2
#define VEK_Greek_finalsmallsigma                       0x7f3
#define VEK_Greek_tau                                   0x7f4
#define VEK_Greek_upsilon                               0x7f5
#define VEK_Greek_phi                                   0x7f6
#define VEK_Greek_chi                                   0x7f7
#define VEK_Greek_psi                                   0x7f8
#define VEK_Greek_omega                                 0x7f9
#define VEK_Greek_switch         0xFF7E  /* Alias for mode_switch */

/*
 * Technical
 * Byte 3 = 8
 */

#define VEK_leftradical                                 0x8a1
#define VEK_topleftradical                              0x8a2
#define VEK_horizconnector                              0x8a3
#define VEK_topintegral                                 0x8a4
#define VEK_botintegral                                 0x8a5
#define VEK_vertconnector                               0x8a6
#define VEK_topleftsqbracket                            0x8a7
#define VEK_botleftsqbracket                            0x8a8
#define VEK_toprightsqbracket                           0x8a9
#define VEK_botrightsqbracket                           0x8aa
#define VEK_topleftparens                               0x8ab
#define VEK_botleftparens                               0x8ac
#define VEK_toprightparens                              0x8ad
#define VEK_botrightparens                              0x8ae
#define VEK_leftmiddlecurlybrace                        0x8af
#define VEK_rightmiddlecurlybrace                       0x8b0
#define VEK_topleftsummation                            0x8b1
#define VEK_botleftsummation                            0x8b2
#define VEK_topvertsummationconnector                   0x8b3
#define VEK_botvertsummationconnector                   0x8b4
#define VEK_toprightsummation                           0x8b5
#define VEK_botrightsummation                           0x8b6
#define VEK_rightmiddlesummation                        0x8b7
#define VEK_lessthanequal                               0x8bc
#define VEK_notequal                                    0x8bd
#define VEK_greaterthanequal                            0x8be
#define VEK_integral                                    0x8bf
#define VEK_therefore                                   0x8c0
#define VEK_variation                                   0x8c1
#define VEK_infinity                                    0x8c2
#define VEK_nabla                                       0x8c5
#define VEK_approximate                                 0x8c8
#define VEK_similarequal                                0x8c9
#define VEK_ifonlyif                                    0x8cd
#define VEK_implies                                     0x8ce
#define VEK_identical                                   0x8cf
#define VEK_radical                                     0x8d6
#define VEK_includedin                                  0x8da
#define VEK_includes                                    0x8db
#define VEK_intersection                                0x8dc
#define VEK_union                                       0x8dd
#define VEK_logicaland                                  0x8de
#define VEK_logicalor                                   0x8df
#define VEK_partialderivative                           0x8ef
#define VEK_function                                    0x8f6
#define VEK_leftarrow                                   0x8fb
#define VEK_uparrow                                     0x8fc
#define VEK_rightarrow                                  0x8fd
#define VEK_downarrow                                   0x8fe

/*
 *  Special
 *  Byte 3 = 9
 */

#define VEK_blank                                       0x9df
#define VEK_soliddiamond                                0x9e0
#define VEK_checkerboard                                0x9e1
#define VEK_ht                                          0x9e2
#define VEK_ff                                          0x9e3
#define VEK_cr                                          0x9e4
#define VEK_lf                                          0x9e5
#define VEK_nl                                          0x9e8
#define VEK_vt                                          0x9e9
#define VEK_lowrightcorner                              0x9ea
#define VEK_uprightcorner                               0x9eb
#define VEK_upleftcorner                                0x9ec
#define VEK_lowleftcorner                               0x9ed
#define VEK_crossinglines                               0x9ee
#define VEK_horizlinescan1                              0x9ef
#define VEK_horizlinescan3                              0x9f0
#define VEK_horizlinescan5                              0x9f1
#define VEK_horizlinescan7                              0x9f2
#define VEK_horizlinescan9                              0x9f3
#define VEK_leftt                                       0x9f4
#define VEK_rightt                                      0x9f5
#define VEK_bott                                        0x9f6
#define VEK_topt                                        0x9f7
#define VEK_vertbar                                     0x9f8

/*
 *  Publishing
 *  Byte 3 = a
 */

#define VEK_emspace                                     0xaa1
#define VEK_enspace                                     0xaa2
#define VEK_em3space                                    0xaa3
#define VEK_em4space                                    0xaa4
#define VEK_digitspace                                  0xaa5
#define VEK_punctspace                                  0xaa6
#define VEK_thinspace                                   0xaa7
#define VEK_hairspace                                   0xaa8
#define VEK_emdash                                      0xaa9
#define VEK_endash                                      0xaaa
#define VEK_signifblank                                 0xaac
#define VEK_ellipsis                                    0xaae
#define VEK_doubbaselinedot                             0xaaf
#define VEK_onethird                                    0xab0
#define VEK_twothirds                                   0xab1
#define VEK_onefifth                                    0xab2
#define VEK_twofifths                                   0xab3
#define VEK_threefifths                                 0xab4
#define VEK_fourfifths                                  0xab5
#define VEK_onesixth                                    0xab6
#define VEK_fivesixths                                  0xab7
#define VEK_careof                                      0xab8
#define VEK_figdash                                     0xabb
#define VEK_leftanglebracket                            0xabc
#define VEK_decimalpoint                                0xabd
#define VEK_rightanglebracket                           0xabe
#define VEK_marker                                      0xabf
#define VEK_oneeighth                                   0xac3
#define VEK_threeeighths                                0xac4
#define VEK_fiveeighths                                 0xac5
#define VEK_seveneighths                                0xac6
#define VEK_trademark                                   0xac9
#define VEK_signaturemark                               0xaca
#define VEK_trademarkincircle                           0xacb
#define VEK_leftopentriangle                            0xacc
#define VEK_rightopentriangle                           0xacd
#define VEK_emopencircle                                0xace
#define VEK_emopenrectangle                             0xacf
#define VEK_leftsinglequotemark                         0xad0
#define VEK_rightsinglequotemark                        0xad1
#define VEK_leftdoublequotemark                         0xad2
#define VEK_rightdoublequotemark                        0xad3
#define VEK_prescription                                0xad4
#define VEK_minutes                                     0xad6
#define VEK_seconds                                     0xad7
#define VEK_latincross                                  0xad9
#define VEK_hexagram                                    0xada
#define VEK_filledrectbullet                            0xadb
#define VEK_filledlefttribullet                         0xadc
#define VEK_filledrighttribullet                        0xadd
#define VEK_emfilledcircle                              0xade
#define VEK_emfilledrect                                0xadf
#define VEK_enopencircbullet                            0xae0
#define VEK_enopensquarebullet                          0xae1
#define VEK_openrectbullet                              0xae2
#define VEK_opentribulletup                             0xae3
#define VEK_opentribulletdown                           0xae4
#define VEK_openstar                                    0xae5
#define VEK_enfilledcircbullet                          0xae6
#define VEK_enfilledsqbullet                            0xae7
#define VEK_filledtribulletup                           0xae8
#define VEK_filledtribulletdown                         0xae9
#define VEK_leftpointer                                 0xaea
#define VEK_rightpointer                                0xaeb
#define VEK_club                                        0xaec
#define VEK_diamond                                     0xaed
#define VEK_heart                                       0xaee
#define VEK_maltesecross                                0xaf0
#define VEK_dagger                                      0xaf1
#define VEK_doubledagger                                0xaf2
#define VEK_checkmark                                   0xaf3
#define VEK_ballotcross                                 0xaf4
#define VEK_musicalsharp                                0xaf5
#define VEK_musicalflat                                 0xaf6
#define VEK_malesymbol                                  0xaf7
#define VEK_femalesymbol                                0xaf8
#define VEK_telephone                                   0xaf9
#define VEK_telephonerecorder                           0xafa
#define VEK_phonographcopyright                         0xafb
#define VEK_caret                                       0xafc
#define VEK_singlelowquotemark                          0xafd
#define VEK_doublelowquotemark                          0xafe
#define VEK_cursor                                      0xaff

/*
 *  APL
 *  Byte 3 = b
 */

#define VEK_leftcaret                                   0xba3
#define VEK_rightcaret                                  0xba6
#define VEK_downcaret                                   0xba8
#define VEK_upcaret                                     0xba9
#define VEK_overbar                                     0xbc0
#define VEK_downtack                                    0xbc2
#define VEK_upshoe                                      0xbc3
#define VEK_downstile                                   0xbc4
#define VEK_underbar                                    0xbc6
#define VEK_jot                                         0xbca
#define VEK_quad                                        0xbcc
#define VEK_uptack                                      0xbce
#define VEK_circle                                      0xbcf
#define VEK_upstile                                     0xbd3
#define VEK_downshoe                                    0xbd6
#define VEK_rightshoe                                   0xbd8
#define VEK_leftshoe                                    0xbda
#define VEK_lefttack                                    0xbdc
#define VEK_righttack                                   0xbfc

/*
 * Hebrew
 * Byte 3 = c
 */

#define VEK_hebrew_doublelowline                        0xcdf
#define VEK_hebrew_aleph                                0xce0
#define VEK_hebrew_bet                                  0xce1
#define VEK_hebrew_beth                                 0xce1  /* deprecated */
#define VEK_hebrew_gimel                                0xce2
#define VEK_hebrew_gimmel                               0xce2  /* deprecated */
#define VEK_hebrew_dalet                                0xce3
#define VEK_hebrew_daleth                               0xce3  /* deprecated */
#define VEK_hebrew_he                                   0xce4
#define VEK_hebrew_waw                                  0xce5
#define VEK_hebrew_zain                                 0xce6
#define VEK_hebrew_zayin                                0xce6  /* deprecated */
#define VEK_hebrew_chet                                 0xce7
#define VEK_hebrew_het                                  0xce7  /* deprecated */
#define VEK_hebrew_tet                                  0xce8
#define VEK_hebrew_teth                                 0xce8  /* deprecated */
#define VEK_hebrew_yod                                  0xce9
#define VEK_hebrew_finalkaph                            0xcea
#define VEK_hebrew_kaph                                 0xceb
#define VEK_hebrew_lamed                                0xcec
#define VEK_hebrew_finalmem                             0xced
#define VEK_hebrew_mem                                  0xcee
#define VEK_hebrew_finalnun                             0xcef
#define VEK_hebrew_nun                                  0xcf0
#define VEK_hebrew_samech                               0xcf1
#define VEK_hebrew_samekh                               0xcf1  /* deprecated */
#define VEK_hebrew_ayin                                 0xcf2
#define VEK_hebrew_finalpe                              0xcf3
#define VEK_hebrew_pe                                   0xcf4
#define VEK_hebrew_finalzade                            0xcf5
#define VEK_hebrew_finalzadi                            0xcf5  /* deprecated */
#define VEK_hebrew_zade                                 0xcf6
#define VEK_hebrew_zadi                                 0xcf6  /* deprecated */
#define VEK_hebrew_qoph                                 0xcf7
#define VEK_hebrew_kuf                                  0xcf7  /* deprecated */
#define VEK_hebrew_resh                                 0xcf8
#define VEK_hebrew_shin                                 0xcf9
#define VEK_hebrew_taw                                  0xcfa
#define VEK_hebrew_taf                                  0xcfa  /* deprecated */
#define VEK_Hebrew_switch        0xFF7E  /* Alias for mode_switch */

/*
 * Thai
 * Byte 3 = d
 */

#define VEK_Thai_kokai					0xda1
#define VEK_Thai_khokhai					0xda2
#define VEK_Thai_khokhuat				0xda3
#define VEK_Thai_khokhwai				0xda4
#define VEK_Thai_khokhon					0xda5
#define VEK_Thai_khorakhang			        0xda6  
#define VEK_Thai_ngongu					0xda7  
#define VEK_Thai_chochan					0xda8  
#define VEK_Thai_choching				0xda9   
#define VEK_Thai_chochang				0xdaa  
#define VEK_Thai_soso					0xdab
#define VEK_Thai_chochoe					0xdac
#define VEK_Thai_yoying					0xdad
#define VEK_Thai_dochada					0xdae
#define VEK_Thai_topatak					0xdaf
#define VEK_Thai_thothan					0xdb0
#define VEK_Thai_thonangmontho			        0xdb1
#define VEK_Thai_thophuthao			        0xdb2
#define VEK_Thai_nonen					0xdb3
#define VEK_Thai_dodek					0xdb4
#define VEK_Thai_totao					0xdb5
#define VEK_Thai_thothung				0xdb6
#define VEK_Thai_thothahan				0xdb7
#define VEK_Thai_thothong	 			0xdb8
#define VEK_Thai_nonu					0xdb9
#define VEK_Thai_bobaimai				0xdba
#define VEK_Thai_popla					0xdbb
#define VEK_Thai_phophung				0xdbc
#define VEK_Thai_fofa					0xdbd
#define VEK_Thai_phophan					0xdbe
#define VEK_Thai_fofan					0xdbf
#define VEK_Thai_phosamphao			        0xdc0
#define VEK_Thai_moma					0xdc1
#define VEK_Thai_yoyak					0xdc2
#define VEK_Thai_rorua					0xdc3
#define VEK_Thai_ru					0xdc4
#define VEK_Thai_loling					0xdc5
#define VEK_Thai_lu					0xdc6
#define VEK_Thai_wowaen					0xdc7
#define VEK_Thai_sosala					0xdc8
#define VEK_Thai_sorusi					0xdc9
#define VEK_Thai_sosua					0xdca
#define VEK_Thai_hohip					0xdcb
#define VEK_Thai_lochula					0xdcc
#define VEK_Thai_oang					0xdcd
#define VEK_Thai_honokhuk				0xdce
#define VEK_Thai_paiyannoi				0xdcf
#define VEK_Thai_saraa					0xdd0
#define VEK_Thai_maihanakat				0xdd1
#define VEK_Thai_saraaa					0xdd2
#define VEK_Thai_saraam					0xdd3
#define VEK_Thai_sarai					0xdd4   
#define VEK_Thai_saraii					0xdd5   
#define VEK_Thai_saraue					0xdd6    
#define VEK_Thai_sarauee					0xdd7    
#define VEK_Thai_sarau					0xdd8    
#define VEK_Thai_sarauu					0xdd9   
#define VEK_Thai_phinthu					0xdda
#define VEK_Thai_maihanakat_maitho   			0xdde
#define VEK_Thai_baht					0xddf
#define VEK_Thai_sarae					0xde0    
#define VEK_Thai_saraae					0xde1
#define VEK_Thai_sarao					0xde2
#define VEK_Thai_saraaimaimuan				0xde3   
#define VEK_Thai_saraaimaimalai				0xde4  
#define VEK_Thai_lakkhangyao				0xde5
#define VEK_Thai_maiyamok				0xde6
#define VEK_Thai_maitaikhu				0xde7
#define VEK_Thai_maiek					0xde8   
#define VEK_Thai_maitho					0xde9
#define VEK_Thai_maitri					0xdea
#define VEK_Thai_maichattawa				0xdeb
#define VEK_Thai_thanthakhat				0xdec
#define VEK_Thai_nikhahit				0xded
#define VEK_Thai_leksun					0xdf0 
#define VEK_Thai_leknung					0xdf1  
#define VEK_Thai_leksong					0xdf2 
#define VEK_Thai_leksam					0xdf3
#define VEK_Thai_leksi					0xdf4  
#define VEK_Thai_lekha					0xdf5  
#define VEK_Thai_lekhok					0xdf6  
#define VEK_Thai_lekchet					0xdf7  
#define VEK_Thai_lekpaet					0xdf8  
#define VEK_Thai_lekkao					0xdf9 

/*
 *   Korean
 *   Byte 3 = e
 */

#define VEK_Hangul		0xff31    /* Hangul start/stop(toggle) */
#define VEK_Hangul_Start		0xff32    /* Hangul start */
#define VEK_Hangul_End		0xff33    /* Hangul end, English start */
#define VEK_Hangul_Hanja		0xff34    /* Start Hangul->Hanja Conversion */
#define VEK_Hangul_Jamo		0xff35    /* Hangul Jamo mode */
#define VEK_Hangul_Romaja	0xff36    /* Hangul Romaja mode */
#define VEK_Hangul_Codeinput	0xff37    /* Hangul code input mode */
#define VEK_Hangul_Jeonja	0xff38    /* Jeonja mode */
#define VEK_Hangul_Banja		0xff39    /* Banja mode */
#define VEK_Hangul_PreHanja	0xff3a    /* Pre Hanja conversion */
#define VEK_Hangul_PostHanja	0xff3b    /* Post Hanja conversion */
#define VEK_Hangul_SingleCandidate	0xff3c    /* Single candidate */
#define VEK_Hangul_MultipleCandidate	0xff3d    /* Multiple candidate */
#define VEK_Hangul_PreviousCandidate	0xff3e    /* Previous candidate */
#define VEK_Hangul_Special	0xff3f    /* Special symbols */
#define VEK_Hangul_switch	0xFF7E    /* Alias for mode_switch */

/* Hangul Consonant Characters */
#define VEK_Hangul_Kiyeog				0xea1
#define VEK_Hangul_SsangKiyeog				0xea2
#define VEK_Hangul_KiyeogSios				0xea3
#define VEK_Hangul_Nieun					0xea4
#define VEK_Hangul_NieunJieuj				0xea5
#define VEK_Hangul_NieunHieuh				0xea6
#define VEK_Hangul_Dikeud				0xea7
#define VEK_Hangul_SsangDikeud				0xea8
#define VEK_Hangul_Rieul					0xea9
#define VEK_Hangul_RieulKiyeog				0xeaa
#define VEK_Hangul_RieulMieum				0xeab
#define VEK_Hangul_RieulPieub				0xeac
#define VEK_Hangul_RieulSios				0xead
#define VEK_Hangul_RieulTieut				0xeae
#define VEK_Hangul_RieulPhieuf				0xeaf
#define VEK_Hangul_RieulHieuh				0xeb0
#define VEK_Hangul_Mieum					0xeb1
#define VEK_Hangul_Pieub					0xeb2
#define VEK_Hangul_SsangPieub				0xeb3
#define VEK_Hangul_PieubSios				0xeb4
#define VEK_Hangul_Sios					0xeb5
#define VEK_Hangul_SsangSios				0xeb6
#define VEK_Hangul_Ieung					0xeb7
#define VEK_Hangul_Jieuj					0xeb8
#define VEK_Hangul_SsangJieuj				0xeb9
#define VEK_Hangul_Cieuc					0xeba
#define VEK_Hangul_Khieuq				0xebb
#define VEK_Hangul_Tieut					0xebc
#define VEK_Hangul_Phieuf				0xebd
#define VEK_Hangul_Hieuh					0xebe

/* Hangul Vowel Characters */
#define VEK_Hangul_A					0xebf
#define VEK_Hangul_AE					0xec0
#define VEK_Hangul_YA					0xec1
#define VEK_Hangul_YAE					0xec2
#define VEK_Hangul_EO					0xec3
#define VEK_Hangul_E					0xec4
#define VEK_Hangul_YEO					0xec5
#define VEK_Hangul_YE					0xec6
#define VEK_Hangul_O					0xec7
#define VEK_Hangul_WA					0xec8
#define VEK_Hangul_WAE					0xec9
#define VEK_Hangul_OE					0xeca
#define VEK_Hangul_YO					0xecb
#define VEK_Hangul_U					0xecc
#define VEK_Hangul_WEO					0xecd
#define VEK_Hangul_WE					0xece
#define VEK_Hangul_WI					0xecf
#define VEK_Hangul_YU					0xed0
#define VEK_Hangul_EU					0xed1
#define VEK_Hangul_YI					0xed2
#define VEK_Hangul_I					0xed3

/* Hangul syllable-final (JongSeong) Characters */
#define VEK_Hangul_J_Kiyeog				0xed4
#define VEK_Hangul_J_SsangKiyeog				0xed5
#define VEK_Hangul_J_KiyeogSios				0xed6
#define VEK_Hangul_J_Nieun				0xed7
#define VEK_Hangul_J_NieunJieuj				0xed8
#define VEK_Hangul_J_NieunHieuh				0xed9
#define VEK_Hangul_J_Dikeud				0xeda
#define VEK_Hangul_J_Rieul				0xedb
#define VEK_Hangul_J_RieulKiyeog				0xedc
#define VEK_Hangul_J_RieulMieum				0xedd
#define VEK_Hangul_J_RieulPieub				0xede
#define VEK_Hangul_J_RieulSios				0xedf
#define VEK_Hangul_J_RieulTieut				0xee0
#define VEK_Hangul_J_RieulPhieuf				0xee1
#define VEK_Hangul_J_RieulHieuh				0xee2
#define VEK_Hangul_J_Mieum				0xee3
#define VEK_Hangul_J_Pieub				0xee4
#define VEK_Hangul_J_PieubSios				0xee5
#define VEK_Hangul_J_Sios				0xee6
#define VEK_Hangul_J_SsangSios				0xee7
#define VEK_Hangul_J_Ieung				0xee8
#define VEK_Hangul_J_Jieuj				0xee9
#define VEK_Hangul_J_Cieuc				0xeea
#define VEK_Hangul_J_Khieuq				0xeeb
#define VEK_Hangul_J_Tieut				0xeec
#define VEK_Hangul_J_Phieuf				0xeed
#define VEK_Hangul_J_Hieuh				0xeee

/* Ancient Hangul Consonant Characters */
#define VEK_Hangul_RieulYeorinHieuh			0xeef
#define VEK_Hangul_SunkyeongeumMieum			0xef0
#define VEK_Hangul_SunkyeongeumPieub			0xef1
#define VEK_Hangul_PanSios				0xef2
#define VEK_Hangul_KkogjiDalrinIeung			0xef3
#define VEK_Hangul_SunkyeongeumPhieuf			0xef4
#define VEK_Hangul_YeorinHieuh				0xef5

/* Ancient Hangul Vowel Characters */
#define VEK_Hangul_AraeA					0xef6
#define VEK_Hangul_AraeAE				0xef7

/* Ancient Hangul syllable-final (JongSeong) Characters */
#define VEK_Hangul_J_PanSios				0xef8
#define VEK_Hangul_J_KkogjiDalrinIeung			0xef9
#define VEK_Hangul_J_YeorinHieuh				0xefa

/* Korean currency symbol */
#define VEK_Korean_Won					0xeff

/* Functions */
/** function veKeysymToString
    Converts the given keysym to a printable string.
    For all keysyms corresponding to printable ASCII characters, this
    is just the character itself.  Other keysyms have symbolic names,
    e.g. "Home" or "Page_Up".  Not all keysyms have names.

    @param ks
    The keysym to convert.

    @returns
    A pointer to a static buffer containing the name of the keysym or
    <code>NULL</code> if there is no known name for the keysym.
 */
char *veKeysymToString(int ks);

/** function veStringToKeysym
    The inverse operation of veKeysymToString.  Converts a keysym
    name to the actual keysym value.  As with veKeysymToString, not
    all keysyms are supported by this function.

    @param str
    The string containing the keysym name.

    @returns
    The retrieved keysym or <code>VEK_UNKNOWN</code> if the name of 
    the keysym is not recognized.
*/
int veStringToKeysym(char *str);

#ifdef __cplusplus
}
#endif

#endif /* VE_KEYSYM_H */
