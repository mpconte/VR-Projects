#ifndef NID_KEYSYM_H
#define NID_KEYSYM_H

/** misc
    This module defines keycodes for keyboard devices.  NID keyboards
    should return these keycodes when the appropriate keys are pressed.
    They are all defined as macros, based upon the X11 keysym definitions.
    All macros begin with the <code>NIDK_</code> prefix.  See the header
    file for the list of macros.
 */

/* Names for special keys, loosely based on the X11 definitions - notice
   that ASCII keys have their ASCII values here */
/* I've kept all X11 definitions including ones that are unlikely to
   ever be used here.  This does not mean that every keyboard implementation
   will map all of these characters - in fact, most keyboard implementations
   are likely to be downright lazy about it... */
/* Prefix for any keysym definition is "NIDK_" */

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
#define NIDK_UNKNOWN            (-1)

/*
 * TTY Functions, cleverly chosen to map to ascii, for convenience of
 * programming, but could have been arbitrary (at the cost of lookup
 * tables in client code.
 */
/* VE note:  we are deliberately keeping the ASCII encoding so that
   on ASCII systems, you can match against either NIDK_x or 'x' */

#define NIDK_BackSpace		0xFF08	/* back space, back char */
#define NIDK_Tab			0xFF09
#define NIDK_Linefeed		0xFF0A	/* Linefeed, LF */
#define NIDK_Clear		0xFF0B
#define NIDK_Return		0xFF0D	/* Return, enter */
#define NIDK_Pause		0xFF13	/* Pause, hold */
#define NIDK_Scroll_Lock		0xFF14
#define NIDK_Sys_Req		0xFF15
#define NIDK_Escape		0xFF1B
#define NIDK_Delete		0xFFFF	/* Delete, rubout */

/* International & multi-key character composition */

#define NIDK_Multi_key		0xFF20  /* Multi-key character compose */
#define NIDK_Codeinput		0xFF37
#define NIDK_SingleCandidate	0xFF3C
#define NIDK_MultipleCandidate	0xFF3D
#define NIDK_PreviousCandidate	0xFF3E

/* Japanese keyboard support */

#define NIDK_Kanji		0xFF21	/* Kanji, Kanji convert */
#define NIDK_Muhenkan		0xFF22  /* Cancel Conversion */
#define NIDK_Henkan_Mode		0xFF23  /* Start/Stop Conversion */
#define NIDK_Henkan		0xFF23  /* Alias for Henkan_Mode */
#define NIDK_Romaji		0xFF24  /* to Romaji */
#define NIDK_Hiragana		0xFF25  /* to Hiragana */
#define NIDK_Katakana		0xFF26  /* to Katakana */
#define NIDK_Hiragana_Katakana	0xFF27  /* Hiragana/Katakana toggle */
#define NIDK_Zenkaku		0xFF28  /* to Zenkaku */
#define NIDK_Hankaku		0xFF29  /* to Hankaku */
#define NIDK_Zenkaku_Hankaku	0xFF2A  /* Zenkaku/Hankaku toggle */
#define NIDK_Touroku		0xFF2B  /* Add to Dictionary */
#define NIDK_Massyo		0xFF2C  /* Delete from Dictionary */
#define NIDK_Kana_Lock		0xFF2D  /* Kana Lock */
#define NIDK_Kana_Shift		0xFF2E  /* Kana Shift */
#define NIDK_Eisu_Shift		0xFF2F  /* Alphanumeric Shift */
#define NIDK_Eisu_toggle		0xFF30  /* Alphanumeric toggle */
#define NIDK_Kanji_Bangou		0xFF37  /* Codeinput */
#define NIDK_Zen_Koho		0xFF3D	/* Multiple/All Candidate(s) */
#define NIDK_Mae_Koho		0xFF3E	/* Previous Candidate */

/* 0xFF31 thru 0xFF3F are under NIDK_KOREAN */

/* Cursor control & motion */

#define NIDK_Home			0xFF50
#define NIDK_Left			0xFF51	/* Move left, left arrow */
#define NIDK_Up			0xFF52	/* Move up, up arrow */
#define NIDK_Right		0xFF53	/* Move right, right arrow */
#define NIDK_Down			0xFF54	/* Move down, down arrow */
#define NIDK_Prior		0xFF55	/* Prior, previous */
#define NIDK_Page_Up		0xFF55
#define NIDK_Next			0xFF56	/* Next */
#define NIDK_Page_Down		0xFF56
#define NIDK_End			0xFF57	/* EOL */
#define NIDK_Begin		0xFF58	/* BOL */


/* Misc Functions */

#define NIDK_Select		0xFF60	/* Select, mark */
#define NIDK_Print		0xFF61
#define NIDK_Execute		0xFF62	/* Execute, run, do */
#define NIDK_Insert		0xFF63	/* Insert, insert here */
#define NIDK_Undo			0xFF65	/* Undo, oops */
#define NIDK_Redo			0xFF66	/* redo, again */
#define NIDK_Menu			0xFF67
#define NIDK_Find			0xFF68	/* Find, search */
#define NIDK_Cancel		0xFF69	/* Cancel, stop, abort, exit */
#define NIDK_Help			0xFF6A	/* Help */
#define NIDK_Break		0xFF6B
#define NIDK_Mode_switch		0xFF7E	/* Character set switch */
#define NIDK_script_switch        0xFF7E  /* Alias for mode_switch */
#define NIDK_Num_Lock		0xFF7F

/* Keypad Functions, keypad numbers cleverly chosen to map to ascii */

#define NIDK_KP_Space		0xFF80	/* space */
#define NIDK_KP_Tab		0xFF89
#define NIDK_KP_Enter		0xFF8D	/* enter */
#define NIDK_KP_F1		0xFF91	/* PF1, KP_A, ... */
#define NIDK_KP_F2		0xFF92
#define NIDK_KP_F3		0xFF93
#define NIDK_KP_F4		0xFF94
#define NIDK_KP_Home		0xFF95
#define NIDK_KP_Left		0xFF96
#define NIDK_KP_Up		0xFF97
#define NIDK_KP_Right		0xFF98
#define NIDK_KP_Down		0xFF99
#define NIDK_KP_Prior		0xFF9A
#define NIDK_KP_Page_Up		0xFF9A
#define NIDK_KP_Next		0xFF9B
#define NIDK_KP_Page_Down		0xFF9B
#define NIDK_KP_End		0xFF9C
#define NIDK_KP_Begin		0xFF9D
#define NIDK_KP_Insert		0xFF9E
#define NIDK_KP_Delete		0xFF9F
#define NIDK_KP_Equal		0xFFBD	/* equals */
#define NIDK_KP_Multiply		0xFFAA
#define NIDK_KP_Add		0xFFAB
#define NIDK_KP_Separator		0xFFAC	/* separator, often comma */
#define NIDK_KP_Subtract		0xFFAD
#define NIDK_KP_Decimal		0xFFAE
#define NIDK_KP_Divide		0xFFAF

#define NIDK_KP_0			0xFFB0
#define NIDK_KP_1			0xFFB1
#define NIDK_KP_2			0xFFB2
#define NIDK_KP_3			0xFFB3
#define NIDK_KP_4			0xFFB4
#define NIDK_KP_5			0xFFB5
#define NIDK_KP_6			0xFFB6
#define NIDK_KP_7			0xFFB7
#define NIDK_KP_8			0xFFB8
#define NIDK_KP_9			0xFFB9



/*
 * Auxilliary Functions; note the duplicate definitions for left and right
 * function keys;  Sun keyboards and a few other manufactures have such
 * function key groups on the left and/or right sides of the keyboard.
 * We've not found a keyboard with more than 35 function keys total.
 */

#define NIDK_F1			0xFFBE
#define NIDK_F2			0xFFBF
#define NIDK_F3			0xFFC0
#define NIDK_F4			0xFFC1
#define NIDK_F5			0xFFC2
#define NIDK_F6			0xFFC3
#define NIDK_F7			0xFFC4
#define NIDK_F8			0xFFC5
#define NIDK_F9			0xFFC6
#define NIDK_F10			0xFFC7
#define NIDK_F11			0xFFC8
#define NIDK_L1			0xFFC8
#define NIDK_F12			0xFFC9
#define NIDK_L2			0xFFC9
#define NIDK_F13			0xFFCA
#define NIDK_L3			0xFFCA
#define NIDK_F14			0xFFCB
#define NIDK_L4			0xFFCB
#define NIDK_F15			0xFFCC
#define NIDK_L5			0xFFCC
#define NIDK_F16			0xFFCD
#define NIDK_L6			0xFFCD
#define NIDK_F17			0xFFCE
#define NIDK_L7			0xFFCE
#define NIDK_F18			0xFFCF
#define NIDK_L8			0xFFCF
#define NIDK_F19			0xFFD0
#define NIDK_L9			0xFFD0
#define NIDK_F20			0xFFD1
#define NIDK_L10			0xFFD1
#define NIDK_F21			0xFFD2
#define NIDK_R1			0xFFD2
#define NIDK_F22			0xFFD3
#define NIDK_R2			0xFFD3
#define NIDK_F23			0xFFD4
#define NIDK_R3			0xFFD4
#define NIDK_F24			0xFFD5
#define NIDK_R4			0xFFD5
#define NIDK_F25			0xFFD6
#define NIDK_R5			0xFFD6
#define NIDK_F26			0xFFD7
#define NIDK_R6			0xFFD7
#define NIDK_F27			0xFFD8
#define NIDK_R7			0xFFD8
#define NIDK_F28			0xFFD9
#define NIDK_R8			0xFFD9
#define NIDK_F29			0xFFDA
#define NIDK_R9			0xFFDA
#define NIDK_F30			0xFFDB
#define NIDK_R10			0xFFDB
#define NIDK_F31			0xFFDC
#define NIDK_R11			0xFFDC
#define NIDK_F32			0xFFDD
#define NIDK_R12			0xFFDD
#define NIDK_F33			0xFFDE
#define NIDK_R13			0xFFDE
#define NIDK_F34			0xFFDF
#define NIDK_R14			0xFFDF
#define NIDK_F35			0xFFE0
#define NIDK_R15			0xFFE0

/* Modifiers */

#define NIDK_Shift_L		0xFFE1	/* Left shift */
#define NIDK_Shift_R		0xFFE2	/* Right shift */
#define NIDK_Control_L		0xFFE3	/* Left control */
#define NIDK_Control_R		0xFFE4	/* Right control */
#define NIDK_Caps_Lock		0xFFE5	/* Caps lock */
#define NIDK_Shift_Lock		0xFFE6	/* Shift lock */

#define NIDK_Meta_L		0xFFE7	/* Left meta */
#define NIDK_Meta_R		0xFFE8	/* Right meta */
#define NIDK_Alt_L		0xFFE9	/* Left alt */
#define NIDK_Alt_R		0xFFEA	/* Right alt */
#define NIDK_Super_L		0xFFEB	/* Left super */
#define NIDK_Super_R		0xFFEC	/* Right super */
#define NIDK_Hyper_L		0xFFED	/* Left hyper */
#define NIDK_Hyper_R		0xFFEE	/* Right hyper */

/*
 * ISO 9995 Function and Modifier Keys
 * Byte 3 = 0xFE
 */

#define	NIDK_ISO_Lock					0xFE01
#define	NIDK_ISO_Level2_Latch				0xFE02
#define	NIDK_ISO_Level3_Shift				0xFE03
#define	NIDK_ISO_Level3_Latch				0xFE04
#define	NIDK_ISO_Level3_Lock				0xFE05
#define	NIDK_ISO_Group_Shift		0xFF7E	/* Alias for mode_switch */
#define	NIDK_ISO_Group_Latch				0xFE06
#define	NIDK_ISO_Group_Lock				0xFE07
#define	NIDK_ISO_Next_Group				0xFE08
#define	NIDK_ISO_Next_Group_Lock				0xFE09
#define	NIDK_ISO_Prev_Group				0xFE0A
#define	NIDK_ISO_Prev_Group_Lock				0xFE0B
#define	NIDK_ISO_First_Group				0xFE0C
#define	NIDK_ISO_First_Group_Lock				0xFE0D
#define	NIDK_ISO_Last_Group				0xFE0E
#define	NIDK_ISO_Last_Group_Lock				0xFE0F

#define	NIDK_ISO_Left_Tab					0xFE20
#define	NIDK_ISO_Move_Line_Up				0xFE21
#define	NIDK_ISO_Move_Line_Down				0xFE22
#define	NIDK_ISO_Partial_Line_Up				0xFE23
#define	NIDK_ISO_Partial_Line_Down			0xFE24
#define	NIDK_ISO_Partial_Space_Left			0xFE25
#define	NIDK_ISO_Partial_Space_Right			0xFE26
#define	NIDK_ISO_Set_Margin_Left				0xFE27
#define	NIDK_ISO_Set_Margin_Right				0xFE28
#define	NIDK_ISO_Release_Margin_Left			0xFE29
#define	NIDK_ISO_Release_Margin_Right			0xFE2A
#define	NIDK_ISO_Release_Both_Margins			0xFE2B
#define	NIDK_ISO_Fast_Cursor_Left				0xFE2C
#define	NIDK_ISO_Fast_Cursor_Right			0xFE2D
#define	NIDK_ISO_Fast_Cursor_Up				0xFE2E
#define	NIDK_ISO_Fast_Cursor_Down				0xFE2F
#define	NIDK_ISO_Continuous_Underline			0xFE30
#define	NIDK_ISO_Discontinuous_Underline			0xFE31
#define	NIDK_ISO_Emphasize				0xFE32
#define	NIDK_ISO_Center_Object				0xFE33
#define	NIDK_ISO_Enter					0xFE34

#define	NIDK_dead_grave					0xFE50
#define	NIDK_dead_acute					0xFE51
#define	NIDK_dead_circumflex				0xFE52
#define	NIDK_dead_tilde					0xFE53
#define	NIDK_dead_macron					0xFE54
#define	NIDK_dead_breve					0xFE55
#define	NIDK_dead_abovedot				0xFE56
#define	NIDK_dead_diaeresis				0xFE57
#define	NIDK_dead_abovering				0xFE58
#define	NIDK_dead_doubleacute				0xFE59
#define	NIDK_dead_caron					0xFE5A
#define	NIDK_dead_cedilla					0xFE5B
#define	NIDK_dead_ogonek					0xFE5C
#define	NIDK_dead_iota					0xFE5D
#define	NIDK_dead_voiced_sound				0xFE5E
#define	NIDK_dead_semivoiced_sound			0xFE5F
#define	NIDK_dead_belowdot				0xFE60

#define	NIDK_First_Virtual_Screen				0xFED0
#define	NIDK_Prev_Virtual_Screen				0xFED1
#define	NIDK_Next_Virtual_Screen				0xFED2
#define	NIDK_Last_Virtual_Screen				0xFED4
#define	NIDK_Terminate_Server				0xFED5

#define	NIDK_AccessX_Enable				0xFE70
#define	NIDK_AccessX_Feedback_Enable			0xFE71
#define	NIDK_RepeatKeys_Enable				0xFE72
#define	NIDK_SlowKeys_Enable				0xFE73
#define	NIDK_BounceKeys_Enable				0xFE74
#define	NIDK_StickyKeys_Enable				0xFE75
#define	NIDK_MouseKeys_Enable				0xFE76
#define	NIDK_MouseKeys_Accel_Enable			0xFE77
#define	NIDK_Overlay1_Enable				0xFE78
#define	NIDK_Overlay2_Enable				0xFE79
#define	NIDK_AudibleBell_Enable				0xFE7A

#define	NIDK_Pointer_Left					0xFEE0
#define	NIDK_Pointer_Right				0xFEE1
#define	NIDK_Pointer_Up					0xFEE2
#define	NIDK_Pointer_Down					0xFEE3
#define	NIDK_Pointer_UpLeft				0xFEE4
#define	NIDK_Pointer_UpRight				0xFEE5
#define	NIDK_Pointer_DownLeft				0xFEE6
#define	NIDK_Pointer_DownRight				0xFEE7
#define	NIDK_Pointer_Button_Dflt				0xFEE8
#define	NIDK_Pointer_Button1				0xFEE9
#define	NIDK_Pointer_Button2				0xFEEA
#define	NIDK_Pointer_Button3				0xFEEB
#define	NIDK_Pointer_Button4				0xFEEC
#define	NIDK_Pointer_Button5				0xFEED
#define	NIDK_Pointer_DblClick_Dflt			0xFEEE
#define	NIDK_Pointer_DblClick1				0xFEEF
#define	NIDK_Pointer_DblClick2				0xFEF0
#define	NIDK_Pointer_DblClick3				0xFEF1
#define	NIDK_Pointer_DblClick4				0xFEF2
#define	NIDK_Pointer_DblClick5				0xFEF3
#define	NIDK_Pointer_Drag_Dflt				0xFEF4
#define	NIDK_Pointer_Drag1				0xFEF5
#define	NIDK_Pointer_Drag2				0xFEF6
#define	NIDK_Pointer_Drag3				0xFEF7
#define	NIDK_Pointer_Drag4				0xFEF8
#define	NIDK_Pointer_Drag5				0xFEFD

#define	NIDK_Pointer_EnableKeys				0xFEF9
#define	NIDK_Pointer_Accelerate				0xFEFA
#define	NIDK_Pointer_DfltBtnNext				0xFEFB
#define	NIDK_Pointer_DfltBtnPrev				0xFEFC

/*
 * 3270 Terminal Keys
 * Byte 3 = 0xFD
 */

#define NIDK_3270_Duplicate      0xFD01
#define NIDK_3270_FieldMark      0xFD02
#define NIDK_3270_Right2         0xFD03
#define NIDK_3270_Left2          0xFD04
#define NIDK_3270_BackTab        0xFD05
#define NIDK_3270_EraseEOF       0xFD06
#define NIDK_3270_EraseInput     0xFD07
#define NIDK_3270_Reset          0xFD08
#define NIDK_3270_Quit           0xFD09
#define NIDK_3270_PA1            0xFD0A
#define NIDK_3270_PA2            0xFD0B
#define NIDK_3270_PA3            0xFD0C
#define NIDK_3270_Test           0xFD0D
#define NIDK_3270_Attn           0xFD0E
#define NIDK_3270_CursorBlink    0xFD0F
#define NIDK_3270_AltCursor      0xFD10
#define NIDK_3270_KeyClick       0xFD11
#define NIDK_3270_Jump           0xFD12
#define NIDK_3270_Ident          0xFD13
#define NIDK_3270_Rule           0xFD14
#define NIDK_3270_Copy           0xFD15
#define NIDK_3270_Play           0xFD16
#define NIDK_3270_Setup          0xFD17
#define NIDK_3270_Record         0xFD18
#define NIDK_3270_ChangeScreen   0xFD19
#define NIDK_3270_DeleteWord     0xFD1A
#define NIDK_3270_ExSelect       0xFD1B
#define NIDK_3270_CursorSelect   0xFD1C
#define NIDK_3270_PrintScreen    0xFD1D
#define NIDK_3270_Enter          0xFD1E

/*
 *  Latin 1
 *  Byte 3 = 0
 */
#define NIDK_space               0x020
#define NIDK_exclam              0x021
#define NIDK_quotedbl            0x022
#define NIDK_numbersign          0x023
#define NIDK_dollar              0x024
#define NIDK_percent             0x025
#define NIDK_ampersand           0x026
#define NIDK_apostrophe          0x027
#define NIDK_quoteright          0x027	/* deprecated */
#define NIDK_parenleft           0x028
#define NIDK_parenright          0x029
#define NIDK_asterisk            0x02a
#define NIDK_plus                0x02b
#define NIDK_comma               0x02c
#define NIDK_minus               0x02d
#define NIDK_period              0x02e
#define NIDK_slash               0x02f
#define NIDK_0                   0x030
#define NIDK_1                   0x031
#define NIDK_2                   0x032
#define NIDK_3                   0x033
#define NIDK_4                   0x034
#define NIDK_5                   0x035
#define NIDK_6                   0x036
#define NIDK_7                   0x037
#define NIDK_8                   0x038
#define NIDK_9                   0x039
#define NIDK_colon               0x03a
#define NIDK_semicolon           0x03b
#define NIDK_less                0x03c
#define NIDK_equal               0x03d
#define NIDK_greater             0x03e
#define NIDK_question            0x03f
#define NIDK_at                  0x040
#define NIDK_A                   0x041
#define NIDK_B                   0x042
#define NIDK_C                   0x043
#define NIDK_D                   0x044
#define NIDK_E                   0x045
#define NIDK_F                   0x046
#define NIDK_G                   0x047
#define NIDK_H                   0x048
#define NIDK_I                   0x049
#define NIDK_J                   0x04a
#define NIDK_K                   0x04b
#define NIDK_L                   0x04c
#define NIDK_M                   0x04d
#define NIDK_N                   0x04e
#define NIDK_O                   0x04f
#define NIDK_P                   0x050
#define NIDK_Q                   0x051
#define NIDK_R                   0x052
#define NIDK_S                   0x053
#define NIDK_T                   0x054
#define NIDK_U                   0x055
#define NIDK_V                   0x056
#define NIDK_W                   0x057
#define NIDK_X                   0x058
#define NIDK_Y                   0x059
#define NIDK_Z                   0x05a
#define NIDK_bracketleft         0x05b
#define NIDK_backslash           0x05c
#define NIDK_bracketright        0x05d
#define NIDK_asciicircum         0x05e
#define NIDK_underscore          0x05f
#define NIDK_grave               0x060
#define NIDK_quoteleft           0x060	/* deprecated */
#define NIDK_a                   0x061
#define NIDK_b                   0x062
#define NIDK_c                   0x063
#define NIDK_d                   0x064
#define NIDK_e                   0x065
#define NIDK_f                   0x066
#define NIDK_g                   0x067
#define NIDK_h                   0x068
#define NIDK_i                   0x069
#define NIDK_j                   0x06a
#define NIDK_k                   0x06b
#define NIDK_l                   0x06c
#define NIDK_m                   0x06d
#define NIDK_n                   0x06e
#define NIDK_o                   0x06f
#define NIDK_p                   0x070
#define NIDK_q                   0x071
#define NIDK_r                   0x072
#define NIDK_s                   0x073
#define NIDK_t                   0x074
#define NIDK_u                   0x075
#define NIDK_v                   0x076
#define NIDK_w                   0x077
#define NIDK_x                   0x078
#define NIDK_y                   0x079
#define NIDK_z                   0x07a
#define NIDK_braceleft           0x07b
#define NIDK_bar                 0x07c
#define NIDK_braceright          0x07d
#define NIDK_asciitilde          0x07e

#define NIDK_nobreakspace        0x0a0
#define NIDK_exclamdown          0x0a1
#define NIDK_cent        	       0x0a2
#define NIDK_sterling            0x0a3
#define NIDK_currency            0x0a4
#define NIDK_yen                 0x0a5
#define NIDK_brokenbar           0x0a6
#define NIDK_section             0x0a7
#define NIDK_diaeresis           0x0a8
#define NIDK_copyright           0x0a9
#define NIDK_ordfeminine         0x0aa
#define NIDK_guillemotleft       0x0ab	/* left angle quotation mark */
#define NIDK_notsign             0x0ac
#define NIDK_hyphen              0x0ad
#define NIDK_registered          0x0ae
#define NIDK_macron              0x0af
#define NIDK_degree              0x0b0
#define NIDK_plusminus           0x0b1
#define NIDK_twosuperior         0x0b2
#define NIDK_threesuperior       0x0b3
#define NIDK_acute               0x0b4
#define NIDK_mu                  0x0b5
#define NIDK_paragraph           0x0b6
#define NIDK_periodcentered      0x0b7
#define NIDK_cedilla             0x0b8
#define NIDK_onesuperior         0x0b9
#define NIDK_masculine           0x0ba
#define NIDK_guillemotright      0x0bb	/* right angle quotation mark */
#define NIDK_onequarter          0x0bc
#define NIDK_onehalf             0x0bd
#define NIDK_threequarters       0x0be
#define NIDK_questiondown        0x0bf
#define NIDK_Agrave              0x0c0
#define NIDK_Aacute              0x0c1
#define NIDK_Acircumflex         0x0c2
#define NIDK_Atilde              0x0c3
#define NIDK_Adiaeresis          0x0c4
#define NIDK_Aring               0x0c5
#define NIDK_AE                  0x0c6
#define NIDK_Ccedilla            0x0c7
#define NIDK_Egrave              0x0c8
#define NIDK_Eacute              0x0c9
#define NIDK_Ecircumflex         0x0ca
#define NIDK_Ediaeresis          0x0cb
#define NIDK_Igrave              0x0cc
#define NIDK_Iacute              0x0cd
#define NIDK_Icircumflex         0x0ce
#define NIDK_Idiaeresis          0x0cf
#define NIDK_ETH                 0x0d0
#define NIDK_Eth                 0x0d0	/* deprecated */
#define NIDK_Ntilde              0x0d1
#define NIDK_Ograve              0x0d2
#define NIDK_Oacute              0x0d3
#define NIDK_Ocircumflex         0x0d4
#define NIDK_Otilde              0x0d5
#define NIDK_Odiaeresis          0x0d6
#define NIDK_multiply            0x0d7
#define NIDK_Ooblique            0x0d8
#define NIDK_Ugrave              0x0d9
#define NIDK_Uacute              0x0da
#define NIDK_Ucircumflex         0x0db
#define NIDK_Udiaeresis          0x0dc
#define NIDK_Yacute              0x0dd
#define NIDK_THORN               0x0de
#define NIDK_Thorn               0x0de	/* deprecated */
#define NIDK_ssharp              0x0df
#define NIDK_agrave              0x0e0
#define NIDK_aacute              0x0e1
#define NIDK_acircumflex         0x0e2
#define NIDK_atilde              0x0e3
#define NIDK_adiaeresis          0x0e4
#define NIDK_aring               0x0e5
#define NIDK_ae                  0x0e6
#define NIDK_ccedilla            0x0e7
#define NIDK_egrave              0x0e8
#define NIDK_eacute              0x0e9
#define NIDK_ecircumflex         0x0ea
#define NIDK_ediaeresis          0x0eb
#define NIDK_igrave              0x0ec
#define NIDK_iacute              0x0ed
#define NIDK_icircumflex         0x0ee
#define NIDK_idiaeresis          0x0ef
#define NIDK_eth                 0x0f0
#define NIDK_ntilde              0x0f1
#define NIDK_ograve              0x0f2
#define NIDK_oacute              0x0f3
#define NIDK_ocircumflex         0x0f4
#define NIDK_otilde              0x0f5
#define NIDK_odiaeresis          0x0f6
#define NIDK_division            0x0f7
#define NIDK_oslash              0x0f8
#define NIDK_ugrave              0x0f9
#define NIDK_uacute              0x0fa
#define NIDK_ucircumflex         0x0fb
#define NIDK_udiaeresis          0x0fc
#define NIDK_yacute              0x0fd
#define NIDK_thorn               0x0fe
#define NIDK_ydiaeresis          0x0ff

/*
 *   Latin 2
 *   Byte 3 = 1
 */

#define NIDK_Aogonek             0x1a1
#define NIDK_breve               0x1a2
#define NIDK_Lstroke             0x1a3
#define NIDK_Lcaron              0x1a5
#define NIDK_Sacute              0x1a6
#define NIDK_Scaron              0x1a9
#define NIDK_Scedilla            0x1aa
#define NIDK_Tcaron              0x1ab
#define NIDK_Zacute              0x1ac
#define NIDK_Zcaron              0x1ae
#define NIDK_Zabovedot           0x1af
#define NIDK_aogonek             0x1b1
#define NIDK_ogonek              0x1b2
#define NIDK_lstroke             0x1b3
#define NIDK_lcaron              0x1b5
#define NIDK_sacute              0x1b6
#define NIDK_caron               0x1b7
#define NIDK_scaron              0x1b9
#define NIDK_scedilla            0x1ba
#define NIDK_tcaron              0x1bb
#define NIDK_zacute              0x1bc
#define NIDK_doubleacute         0x1bd
#define NIDK_zcaron              0x1be
#define NIDK_zabovedot           0x1bf
#define NIDK_Racute              0x1c0
#define NIDK_Abreve              0x1c3
#define NIDK_Lacute              0x1c5
#define NIDK_Cacute              0x1c6
#define NIDK_Ccaron              0x1c8
#define NIDK_Eogonek             0x1ca
#define NIDK_Ecaron              0x1cc
#define NIDK_Dcaron              0x1cf
#define NIDK_Dstroke             0x1d0
#define NIDK_Nacute              0x1d1
#define NIDK_Ncaron              0x1d2
#define NIDK_Odoubleacute        0x1d5
#define NIDK_Rcaron              0x1d8
#define NIDK_Uring               0x1d9
#define NIDK_Udoubleacute        0x1db
#define NIDK_Tcedilla            0x1de
#define NIDK_racute              0x1e0
#define NIDK_abreve              0x1e3
#define NIDK_lacute              0x1e5
#define NIDK_cacute              0x1e6
#define NIDK_ccaron              0x1e8
#define NIDK_eogonek             0x1ea
#define NIDK_ecaron              0x1ec
#define NIDK_dcaron              0x1ef
#define NIDK_dstroke             0x1f0
#define NIDK_nacute              0x1f1
#define NIDK_ncaron              0x1f2
#define NIDK_odoubleacute        0x1f5
#define NIDK_udoubleacute        0x1fb
#define NIDK_rcaron              0x1f8
#define NIDK_uring               0x1f9
#define NIDK_tcedilla            0x1fe
#define NIDK_abovedot            0x1ff

/*
 *   Latin 3
 *   Byte 3 = 2
 */

#define NIDK_Hstroke             0x2a1
#define NIDK_Hcircumflex         0x2a6
#define NIDK_Iabovedot           0x2a9
#define NIDK_Gbreve              0x2ab
#define NIDK_Jcircumflex         0x2ac
#define NIDK_hstroke             0x2b1
#define NIDK_hcircumflex         0x2b6
#define NIDK_idotless            0x2b9
#define NIDK_gbreve              0x2bb
#define NIDK_jcircumflex         0x2bc
#define NIDK_Cabovedot           0x2c5
#define NIDK_Ccircumflex         0x2c6
#define NIDK_Gabovedot           0x2d5
#define NIDK_Gcircumflex         0x2d8
#define NIDK_Ubreve              0x2dd
#define NIDK_Scircumflex         0x2de
#define NIDK_cabovedot           0x2e5
#define NIDK_ccircumflex         0x2e6
#define NIDK_gabovedot           0x2f5
#define NIDK_gcircumflex         0x2f8
#define NIDK_ubreve              0x2fd
#define NIDK_scircumflex         0x2fe


/*
 *   Latin 4
 *   Byte 3 = 3
 */

#define NIDK_kra                 0x3a2
#define NIDK_kappa               0x3a2	/* deprecated */
#define NIDK_Rcedilla            0x3a3
#define NIDK_Itilde              0x3a5
#define NIDK_Lcedilla            0x3a6
#define NIDK_Emacron             0x3aa
#define NIDK_Gcedilla            0x3ab
#define NIDK_Tslash              0x3ac
#define NIDK_rcedilla            0x3b3
#define NIDK_itilde              0x3b5
#define NIDK_lcedilla            0x3b6
#define NIDK_emacron             0x3ba
#define NIDK_gcedilla            0x3bb
#define NIDK_tslash              0x3bc
#define NIDK_ENG                 0x3bd
#define NIDK_eng                 0x3bf
#define NIDK_Amacron             0x3c0
#define NIDK_Iogonek             0x3c7
#define NIDK_Eabovedot           0x3cc
#define NIDK_Imacron             0x3cf
#define NIDK_Ncedilla            0x3d1
#define NIDK_Omacron             0x3d2
#define NIDK_Kcedilla            0x3d3
#define NIDK_Uogonek             0x3d9
#define NIDK_Utilde              0x3dd
#define NIDK_Umacron             0x3de
#define NIDK_amacron             0x3e0
#define NIDK_iogonek             0x3e7
#define NIDK_eabovedot           0x3ec
#define NIDK_imacron             0x3ef
#define NIDK_ncedilla            0x3f1
#define NIDK_omacron             0x3f2
#define NIDK_kcedilla            0x3f3
#define NIDK_uogonek             0x3f9
#define NIDK_utilde              0x3fd
#define NIDK_umacron             0x3fe

/*
 * Katakana
 * Byte 3 = 4
 */

#define NIDK_overline				       0x47e
#define NIDK_kana_fullstop                               0x4a1
#define NIDK_kana_openingbracket                         0x4a2
#define NIDK_kana_closingbracket                         0x4a3
#define NIDK_kana_comma                                  0x4a4
#define NIDK_kana_conjunctive                            0x4a5
#define NIDK_kana_middledot                              0x4a5  /* deprecated */
#define NIDK_kana_WO                                     0x4a6
#define NIDK_kana_a                                      0x4a7
#define NIDK_kana_i                                      0x4a8
#define NIDK_kana_u                                      0x4a9
#define NIDK_kana_e                                      0x4aa
#define NIDK_kana_o                                      0x4ab
#define NIDK_kana_ya                                     0x4ac
#define NIDK_kana_yu                                     0x4ad
#define NIDK_kana_yo                                     0x4ae
#define NIDK_kana_tsu                                    0x4af
#define NIDK_kana_tu                                     0x4af  /* deprecated */
#define NIDK_prolongedsound                              0x4b0
#define NIDK_kana_A                                      0x4b1
#define NIDK_kana_I                                      0x4b2
#define NIDK_kana_U                                      0x4b3
#define NIDK_kana_E                                      0x4b4
#define NIDK_kana_O                                      0x4b5
#define NIDK_kana_KA                                     0x4b6
#define NIDK_kana_KI                                     0x4b7
#define NIDK_kana_KU                                     0x4b8
#define NIDK_kana_KE                                     0x4b9
#define NIDK_kana_KO                                     0x4ba
#define NIDK_kana_SA                                     0x4bb
#define NIDK_kana_SHI                                    0x4bc
#define NIDK_kana_SU                                     0x4bd
#define NIDK_kana_SE                                     0x4be
#define NIDK_kana_SO                                     0x4bf
#define NIDK_kana_TA                                     0x4c0
#define NIDK_kana_CHI                                    0x4c1
#define NIDK_kana_TI                                     0x4c1  /* deprecated */
#define NIDK_kana_TSU                                    0x4c2
#define NIDK_kana_TU                                     0x4c2  /* deprecated */
#define NIDK_kana_TE                                     0x4c3
#define NIDK_kana_TO                                     0x4c4
#define NIDK_kana_NA                                     0x4c5
#define NIDK_kana_NI                                     0x4c6
#define NIDK_kana_NU                                     0x4c7
#define NIDK_kana_NE                                     0x4c8
#define NIDK_kana_NO                                     0x4c9
#define NIDK_kana_HA                                     0x4ca
#define NIDK_kana_HI                                     0x4cb
#define NIDK_kana_FU                                     0x4cc
#define NIDK_kana_HU                                     0x4cc  /* deprecated */
#define NIDK_kana_HE                                     0x4cd
#define NIDK_kana_HO                                     0x4ce
#define NIDK_kana_MA                                     0x4cf
#define NIDK_kana_MI                                     0x4d0
#define NIDK_kana_MU                                     0x4d1
#define NIDK_kana_ME                                     0x4d2
#define NIDK_kana_MO                                     0x4d3
#define NIDK_kana_YA                                     0x4d4
#define NIDK_kana_YU                                     0x4d5
#define NIDK_kana_YO                                     0x4d6
#define NIDK_kana_RA                                     0x4d7
#define NIDK_kana_RI                                     0x4d8
#define NIDK_kana_RU                                     0x4d9
#define NIDK_kana_RE                                     0x4da
#define NIDK_kana_RO                                     0x4db
#define NIDK_kana_WA                                     0x4dc
#define NIDK_kana_N                                      0x4dd
#define NIDK_voicedsound                                 0x4de
#define NIDK_semivoicedsound                             0x4df
#define NIDK_kana_switch          0xFF7E  /* Alias for mode_switch */

/*
 *  Arabic
 *  Byte 3 = 5
 */

#define NIDK_Arabic_comma                                0x5ac
#define NIDK_Arabic_semicolon                            0x5bb
#define NIDK_Arabic_question_mark                        0x5bf
#define NIDK_Arabic_hamza                                0x5c1
#define NIDK_Arabic_maddaonalef                          0x5c2
#define NIDK_Arabic_hamzaonalef                          0x5c3
#define NIDK_Arabic_hamzaonwaw                           0x5c4
#define NIDK_Arabic_hamzaunderalef                       0x5c5
#define NIDK_Arabic_hamzaonyeh                           0x5c6
#define NIDK_Arabic_alef                                 0x5c7
#define NIDK_Arabic_beh                                  0x5c8
#define NIDK_Arabic_tehmarbuta                           0x5c9
#define NIDK_Arabic_teh                                  0x5ca
#define NIDK_Arabic_theh                                 0x5cb
#define NIDK_Arabic_jeem                                 0x5cc
#define NIDK_Arabic_hah                                  0x5cd
#define NIDK_Arabic_khah                                 0x5ce
#define NIDK_Arabic_dal                                  0x5cf
#define NIDK_Arabic_thal                                 0x5d0
#define NIDK_Arabic_ra                                   0x5d1
#define NIDK_Arabic_zain                                 0x5d2
#define NIDK_Arabic_seen                                 0x5d3
#define NIDK_Arabic_sheen                                0x5d4
#define NIDK_Arabic_sad                                  0x5d5
#define NIDK_Arabic_dad                                  0x5d6
#define NIDK_Arabic_tah                                  0x5d7
#define NIDK_Arabic_zah                                  0x5d8
#define NIDK_Arabic_ain                                  0x5d9
#define NIDK_Arabic_ghain                                0x5da
#define NIDK_Arabic_tatweel                              0x5e0
#define NIDK_Arabic_feh                                  0x5e1
#define NIDK_Arabic_qaf                                  0x5e2
#define NIDK_Arabic_kaf                                  0x5e3
#define NIDK_Arabic_lam                                  0x5e4
#define NIDK_Arabic_meem                                 0x5e5
#define NIDK_Arabic_noon                                 0x5e6
#define NIDK_Arabic_ha                                   0x5e7
#define NIDK_Arabic_heh                                  0x5e7  /* deprecated */
#define NIDK_Arabic_waw                                  0x5e8
#define NIDK_Arabic_alefmaksura                          0x5e9
#define NIDK_Arabic_yeh                                  0x5ea
#define NIDK_Arabic_fathatan                             0x5eb
#define NIDK_Arabic_dammatan                             0x5ec
#define NIDK_Arabic_kasratan                             0x5ed
#define NIDK_Arabic_fatha                                0x5ee
#define NIDK_Arabic_damma                                0x5ef
#define NIDK_Arabic_kasra                                0x5f0
#define NIDK_Arabic_shadda                               0x5f1
#define NIDK_Arabic_sukun                                0x5f2
#define NIDK_Arabic_switch        0xFF7E  /* Alias for mode_switch */

/*
 * Cyrillic
 * Byte 3 = 6
 */
#define NIDK_Serbian_dje                                 0x6a1
#define NIDK_Macedonia_gje                               0x6a2
#define NIDK_Cyrillic_io                                 0x6a3
#define NIDK_Ukrainian_ie                                0x6a4
#define NIDK_Ukranian_je                                 0x6a4  /* deprecated */
#define NIDK_Macedonia_dse                               0x6a5
#define NIDK_Ukrainian_i                                 0x6a6
#define NIDK_Ukranian_i                                  0x6a6  /* deprecated */
#define NIDK_Ukrainian_yi                                0x6a7
#define NIDK_Ukranian_yi                                 0x6a7  /* deprecated */
#define NIDK_Cyrillic_je                                 0x6a8
#define NIDK_Serbian_je                                  0x6a8  /* deprecated */
#define NIDK_Cyrillic_lje                                0x6a9
#define NIDK_Serbian_lje                                 0x6a9  /* deprecated */
#define NIDK_Cyrillic_nje                                0x6aa
#define NIDK_Serbian_nje                                 0x6aa  /* deprecated */
#define NIDK_Serbian_tshe                                0x6ab
#define NIDK_Macedonia_kje                               0x6ac
#define NIDK_Byelorussian_shortu                         0x6ae
#define NIDK_Cyrillic_dzhe                               0x6af
#define NIDK_Serbian_dze                                 0x6af  /* deprecated */
#define NIDK_numerosign                                  0x6b0
#define NIDK_Serbian_DJE                                 0x6b1
#define NIDK_Macedonia_GJE                               0x6b2
#define NIDK_Cyrillic_IO                                 0x6b3
#define NIDK_Ukrainian_IE                                0x6b4
#define NIDK_Ukranian_JE                                 0x6b4  /* deprecated */
#define NIDK_Macedonia_DSE                               0x6b5
#define NIDK_Ukrainian_I                                 0x6b6
#define NIDK_Ukranian_I                                  0x6b6  /* deprecated */
#define NIDK_Ukrainian_YI                                0x6b7
#define NIDK_Ukranian_YI                                 0x6b7  /* deprecated */
#define NIDK_Cyrillic_JE                                 0x6b8
#define NIDK_Serbian_JE                                  0x6b8  /* deprecated */
#define NIDK_Cyrillic_LJE                                0x6b9
#define NIDK_Serbian_LJE                                 0x6b9  /* deprecated */
#define NIDK_Cyrillic_NJE                                0x6ba
#define NIDK_Serbian_NJE                                 0x6ba  /* deprecated */
#define NIDK_Serbian_TSHE                                0x6bb
#define NIDK_Macedonia_KJE                               0x6bc
#define NIDK_Byelorussian_SHORTU                         0x6be
#define NIDK_Cyrillic_DZHE                               0x6bf
#define NIDK_Serbian_DZE                                 0x6bf  /* deprecated */
#define NIDK_Cyrillic_yu                                 0x6c0
#define NIDK_Cyrillic_a                                  0x6c1
#define NIDK_Cyrillic_be                                 0x6c2
#define NIDK_Cyrillic_tse                                0x6c3
#define NIDK_Cyrillic_de                                 0x6c4
#define NIDK_Cyrillic_ie                                 0x6c5
#define NIDK_Cyrillic_ef                                 0x6c6
#define NIDK_Cyrillic_ghe                                0x6c7
#define NIDK_Cyrillic_ha                                 0x6c8
#define NIDK_Cyrillic_i                                  0x6c9
#define NIDK_Cyrillic_shorti                             0x6ca
#define NIDK_Cyrillic_ka                                 0x6cb
#define NIDK_Cyrillic_el                                 0x6cc
#define NIDK_Cyrillic_em                                 0x6cd
#define NIDK_Cyrillic_en                                 0x6ce
#define NIDK_Cyrillic_o                                  0x6cf
#define NIDK_Cyrillic_pe                                 0x6d0
#define NIDK_Cyrillic_ya                                 0x6d1
#define NIDK_Cyrillic_er                                 0x6d2
#define NIDK_Cyrillic_es                                 0x6d3
#define NIDK_Cyrillic_te                                 0x6d4
#define NIDK_Cyrillic_u                                  0x6d5
#define NIDK_Cyrillic_zhe                                0x6d6
#define NIDK_Cyrillic_ve                                 0x6d7
#define NIDK_Cyrillic_softsign                           0x6d8
#define NIDK_Cyrillic_yeru                               0x6d9
#define NIDK_Cyrillic_ze                                 0x6da
#define NIDK_Cyrillic_sha                                0x6db
#define NIDK_Cyrillic_e                                  0x6dc
#define NIDK_Cyrillic_shcha                              0x6dd
#define NIDK_Cyrillic_che                                0x6de
#define NIDK_Cyrillic_hardsign                           0x6df
#define NIDK_Cyrillic_YU                                 0x6e0
#define NIDK_Cyrillic_A                                  0x6e1
#define NIDK_Cyrillic_BE                                 0x6e2
#define NIDK_Cyrillic_TSE                                0x6e3
#define NIDK_Cyrillic_DE                                 0x6e4
#define NIDK_Cyrillic_IE                                 0x6e5
#define NIDK_Cyrillic_EF                                 0x6e6
#define NIDK_Cyrillic_GHE                                0x6e7
#define NIDK_Cyrillic_HA                                 0x6e8
#define NIDK_Cyrillic_I                                  0x6e9
#define NIDK_Cyrillic_SHORTI                             0x6ea
#define NIDK_Cyrillic_KA                                 0x6eb
#define NIDK_Cyrillic_EL                                 0x6ec
#define NIDK_Cyrillic_EM                                 0x6ed
#define NIDK_Cyrillic_EN                                 0x6ee
#define NIDK_Cyrillic_O                                  0x6ef
#define NIDK_Cyrillic_PE                                 0x6f0
#define NIDK_Cyrillic_YA                                 0x6f1
#define NIDK_Cyrillic_ER                                 0x6f2
#define NIDK_Cyrillic_ES                                 0x6f3
#define NIDK_Cyrillic_TE                                 0x6f4
#define NIDK_Cyrillic_U                                  0x6f5
#define NIDK_Cyrillic_ZHE                                0x6f6
#define NIDK_Cyrillic_VE                                 0x6f7
#define NIDK_Cyrillic_SOFTSIGN                           0x6f8
#define NIDK_Cyrillic_YERU                               0x6f9
#define NIDK_Cyrillic_ZE                                 0x6fa
#define NIDK_Cyrillic_SHA                                0x6fb
#define NIDK_Cyrillic_E                                  0x6fc
#define NIDK_Cyrillic_SHCHA                              0x6fd
#define NIDK_Cyrillic_CHE                                0x6fe
#define NIDK_Cyrillic_HARDSIGN                           0x6ff

/*
 * Greek
 * Byte 3 = 7
 */

#define NIDK_Greek_ALPHAaccent                           0x7a1
#define NIDK_Greek_EPSILONaccent                         0x7a2
#define NIDK_Greek_ETAaccent                             0x7a3
#define NIDK_Greek_IOTAaccent                            0x7a4
#define NIDK_Greek_IOTAdiaeresis                         0x7a5
#define NIDK_Greek_OMICRONaccent                         0x7a7
#define NIDK_Greek_UPSILONaccent                         0x7a8
#define NIDK_Greek_UPSILONdieresis                       0x7a9
#define NIDK_Greek_OMEGAaccent                           0x7ab
#define NIDK_Greek_accentdieresis                        0x7ae
#define NIDK_Greek_horizbar                              0x7af
#define NIDK_Greek_alphaaccent                           0x7b1
#define NIDK_Greek_epsilonaccent                         0x7b2
#define NIDK_Greek_etaaccent                             0x7b3
#define NIDK_Greek_iotaaccent                            0x7b4
#define NIDK_Greek_iotadieresis                          0x7b5
#define NIDK_Greek_iotaaccentdieresis                    0x7b6
#define NIDK_Greek_omicronaccent                         0x7b7
#define NIDK_Greek_upsilonaccent                         0x7b8
#define NIDK_Greek_upsilondieresis                       0x7b9
#define NIDK_Greek_upsilonaccentdieresis                 0x7ba
#define NIDK_Greek_omegaaccent                           0x7bb
#define NIDK_Greek_ALPHA                                 0x7c1
#define NIDK_Greek_BETA                                  0x7c2
#define NIDK_Greek_GAMMA                                 0x7c3
#define NIDK_Greek_DELTA                                 0x7c4
#define NIDK_Greek_EPSILON                               0x7c5
#define NIDK_Greek_ZETA                                  0x7c6
#define NIDK_Greek_ETA                                   0x7c7
#define NIDK_Greek_THETA                                 0x7c8
#define NIDK_Greek_IOTA                                  0x7c9
#define NIDK_Greek_KAPPA                                 0x7ca
#define NIDK_Greek_LAMDA                                 0x7cb
#define NIDK_Greek_LAMBDA                                0x7cb
#define NIDK_Greek_MU                                    0x7cc
#define NIDK_Greek_NU                                    0x7cd
#define NIDK_Greek_XI                                    0x7ce
#define NIDK_Greek_OMICRON                               0x7cf
#define NIDK_Greek_PI                                    0x7d0
#define NIDK_Greek_RHO                                   0x7d1
#define NIDK_Greek_SIGMA                                 0x7d2
#define NIDK_Greek_TAU                                   0x7d4
#define NIDK_Greek_UPSILON                               0x7d5
#define NIDK_Greek_PHI                                   0x7d6
#define NIDK_Greek_CHI                                   0x7d7
#define NIDK_Greek_PSI                                   0x7d8
#define NIDK_Greek_OMEGA                                 0x7d9
#define NIDK_Greek_alpha                                 0x7e1
#define NIDK_Greek_beta                                  0x7e2
#define NIDK_Greek_gamma                                 0x7e3
#define NIDK_Greek_delta                                 0x7e4
#define NIDK_Greek_epsilon                               0x7e5
#define NIDK_Greek_zeta                                  0x7e6
#define NIDK_Greek_eta                                   0x7e7
#define NIDK_Greek_theta                                 0x7e8
#define NIDK_Greek_iota                                  0x7e9
#define NIDK_Greek_kappa                                 0x7ea
#define NIDK_Greek_lamda                                 0x7eb
#define NIDK_Greek_lambda                                0x7eb
#define NIDK_Greek_mu                                    0x7ec
#define NIDK_Greek_nu                                    0x7ed
#define NIDK_Greek_xi                                    0x7ee
#define NIDK_Greek_omicron                               0x7ef
#define NIDK_Greek_pi                                    0x7f0
#define NIDK_Greek_rho                                   0x7f1
#define NIDK_Greek_sigma                                 0x7f2
#define NIDK_Greek_finalsmallsigma                       0x7f3
#define NIDK_Greek_tau                                   0x7f4
#define NIDK_Greek_upsilon                               0x7f5
#define NIDK_Greek_phi                                   0x7f6
#define NIDK_Greek_chi                                   0x7f7
#define NIDK_Greek_psi                                   0x7f8
#define NIDK_Greek_omega                                 0x7f9
#define NIDK_Greek_switch         0xFF7E  /* Alias for mode_switch */

/*
 * Technical
 * Byte 3 = 8
 */

#define NIDK_leftradical                                 0x8a1
#define NIDK_topleftradical                              0x8a2
#define NIDK_horizconnector                              0x8a3
#define NIDK_topintegral                                 0x8a4
#define NIDK_botintegral                                 0x8a5
#define NIDK_vertconnector                               0x8a6
#define NIDK_topleftsqbracket                            0x8a7
#define NIDK_botleftsqbracket                            0x8a8
#define NIDK_toprightsqbracket                           0x8a9
#define NIDK_botrightsqbracket                           0x8aa
#define NIDK_topleftparens                               0x8ab
#define NIDK_botleftparens                               0x8ac
#define NIDK_toprightparens                              0x8ad
#define NIDK_botrightparens                              0x8ae
#define NIDK_leftmiddlecurlybrace                        0x8af
#define NIDK_rightmiddlecurlybrace                       0x8b0
#define NIDK_topleftsummation                            0x8b1
#define NIDK_botleftsummation                            0x8b2
#define NIDK_topvertsummationconnector                   0x8b3
#define NIDK_botvertsummationconnector                   0x8b4
#define NIDK_toprightsummation                           0x8b5
#define NIDK_botrightsummation                           0x8b6
#define NIDK_rightmiddlesummation                        0x8b7
#define NIDK_lessthanequal                               0x8bc
#define NIDK_notequal                                    0x8bd
#define NIDK_greaterthanequal                            0x8be
#define NIDK_integral                                    0x8bf
#define NIDK_therefore                                   0x8c0
#define NIDK_variation                                   0x8c1
#define NIDK_infinity                                    0x8c2
#define NIDK_nabla                                       0x8c5
#define NIDK_approximate                                 0x8c8
#define NIDK_similarequal                                0x8c9
#define NIDK_ifonlyif                                    0x8cd
#define NIDK_implies                                     0x8ce
#define NIDK_identical                                   0x8cf
#define NIDK_radical                                     0x8d6
#define NIDK_includedin                                  0x8da
#define NIDK_includes                                    0x8db
#define NIDK_intersection                                0x8dc
#define NIDK_union                                       0x8dd
#define NIDK_logicaland                                  0x8de
#define NIDK_logicalor                                   0x8df
#define NIDK_partialderivative                           0x8ef
#define NIDK_function                                    0x8f6
#define NIDK_leftarrow                                   0x8fb
#define NIDK_uparrow                                     0x8fc
#define NIDK_rightarrow                                  0x8fd
#define NIDK_downarrow                                   0x8fe

/*
 *  Special
 *  Byte 3 = 9
 */

#define NIDK_blank                                       0x9df
#define NIDK_soliddiamond                                0x9e0
#define NIDK_checkerboard                                0x9e1
#define NIDK_ht                                          0x9e2
#define NIDK_ff                                          0x9e3
#define NIDK_cr                                          0x9e4
#define NIDK_lf                                          0x9e5
#define NIDK_nl                                          0x9e8
#define NIDK_vt                                          0x9e9
#define NIDK_lowrightcorner                              0x9ea
#define NIDK_uprightcorner                               0x9eb
#define NIDK_upleftcorner                                0x9ec
#define NIDK_lowleftcorner                               0x9ed
#define NIDK_crossinglines                               0x9ee
#define NIDK_horizlinescan1                              0x9ef
#define NIDK_horizlinescan3                              0x9f0
#define NIDK_horizlinescan5                              0x9f1
#define NIDK_horizlinescan7                              0x9f2
#define NIDK_horizlinescan9                              0x9f3
#define NIDK_leftt                                       0x9f4
#define NIDK_rightt                                      0x9f5
#define NIDK_bott                                        0x9f6
#define NIDK_topt                                        0x9f7
#define NIDK_vertbar                                     0x9f8

/*
 *  Publishing
 *  Byte 3 = a
 */

#define NIDK_emspace                                     0xaa1
#define NIDK_enspace                                     0xaa2
#define NIDK_em3space                                    0xaa3
#define NIDK_em4space                                    0xaa4
#define NIDK_digitspace                                  0xaa5
#define NIDK_punctspace                                  0xaa6
#define NIDK_thinspace                                   0xaa7
#define NIDK_hairspace                                   0xaa8
#define NIDK_emdash                                      0xaa9
#define NIDK_endash                                      0xaaa
#define NIDK_signifblank                                 0xaac
#define NIDK_ellipsis                                    0xaae
#define NIDK_doubbaselinedot                             0xaaf
#define NIDK_onethird                                    0xab0
#define NIDK_twothirds                                   0xab1
#define NIDK_onefifth                                    0xab2
#define NIDK_twofifths                                   0xab3
#define NIDK_threefifths                                 0xab4
#define NIDK_fourfifths                                  0xab5
#define NIDK_onesixth                                    0xab6
#define NIDK_fivesixths                                  0xab7
#define NIDK_careof                                      0xab8
#define NIDK_figdash                                     0xabb
#define NIDK_leftanglebracket                            0xabc
#define NIDK_decimalpoint                                0xabd
#define NIDK_rightanglebracket                           0xabe
#define NIDK_marker                                      0xabf
#define NIDK_oneeighth                                   0xac3
#define NIDK_threeeighths                                0xac4
#define NIDK_fiveeighths                                 0xac5
#define NIDK_seveneighths                                0xac6
#define NIDK_trademark                                   0xac9
#define NIDK_signaturemark                               0xaca
#define NIDK_trademarkincircle                           0xacb
#define NIDK_leftopentriangle                            0xacc
#define NIDK_rightopentriangle                           0xacd
#define NIDK_emopencircle                                0xace
#define NIDK_emopenrectangle                             0xacf
#define NIDK_leftsinglequotemark                         0xad0
#define NIDK_rightsinglequotemark                        0xad1
#define NIDK_leftdoublequotemark                         0xad2
#define NIDK_rightdoublequotemark                        0xad3
#define NIDK_prescription                                0xad4
#define NIDK_minutes                                     0xad6
#define NIDK_seconds                                     0xad7
#define NIDK_latincross                                  0xad9
#define NIDK_hexagram                                    0xada
#define NIDK_filledrectbullet                            0xadb
#define NIDK_filledlefttribullet                         0xadc
#define NIDK_filledrighttribullet                        0xadd
#define NIDK_emfilledcircle                              0xade
#define NIDK_emfilledrect                                0xadf
#define NIDK_enopencircbullet                            0xae0
#define NIDK_enopensquarebullet                          0xae1
#define NIDK_openrectbullet                              0xae2
#define NIDK_opentribulletup                             0xae3
#define NIDK_opentribulletdown                           0xae4
#define NIDK_openstar                                    0xae5
#define NIDK_enfilledcircbullet                          0xae6
#define NIDK_enfilledsqbullet                            0xae7
#define NIDK_filledtribulletup                           0xae8
#define NIDK_filledtribulletdown                         0xae9
#define NIDK_leftpointer                                 0xaea
#define NIDK_rightpointer                                0xaeb
#define NIDK_club                                        0xaec
#define NIDK_diamond                                     0xaed
#define NIDK_heart                                       0xaee
#define NIDK_maltesecross                                0xaf0
#define NIDK_dagger                                      0xaf1
#define NIDK_doubledagger                                0xaf2
#define NIDK_checkmark                                   0xaf3
#define NIDK_ballotcross                                 0xaf4
#define NIDK_musicalsharp                                0xaf5
#define NIDK_musicalflat                                 0xaf6
#define NIDK_malesymbol                                  0xaf7
#define NIDK_femalesymbol                                0xaf8
#define NIDK_telephone                                   0xaf9
#define NIDK_telephonerecorder                           0xafa
#define NIDK_phonographcopyright                         0xafb
#define NIDK_caret                                       0xafc
#define NIDK_singlelowquotemark                          0xafd
#define NIDK_doublelowquotemark                          0xafe
#define NIDK_cursor                                      0xaff

/*
 *  APL
 *  Byte 3 = b
 */

#define NIDK_leftcaret                                   0xba3
#define NIDK_rightcaret                                  0xba6
#define NIDK_downcaret                                   0xba8
#define NIDK_upcaret                                     0xba9
#define NIDK_overbar                                     0xbc0
#define NIDK_downtack                                    0xbc2
#define NIDK_upshoe                                      0xbc3
#define NIDK_downstile                                   0xbc4
#define NIDK_underbar                                    0xbc6
#define NIDK_jot                                         0xbca
#define NIDK_quad                                        0xbcc
#define NIDK_uptack                                      0xbce
#define NIDK_circle                                      0xbcf
#define NIDK_upstile                                     0xbd3
#define NIDK_downshoe                                    0xbd6
#define NIDK_rightshoe                                   0xbd8
#define NIDK_leftshoe                                    0xbda
#define NIDK_lefttack                                    0xbdc
#define NIDK_righttack                                   0xbfc

/*
 * Hebrew
 * Byte 3 = c
 */

#define NIDK_hebrew_doublelowline                        0xcdf
#define NIDK_hebrew_aleph                                0xce0
#define NIDK_hebrew_bet                                  0xce1
#define NIDK_hebrew_beth                                 0xce1  /* deprecated */
#define NIDK_hebrew_gimel                                0xce2
#define NIDK_hebrew_gimmel                               0xce2  /* deprecated */
#define NIDK_hebrew_dalet                                0xce3
#define NIDK_hebrew_daleth                               0xce3  /* deprecated */
#define NIDK_hebrew_he                                   0xce4
#define NIDK_hebrew_waw                                  0xce5
#define NIDK_hebrew_zain                                 0xce6
#define NIDK_hebrew_zayin                                0xce6  /* deprecated */
#define NIDK_hebrew_chet                                 0xce7
#define NIDK_hebrew_het                                  0xce7  /* deprecated */
#define NIDK_hebrew_tet                                  0xce8
#define NIDK_hebrew_teth                                 0xce8  /* deprecated */
#define NIDK_hebrew_yod                                  0xce9
#define NIDK_hebrew_finalkaph                            0xcea
#define NIDK_hebrew_kaph                                 0xceb
#define NIDK_hebrew_lamed                                0xcec
#define NIDK_hebrew_finalmem                             0xced
#define NIDK_hebrew_mem                                  0xcee
#define NIDK_hebrew_finalnun                             0xcef
#define NIDK_hebrew_nun                                  0xcf0
#define NIDK_hebrew_samech                               0xcf1
#define NIDK_hebrew_samekh                               0xcf1  /* deprecated */
#define NIDK_hebrew_ayin                                 0xcf2
#define NIDK_hebrew_finalpe                              0xcf3
#define NIDK_hebrew_pe                                   0xcf4
#define NIDK_hebrew_finalzade                            0xcf5
#define NIDK_hebrew_finalzadi                            0xcf5  /* deprecated */
#define NIDK_hebrew_zade                                 0xcf6
#define NIDK_hebrew_zadi                                 0xcf6  /* deprecated */
#define NIDK_hebrew_qoph                                 0xcf7
#define NIDK_hebrew_kuf                                  0xcf7  /* deprecated */
#define NIDK_hebrew_resh                                 0xcf8
#define NIDK_hebrew_shin                                 0xcf9
#define NIDK_hebrew_taw                                  0xcfa
#define NIDK_hebrew_taf                                  0xcfa  /* deprecated */
#define NIDK_Hebrew_switch        0xFF7E  /* Alias for mode_switch */

/*
 * Thai
 * Byte 3 = d
 */

#define NIDK_Thai_kokai					0xda1
#define NIDK_Thai_khokhai					0xda2
#define NIDK_Thai_khokhuat				0xda3
#define NIDK_Thai_khokhwai				0xda4
#define NIDK_Thai_khokhon					0xda5
#define NIDK_Thai_khorakhang			        0xda6  
#define NIDK_Thai_ngongu					0xda7  
#define NIDK_Thai_chochan					0xda8  
#define NIDK_Thai_choching				0xda9   
#define NIDK_Thai_chochang				0xdaa  
#define NIDK_Thai_soso					0xdab
#define NIDK_Thai_chochoe					0xdac
#define NIDK_Thai_yoying					0xdad
#define NIDK_Thai_dochada					0xdae
#define NIDK_Thai_topatak					0xdaf
#define NIDK_Thai_thothan					0xdb0
#define NIDK_Thai_thonangmontho			        0xdb1
#define NIDK_Thai_thophuthao			        0xdb2
#define NIDK_Thai_nonen					0xdb3
#define NIDK_Thai_dodek					0xdb4
#define NIDK_Thai_totao					0xdb5
#define NIDK_Thai_thothung				0xdb6
#define NIDK_Thai_thothahan				0xdb7
#define NIDK_Thai_thothong	 			0xdb8
#define NIDK_Thai_nonu					0xdb9
#define NIDK_Thai_bobaimai				0xdba
#define NIDK_Thai_popla					0xdbb
#define NIDK_Thai_phophung				0xdbc
#define NIDK_Thai_fofa					0xdbd
#define NIDK_Thai_phophan					0xdbe
#define NIDK_Thai_fofan					0xdbf
#define NIDK_Thai_phosamphao			        0xdc0
#define NIDK_Thai_moma					0xdc1
#define NIDK_Thai_yoyak					0xdc2
#define NIDK_Thai_rorua					0xdc3
#define NIDK_Thai_ru					0xdc4
#define NIDK_Thai_loling					0xdc5
#define NIDK_Thai_lu					0xdc6
#define NIDK_Thai_wowaen					0xdc7
#define NIDK_Thai_sosala					0xdc8
#define NIDK_Thai_sorusi					0xdc9
#define NIDK_Thai_sosua					0xdca
#define NIDK_Thai_hohip					0xdcb
#define NIDK_Thai_lochula					0xdcc
#define NIDK_Thai_oang					0xdcd
#define NIDK_Thai_honokhuk				0xdce
#define NIDK_Thai_paiyannoi				0xdcf
#define NIDK_Thai_saraa					0xdd0
#define NIDK_Thai_maihanakat				0xdd1
#define NIDK_Thai_saraaa					0xdd2
#define NIDK_Thai_saraam					0xdd3
#define NIDK_Thai_sarai					0xdd4   
#define NIDK_Thai_saraii					0xdd5   
#define NIDK_Thai_saraue					0xdd6    
#define NIDK_Thai_sarauee					0xdd7    
#define NIDK_Thai_sarau					0xdd8    
#define NIDK_Thai_sarauu					0xdd9   
#define NIDK_Thai_phinthu					0xdda
#define NIDK_Thai_maihanakat_maitho   			0xdde
#define NIDK_Thai_baht					0xddf
#define NIDK_Thai_sarae					0xde0    
#define NIDK_Thai_saraae					0xde1
#define NIDK_Thai_sarao					0xde2
#define NIDK_Thai_saraaimaimuan				0xde3   
#define NIDK_Thai_saraaimaimalai				0xde4  
#define NIDK_Thai_lakkhangyao				0xde5
#define NIDK_Thai_maiyamok				0xde6
#define NIDK_Thai_maitaikhu				0xde7
#define NIDK_Thai_maiek					0xde8   
#define NIDK_Thai_maitho					0xde9
#define NIDK_Thai_maitri					0xdea
#define NIDK_Thai_maichattawa				0xdeb
#define NIDK_Thai_thanthakhat				0xdec
#define NIDK_Thai_nikhahit				0xded
#define NIDK_Thai_leksun					0xdf0 
#define NIDK_Thai_leknung					0xdf1  
#define NIDK_Thai_leksong					0xdf2 
#define NIDK_Thai_leksam					0xdf3
#define NIDK_Thai_leksi					0xdf4  
#define NIDK_Thai_lekha					0xdf5  
#define NIDK_Thai_lekhok					0xdf6  
#define NIDK_Thai_lekchet					0xdf7  
#define NIDK_Thai_lekpaet					0xdf8  
#define NIDK_Thai_lekkao					0xdf9 

/*
 *   Korean
 *   Byte 3 = e
 */

#define NIDK_Hangul		0xff31    /* Hangul start/stop(toggle) */
#define NIDK_Hangul_Start		0xff32    /* Hangul start */
#define NIDK_Hangul_End		0xff33    /* Hangul end, English start */
#define NIDK_Hangul_Hanja		0xff34    /* Start Hangul->Hanja Conversion */
#define NIDK_Hangul_Jamo		0xff35    /* Hangul Jamo mode */
#define NIDK_Hangul_Romaja	0xff36    /* Hangul Romaja mode */
#define NIDK_Hangul_Codeinput	0xff37    /* Hangul code input mode */
#define NIDK_Hangul_Jeonja	0xff38    /* Jeonja mode */
#define NIDK_Hangul_Banja		0xff39    /* Banja mode */
#define NIDK_Hangul_PreHanja	0xff3a    /* Pre Hanja conversion */
#define NIDK_Hangul_PostHanja	0xff3b    /* Post Hanja conversion */
#define NIDK_Hangul_SingleCandidate	0xff3c    /* Single candidate */
#define NIDK_Hangul_MultipleCandidate	0xff3d    /* Multiple candidate */
#define NIDK_Hangul_PreviousCandidate	0xff3e    /* Previous candidate */
#define NIDK_Hangul_Special	0xff3f    /* Special symbols */
#define NIDK_Hangul_switch	0xFF7E    /* Alias for mode_switch */

/* Hangul Consonant Characters */
#define NIDK_Hangul_Kiyeog				0xea1
#define NIDK_Hangul_SsangKiyeog				0xea2
#define NIDK_Hangul_KiyeogSios				0xea3
#define NIDK_Hangul_Nieun					0xea4
#define NIDK_Hangul_NieunJieuj				0xea5
#define NIDK_Hangul_NieunHieuh				0xea6
#define NIDK_Hangul_Dikeud				0xea7
#define NIDK_Hangul_SsangDikeud				0xea8
#define NIDK_Hangul_Rieul					0xea9
#define NIDK_Hangul_RieulKiyeog				0xeaa
#define NIDK_Hangul_RieulMieum				0xeab
#define NIDK_Hangul_RieulPieub				0xeac
#define NIDK_Hangul_RieulSios				0xead
#define NIDK_Hangul_RieulTieut				0xeae
#define NIDK_Hangul_RieulPhieuf				0xeaf
#define NIDK_Hangul_RieulHieuh				0xeb0
#define NIDK_Hangul_Mieum					0xeb1
#define NIDK_Hangul_Pieub					0xeb2
#define NIDK_Hangul_SsangPieub				0xeb3
#define NIDK_Hangul_PieubSios				0xeb4
#define NIDK_Hangul_Sios					0xeb5
#define NIDK_Hangul_SsangSios				0xeb6
#define NIDK_Hangul_Ieung					0xeb7
#define NIDK_Hangul_Jieuj					0xeb8
#define NIDK_Hangul_SsangJieuj				0xeb9
#define NIDK_Hangul_Cieuc					0xeba
#define NIDK_Hangul_Khieuq				0xebb
#define NIDK_Hangul_Tieut					0xebc
#define NIDK_Hangul_Phieuf				0xebd
#define NIDK_Hangul_Hieuh					0xebe

/* Hangul Vowel Characters */
#define NIDK_Hangul_A					0xebf
#define NIDK_Hangul_AE					0xec0
#define NIDK_Hangul_YA					0xec1
#define NIDK_Hangul_YAE					0xec2
#define NIDK_Hangul_EO					0xec3
#define NIDK_Hangul_E					0xec4
#define NIDK_Hangul_YEO					0xec5
#define NIDK_Hangul_YE					0xec6
#define NIDK_Hangul_O					0xec7
#define NIDK_Hangul_WA					0xec8
#define NIDK_Hangul_WAE					0xec9
#define NIDK_Hangul_OE					0xeca
#define NIDK_Hangul_YO					0xecb
#define NIDK_Hangul_U					0xecc
#define NIDK_Hangul_WEO					0xecd
#define NIDK_Hangul_WE					0xece
#define NIDK_Hangul_WI					0xecf
#define NIDK_Hangul_YU					0xed0
#define NIDK_Hangul_EU					0xed1
#define NIDK_Hangul_YI					0xed2
#define NIDK_Hangul_I					0xed3

/* Hangul syllable-final (JongSeong) Characters */
#define NIDK_Hangul_J_Kiyeog				0xed4
#define NIDK_Hangul_J_SsangKiyeog				0xed5
#define NIDK_Hangul_J_KiyeogSios				0xed6
#define NIDK_Hangul_J_Nieun				0xed7
#define NIDK_Hangul_J_NieunJieuj				0xed8
#define NIDK_Hangul_J_NieunHieuh				0xed9
#define NIDK_Hangul_J_Dikeud				0xeda
#define NIDK_Hangul_J_Rieul				0xedb
#define NIDK_Hangul_J_RieulKiyeog				0xedc
#define NIDK_Hangul_J_RieulMieum				0xedd
#define NIDK_Hangul_J_RieulPieub				0xede
#define NIDK_Hangul_J_RieulSios				0xedf
#define NIDK_Hangul_J_RieulTieut				0xee0
#define NIDK_Hangul_J_RieulPhieuf				0xee1
#define NIDK_Hangul_J_RieulHieuh				0xee2
#define NIDK_Hangul_J_Mieum				0xee3
#define NIDK_Hangul_J_Pieub				0xee4
#define NIDK_Hangul_J_PieubSios				0xee5
#define NIDK_Hangul_J_Sios				0xee6
#define NIDK_Hangul_J_SsangSios				0xee7
#define NIDK_Hangul_J_Ieung				0xee8
#define NIDK_Hangul_J_Jieuj				0xee9
#define NIDK_Hangul_J_Cieuc				0xeea
#define NIDK_Hangul_J_Khieuq				0xeeb
#define NIDK_Hangul_J_Tieut				0xeec
#define NIDK_Hangul_J_Phieuf				0xeed
#define NIDK_Hangul_J_Hieuh				0xeee

/* Ancient Hangul Consonant Characters */
#define NIDK_Hangul_RieulYeorinHieuh			0xeef
#define NIDK_Hangul_SunkyeongeumMieum			0xef0
#define NIDK_Hangul_SunkyeongeumPieub			0xef1
#define NIDK_Hangul_PanSios				0xef2
#define NIDK_Hangul_KkogjiDalrinIeung			0xef3
#define NIDK_Hangul_SunkyeongeumPhieuf			0xef4
#define NIDK_Hangul_YeorinHieuh				0xef5

/* Ancient Hangul Vowel Characters */
#define NIDK_Hangul_AraeA					0xef6
#define NIDK_Hangul_AraeAE				0xef7

/* Ancient Hangul syllable-final (JongSeong) Characters */
#define NIDK_Hangul_J_PanSios				0xef8
#define NIDK_Hangul_J_KkogjiDalrinIeung			0xef9
#define NIDK_Hangul_J_YeorinHieuh				0xefa

/* Korean currency symbol */
#define NIDK_Korean_Won					0xeff

#endif /* NID_KEYSYM_H */
