// rte_window.h
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_RTE_WINDOW_H
#define HAS_LIVES_RTE_WINDOW_H

#define RTE_INFO_WIDTH ((int)(550.*widget_opts.scale))
#define RTE_INFO_HEIGHT ((int)(400.*widget_opts.scale))

#define DEF_FX_KEYMODES "8" // keep as string

void rte_window_set_interactive(boolean interactive);

void check_string_choice_params(weed_plant_t *inst);

void on_assign_rte_keys_activate(LiVESMenuItem *, livespointer);
void on_rte_info_clicked(LiVESButton *, livespointer data);
void load_default_keymap(void);
void rtew_combo_set_text(int key, int mode, const char *txt);
void rtew_set_keych(int key, boolean on);
void rtew_set_key_check_state(void);
void rtew_set_keygr(int key);
void rtew_set_mode_radio(int key, int mode);
void rtew_set_grab_button(boolean on);
void update_pwindow(int key, int i, LiVESList *list);
boolean on_rtew_delete_event(LiVESWidget *widget, LiVESXEventDelete *event, livespointer user_data);

void rte_set_defs_activate(LiVESMenuItem *, livespointer user_data);
void rte_set_defs_cancel(LiVESButton *, lives_rfx_t *);
void rte_set_defs_ok(LiVESButton *, lives_rfx_t *);
void rte_reset_defs_clicked(LiVESButton *, lives_rfx_t *rfx);
void rte_set_key_defs(LiVESButton *, lives_rfx_t *);
void on_save_rte_defs_activate(LiVESMenuItem *, livespointer);
boolean on_clear_all_clicked(LiVESButton *, livespointer user_data);

void on_clear_clicked(LiVESButton *button, livespointer user_data);

boolean rte_window_hidden(void);

LiVESWidget *refresh_rte_window(void);

LiVESWidget *rte_window;


#endif // HAS_LIVES_RTE_WINDOW_H
