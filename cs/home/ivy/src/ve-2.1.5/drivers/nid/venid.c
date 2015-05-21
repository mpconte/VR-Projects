/* A simple driver to interface with a NID (Network Input Device) server */
/* Use the following in the description:
   server <servername>
   port <portnumber>  [optional: defaults to 1138]
   Default behaviour is to take first device returned by ENUM_DEVICES, to
   be specific:
   devname name    - first device whose name matches glob expr (default "*")
   devtype type    - first device whose type matches glob expr (default "*")
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <ve.h>
#include <nid.h>
#include <nid_keysym.h>

#define MODULE "driver:nid"

typedef struct vei_nid_elem {
  NidElementInfo info;
  NidElementState state;
  nid_int32 dirty;
} VeiNidElem;

typedef struct vei_nid {
  VeDevice *device;
  int conn;
  int compressed;
  int num_elements;
  VeiNidElem **elems; /* array of pointers to elements indexed by elemid */
} VeiNid;

static int glob_match(char *expr, char *str) {
  /* this is non-destructive */
  int min,max,st;

  if (!expr || !str)
    return 0;

  while(*expr != '\0' && *str != '\0') {
    /* get next element */
    switch (*expr) {
    case '[':
      expr++;
      st = 0;
      while (*expr != ']' && *expr != '\0') {
	min = *expr++;
	if (min == '\\') {
	  if (*expr == '\0')
	    return 0; /* syntax error */
	  min = *expr++;
	}
	if (*expr == '-') {
	  /* find max */
	  expr++;
	  if (*expr == '\\')
	    expr++;
	  max = *expr++;
	} else
	  max = min;
	if (min == '\0' || max == '\0')
	  return 0; /* syntax error */
	if (*str >= min && *str <= max) {
	  str++;
	  st = 1;
	  break;
	}
      }
      if (!st)
	return 0; /* nothing in the range matched */
      /* skip to end of range */
      while (*expr != ']' && *expr != '\0')
	expr++;
      if (*expr != ']')
	return 0; /* syntax error */
      expr++;
      break;
    case '{':
      {
	char *e_rem, *sub_end;
	expr++;
	/* find end of expression */
	e_rem = expr;
	while (*e_rem && *e_rem != '}') {
	  if (*e_rem == '\\')
	    e_rem++;
	  if (*e_rem)
	    e_rem++;
	}
	if (e_rem)
	  e_rem++;
	/* e_rem now points to whatever is after the multiple choice */
	/* pick out each subexpression and try it */
	while (expr < e_rem) {
	  /* find the end of the next subexpression */
	  sub_end = expr;
	  while(sub_end < e_rem && *sub_end != ',' && *sub_end != '}') {
	    if (*sub_end == '\\')
	      sub_end++;
	    if (sub_end < e_rem)
	      sub_end++;
	  }
	  /* sub_end now points to the character just beyond the last of
	     the next subexpr */
	  if (strncmp(expr,str,sub_end-expr) == 0 && 
	      glob_match(e_rem,str+(sub_end-expr)))
	    return 1; /* this subexpression works with the remaining */
	  
	  /* if we get here, then this subexpression doesn't work out */
	  expr = sub_end+1;
	  if (expr > e_rem)
	    expr = e_rem;
	}
	/* if we get here, then none of the choices worked out - the whole
	   expression fails */
	return 0;
      }
      break;

    case '?':
      if (*str == '\0')
	return 0;
      expr++;
      str++;
      break;

    case '*':
      expr++;
      /* special case - end of expr */
      if (!*expr)
	return 1; /* nothing left in expression */
      while (*str) {
	if (glob_match(expr,str))
	  return 1;
	str++;
      }
      return glob_match(expr,str);

    case '\\':
      expr++;
      /* fall-through */
    default:
      if (*expr != *str)
	return 0;
      expr++;
      str++;
    }
  }

  if (*expr == '\0' && *str == '\0')
    return 1;
  else
    /* didn't end in the same spot */
    return 0;
}

/* Translation table for ve_keysyms */
/* This is 64k but it's static so it doesn't change over time... */
static int *trans_table = NULL;
static int trans_table_ready = 0;
static void init_trans_table() {
  if (!trans_table)
    trans_table = calloc(65536,sizeof(int));
  trans_table[NIDK_BackSpace] = VEK_BackSpace;
  trans_table[NIDK_Tab] = VEK_Tab;
  trans_table[NIDK_Linefeed] = VEK_Linefeed;
  trans_table[NIDK_Clear] = VEK_Clear;
  trans_table[NIDK_Return] = VEK_Return;
  trans_table[NIDK_Pause] = VEK_Pause;
  trans_table[NIDK_Scroll_Lock] = VEK_Scroll_Lock;
  trans_table[NIDK_Sys_Req] = VEK_Sys_Req;
  trans_table[NIDK_Escape] = VEK_Escape;
  trans_table[NIDK_Delete] = VEK_Delete;
  trans_table[NIDK_Multi_key] = VEK_Multi_key;
  trans_table[NIDK_SingleCandidate] = VEK_SingleCandidate;
  trans_table[NIDK_MultipleCandidate] = VEK_MultipleCandidate;
  trans_table[NIDK_PreviousCandidate] = VEK_PreviousCandidate;
  trans_table[NIDK_Kanji] = VEK_Kanji;
  trans_table[NIDK_Muhenkan] = VEK_Muhenkan;
  trans_table[NIDK_Henkan_Mode] = VEK_Henkan_Mode;
  trans_table[NIDK_Henkan] = VEK_Henkan;
  trans_table[NIDK_Romaji] = VEK_Romaji;
  trans_table[NIDK_Hiragana] = VEK_Hiragana;
  trans_table[NIDK_Katakana] = VEK_Katakana;
  trans_table[NIDK_Hiragana_Katakana] = VEK_Hiragana_Katakana;
  trans_table[NIDK_Zenkaku] = VEK_Zenkaku;
  trans_table[NIDK_Hankaku] = VEK_Hankaku;
  trans_table[NIDK_Zenkaku_Hankaku] = VEK_Zenkaku_Hankaku;
  trans_table[NIDK_Touroku] = VEK_Touroku;
  trans_table[NIDK_Massyo] = VEK_Massyo;
  trans_table[NIDK_Kana_Lock] = VEK_Kana_Lock;
  trans_table[NIDK_Kana_Shift] = VEK_Kana_Shift;
  trans_table[NIDK_Eisu_Shift] = VEK_Eisu_Shift;
  trans_table[NIDK_Eisu_toggle] = VEK_Eisu_toggle;
  trans_table[NIDK_Zen_Koho] = VEK_Zen_Koho;
  trans_table[NIDK_Mae_Koho] = VEK_Mae_Koho;
  trans_table[NIDK_Home] = VEK_Home;
  trans_table[NIDK_Left] = VEK_Left;
  trans_table[NIDK_Up] = VEK_Up;
  trans_table[NIDK_Right] = VEK_Right;
  trans_table[NIDK_Down] = VEK_Down;
  trans_table[NIDK_Prior] = VEK_Prior;
  trans_table[NIDK_Page_Up] = VEK_Page_Up;
  trans_table[NIDK_Next] = VEK_Next;
  trans_table[NIDK_Page_Down] = VEK_Page_Down;
  trans_table[NIDK_End] = VEK_End;
  trans_table[NIDK_Begin] = VEK_Begin;
  trans_table[NIDK_Select] = VEK_Select;
  trans_table[NIDK_Print] = VEK_Print;
  trans_table[NIDK_Execute] = VEK_Execute;
  trans_table[NIDK_Insert] = VEK_Insert;
  trans_table[NIDK_Undo] = VEK_Undo;
  trans_table[NIDK_Redo] = VEK_Redo;
  trans_table[NIDK_Menu] = VEK_Menu;
  trans_table[NIDK_Find] = VEK_Find;
  trans_table[NIDK_Cancel] = VEK_Cancel;
  trans_table[NIDK_Help] = VEK_Help;
  trans_table[NIDK_Break] = VEK_Break;
  trans_table[NIDK_Mode_switch] = VEK_Mode_switch;
  trans_table[NIDK_script_switch] = VEK_script_switch;
  trans_table[NIDK_Num_Lock] = VEK_Num_Lock;
  trans_table[NIDK_KP_Space] = VEK_KP_Space;
  trans_table[NIDK_KP_Tab] = VEK_KP_Tab;
  trans_table[NIDK_KP_Enter] = VEK_KP_Enter;
  trans_table[NIDK_KP_F1] = VEK_KP_F1;
  trans_table[NIDK_KP_F2] = VEK_KP_F2;
  trans_table[NIDK_KP_F3] = VEK_KP_F3;
  trans_table[NIDK_KP_F4] = VEK_KP_F4;
  trans_table[NIDK_KP_Home] = VEK_KP_Home;
  trans_table[NIDK_KP_Left] = VEK_KP_Left;
  trans_table[NIDK_KP_Up] = VEK_KP_Up;
  trans_table[NIDK_KP_Right] = VEK_KP_Right;
  trans_table[NIDK_KP_Down] = VEK_KP_Down;
  trans_table[NIDK_KP_Prior] = VEK_KP_Prior;
  trans_table[NIDK_KP_Page_Up] = VEK_KP_Page_Up;
  trans_table[NIDK_KP_Next] = VEK_KP_Next;
  trans_table[NIDK_KP_Page_Down] = VEK_KP_Page_Down;
  trans_table[NIDK_KP_End] = VEK_KP_End;
  trans_table[NIDK_KP_Begin] = VEK_KP_Begin;
  trans_table[NIDK_KP_Insert] = VEK_KP_Insert;
  trans_table[NIDK_KP_Delete] = VEK_KP_Delete;
  trans_table[NIDK_KP_Equal] = VEK_KP_Equal;
  trans_table[NIDK_KP_Multiply] = VEK_KP_Multiply;
  trans_table[NIDK_KP_Add] = VEK_KP_Add;
  trans_table[NIDK_KP_Separator] = VEK_KP_Separator;
  trans_table[NIDK_KP_Subtract] = VEK_KP_Subtract;
  trans_table[NIDK_KP_Decimal] = VEK_KP_Decimal;
  trans_table[NIDK_KP_Divide] = VEK_KP_Divide;
  trans_table[NIDK_KP_0] = VEK_KP_0;
  trans_table[NIDK_KP_1] = VEK_KP_1;
  trans_table[NIDK_KP_2] = VEK_KP_2;
  trans_table[NIDK_KP_3] = VEK_KP_3;
  trans_table[NIDK_KP_4] = VEK_KP_4;
  trans_table[NIDK_KP_5] = VEK_KP_5;
  trans_table[NIDK_KP_6] = VEK_KP_6;
  trans_table[NIDK_KP_7] = VEK_KP_7;
  trans_table[NIDK_KP_8] = VEK_KP_8;
  trans_table[NIDK_KP_9] = VEK_KP_9;
  trans_table[NIDK_F1] = VEK_F1;
  trans_table[NIDK_F2] = VEK_F2;
  trans_table[NIDK_F3] = VEK_F3;
  trans_table[NIDK_F4] = VEK_F4;
  trans_table[NIDK_F5] = VEK_F5;
  trans_table[NIDK_F6] = VEK_F6;
  trans_table[NIDK_F7] = VEK_F7;
  trans_table[NIDK_F8] = VEK_F8;
  trans_table[NIDK_F9] = VEK_F9;
  trans_table[NIDK_F10] = VEK_F10;
  trans_table[NIDK_F11] = VEK_F11;
  trans_table[NIDK_L1] = VEK_L1;
  trans_table[NIDK_F12] = VEK_F12;
  trans_table[NIDK_L2] = VEK_L2;
  trans_table[NIDK_F13] = VEK_F13;
  trans_table[NIDK_L3] = VEK_L3;
  trans_table[NIDK_F14] = VEK_F14;
  trans_table[NIDK_L4] = VEK_L4;
  trans_table[NIDK_F15] = VEK_F15;
  trans_table[NIDK_L5] = VEK_L5;
  trans_table[NIDK_F16] = VEK_F16;
  trans_table[NIDK_L6] = VEK_L6;
  trans_table[NIDK_F17] = VEK_F17;
  trans_table[NIDK_L7] = VEK_L7;
  trans_table[NIDK_F18] = VEK_F18;
  trans_table[NIDK_L8] = VEK_L8;
  trans_table[NIDK_F19] = VEK_F19;
  trans_table[NIDK_L9] = VEK_L9;
  trans_table[NIDK_F20] = VEK_F20;
  trans_table[NIDK_L10] = VEK_L10;
  trans_table[NIDK_F21] = VEK_F21;
  trans_table[NIDK_R1] = VEK_R1;
  trans_table[NIDK_F22] = VEK_F22;
  trans_table[NIDK_R2] = VEK_R2;
  trans_table[NIDK_F23] = VEK_F23;
  trans_table[NIDK_R3] = VEK_R3;
  trans_table[NIDK_F24] = VEK_F24;
  trans_table[NIDK_R4] = VEK_R4;
  trans_table[NIDK_F25] = VEK_F25;
  trans_table[NIDK_R5] = VEK_R5;
  trans_table[NIDK_F26] = VEK_F26;
  trans_table[NIDK_R6] = VEK_R6;
  trans_table[NIDK_F27] = VEK_F27;
  trans_table[NIDK_R7] = VEK_R7;
  trans_table[NIDK_F28] = VEK_F28;
  trans_table[NIDK_R8] = VEK_R8;
  trans_table[NIDK_F29] = VEK_F29;
  trans_table[NIDK_R9] = VEK_R9;
  trans_table[NIDK_F30] = VEK_F30;
  trans_table[NIDK_R10] = VEK_R10;
  trans_table[NIDK_F31] = VEK_F31;
  trans_table[NIDK_R11] = VEK_R11;
  trans_table[NIDK_F32] = VEK_F32;
  trans_table[NIDK_R12] = VEK_R12;
  trans_table[NIDK_F33] = VEK_F33;
  trans_table[NIDK_R13] = VEK_R13;
  trans_table[NIDK_F34] = VEK_F34;
  trans_table[NIDK_R14] = VEK_R14;
  trans_table[NIDK_F35] = VEK_F35;
  trans_table[NIDK_R15] = VEK_R15;
  trans_table[NIDK_Shift_L] = VEK_Shift_L;
  trans_table[NIDK_Shift_R] = VEK_Shift_R;
  trans_table[NIDK_Control_L] = VEK_Control_L;
  trans_table[NIDK_Control_R] = VEK_Control_R;
  trans_table[NIDK_Caps_Lock] = VEK_Caps_Lock;
  trans_table[NIDK_Shift_Lock] = VEK_Shift_Lock;
  trans_table[NIDK_Meta_L] = VEK_Meta_L;
  trans_table[NIDK_Meta_R] = VEK_Meta_R;
  trans_table[NIDK_Alt_L] = VEK_Alt_L;
  trans_table[NIDK_Alt_R] = VEK_Alt_R;
  trans_table[NIDK_Super_L] = VEK_Super_L;
  trans_table[NIDK_Super_R] = VEK_Super_R;
  trans_table[NIDK_Hyper_L] = VEK_Hyper_L;
  trans_table[NIDK_Hyper_R] = VEK_Hyper_R;
  trans_table[NIDK_ISO_Lock] = VEK_ISO_Lock;
  trans_table[NIDK_ISO_Level2_Latch] = VEK_ISO_Level2_Latch;
  trans_table[NIDK_ISO_Level3_Shift] = VEK_ISO_Level3_Shift;
  trans_table[NIDK_ISO_Level3_Latch] = VEK_ISO_Level3_Latch;
  trans_table[NIDK_ISO_Level3_Lock] = VEK_ISO_Level3_Lock;
  trans_table[NIDK_ISO_Group_Shift] = VEK_ISO_Group_Shift;
  trans_table[NIDK_ISO_Group_Latch] = VEK_ISO_Group_Latch;
  trans_table[NIDK_ISO_Group_Lock] = VEK_ISO_Group_Lock;
  trans_table[NIDK_ISO_Next_Group] = VEK_ISO_Next_Group;
  trans_table[NIDK_ISO_Next_Group_Lock] = VEK_ISO_Next_Group_Lock;
  trans_table[NIDK_ISO_Prev_Group] = VEK_ISO_Prev_Group;
  trans_table[NIDK_ISO_Prev_Group_Lock] = VEK_ISO_Prev_Group_Lock;
  trans_table[NIDK_ISO_First_Group] = VEK_ISO_First_Group;
  trans_table[NIDK_ISO_First_Group_Lock] = VEK_ISO_First_Group_Lock;
  trans_table[NIDK_ISO_Last_Group] = VEK_ISO_Last_Group;
  trans_table[NIDK_ISO_Last_Group_Lock] = VEK_ISO_Last_Group_Lock;
  trans_table[NIDK_ISO_Left_Tab] = VEK_ISO_Left_Tab;
  trans_table[NIDK_ISO_Move_Line_Up] = VEK_ISO_Move_Line_Up;
  trans_table[NIDK_ISO_Move_Line_Down] = VEK_ISO_Move_Line_Down;
  trans_table[NIDK_ISO_Partial_Line_Up] = VEK_ISO_Partial_Line_Up;
  trans_table[NIDK_ISO_Partial_Line_Down] = VEK_ISO_Partial_Line_Down;
  trans_table[NIDK_ISO_Partial_Space_Left] = VEK_ISO_Partial_Space_Left;
  trans_table[NIDK_ISO_Partial_Space_Right] = VEK_ISO_Partial_Space_Right;
  trans_table[NIDK_ISO_Set_Margin_Left] = VEK_ISO_Set_Margin_Left;
  trans_table[NIDK_ISO_Set_Margin_Right] = VEK_ISO_Set_Margin_Right;
  trans_table[NIDK_ISO_Release_Margin_Left] = VEK_ISO_Release_Margin_Left;
  trans_table[NIDK_ISO_Release_Margin_Right] = VEK_ISO_Release_Margin_Right;
  trans_table[NIDK_ISO_Release_Both_Margins] = VEK_ISO_Release_Both_Margins;
  trans_table[NIDK_ISO_Fast_Cursor_Left] = VEK_ISO_Fast_Cursor_Left;
  trans_table[NIDK_ISO_Fast_Cursor_Right] = VEK_ISO_Fast_Cursor_Right;
  trans_table[NIDK_ISO_Fast_Cursor_Up] = VEK_ISO_Fast_Cursor_Up;
  trans_table[NIDK_ISO_Fast_Cursor_Down] = VEK_ISO_Fast_Cursor_Down;
  trans_table[NIDK_ISO_Continuous_Underline] = VEK_ISO_Continuous_Underline;
  trans_table[NIDK_ISO_Discontinuous_Underline] = VEK_ISO_Discontinuous_Underline;
  trans_table[NIDK_ISO_Emphasize] = VEK_ISO_Emphasize;
  trans_table[NIDK_ISO_Center_Object] = VEK_ISO_Center_Object;
  trans_table[NIDK_ISO_Enter] = VEK_ISO_Enter;
  trans_table[NIDK_dead_grave] = VEK_dead_grave;
  trans_table[NIDK_dead_acute] = VEK_dead_acute;
  trans_table[NIDK_dead_circumflex] = VEK_dead_circumflex;
  trans_table[NIDK_dead_tilde] = VEK_dead_tilde;
  trans_table[NIDK_dead_macron] = VEK_dead_macron;
  trans_table[NIDK_dead_breve] = VEK_dead_breve;
  trans_table[NIDK_dead_abovedot] = VEK_dead_abovedot;
  trans_table[NIDK_dead_diaeresis] = VEK_dead_diaeresis;
  trans_table[NIDK_dead_abovering] = VEK_dead_abovering;
  trans_table[NIDK_dead_doubleacute] = VEK_dead_doubleacute;
  trans_table[NIDK_dead_caron] = VEK_dead_caron;
  trans_table[NIDK_dead_cedilla] = VEK_dead_cedilla;
  trans_table[NIDK_dead_ogonek] = VEK_dead_ogonek;
  trans_table[NIDK_dead_iota] = VEK_dead_iota;
  trans_table[NIDK_dead_belowdot] = VEK_dead_belowdot;
  trans_table[NIDK_First_Virtual_Screen] = VEK_First_Virtual_Screen;
  trans_table[NIDK_Prev_Virtual_Screen] = VEK_Prev_Virtual_Screen;
  trans_table[NIDK_Next_Virtual_Screen] = VEK_Next_Virtual_Screen;
  trans_table[NIDK_Last_Virtual_Screen] = VEK_Last_Virtual_Screen;
  trans_table[NIDK_Terminate_Server] = VEK_Terminate_Server;
  trans_table[NIDK_AccessX_Enable] = VEK_AccessX_Enable;
  trans_table[NIDK_AccessX_Feedback_Enable] = VEK_AccessX_Feedback_Enable;
  trans_table[NIDK_RepeatKeys_Enable] = VEK_RepeatKeys_Enable;
  trans_table[NIDK_SlowKeys_Enable] = VEK_SlowKeys_Enable;
  trans_table[NIDK_BounceKeys_Enable] = VEK_BounceKeys_Enable;
  trans_table[NIDK_StickyKeys_Enable] = VEK_StickyKeys_Enable;
  trans_table[NIDK_MouseKeys_Enable] = VEK_MouseKeys_Enable;
  trans_table[NIDK_MouseKeys_Accel_Enable] = VEK_MouseKeys_Accel_Enable;
  trans_table[NIDK_Overlay1_Enable] = VEK_Overlay1_Enable;
  trans_table[NIDK_Overlay2_Enable] = VEK_Overlay2_Enable;
  trans_table[NIDK_AudibleBell_Enable] = VEK_AudibleBell_Enable;
  trans_table[NIDK_Pointer_Left] = VEK_Pointer_Left;
  trans_table[NIDK_Pointer_Right] = VEK_Pointer_Right;
  trans_table[NIDK_Pointer_Up] = VEK_Pointer_Up;
  trans_table[NIDK_Pointer_Down] = VEK_Pointer_Down;
  trans_table[NIDK_Pointer_UpLeft] = VEK_Pointer_UpLeft;
  trans_table[NIDK_Pointer_UpRight] = VEK_Pointer_UpRight;
  trans_table[NIDK_Pointer_DownLeft] = VEK_Pointer_DownLeft;
  trans_table[NIDK_Pointer_DownRight] = VEK_Pointer_DownRight;
  trans_table[NIDK_Pointer_Button_Dflt] = VEK_Pointer_Button_Dflt;
  trans_table[NIDK_Pointer_Button1] = VEK_Pointer_Button1;
  trans_table[NIDK_Pointer_Button2] = VEK_Pointer_Button2;
  trans_table[NIDK_Pointer_Button3] = VEK_Pointer_Button3;
  trans_table[NIDK_Pointer_Button4] = VEK_Pointer_Button4;
  trans_table[NIDK_Pointer_Button5] = VEK_Pointer_Button5;
  trans_table[NIDK_Pointer_DblClick_Dflt] = VEK_Pointer_DblClick_Dflt;
  trans_table[NIDK_Pointer_DblClick1] = VEK_Pointer_DblClick1;
  trans_table[NIDK_Pointer_DblClick2] = VEK_Pointer_DblClick2;
  trans_table[NIDK_Pointer_DblClick3] = VEK_Pointer_DblClick3;
  trans_table[NIDK_Pointer_DblClick4] = VEK_Pointer_DblClick4;
  trans_table[NIDK_Pointer_DblClick5] = VEK_Pointer_DblClick5;
  trans_table[NIDK_Pointer_Drag_Dflt] = VEK_Pointer_Drag_Dflt;
  trans_table[NIDK_Pointer_Drag1] = VEK_Pointer_Drag1;
  trans_table[NIDK_Pointer_Drag2] = VEK_Pointer_Drag2;
  trans_table[NIDK_Pointer_Drag3] = VEK_Pointer_Drag3;
  trans_table[NIDK_Pointer_Drag4] = VEK_Pointer_Drag4;
  trans_table[NIDK_Pointer_Drag5] = VEK_Pointer_Drag5;
  trans_table[NIDK_Pointer_EnableKeys] = VEK_Pointer_EnableKeys;
  trans_table[NIDK_Pointer_Accelerate] = VEK_Pointer_Accelerate;
  trans_table[NIDK_Pointer_DfltBtnNext] = VEK_Pointer_DfltBtnNext;
  trans_table[NIDK_Pointer_DfltBtnPrev] = VEK_Pointer_DfltBtnPrev;
  trans_table[NIDK_space] = VEK_space;
  trans_table[NIDK_exclam] = VEK_exclam;
  trans_table[NIDK_quotedbl] = VEK_quotedbl;
  trans_table[NIDK_numbersign] = VEK_numbersign;
  trans_table[NIDK_dollar] = VEK_dollar;
  trans_table[NIDK_percent] = VEK_percent;
  trans_table[NIDK_ampersand] = VEK_ampersand;
  trans_table[NIDK_apostrophe] = VEK_apostrophe;
  trans_table[NIDK_quoteright] = VEK_quoteright;
  trans_table[NIDK_parenleft] = VEK_parenleft;
  trans_table[NIDK_parenright] = VEK_parenright;
  trans_table[NIDK_asterisk] = VEK_asterisk;
  trans_table[NIDK_plus] = VEK_plus;
  trans_table[NIDK_comma] = VEK_comma;
  trans_table[NIDK_minus] = VEK_minus;
  trans_table[NIDK_period] = VEK_period;
  trans_table[NIDK_slash] = VEK_slash;
  trans_table[NIDK_0] = VEK_0;
  trans_table[NIDK_1] = VEK_1;
  trans_table[NIDK_2] = VEK_2;
  trans_table[NIDK_3] = VEK_3;
  trans_table[NIDK_4] = VEK_4;
  trans_table[NIDK_5] = VEK_5;
  trans_table[NIDK_6] = VEK_6;
  trans_table[NIDK_7] = VEK_7;
  trans_table[NIDK_8] = VEK_8;
  trans_table[NIDK_9] = VEK_9;
  trans_table[NIDK_colon] = VEK_colon;
  trans_table[NIDK_semicolon] = VEK_semicolon;
  trans_table[NIDK_less] = VEK_less;
  trans_table[NIDK_equal] = VEK_equal;
  trans_table[NIDK_greater] = VEK_greater;
  trans_table[NIDK_question] = VEK_question;
  trans_table[NIDK_at] = VEK_at;
  trans_table[NIDK_A] = VEK_A;
  trans_table[NIDK_B] = VEK_B;
  trans_table[NIDK_C] = VEK_C;
  trans_table[NIDK_D] = VEK_D;
  trans_table[NIDK_E] = VEK_E;
  trans_table[NIDK_F] = VEK_F;
  trans_table[NIDK_G] = VEK_G;
  trans_table[NIDK_H] = VEK_H;
  trans_table[NIDK_I] = VEK_I;
  trans_table[NIDK_J] = VEK_J;
  trans_table[NIDK_K] = VEK_K;
  trans_table[NIDK_L] = VEK_L;
  trans_table[NIDK_M] = VEK_M;
  trans_table[NIDK_N] = VEK_N;
  trans_table[NIDK_O] = VEK_O;
  trans_table[NIDK_P] = VEK_P;
  trans_table[NIDK_Q] = VEK_Q;
  trans_table[NIDK_R] = VEK_R;
  trans_table[NIDK_S] = VEK_S;
  trans_table[NIDK_T] = VEK_T;
  trans_table[NIDK_U] = VEK_U;
  trans_table[NIDK_V] = VEK_V;
  trans_table[NIDK_W] = VEK_W;
  trans_table[NIDK_X] = VEK_X;
  trans_table[NIDK_Y] = VEK_Y;
  trans_table[NIDK_Z] = VEK_Z;
  trans_table[NIDK_bracketleft] = VEK_bracketleft;
  trans_table[NIDK_backslash] = VEK_backslash;
  trans_table[NIDK_bracketright] = VEK_bracketright;
  trans_table[NIDK_asciicircum] = VEK_asciicircum;
  trans_table[NIDK_underscore] = VEK_underscore;
  trans_table[NIDK_grave] = VEK_grave;
  trans_table[NIDK_quoteleft] = VEK_quoteleft;
  trans_table[NIDK_a] = VEK_a;
  trans_table[NIDK_b] = VEK_b;
  trans_table[NIDK_c] = VEK_c;
  trans_table[NIDK_d] = VEK_d;
  trans_table[NIDK_e] = VEK_e;
  trans_table[NIDK_f] = VEK_f;
  trans_table[NIDK_g] = VEK_g;
  trans_table[NIDK_h] = VEK_h;
  trans_table[NIDK_i] = VEK_i;
  trans_table[NIDK_j] = VEK_j;
  trans_table[NIDK_k] = VEK_k;
  trans_table[NIDK_l] = VEK_l;
  trans_table[NIDK_m] = VEK_m;
  trans_table[NIDK_n] = VEK_n;
  trans_table[NIDK_o] = VEK_o;
  trans_table[NIDK_p] = VEK_p;
  trans_table[NIDK_q] = VEK_q;
  trans_table[NIDK_r] = VEK_r;
  trans_table[NIDK_s] = VEK_s;
  trans_table[NIDK_t] = VEK_t;
  trans_table[NIDK_u] = VEK_u;
  trans_table[NIDK_v] = VEK_v;
  trans_table[NIDK_w] = VEK_w;
  trans_table[NIDK_x] = VEK_x;
  trans_table[NIDK_y] = VEK_y;
  trans_table[NIDK_z] = VEK_z;
  trans_table[NIDK_braceleft] = VEK_braceleft;
  trans_table[NIDK_bar] = VEK_bar;
  trans_table[NIDK_braceright] = VEK_braceright;
  trans_table[NIDK_asciitilde] = VEK_asciitilde;
  trans_table[NIDK_nobreakspace] = VEK_nobreakspace;
  trans_table[NIDK_exclamdown] = VEK_exclamdown;
  trans_table[NIDK_cent] = VEK_cent;
  trans_table[NIDK_sterling] = VEK_sterling;
  trans_table[NIDK_currency] = VEK_currency;
  trans_table[NIDK_yen] = VEK_yen;
  trans_table[NIDK_brokenbar] = VEK_brokenbar;
  trans_table[NIDK_section] = VEK_section;
  trans_table[NIDK_diaeresis] = VEK_diaeresis;
  trans_table[NIDK_copyright] = VEK_copyright;
  trans_table[NIDK_ordfeminine] = VEK_ordfeminine;
  trans_table[NIDK_guillemotleft] = VEK_guillemotleft;
  trans_table[NIDK_notsign] = VEK_notsign;
  trans_table[NIDK_hyphen] = VEK_hyphen;
  trans_table[NIDK_registered] = VEK_registered;
  trans_table[NIDK_macron] = VEK_macron;
  trans_table[NIDK_degree] = VEK_degree;
  trans_table[NIDK_plusminus] = VEK_plusminus;
  trans_table[NIDK_twosuperior] = VEK_twosuperior;
  trans_table[NIDK_threesuperior] = VEK_threesuperior;
  trans_table[NIDK_acute] = VEK_acute;
  trans_table[NIDK_mu] = VEK_mu;
  trans_table[NIDK_paragraph] = VEK_paragraph;
  trans_table[NIDK_periodcentered] = VEK_periodcentered;
  trans_table[NIDK_cedilla] = VEK_cedilla;
  trans_table[NIDK_onesuperior] = VEK_onesuperior;
  trans_table[NIDK_masculine] = VEK_masculine;
  trans_table[NIDK_guillemotright] = VEK_guillemotright;
  trans_table[NIDK_onequarter] = VEK_onequarter;
  trans_table[NIDK_onehalf] = VEK_onehalf;
  trans_table[NIDK_threequarters] = VEK_threequarters;
  trans_table[NIDK_questiondown] = VEK_questiondown;
  trans_table[NIDK_Agrave] = VEK_Agrave;
  trans_table[NIDK_Aacute] = VEK_Aacute;
  trans_table[NIDK_Acircumflex] = VEK_Acircumflex;
  trans_table[NIDK_Atilde] = VEK_Atilde;
  trans_table[NIDK_Adiaeresis] = VEK_Adiaeresis;
  trans_table[NIDK_Aring] = VEK_Aring;
  trans_table[NIDK_AE] = VEK_AE;
  trans_table[NIDK_Ccedilla] = VEK_Ccedilla;
  trans_table[NIDK_Egrave] = VEK_Egrave;
  trans_table[NIDK_Eacute] = VEK_Eacute;
  trans_table[NIDK_Ecircumflex] = VEK_Ecircumflex;
  trans_table[NIDK_Ediaeresis] = VEK_Ediaeresis;
  trans_table[NIDK_Igrave] = VEK_Igrave;
  trans_table[NIDK_Iacute] = VEK_Iacute;
  trans_table[NIDK_Icircumflex] = VEK_Icircumflex;
  trans_table[NIDK_Idiaeresis] = VEK_Idiaeresis;
  trans_table[NIDK_ETH] = VEK_ETH;
  trans_table[NIDK_Eth] = VEK_Eth;
  trans_table[NIDK_Ntilde] = VEK_Ntilde;
  trans_table[NIDK_Ograve] = VEK_Ograve;
  trans_table[NIDK_Oacute] = VEK_Oacute;
  trans_table[NIDK_Ocircumflex] = VEK_Ocircumflex;
  trans_table[NIDK_Otilde] = VEK_Otilde;
  trans_table[NIDK_Odiaeresis] = VEK_Odiaeresis;
  trans_table[NIDK_multiply] = VEK_multiply;
  trans_table[NIDK_Ooblique] = VEK_Ooblique;
  trans_table[NIDK_Ugrave] = VEK_Ugrave;
  trans_table[NIDK_Uacute] = VEK_Uacute;
  trans_table[NIDK_Ucircumflex] = VEK_Ucircumflex;
  trans_table[NIDK_Udiaeresis] = VEK_Udiaeresis;
  trans_table[NIDK_Yacute] = VEK_Yacute;
  trans_table[NIDK_THORN] = VEK_THORN;
  trans_table[NIDK_Thorn] = VEK_Thorn;
  trans_table[NIDK_ssharp] = VEK_ssharp;
  trans_table[NIDK_agrave] = VEK_agrave;
  trans_table[NIDK_aacute] = VEK_aacute;
  trans_table[NIDK_acircumflex] = VEK_acircumflex;
  trans_table[NIDK_atilde] = VEK_atilde;
  trans_table[NIDK_adiaeresis] = VEK_adiaeresis;
  trans_table[NIDK_aring] = VEK_aring;
  trans_table[NIDK_ae] = VEK_ae;
  trans_table[NIDK_ccedilla] = VEK_ccedilla;
  trans_table[NIDK_egrave] = VEK_egrave;
  trans_table[NIDK_eacute] = VEK_eacute;
  trans_table[NIDK_ecircumflex] = VEK_ecircumflex;
  trans_table[NIDK_ediaeresis] = VEK_ediaeresis;
  trans_table[NIDK_igrave] = VEK_igrave;
  trans_table[NIDK_iacute] = VEK_iacute;
  trans_table[NIDK_icircumflex] = VEK_icircumflex;
  trans_table[NIDK_idiaeresis] = VEK_idiaeresis;
  trans_table[NIDK_eth] = VEK_eth;
  trans_table[NIDK_ntilde] = VEK_ntilde;
  trans_table[NIDK_ograve] = VEK_ograve;
  trans_table[NIDK_oacute] = VEK_oacute;
  trans_table[NIDK_ocircumflex] = VEK_ocircumflex;
  trans_table[NIDK_otilde] = VEK_otilde;
  trans_table[NIDK_odiaeresis] = VEK_odiaeresis;
  trans_table[NIDK_division] = VEK_division;
  trans_table[NIDK_oslash] = VEK_oslash;
  trans_table[NIDK_ugrave] = VEK_ugrave;
  trans_table[NIDK_uacute] = VEK_uacute;
  trans_table[NIDK_ucircumflex] = VEK_ucircumflex;
  trans_table[NIDK_udiaeresis] = VEK_udiaeresis;
  trans_table[NIDK_yacute] = VEK_yacute;
  trans_table[NIDK_thorn] = VEK_thorn;
  trans_table[NIDK_ydiaeresis] = VEK_ydiaeresis;
  trans_table[NIDK_Aogonek] = VEK_Aogonek;
  trans_table[NIDK_breve] = VEK_breve;
  trans_table[NIDK_Lstroke] = VEK_Lstroke;
  trans_table[NIDK_Lcaron] = VEK_Lcaron;
  trans_table[NIDK_Sacute] = VEK_Sacute;
  trans_table[NIDK_Scaron] = VEK_Scaron;
  trans_table[NIDK_Scedilla] = VEK_Scedilla;
  trans_table[NIDK_Tcaron] = VEK_Tcaron;
  trans_table[NIDK_Zacute] = VEK_Zacute;
  trans_table[NIDK_Zcaron] = VEK_Zcaron;
  trans_table[NIDK_Zabovedot] = VEK_Zabovedot;
  trans_table[NIDK_aogonek] = VEK_aogonek;
  trans_table[NIDK_ogonek] = VEK_ogonek;
  trans_table[NIDK_lstroke] = VEK_lstroke;
  trans_table[NIDK_lcaron] = VEK_lcaron;
  trans_table[NIDK_sacute] = VEK_sacute;
  trans_table[NIDK_caron] = VEK_caron;
  trans_table[NIDK_scaron] = VEK_scaron;
  trans_table[NIDK_scedilla] = VEK_scedilla;
  trans_table[NIDK_tcaron] = VEK_tcaron;
  trans_table[NIDK_zacute] = VEK_zacute;
  trans_table[NIDK_doubleacute] = VEK_doubleacute;
  trans_table[NIDK_zcaron] = VEK_zcaron;
  trans_table[NIDK_zabovedot] = VEK_zabovedot;
  trans_table[NIDK_Racute] = VEK_Racute;
  trans_table[NIDK_Abreve] = VEK_Abreve;
  trans_table[NIDK_Lacute] = VEK_Lacute;
  trans_table[NIDK_Cacute] = VEK_Cacute;
  trans_table[NIDK_Ccaron] = VEK_Ccaron;
  trans_table[NIDK_Eogonek] = VEK_Eogonek;
  trans_table[NIDK_Ecaron] = VEK_Ecaron;
  trans_table[NIDK_Dcaron] = VEK_Dcaron;
  trans_table[NIDK_Dstroke] = VEK_Dstroke;
  trans_table[NIDK_Nacute] = VEK_Nacute;
  trans_table[NIDK_Ncaron] = VEK_Ncaron;
  trans_table[NIDK_Odoubleacute] = VEK_Odoubleacute;
  trans_table[NIDK_Rcaron] = VEK_Rcaron;
  trans_table[NIDK_Uring] = VEK_Uring;
  trans_table[NIDK_Udoubleacute] = VEK_Udoubleacute;
  trans_table[NIDK_Tcedilla] = VEK_Tcedilla;
  trans_table[NIDK_racute] = VEK_racute;
  trans_table[NIDK_abreve] = VEK_abreve;
  trans_table[NIDK_lacute] = VEK_lacute;
  trans_table[NIDK_cacute] = VEK_cacute;
  trans_table[NIDK_ccaron] = VEK_ccaron;
  trans_table[NIDK_eogonek] = VEK_eogonek;
  trans_table[NIDK_ecaron] = VEK_ecaron;
  trans_table[NIDK_dcaron] = VEK_dcaron;
  trans_table[NIDK_dstroke] = VEK_dstroke;
  trans_table[NIDK_nacute] = VEK_nacute;
  trans_table[NIDK_ncaron] = VEK_ncaron;
  trans_table[NIDK_odoubleacute] = VEK_odoubleacute;
  trans_table[NIDK_udoubleacute] = VEK_udoubleacute;
  trans_table[NIDK_rcaron] = VEK_rcaron;
  trans_table[NIDK_uring] = VEK_uring;
  trans_table[NIDK_tcedilla] = VEK_tcedilla;
  trans_table[NIDK_abovedot] = VEK_abovedot;
  trans_table[NIDK_Hstroke] = VEK_Hstroke;
  trans_table[NIDK_Hcircumflex] = VEK_Hcircumflex;
  trans_table[NIDK_Iabovedot] = VEK_Iabovedot;
  trans_table[NIDK_Gbreve] = VEK_Gbreve;
  trans_table[NIDK_Jcircumflex] = VEK_Jcircumflex;
  trans_table[NIDK_hstroke] = VEK_hstroke;
  trans_table[NIDK_hcircumflex] = VEK_hcircumflex;
  trans_table[NIDK_idotless] = VEK_idotless;
  trans_table[NIDK_gbreve] = VEK_gbreve;
  trans_table[NIDK_jcircumflex] = VEK_jcircumflex;
  trans_table[NIDK_Cabovedot] = VEK_Cabovedot;
  trans_table[NIDK_Ccircumflex] = VEK_Ccircumflex;
  trans_table[NIDK_Gabovedot] = VEK_Gabovedot;
  trans_table[NIDK_Gcircumflex] = VEK_Gcircumflex;
  trans_table[NIDK_Ubreve] = VEK_Ubreve;
  trans_table[NIDK_Scircumflex] = VEK_Scircumflex;
  trans_table[NIDK_cabovedot] = VEK_cabovedot;
  trans_table[NIDK_ccircumflex] = VEK_ccircumflex;
  trans_table[NIDK_gabovedot] = VEK_gabovedot;
  trans_table[NIDK_gcircumflex] = VEK_gcircumflex;
  trans_table[NIDK_ubreve] = VEK_ubreve;
  trans_table[NIDK_scircumflex] = VEK_scircumflex;
  trans_table[NIDK_kra] = VEK_kra;
  trans_table[NIDK_kappa] = VEK_kappa;
  trans_table[NIDK_Rcedilla] = VEK_Rcedilla;
  trans_table[NIDK_Itilde] = VEK_Itilde;
  trans_table[NIDK_Lcedilla] = VEK_Lcedilla;
  trans_table[NIDK_Emacron] = VEK_Emacron;
  trans_table[NIDK_Gcedilla] = VEK_Gcedilla;
  trans_table[NIDK_Tslash] = VEK_Tslash;
  trans_table[NIDK_rcedilla] = VEK_rcedilla;
  trans_table[NIDK_itilde] = VEK_itilde;
  trans_table[NIDK_lcedilla] = VEK_lcedilla;
  trans_table[NIDK_emacron] = VEK_emacron;
  trans_table[NIDK_gcedilla] = VEK_gcedilla;
  trans_table[NIDK_tslash] = VEK_tslash;
  trans_table[NIDK_ENG] = VEK_ENG;
  trans_table[NIDK_eng] = VEK_eng;
  trans_table[NIDK_Amacron] = VEK_Amacron;
  trans_table[NIDK_Iogonek] = VEK_Iogonek;
  trans_table[NIDK_Eabovedot] = VEK_Eabovedot;
  trans_table[NIDK_Imacron] = VEK_Imacron;
  trans_table[NIDK_Ncedilla] = VEK_Ncedilla;
  trans_table[NIDK_Omacron] = VEK_Omacron;
  trans_table[NIDK_Kcedilla] = VEK_Kcedilla;
  trans_table[NIDK_Uogonek] = VEK_Uogonek;
  trans_table[NIDK_Utilde] = VEK_Utilde;
  trans_table[NIDK_Umacron] = VEK_Umacron;
  trans_table[NIDK_amacron] = VEK_amacron;
  trans_table[NIDK_iogonek] = VEK_iogonek;
  trans_table[NIDK_eabovedot] = VEK_eabovedot;
  trans_table[NIDK_imacron] = VEK_imacron;
  trans_table[NIDK_ncedilla] = VEK_ncedilla;
  trans_table[NIDK_omacron] = VEK_omacron;
  trans_table[NIDK_kcedilla] = VEK_kcedilla;
  trans_table[NIDK_uogonek] = VEK_uogonek;
  trans_table[NIDK_utilde] = VEK_utilde;
  trans_table[NIDK_umacron] = VEK_umacron;
  trans_table_ready = 1;
}

static int lookup_key_code(int kc) {
  if (!trans_table_ready)
    init_trans_table();
  return trans_table[kc];
}

static void push_nid_event(VeiNid *vn, VeiNidElem *e) {
  VeDeviceEvent *ve;
  VeDeviceElement *el;

  el = veDeviceFindElem(vn->device->model,e->info.name);
  assert(el != NULL);
  /* oops! - can't use veDeviceEventCreate() here - the allocated
     content area gets clobbered by the far-more-useful copy below */
  ve = calloc(1,sizeof(VeDeviceEvent));
  assert(ve != NULL);
  ve->index = -1;
  ve->device = veDupString(vn->device->name);
  ve->elem = veDupString(e->info.name);
  ve->timestamp = e->state.timestamp;
  /* update model and fill in event */
  switch(el->content->type) {
  case VE_ELEM_TRIGGER:
    /* no data to fill in */
    break;
  case VE_ELEM_SWITCH:
    {
      VeDeviceE_Switch *sw = (VeDeviceE_Switch *)(el->content);
      sw->state = e->state.data.switch_;
    }
    break;
  case VE_ELEM_VALUATOR:
    {
      VeDeviceE_Valuator *val = (VeDeviceE_Valuator *)(el->content);
      val->value = e->state.data.valuator;
    }
    break;
  case VE_ELEM_VECTOR:
    {
      VeDeviceE_Vector *vec = (VeDeviceE_Vector *)(el->content);
      int k;
      assert(vec->size == e->state.data.vector.size);
      for(k = 0; k < vec->size; k++)
	vec->value[k] = e->state.data.vector.data[k];
    }
    break;
  case VE_ELEM_KEYBOARD:
    {
      VeDeviceE_Keyboard *kbd = (VeDeviceE_Keyboard *)(el->content);
      kbd->state = e->state.data.keyboard.state;
      kbd->key = lookup_key_code(e->state.data.keyboard.code);
      if (!kbd->key) {
	/* ignore this - unmapped key */
	veDeviceEventDestroy(ve);
	return;
      }
    }
    break;
  default:
    assert("invalid content type" == NULL);
    break;
  }
  ve->content = veDeviceEContentCopy(el->content);
  VE_DEBUG(3,("NID pushing event"));
  /* It's somebody else's problem now... */
  veDeviceInsertEvent(ve);
}

static void *nid_compressed_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeiNid *vn = (VeiNid *)(d->instance->idata);
  NidPacket *p;
  NidStateList *sl;
  int k;

  while(p = nidReceivePacket(vn->conn,-1)) {
    if (p->header.type == NID_PKT_EVENTS_AVAIL && 
	(sl = nidDumpEvents(vn->conn))) {
      VE_DEBUG(3,("NID: got packet"));
      vePfEvent(MODULE,"packet","%s",d->name);
      veLockFrame();
      for(k = 0; k < sl->num_states; k++) {
	VeiNidElem *e;
	/* make sure the server didn't report an element it
	   didn't mention before */
	assert(sl->states[k].elemid < vn->num_elements);
	assert(vn->elems[sl->states[k].elemid] != NULL);
	e = vn->elems[sl->states[k].elemid];
	e->state = sl->states[k];
	push_nid_event(vn,e);
      }
      veUnlockFrame();
      nidFreeStateList(sl);
    }
    nidFreePacket(p);
  }
  return NULL;
}

static void *nid_uncompressed_thread(void *v) {
  VeDevice *d = (VeDevice *)v;
  VeiNid *vn = (VeiNid *)(d->instance->idata);
  NidStateList *sl;
  int k;

  while (sl = nidNextEvents(vn->conn,1)) {
    VE_DEBUG(3,("NID: got packet"));
    vePfEvent(MODULE,"packet","%s",d->name);
    veLockFrame();
    do {
      for(k = 0; k < sl->num_states; k++) {
	VeiNidElem *e;
	/* make sure the server didn't report an element it
	   didn't mention before */
	assert(sl->states[k].elemid < vn->num_elements);
	assert(vn->elems[sl->states[k].elemid] != NULL);
	e = vn->elems[sl->states[k].elemid];
	e->state = sl->states[k];
	push_nid_event(vn,e);
      }
      nidFreeStateList(sl);
    } while (sl = nidNextEvents(vn->conn,0));
    veUnlockFrame();
  }
  return NULL;
}

static VeDevice *new_nid_driver(VeDeviceDriver *driver, VeDeviceDesc *desc,
				VeStrMap override) {
  /* decode options to figure out where to talk to */
  char *host, *p;
  char *devname, *devtype;
  int port, j, k, max, compress;
  VeiNid *vn;
  NidElementList *el;
  int device;
  VeDevice *d;
  VeDeviceInstance *i;

  i = veDeviceInstanceInit(driver,NULL,desc,override);

  if (!(host = veDeviceInstOption(i,"server"))) {
    veError(MODULE,"NID server not specified");
    return NULL;
  }
  if (!(p = veDeviceInstOption(i,"port")) || sscanf(p,"%d",&port) != 1)
    port = NID_DEFAULT_PORT;

  vn = malloc(sizeof(VeiNid));
  assert(vn != NULL);

  /* connect to NID server and look for the device we want */
  if ((vn->conn = nidOpen(host,port)) < 0) {
    veError(MODULE,"could not open NID connection");
    free(vn);
    return NULL;
  }

  if (!(p = veDeviceInstOption(i,"compress")) || sscanf(p,"%d",&compress) != 1)
    compress = 0;
  
  vn->compressed = 0;
  if (compress) {
    if (nidCompressEvents(vn->conn))
      veWarning(MODULE,"NID connection does not support compressed events - falling back on streaming");
    else
      vn->compressed = 1;
  }

  if (p = veDeviceInstOption(i,"transport")) {
    veNotice(MODULE,"changing transport to %s",p);
    if (nidChangeTransport(vn->conn,p))
      veWarning(MODULE,"failed to change transport on NID connection to %s",p);
  }

  if (!(devname = veDeviceInstOption(i,"devname")))
    devname = "*";
  if (!(devtype = veDeviceInstOption(i,"devtype")))
    devtype = "*";

  if (p = veDeviceInstOption(i,"update")) {
    veNotice(MODULE,"setting update rate to %s",p);
    if (nidSetValue(vn->conn,NID_VALUE_REFRESH_RATE,atoi(p)))
      veWarning(MODULE,"failed to set update rate to %s",p);
  }
  /* assuming that is okay, get list of elements for default device */
  {
    NidDeviceList *dl;
    if (!(dl = nidEnumDevices(vn->conn))) {
      veError(MODULE,"could not enum devices");
      free(vn);
      return NULL;
    }
    if (dl->num_devices <= 0) {
      veError(MODULE,"no devices on NID server");
      free(vn);
      return NULL;
    }
    /* choose the first device that matches the spec */
    device = -1;
    for(k = 0; k < dl->num_devices; k++) {
      VE_DEBUG(2,("NID: matching device: (%s:%s)",dl->devices[k].name,
		  dl->devices[k].type));
      if (glob_match(devname,dl->devices[k].name) &&
	  glob_match(devtype,dl->devices[k].type)) {
	device = dl->devices[k].devid;
	break;
      }
    }
    nidFreeDeviceList(dl);
  }
  if (device < 0) {
    veError(MODULE,"no match for device name/type (%s,%s) found on server",
	    devname, devtype);
    free(vn);
    return NULL;
  }

  if (!(el = nidEnumElements(vn->conn,device))) {
    veError(MODULE,"no elements in device %d on NID server", device);
    return NULL;
  }
  
  VE_DEBUG(2,("NID Element List: %d elements",el->num_elems));

  max = 0;
  for(k = 0; k < el->num_elems; k++) {
    VE_DEBUG(2,("NID Element: %s %d (max=%d)",el->elements[k].name,el->elements[k].elemid,max));
    if (el->elements[k].elemid > max)
      max = el->elements[k].elemid;
  }
  
  if (max <= 0) {
    veError(MODULE,"element ids are invalid (nothing > 0)");
    return NULL;
  }

  vn->num_elements = max+1;
  vn->elems = malloc(sizeof(VeiNidElem *)*(max+1));
  assert(vn->elems != NULL);

  /* now listen to those elements */
  {
    NidElementId *ids;
    NidStateList *sl;
    ids = malloc(vn->num_elements * sizeof(NidElementId));
    assert(ids != NULL);
    for(k = 0; k < el->num_elems; k++) {
      ids[k].devid = device;
      ids[k].elemid = el->elements[k].elemid;
      vn->elems[el->elements[k].elemid] = calloc(1,sizeof(VeiNidElem));
      assert(vn->elems[el->elements[k].elemid] != NULL);
      vn->elems[el->elements[k].elemid]->info = el->elements[k];
    }
    sl = nidQueryElements(vn->conn,ids,el->num_elems);
    for(k = 0; k < sl->num_states; k++) {
      vn->elems[sl->states[k].elemid]->state = sl->states[k];
    }
    nidListenElements(vn->conn,ids,el->num_elems);
    free(ids);
    nidFreeStateList(sl);
  }
  
  i->idata = (void *)vn;
  d = veDeviceCreate(desc->name);
  d->instance = i;
  d->model = veDeviceCreateModel();
  vn->device = d;

  /* Build VeDeviceModel for this device... */
  /* FUTURE PARANOIA:  Rewrite this to check for buffer overflows in the
     string (bloody C strings...) */
  for(k = 0; k < el->num_elems; k++) {
    char str[1024];
    switch(el->elements[k].type) {
    case NID_ELEM_TRIGGER:
      sprintf(str, "%s trigger", el->elements[k].name);
      break;
    case NID_ELEM_SWITCH:
      sprintf(str, "%s switch", el->elements[k].name);
      break;
    case NID_ELEM_VALUATOR:
      sprintf(str, "%s valuator %g %g", el->elements[k].name,
	      el->elements[k].min[0], el->elements[k].max[0]);
      break;
    case NID_ELEM_KEYBOARD: /* no models for keyboards */
      break;
    case NID_ELEM_VECTOR:
      sprintf(str, "%s vector %d", el->elements[k].name, 
	      el->elements[k].vsize);
      for(j = 0; j < el->elements[k].vsize; j++) {
	char str2[80];
	sprintf(str2," {%g %g}",el->elements[k].min[j],el->elements[k].max[j]);
	strcat(str,str2);
      }
      break;
    }
    veDeviceAddElemSpec(d->model,str);
  }
  nidFreeElementList(el);

  VE_DEBUG(2,("NID device created: %s",desc->name));
  /* enter thread */
  veThreadInitDelayed((vn->compressed ? nid_compressed_thread : nid_uncompressed_thread),d,0,0);
  return d;
}

static VeDeviceDriver nid_drv = {
  "nid", new_nid_driver
};

/* for dynamically loaded driver */
void VE_DRIVER_INIT(niddrv) (void) {
  veDeviceAddDriver(&nid_drv);
}
