/* Based on the xkey.c hack - open up a display and retrieve mouse/keyboard
   events - this should not interfere with any running programs... */

 /* xkey.c was written by: */
/*
 *    Dominic Giampaolo (nick@cs.maxine.wpi.edu)
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

#include <ve_debug.h>
#include <ve_device.h>
#include <ve_driver.h>
#include <ve_keysym.h>

#define MODULE "driver:x11"

struct driver_x11_private {
  Display *dpy;
};

/* Translation table for ve_keysyms */
/* This is 64k but it's static so it doesn't change over time... */
static int *trans_table = NULL;
static int trans_table_ready = 0;
static void init_trans_table() {
  if (!trans_table)
    trans_table = calloc(65536,sizeof(int));
  trans_table[XK_BackSpace] = VEK_BackSpace;
  trans_table[XK_Tab] = VEK_Tab;
  trans_table[XK_Linefeed] = VEK_Linefeed;
  trans_table[XK_Clear] = VEK_Clear;
  trans_table[XK_Return] = VEK_Return;
  trans_table[XK_Pause] = VEK_Pause;
  trans_table[XK_Scroll_Lock] = VEK_Scroll_Lock;
  trans_table[XK_Sys_Req] = VEK_Sys_Req;
  trans_table[XK_Escape] = VEK_Escape;
  trans_table[XK_Delete] = VEK_Delete;
  trans_table[XK_Multi_key] = VEK_Multi_key;
  trans_table[XK_SingleCandidate] = VEK_SingleCandidate;
  trans_table[XK_MultipleCandidate] = VEK_MultipleCandidate;
  trans_table[XK_PreviousCandidate] = VEK_PreviousCandidate;
  trans_table[XK_Kanji] = VEK_Kanji;
  trans_table[XK_Muhenkan] = VEK_Muhenkan;
  trans_table[XK_Henkan_Mode] = VEK_Henkan_Mode;
  trans_table[XK_Henkan] = VEK_Henkan;
  trans_table[XK_Romaji] = VEK_Romaji;
  trans_table[XK_Hiragana] = VEK_Hiragana;
  trans_table[XK_Katakana] = VEK_Katakana;
  trans_table[XK_Hiragana_Katakana] = VEK_Hiragana_Katakana;
  trans_table[XK_Zenkaku] = VEK_Zenkaku;
  trans_table[XK_Hankaku] = VEK_Hankaku;
  trans_table[XK_Zenkaku_Hankaku] = VEK_Zenkaku_Hankaku;
  trans_table[XK_Touroku] = VEK_Touroku;
  trans_table[XK_Massyo] = VEK_Massyo;
  trans_table[XK_Kana_Lock] = VEK_Kana_Lock;
  trans_table[XK_Kana_Shift] = VEK_Kana_Shift;
  trans_table[XK_Eisu_Shift] = VEK_Eisu_Shift;
  trans_table[XK_Eisu_toggle] = VEK_Eisu_toggle;
  trans_table[XK_Zen_Koho] = VEK_Zen_Koho;
  trans_table[XK_Mae_Koho] = VEK_Mae_Koho;
  trans_table[XK_Home] = VEK_Home;
  trans_table[XK_Left] = VEK_Left;
  trans_table[XK_Up] = VEK_Up;
  trans_table[XK_Right] = VEK_Right;
  trans_table[XK_Down] = VEK_Down;
  trans_table[XK_Prior] = VEK_Prior;
  trans_table[XK_Page_Up] = VEK_Page_Up;
  trans_table[XK_Next] = VEK_Next;
  trans_table[XK_Page_Down] = VEK_Page_Down;
  trans_table[XK_End] = VEK_End;
  trans_table[XK_Begin] = VEK_Begin;
  trans_table[XK_Select] = VEK_Select;
  trans_table[XK_Print] = VEK_Print;
  trans_table[XK_Execute] = VEK_Execute;
  trans_table[XK_Insert] = VEK_Insert;
  trans_table[XK_Undo] = VEK_Undo;
  trans_table[XK_Redo] = VEK_Redo;
  trans_table[XK_Menu] = VEK_Menu;
  trans_table[XK_Find] = VEK_Find;
  trans_table[XK_Cancel] = VEK_Cancel;
  trans_table[XK_Help] = VEK_Help;
  trans_table[XK_Break] = VEK_Break;
  trans_table[XK_Mode_switch] = VEK_Mode_switch;
  trans_table[XK_script_switch] = VEK_script_switch;
  trans_table[XK_Num_Lock] = VEK_Num_Lock;
  trans_table[XK_KP_Space] = VEK_KP_Space;
  trans_table[XK_KP_Tab] = VEK_KP_Tab;
  trans_table[XK_KP_Enter] = VEK_KP_Enter;
  trans_table[XK_KP_F1] = VEK_KP_F1;
  trans_table[XK_KP_F2] = VEK_KP_F2;
  trans_table[XK_KP_F3] = VEK_KP_F3;
  trans_table[XK_KP_F4] = VEK_KP_F4;
  trans_table[XK_KP_Home] = VEK_KP_Home;
  trans_table[XK_KP_Left] = VEK_KP_Left;
  trans_table[XK_KP_Up] = VEK_KP_Up;
  trans_table[XK_KP_Right] = VEK_KP_Right;
  trans_table[XK_KP_Down] = VEK_KP_Down;
  trans_table[XK_KP_Prior] = VEK_KP_Prior;
  trans_table[XK_KP_Page_Up] = VEK_KP_Page_Up;
  trans_table[XK_KP_Next] = VEK_KP_Next;
  trans_table[XK_KP_Page_Down] = VEK_KP_Page_Down;
  trans_table[XK_KP_End] = VEK_KP_End;
  trans_table[XK_KP_Begin] = VEK_KP_Begin;
  trans_table[XK_KP_Insert] = VEK_KP_Insert;
  trans_table[XK_KP_Delete] = VEK_KP_Delete;
  trans_table[XK_KP_Equal] = VEK_KP_Equal;
  trans_table[XK_KP_Multiply] = VEK_KP_Multiply;
  trans_table[XK_KP_Add] = VEK_KP_Add;
  trans_table[XK_KP_Separator] = VEK_KP_Separator;
  trans_table[XK_KP_Subtract] = VEK_KP_Subtract;
  trans_table[XK_KP_Decimal] = VEK_KP_Decimal;
  trans_table[XK_KP_Divide] = VEK_KP_Divide;
  trans_table[XK_KP_0] = VEK_KP_0;
  trans_table[XK_KP_1] = VEK_KP_1;
  trans_table[XK_KP_2] = VEK_KP_2;
  trans_table[XK_KP_3] = VEK_KP_3;
  trans_table[XK_KP_4] = VEK_KP_4;
  trans_table[XK_KP_5] = VEK_KP_5;
  trans_table[XK_KP_6] = VEK_KP_6;
  trans_table[XK_KP_7] = VEK_KP_7;
  trans_table[XK_KP_8] = VEK_KP_8;
  trans_table[XK_KP_9] = VEK_KP_9;
  trans_table[XK_F1] = VEK_F1;
  trans_table[XK_F2] = VEK_F2;
  trans_table[XK_F3] = VEK_F3;
  trans_table[XK_F4] = VEK_F4;
  trans_table[XK_F5] = VEK_F5;
  trans_table[XK_F6] = VEK_F6;
  trans_table[XK_F7] = VEK_F7;
  trans_table[XK_F8] = VEK_F8;
  trans_table[XK_F9] = VEK_F9;
  trans_table[XK_F10] = VEK_F10;
  trans_table[XK_F11] = VEK_F11;
  trans_table[XK_L1] = VEK_L1;
  trans_table[XK_F12] = VEK_F12;
  trans_table[XK_L2] = VEK_L2;
  trans_table[XK_F13] = VEK_F13;
  trans_table[XK_L3] = VEK_L3;
  trans_table[XK_F14] = VEK_F14;
  trans_table[XK_L4] = VEK_L4;
  trans_table[XK_F15] = VEK_F15;
  trans_table[XK_L5] = VEK_L5;
  trans_table[XK_F16] = VEK_F16;
  trans_table[XK_L6] = VEK_L6;
  trans_table[XK_F17] = VEK_F17;
  trans_table[XK_L7] = VEK_L7;
  trans_table[XK_F18] = VEK_F18;
  trans_table[XK_L8] = VEK_L8;
  trans_table[XK_F19] = VEK_F19;
  trans_table[XK_L9] = VEK_L9;
  trans_table[XK_F20] = VEK_F20;
  trans_table[XK_L10] = VEK_L10;
  trans_table[XK_F21] = VEK_F21;
  trans_table[XK_R1] = VEK_R1;
  trans_table[XK_F22] = VEK_F22;
  trans_table[XK_R2] = VEK_R2;
  trans_table[XK_F23] = VEK_F23;
  trans_table[XK_R3] = VEK_R3;
  trans_table[XK_F24] = VEK_F24;
  trans_table[XK_R4] = VEK_R4;
  trans_table[XK_F25] = VEK_F25;
  trans_table[XK_R5] = VEK_R5;
  trans_table[XK_F26] = VEK_F26;
  trans_table[XK_R6] = VEK_R6;
  trans_table[XK_F27] = VEK_F27;
  trans_table[XK_R7] = VEK_R7;
  trans_table[XK_F28] = VEK_F28;
  trans_table[XK_R8] = VEK_R8;
  trans_table[XK_F29] = VEK_F29;
  trans_table[XK_R9] = VEK_R9;
  trans_table[XK_F30] = VEK_F30;
  trans_table[XK_R10] = VEK_R10;
  trans_table[XK_F31] = VEK_F31;
  trans_table[XK_R11] = VEK_R11;
  trans_table[XK_F32] = VEK_F32;
  trans_table[XK_R12] = VEK_R12;
  trans_table[XK_F33] = VEK_F33;
  trans_table[XK_R13] = VEK_R13;
  trans_table[XK_F34] = VEK_F34;
  trans_table[XK_R14] = VEK_R14;
  trans_table[XK_F35] = VEK_F35;
  trans_table[XK_R15] = VEK_R15;
  trans_table[XK_Shift_L] = VEK_Shift_L;
  trans_table[XK_Shift_R] = VEK_Shift_R;
  trans_table[XK_Control_L] = VEK_Control_L;
  trans_table[XK_Control_R] = VEK_Control_R;
  trans_table[XK_Caps_Lock] = VEK_Caps_Lock;
  trans_table[XK_Shift_Lock] = VEK_Shift_Lock;
  trans_table[XK_Meta_L] = VEK_Meta_L;
  trans_table[XK_Meta_R] = VEK_Meta_R;
  trans_table[XK_Alt_L] = VEK_Alt_L;
  trans_table[XK_Alt_R] = VEK_Alt_R;
  trans_table[XK_Super_L] = VEK_Super_L;
  trans_table[XK_Super_R] = VEK_Super_R;
  trans_table[XK_Hyper_L] = VEK_Hyper_L;
  trans_table[XK_Hyper_R] = VEK_Hyper_R;
  trans_table[XK_ISO_Lock] = VEK_ISO_Lock;
  trans_table[XK_ISO_Level2_Latch] = VEK_ISO_Level2_Latch;
  trans_table[XK_ISO_Level3_Shift] = VEK_ISO_Level3_Shift;
  trans_table[XK_ISO_Level3_Latch] = VEK_ISO_Level3_Latch;
  trans_table[XK_ISO_Level3_Lock] = VEK_ISO_Level3_Lock;
  trans_table[XK_ISO_Group_Shift] = VEK_ISO_Group_Shift;
  trans_table[XK_ISO_Group_Latch] = VEK_ISO_Group_Latch;
  trans_table[XK_ISO_Group_Lock] = VEK_ISO_Group_Lock;
  trans_table[XK_ISO_Next_Group] = VEK_ISO_Next_Group;
  trans_table[XK_ISO_Next_Group_Lock] = VEK_ISO_Next_Group_Lock;
  trans_table[XK_ISO_Prev_Group] = VEK_ISO_Prev_Group;
  trans_table[XK_ISO_Prev_Group_Lock] = VEK_ISO_Prev_Group_Lock;
  trans_table[XK_ISO_First_Group] = VEK_ISO_First_Group;
  trans_table[XK_ISO_First_Group_Lock] = VEK_ISO_First_Group_Lock;
  trans_table[XK_ISO_Last_Group] = VEK_ISO_Last_Group;
  trans_table[XK_ISO_Last_Group_Lock] = VEK_ISO_Last_Group_Lock;
  trans_table[XK_ISO_Left_Tab] = VEK_ISO_Left_Tab;
  trans_table[XK_ISO_Move_Line_Up] = VEK_ISO_Move_Line_Up;
  trans_table[XK_ISO_Move_Line_Down] = VEK_ISO_Move_Line_Down;
  trans_table[XK_ISO_Partial_Line_Up] = VEK_ISO_Partial_Line_Up;
  trans_table[XK_ISO_Partial_Line_Down] = VEK_ISO_Partial_Line_Down;
  trans_table[XK_ISO_Partial_Space_Left] = VEK_ISO_Partial_Space_Left;
  trans_table[XK_ISO_Partial_Space_Right] = VEK_ISO_Partial_Space_Right;
  trans_table[XK_ISO_Set_Margin_Left] = VEK_ISO_Set_Margin_Left;
  trans_table[XK_ISO_Set_Margin_Right] = VEK_ISO_Set_Margin_Right;
  trans_table[XK_ISO_Release_Margin_Left] = VEK_ISO_Release_Margin_Left;
  trans_table[XK_ISO_Release_Margin_Right] = VEK_ISO_Release_Margin_Right;
  trans_table[XK_ISO_Release_Both_Margins] = VEK_ISO_Release_Both_Margins;
  trans_table[XK_ISO_Fast_Cursor_Left] = VEK_ISO_Fast_Cursor_Left;
  trans_table[XK_ISO_Fast_Cursor_Right] = VEK_ISO_Fast_Cursor_Right;
  trans_table[XK_ISO_Fast_Cursor_Up] = VEK_ISO_Fast_Cursor_Up;
  trans_table[XK_ISO_Fast_Cursor_Down] = VEK_ISO_Fast_Cursor_Down;
  trans_table[XK_ISO_Continuous_Underline] = VEK_ISO_Continuous_Underline;
  trans_table[XK_ISO_Discontinuous_Underline] = VEK_ISO_Discontinuous_Underline;
  trans_table[XK_ISO_Emphasize] = VEK_ISO_Emphasize;
  trans_table[XK_ISO_Center_Object] = VEK_ISO_Center_Object;
  trans_table[XK_ISO_Enter] = VEK_ISO_Enter;
  trans_table[XK_dead_grave] = VEK_dead_grave;
  trans_table[XK_dead_acute] = VEK_dead_acute;
  trans_table[XK_dead_circumflex] = VEK_dead_circumflex;
  trans_table[XK_dead_tilde] = VEK_dead_tilde;
  trans_table[XK_dead_macron] = VEK_dead_macron;
  trans_table[XK_dead_breve] = VEK_dead_breve;
  trans_table[XK_dead_abovedot] = VEK_dead_abovedot;
  trans_table[XK_dead_diaeresis] = VEK_dead_diaeresis;
  trans_table[XK_dead_abovering] = VEK_dead_abovering;
  trans_table[XK_dead_doubleacute] = VEK_dead_doubleacute;
  trans_table[XK_dead_caron] = VEK_dead_caron;
  trans_table[XK_dead_cedilla] = VEK_dead_cedilla;
  trans_table[XK_dead_ogonek] = VEK_dead_ogonek;
  trans_table[XK_dead_iota] = VEK_dead_iota;
  trans_table[XK_dead_belowdot] = VEK_dead_belowdot;
  trans_table[XK_First_Virtual_Screen] = VEK_First_Virtual_Screen;
  trans_table[XK_Prev_Virtual_Screen] = VEK_Prev_Virtual_Screen;
  trans_table[XK_Next_Virtual_Screen] = VEK_Next_Virtual_Screen;
  trans_table[XK_Last_Virtual_Screen] = VEK_Last_Virtual_Screen;
  trans_table[XK_Terminate_Server] = VEK_Terminate_Server;
  trans_table[XK_AccessX_Enable] = VEK_AccessX_Enable;
  trans_table[XK_AccessX_Feedback_Enable] = VEK_AccessX_Feedback_Enable;
  trans_table[XK_RepeatKeys_Enable] = VEK_RepeatKeys_Enable;
  trans_table[XK_SlowKeys_Enable] = VEK_SlowKeys_Enable;
  trans_table[XK_BounceKeys_Enable] = VEK_BounceKeys_Enable;
  trans_table[XK_StickyKeys_Enable] = VEK_StickyKeys_Enable;
  trans_table[XK_MouseKeys_Enable] = VEK_MouseKeys_Enable;
  trans_table[XK_MouseKeys_Accel_Enable] = VEK_MouseKeys_Accel_Enable;
  trans_table[XK_Overlay1_Enable] = VEK_Overlay1_Enable;
  trans_table[XK_Overlay2_Enable] = VEK_Overlay2_Enable;
  trans_table[XK_AudibleBell_Enable] = VEK_AudibleBell_Enable;
  trans_table[XK_Pointer_Left] = VEK_Pointer_Left;
  trans_table[XK_Pointer_Right] = VEK_Pointer_Right;
  trans_table[XK_Pointer_Up] = VEK_Pointer_Up;
  trans_table[XK_Pointer_Down] = VEK_Pointer_Down;
  trans_table[XK_Pointer_UpLeft] = VEK_Pointer_UpLeft;
  trans_table[XK_Pointer_UpRight] = VEK_Pointer_UpRight;
  trans_table[XK_Pointer_DownLeft] = VEK_Pointer_DownLeft;
  trans_table[XK_Pointer_DownRight] = VEK_Pointer_DownRight;
  trans_table[XK_Pointer_Button_Dflt] = VEK_Pointer_Button_Dflt;
  trans_table[XK_Pointer_Button1] = VEK_Pointer_Button1;
  trans_table[XK_Pointer_Button2] = VEK_Pointer_Button2;
  trans_table[XK_Pointer_Button3] = VEK_Pointer_Button3;
  trans_table[XK_Pointer_Button4] = VEK_Pointer_Button4;
  trans_table[XK_Pointer_Button5] = VEK_Pointer_Button5;
  trans_table[XK_Pointer_DblClick_Dflt] = VEK_Pointer_DblClick_Dflt;
  trans_table[XK_Pointer_DblClick1] = VEK_Pointer_DblClick1;
  trans_table[XK_Pointer_DblClick2] = VEK_Pointer_DblClick2;
  trans_table[XK_Pointer_DblClick3] = VEK_Pointer_DblClick3;
  trans_table[XK_Pointer_DblClick4] = VEK_Pointer_DblClick4;
  trans_table[XK_Pointer_DblClick5] = VEK_Pointer_DblClick5;
  trans_table[XK_Pointer_Drag_Dflt] = VEK_Pointer_Drag_Dflt;
  trans_table[XK_Pointer_Drag1] = VEK_Pointer_Drag1;
  trans_table[XK_Pointer_Drag2] = VEK_Pointer_Drag2;
  trans_table[XK_Pointer_Drag3] = VEK_Pointer_Drag3;
  trans_table[XK_Pointer_Drag4] = VEK_Pointer_Drag4;
  trans_table[XK_Pointer_Drag5] = VEK_Pointer_Drag5;
  trans_table[XK_Pointer_EnableKeys] = VEK_Pointer_EnableKeys;
  trans_table[XK_Pointer_Accelerate] = VEK_Pointer_Accelerate;
  trans_table[XK_Pointer_DfltBtnNext] = VEK_Pointer_DfltBtnNext;
  trans_table[XK_Pointer_DfltBtnPrev] = VEK_Pointer_DfltBtnPrev;
  trans_table[XK_space] = VEK_space;
  trans_table[XK_exclam] = VEK_exclam;
  trans_table[XK_quotedbl] = VEK_quotedbl;
  trans_table[XK_numbersign] = VEK_numbersign;
  trans_table[XK_dollar] = VEK_dollar;
  trans_table[XK_percent] = VEK_percent;
  trans_table[XK_ampersand] = VEK_ampersand;
  trans_table[XK_apostrophe] = VEK_apostrophe;
  trans_table[XK_quoteright] = VEK_quoteright;
  trans_table[XK_parenleft] = VEK_parenleft;
  trans_table[XK_parenright] = VEK_parenright;
  trans_table[XK_asterisk] = VEK_asterisk;
  trans_table[XK_plus] = VEK_plus;
  trans_table[XK_comma] = VEK_comma;
  trans_table[XK_minus] = VEK_minus;
  trans_table[XK_period] = VEK_period;
  trans_table[XK_slash] = VEK_slash;
  trans_table[XK_0] = VEK_0;
  trans_table[XK_1] = VEK_1;
  trans_table[XK_2] = VEK_2;
  trans_table[XK_3] = VEK_3;
  trans_table[XK_4] = VEK_4;
  trans_table[XK_5] = VEK_5;
  trans_table[XK_6] = VEK_6;
  trans_table[XK_7] = VEK_7;
  trans_table[XK_8] = VEK_8;
  trans_table[XK_9] = VEK_9;
  trans_table[XK_colon] = VEK_colon;
  trans_table[XK_semicolon] = VEK_semicolon;
  trans_table[XK_less] = VEK_less;
  trans_table[XK_equal] = VEK_equal;
  trans_table[XK_greater] = VEK_greater;
  trans_table[XK_question] = VEK_question;
  trans_table[XK_at] = VEK_at;
  trans_table[XK_A] = VEK_A;
  trans_table[XK_B] = VEK_B;
  trans_table[XK_C] = VEK_C;
  trans_table[XK_D] = VEK_D;
  trans_table[XK_E] = VEK_E;
  trans_table[XK_F] = VEK_F;
  trans_table[XK_G] = VEK_G;
  trans_table[XK_H] = VEK_H;
  trans_table[XK_I] = VEK_I;
  trans_table[XK_J] = VEK_J;
  trans_table[XK_K] = VEK_K;
  trans_table[XK_L] = VEK_L;
  trans_table[XK_M] = VEK_M;
  trans_table[XK_N] = VEK_N;
  trans_table[XK_O] = VEK_O;
  trans_table[XK_P] = VEK_P;
  trans_table[XK_Q] = VEK_Q;
  trans_table[XK_R] = VEK_R;
  trans_table[XK_S] = VEK_S;
  trans_table[XK_T] = VEK_T;
  trans_table[XK_U] = VEK_U;
  trans_table[XK_V] = VEK_V;
  trans_table[XK_W] = VEK_W;
  trans_table[XK_X] = VEK_X;
  trans_table[XK_Y] = VEK_Y;
  trans_table[XK_Z] = VEK_Z;
  trans_table[XK_bracketleft] = VEK_bracketleft;
  trans_table[XK_backslash] = VEK_backslash;
  trans_table[XK_bracketright] = VEK_bracketright;
  trans_table[XK_asciicircum] = VEK_asciicircum;
  trans_table[XK_underscore] = VEK_underscore;
  trans_table[XK_grave] = VEK_grave;
  trans_table[XK_quoteleft] = VEK_quoteleft;
  trans_table[XK_a] = VEK_a;
  trans_table[XK_b] = VEK_b;
  trans_table[XK_c] = VEK_c;
  trans_table[XK_d] = VEK_d;
  trans_table[XK_e] = VEK_e;
  trans_table[XK_f] = VEK_f;
  trans_table[XK_g] = VEK_g;
  trans_table[XK_h] = VEK_h;
  trans_table[XK_i] = VEK_i;
  trans_table[XK_j] = VEK_j;
  trans_table[XK_k] = VEK_k;
  trans_table[XK_l] = VEK_l;
  trans_table[XK_m] = VEK_m;
  trans_table[XK_n] = VEK_n;
  trans_table[XK_o] = VEK_o;
  trans_table[XK_p] = VEK_p;
  trans_table[XK_q] = VEK_q;
  trans_table[XK_r] = VEK_r;
  trans_table[XK_s] = VEK_s;
  trans_table[XK_t] = VEK_t;
  trans_table[XK_u] = VEK_u;
  trans_table[XK_v] = VEK_v;
  trans_table[XK_w] = VEK_w;
  trans_table[XK_x] = VEK_x;
  trans_table[XK_y] = VEK_y;
  trans_table[XK_z] = VEK_z;
  trans_table[XK_braceleft] = VEK_braceleft;
  trans_table[XK_bar] = VEK_bar;
  trans_table[XK_braceright] = VEK_braceright;
  trans_table[XK_asciitilde] = VEK_asciitilde;
  trans_table[XK_nobreakspace] = VEK_nobreakspace;
  trans_table[XK_exclamdown] = VEK_exclamdown;
  trans_table[XK_cent] = VEK_cent;
  trans_table[XK_sterling] = VEK_sterling;
  trans_table[XK_currency] = VEK_currency;
  trans_table[XK_yen] = VEK_yen;
  trans_table[XK_brokenbar] = VEK_brokenbar;
  trans_table[XK_section] = VEK_section;
  trans_table[XK_diaeresis] = VEK_diaeresis;
  trans_table[XK_copyright] = VEK_copyright;
  trans_table[XK_ordfeminine] = VEK_ordfeminine;
  trans_table[XK_guillemotleft] = VEK_guillemotleft;
  trans_table[XK_notsign] = VEK_notsign;
  trans_table[XK_hyphen] = VEK_hyphen;
  trans_table[XK_registered] = VEK_registered;
  trans_table[XK_macron] = VEK_macron;
  trans_table[XK_degree] = VEK_degree;
  trans_table[XK_plusminus] = VEK_plusminus;
  trans_table[XK_twosuperior] = VEK_twosuperior;
  trans_table[XK_threesuperior] = VEK_threesuperior;
  trans_table[XK_acute] = VEK_acute;
  trans_table[XK_mu] = VEK_mu;
  trans_table[XK_paragraph] = VEK_paragraph;
  trans_table[XK_periodcentered] = VEK_periodcentered;
  trans_table[XK_cedilla] = VEK_cedilla;
  trans_table[XK_onesuperior] = VEK_onesuperior;
  trans_table[XK_masculine] = VEK_masculine;
  trans_table[XK_guillemotright] = VEK_guillemotright;
  trans_table[XK_onequarter] = VEK_onequarter;
  trans_table[XK_onehalf] = VEK_onehalf;
  trans_table[XK_threequarters] = VEK_threequarters;
  trans_table[XK_questiondown] = VEK_questiondown;
  trans_table[XK_Agrave] = VEK_Agrave;
  trans_table[XK_Aacute] = VEK_Aacute;
  trans_table[XK_Acircumflex] = VEK_Acircumflex;
  trans_table[XK_Atilde] = VEK_Atilde;
  trans_table[XK_Adiaeresis] = VEK_Adiaeresis;
  trans_table[XK_Aring] = VEK_Aring;
  trans_table[XK_AE] = VEK_AE;
  trans_table[XK_Ccedilla] = VEK_Ccedilla;
  trans_table[XK_Egrave] = VEK_Egrave;
  trans_table[XK_Eacute] = VEK_Eacute;
  trans_table[XK_Ecircumflex] = VEK_Ecircumflex;
  trans_table[XK_Ediaeresis] = VEK_Ediaeresis;
  trans_table[XK_Igrave] = VEK_Igrave;
  trans_table[XK_Iacute] = VEK_Iacute;
  trans_table[XK_Icircumflex] = VEK_Icircumflex;
  trans_table[XK_Idiaeresis] = VEK_Idiaeresis;
  trans_table[XK_ETH] = VEK_ETH;
  trans_table[XK_Eth] = VEK_Eth;
  trans_table[XK_Ntilde] = VEK_Ntilde;
  trans_table[XK_Ograve] = VEK_Ograve;
  trans_table[XK_Oacute] = VEK_Oacute;
  trans_table[XK_Ocircumflex] = VEK_Ocircumflex;
  trans_table[XK_Otilde] = VEK_Otilde;
  trans_table[XK_Odiaeresis] = VEK_Odiaeresis;
  trans_table[XK_multiply] = VEK_multiply;
  trans_table[XK_Ooblique] = VEK_Ooblique;
  trans_table[XK_Ugrave] = VEK_Ugrave;
  trans_table[XK_Uacute] = VEK_Uacute;
  trans_table[XK_Ucircumflex] = VEK_Ucircumflex;
  trans_table[XK_Udiaeresis] = VEK_Udiaeresis;
  trans_table[XK_Yacute] = VEK_Yacute;
  trans_table[XK_THORN] = VEK_THORN;
  trans_table[XK_Thorn] = VEK_Thorn;
  trans_table[XK_ssharp] = VEK_ssharp;
  trans_table[XK_agrave] = VEK_agrave;
  trans_table[XK_aacute] = VEK_aacute;
  trans_table[XK_acircumflex] = VEK_acircumflex;
  trans_table[XK_atilde] = VEK_atilde;
  trans_table[XK_adiaeresis] = VEK_adiaeresis;
  trans_table[XK_aring] = VEK_aring;
  trans_table[XK_ae] = VEK_ae;
  trans_table[XK_ccedilla] = VEK_ccedilla;
  trans_table[XK_egrave] = VEK_egrave;
  trans_table[XK_eacute] = VEK_eacute;
  trans_table[XK_ecircumflex] = VEK_ecircumflex;
  trans_table[XK_ediaeresis] = VEK_ediaeresis;
  trans_table[XK_igrave] = VEK_igrave;
  trans_table[XK_iacute] = VEK_iacute;
  trans_table[XK_icircumflex] = VEK_icircumflex;
  trans_table[XK_idiaeresis] = VEK_idiaeresis;
  trans_table[XK_eth] = VEK_eth;
  trans_table[XK_ntilde] = VEK_ntilde;
  trans_table[XK_ograve] = VEK_ograve;
  trans_table[XK_oacute] = VEK_oacute;
  trans_table[XK_ocircumflex] = VEK_ocircumflex;
  trans_table[XK_otilde] = VEK_otilde;
  trans_table[XK_odiaeresis] = VEK_odiaeresis;
  trans_table[XK_division] = VEK_division;
  trans_table[XK_oslash] = VEK_oslash;
  trans_table[XK_ugrave] = VEK_ugrave;
  trans_table[XK_uacute] = VEK_uacute;
  trans_table[XK_ucircumflex] = VEK_ucircumflex;
  trans_table[XK_udiaeresis] = VEK_udiaeresis;
  trans_table[XK_yacute] = VEK_yacute;
  trans_table[XK_thorn] = VEK_thorn;
  trans_table[XK_ydiaeresis] = VEK_ydiaeresis;
  trans_table[XK_Aogonek] = VEK_Aogonek;
  trans_table[XK_breve] = VEK_breve;
  trans_table[XK_Lstroke] = VEK_Lstroke;
  trans_table[XK_Lcaron] = VEK_Lcaron;
  trans_table[XK_Sacute] = VEK_Sacute;
  trans_table[XK_Scaron] = VEK_Scaron;
  trans_table[XK_Scedilla] = VEK_Scedilla;
  trans_table[XK_Tcaron] = VEK_Tcaron;
  trans_table[XK_Zacute] = VEK_Zacute;
  trans_table[XK_Zcaron] = VEK_Zcaron;
  trans_table[XK_Zabovedot] = VEK_Zabovedot;
  trans_table[XK_aogonek] = VEK_aogonek;
  trans_table[XK_ogonek] = VEK_ogonek;
  trans_table[XK_lstroke] = VEK_lstroke;
  trans_table[XK_lcaron] = VEK_lcaron;
  trans_table[XK_sacute] = VEK_sacute;
  trans_table[XK_caron] = VEK_caron;
  trans_table[XK_scaron] = VEK_scaron;
  trans_table[XK_scedilla] = VEK_scedilla;
  trans_table[XK_tcaron] = VEK_tcaron;
  trans_table[XK_zacute] = VEK_zacute;
  trans_table[XK_doubleacute] = VEK_doubleacute;
  trans_table[XK_zcaron] = VEK_zcaron;
  trans_table[XK_zabovedot] = VEK_zabovedot;
  trans_table[XK_Racute] = VEK_Racute;
  trans_table[XK_Abreve] = VEK_Abreve;
  trans_table[XK_Lacute] = VEK_Lacute;
  trans_table[XK_Cacute] = VEK_Cacute;
  trans_table[XK_Ccaron] = VEK_Ccaron;
  trans_table[XK_Eogonek] = VEK_Eogonek;
  trans_table[XK_Ecaron] = VEK_Ecaron;
  trans_table[XK_Dcaron] = VEK_Dcaron;
  trans_table[XK_Dstroke] = VEK_Dstroke;
  trans_table[XK_Nacute] = VEK_Nacute;
  trans_table[XK_Ncaron] = VEK_Ncaron;
  trans_table[XK_Odoubleacute] = VEK_Odoubleacute;
  trans_table[XK_Rcaron] = VEK_Rcaron;
  trans_table[XK_Uring] = VEK_Uring;
  trans_table[XK_Udoubleacute] = VEK_Udoubleacute;
  trans_table[XK_Tcedilla] = VEK_Tcedilla;
  trans_table[XK_racute] = VEK_racute;
  trans_table[XK_abreve] = VEK_abreve;
  trans_table[XK_lacute] = VEK_lacute;
  trans_table[XK_cacute] = VEK_cacute;
  trans_table[XK_ccaron] = VEK_ccaron;
  trans_table[XK_eogonek] = VEK_eogonek;
  trans_table[XK_ecaron] = VEK_ecaron;
  trans_table[XK_dcaron] = VEK_dcaron;
  trans_table[XK_dstroke] = VEK_dstroke;
  trans_table[XK_nacute] = VEK_nacute;
  trans_table[XK_ncaron] = VEK_ncaron;
  trans_table[XK_odoubleacute] = VEK_odoubleacute;
  trans_table[XK_udoubleacute] = VEK_udoubleacute;
  trans_table[XK_rcaron] = VEK_rcaron;
  trans_table[XK_uring] = VEK_uring;
  trans_table[XK_tcedilla] = VEK_tcedilla;
  trans_table[XK_abovedot] = VEK_abovedot;
  trans_table[XK_Hstroke] = VEK_Hstroke;
  trans_table[XK_Hcircumflex] = VEK_Hcircumflex;
  trans_table[XK_Iabovedot] = VEK_Iabovedot;
  trans_table[XK_Gbreve] = VEK_Gbreve;
  trans_table[XK_Jcircumflex] = VEK_Jcircumflex;
  trans_table[XK_hstroke] = VEK_hstroke;
  trans_table[XK_hcircumflex] = VEK_hcircumflex;
  trans_table[XK_idotless] = VEK_idotless;
  trans_table[XK_gbreve] = VEK_gbreve;
  trans_table[XK_jcircumflex] = VEK_jcircumflex;
  trans_table[XK_Cabovedot] = VEK_Cabovedot;
  trans_table[XK_Ccircumflex] = VEK_Ccircumflex;
  trans_table[XK_Gabovedot] = VEK_Gabovedot;
  trans_table[XK_Gcircumflex] = VEK_Gcircumflex;
  trans_table[XK_Ubreve] = VEK_Ubreve;
  trans_table[XK_Scircumflex] = VEK_Scircumflex;
  trans_table[XK_cabovedot] = VEK_cabovedot;
  trans_table[XK_ccircumflex] = VEK_ccircumflex;
  trans_table[XK_gabovedot] = VEK_gabovedot;
  trans_table[XK_gcircumflex] = VEK_gcircumflex;
  trans_table[XK_ubreve] = VEK_ubreve;
  trans_table[XK_scircumflex] = VEK_scircumflex;
  trans_table[XK_kra] = VEK_kra;
  trans_table[XK_kappa] = VEK_kappa;
  trans_table[XK_Rcedilla] = VEK_Rcedilla;
  trans_table[XK_Itilde] = VEK_Itilde;
  trans_table[XK_Lcedilla] = VEK_Lcedilla;
  trans_table[XK_Emacron] = VEK_Emacron;
  trans_table[XK_Gcedilla] = VEK_Gcedilla;
  trans_table[XK_Tslash] = VEK_Tslash;
  trans_table[XK_rcedilla] = VEK_rcedilla;
  trans_table[XK_itilde] = VEK_itilde;
  trans_table[XK_lcedilla] = VEK_lcedilla;
  trans_table[XK_emacron] = VEK_emacron;
  trans_table[XK_gcedilla] = VEK_gcedilla;
  trans_table[XK_tslash] = VEK_tslash;
  trans_table[XK_ENG] = VEK_ENG;
  trans_table[XK_eng] = VEK_eng;
  trans_table[XK_Amacron] = VEK_Amacron;
  trans_table[XK_Iogonek] = VEK_Iogonek;
  trans_table[XK_Eabovedot] = VEK_Eabovedot;
  trans_table[XK_Imacron] = VEK_Imacron;
  trans_table[XK_Ncedilla] = VEK_Ncedilla;
  trans_table[XK_Omacron] = VEK_Omacron;
  trans_table[XK_Kcedilla] = VEK_Kcedilla;
  trans_table[XK_Uogonek] = VEK_Uogonek;
  trans_table[XK_Utilde] = VEK_Utilde;
  trans_table[XK_Umacron] = VEK_Umacron;
  trans_table[XK_amacron] = VEK_amacron;
  trans_table[XK_iogonek] = VEK_iogonek;
  trans_table[XK_eabovedot] = VEK_eabovedot;
  trans_table[XK_imacron] = VEK_imacron;
  trans_table[XK_ncedilla] = VEK_ncedilla;
  trans_table[XK_omacron] = VEK_omacron;
  trans_table[XK_kcedilla] = VEK_kcedilla;
  trans_table[XK_uogonek] = VEK_uogonek;
  trans_table[XK_utilde] = VEK_utilde;
  trans_table[XK_umacron] = VEK_umacron;
  trans_table_ready = 1;
}

static int lookup_key_code(XEvent *ev, char *str, int n) {
  int x;
  KeySym ks;

  if (!trans_table_ready)
    init_trans_table();
  if (ev) {
    XLookupString((XKeyEvent *)ev,str,n,&ks,NULL);
    x = trans_table[ks];
    if (x != 0)
      return x;
  }
  return VEK_UNKNOWN;
}

static void snoop_all_windows(Display *d, Window root, unsigned long type)
{
  static int level = 0;
  Window parent, *children;
  unsigned int nchildren;
  int stat, i;

  level++;

  stat = XQueryTree(d, root, &root, &parent, &children, &nchildren);
  if (stat == False) {
    fprintf(stderr, "Can't query window tree...\n");
    return;
  }

  if (nchildren == 0)
    return;

  XSelectInput(d, root, type);

  for(i=0; i < nchildren; i++) {
    XSelectInput(d, children[i], type);
    snoop_all_windows(d,children[i], type);
  }

  XFree((char *)children);
}

static void *kbd_thread(void *v) {
  XEvent e;
  XKeyEvent *ke;
  VeDeviceEvent *ve;
  VeDeviceE_Keyboard *k;
  VeDevice *d = (VeDevice *)v;
  Display *dpy = (Display *)(d->instance->idata);
  long event_mask = KeyPressMask|KeyReleaseMask;
  char str[80], *xstr;

  VE_DEBUG(1,("x11keyboard %s: thread started", d->name));

  veLockCallbacks();
  /* Let X know that I'm interested in events */
  snoop_all_windows(dpy,DefaultRootWindow(dpy), event_mask);
  veUnlockCallbacks();

  VE_DEBUG(1,("x11keyboard %s: all windows snooped", d->name));

  ke = (XKeyEvent *)&e;
  while(1) {
    XNextEvent(dpy,&e);
    if (e.type != KeyPress && e.type != KeyRelease) {
      VE_DEBUG(1,("x11keyboard %s: discarding event", d->name));
      continue;
    }
    /* build event */
    veLockFrame();
    ve = veDeviceEventCreate(VE_ELEM_KEYBOARD,0);
    ve->device = veDupString(d->name);
    k = VE_EVENT_KEYBOARD(ve);
    str[0] = '\0';
    k->key = lookup_key_code(&e,str,80);
    VE_DEBUG(2,("x11keyboard %s: keycode %d",d->name,k->key));
    if (xstr = veKeysymToString(k->key)) {
      VE_DEBUG(2,("x11keyboard %s: using name '%s'",d->name,xstr));
      ve->elem = veDupString(xstr);
    } else {
      VE_DEBUG(2,("x11keyboard %s: using ascii name '%s'",d->name,str));
      ve->elem = veDupString(str);
    }
    /* This is still wrong, but less horribly so */
    ve->timestamp = veClock();
    k->state = (e.type == KeyPress) ? 1 : 0;
    VE_DEBUG(1,("x11keyboard %s: event %d '%s'",d->name,k->key,ve->elem));
    /* send event off to processing */
    veDeviceInsertEvent(ve);
    veUnlockFrame();
  }
}

/* Important limitation:  trying to use the mouse on the same display twice
   will end up with only one working device - the other will never report
   any events due to the exclusive grab by one of them. */
static void *mouse_thread(void *v) {
  XEvent e,next;
  XButtonEvent *be;
  XMotionEvent *me;
  VeDeviceEvent *ve;
  VeDeviceE_Switch *sw;
  VeDeviceE_Vector *vec;
  VeDevice *d = (VeDevice *)v;
  struct driver_x11_private *priv = (struct driver_x11_private *)(d->instance->idata);
  long event_mask = ButtonPressMask|ButtonReleaseMask|PointerMotionMask;
  Display *dpy = priv->dpy;
  char *enm;
  unsigned r_width, r_height;
  Window dummy_win;
  int dummy_int[2], i;
  unsigned dummy_uns[2];

  VE_DEBUG(1,("x11mouse %s: thread started", d->name));

  XGetGeometry(dpy,DefaultRootWindow(dpy),&dummy_win,
	       &(dummy_int[0]),&(dummy_int[1]),
	       &r_width,&r_height,&(dummy_uns[0]),&(dummy_uns[1]));

  veLockCallbacks();
  /* Let X know that I'm interested in events */
  if ((i = XGrabPointer(priv->dpy,DefaultRootWindow(dpy),False,event_mask,
			GrabModeAsync,GrabModeAsync,
			None,None,CurrentTime)) != GrabSuccess) {
    char *msg;
    switch (i) {
    case GrabNotViewable: msg = "GrabNotViewable"; break;
    case AlreadyGrabbed: msg = "AlreadyGrabbed"; break;
    case GrabFrozen: msg = "GrabFrozen"; break;
    case GrabInvalidTime: msg = "GrabInvalidTime"; break;
    default: msg = "<unknown>"; break;
    }
    veError(MODULE, "device %s disabled: cannot grab pointer (result = %d, '%s')", 
	    d->name, i, msg);
    XCloseDisplay(priv->dpy);
    free(priv);
    veUnlockCallbacks();
    return NULL;
  }
  veUnlockCallbacks();

  VE_DEBUG(1,("x11mouse %s: events registered - entering loop", d->name));

  be = (XButtonEvent *)&e;
  me = (XMotionEvent *)&e;

  while(1) {
    XNextEvent(dpy,&e);
    switch(e.type) {
    case ButtonPress:   /* mouse */
    case ButtonRelease:
      switch(be->button) {
      case Button1: enm = "left"; break;
      case Button2: enm = "middle"; break;
      case Button3: enm = "right"; break;
      default: enm = NULL;
      }
      if (!enm) {
	VE_DEBUG(2,("x11mouse %s: ignoring unknown button event",d->name));
	continue;
      }
      veLockFrame();
      ve = veDeviceEventCreate(VE_ELEM_SWITCH,0);
      ve->device = veDupString(d->name);
      ve->elem = veDupString(enm);
      /* The following is wrong, but without a way to reference the 
	 X server time (which is relative to its last reset) to the
	 absolute wall clock, this is the best we can do for now. */
      ve->timestamp = veClock();
      sw = VE_EVENT_SWITCH(ve);
      sw->state = (e.type == ButtonPress) ? 1 : 0;
      /* process this event */
      VE_DEBUG(2,("x11mouse %s: button %s %d", ve->elem, sw->state));
      veDeviceInsertEvent(ve);
      veUnlockFrame();
      break;
      
    case MotionNotify:
      /* get only the most recent motion - skip ahead in the queue */
      while(XEventsQueued(dpy,QueuedAfterReading) > 0) {
	XPeekEvent(dpy,&next);
	if (next.type == MotionNotify) {
	  VE_DEBUG(2,("x11mouse %s: skipping redundant motion event", d->name));
	  XNextEvent(dpy,&next);
	} else
	  break;
      }
      veLockFrame();
      ve = veDeviceEventCreate(VE_ELEM_VECTOR,2);
      ve->device = veDupString(d->name);
      ve->elem = veDupString("axes");
      /* Again - wrong, but better than previous */
      ve->timestamp = veClock();
      vec = VE_EVENT_VECTOR(ve);
      vec->min[0] = vec->min[1] = -1.0;
      vec->max[0] = vec->max[1] = 1.0;
      vec->value[0] = (2.0*me->x_root/(float)(r_width-1))-1.0;
      vec->value[1] = (2.0*me->y_root/(float)(r_height-1))-1.0;
      /* process this event */
      VE_DEBUG(2,("x11mouse %s: motion vector %g %g", d->name,
		  vec->value[0], vec->value[1]));
      veDeviceInsertEvent(ve);
      veUnlockFrame();
      break;

    default:
      /* ignore other events */
      ;
    }
  }
}

static VeDevice *new_kbd_driver(VeDeviceDriver *driver, VeDeviceDesc *desc,
				VeStrMap override) {
  VeDevice *d;
  Display *dpy;
  char *dpyname;
  VeDeviceInstance *i;

  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  if (!trans_table_ready)
    init_trans_table();
  
  i = veDeviceInstanceInit(driver,NULL,desc,override);

  dpyname = veDeviceInstOption(i,"display");
  if (dpyname && strcmp(dpyname,"default") == 0)
    dpyname = NULL;

  if (!(dpy = XOpenDisplay(dpyname))) {
    veError(MODULE, "device %s disabled: cannot open display: %s",
	    desc->name, dpyname ? dpyname : "<NULL>");
    return NULL;
  }
  d = veDeviceCreate(desc->name);
  /* keyboard devices do not have a model */
  i->idata = (void *)dpy;
  d->instance = i;

  veThreadInitDelayed(kbd_thread,d,0,0);
  return d;
}

static VeDevice *new_mouse_driver(VeDeviceDriver *driver, VeDeviceDesc *desc,
				  VeStrMap override) {
  VeDevice *d;
  char *dpyname;
  struct driver_x11_private *priv;
  VeDeviceInstance *i;

  assert(desc != NULL);
  assert(desc->name != NULL);
  assert(desc->type != NULL);

  VE_DEBUG(1,("x11mouse - creating new mouse driver"));
  
  i = veDeviceInstanceInit(driver,NULL,desc,override);

  dpyname = veDeviceInstOption(i,"display");
  if (dpyname && strcmp(dpyname,"default") == 0)
    dpyname = NULL;

  priv = malloc(sizeof(struct driver_x11_private));
  assert(priv != NULL);

  if (!(priv->dpy = XOpenDisplay(dpyname))) {
    veError(MODULE, "device %s disabled: cannot open display: %s",
	    desc->name, dpyname ? dpyname : "<NULL>");
    free(priv);
    return NULL;
  }

  d = veDeviceCreate(desc->name);
  i->idata = (void *)priv;
  d->instance = i;
  /* build model of 3-button mouse */
  d->model = veDeviceCreateModel();
  veDeviceAddElemSpec(d->model,"axes vector 2 {-1.0 1.0} {-1.0 1.0}");
  veDeviceAddElemSpec(d->model,"left switch");
  veDeviceAddElemSpec(d->model,"middle switch");
  veDeviceAddElemSpec(d->model,"right switch");
  veThreadInitDelayed(mouse_thread,d,0,0);
  return d;
}

static VeDeviceDriver kbd_drv = {
  "x11keyboard", new_kbd_driver
};

static VeDeviceDriver mouse_drv = {
  "x11mouse", new_mouse_driver
};

void VE_DRIVER_INIT(x11drv) (void) {
  veDeviceAddDriver(&kbd_drv);
  veDeviceAddDriver(&mouse_drv);
}
