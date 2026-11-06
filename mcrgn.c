#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_STEPS 200

typedef enum {
    ACTION_MOVE,
    ACTION_CLICK,
    ACTION_KEY,
    ACTION_SLEEP
} ActionType;

typedef struct {
    ActionType type;
    int x, y;
    int button;
    char key[64];
    int delay_ms;
} Action;

static Action actions[MAX_STEPS];
static int action_count = 0;
static int editing_index = -1;

static GtkWidget *list_store;
static GtkWidget *tree_view;
static GtkWidget *entry_x, *entry_y, *entry_key, *entry_delay;
static GtkWidget *combo_click_type;
static GtkWidget *btn_add_move;

enum {
    COL_NUM,
    COL_DESC,
    COL_DELAY,
    COL_RAW_INDEX,
    NUM_COLS
};

static void update_list_view() {
    GtkListStore *store = GTK_LIST_STORE(list_store);
    gtk_list_store_clear(store);

    GtkTreeIter iter;
    for (int i = 0; i < action_count; i++) {
        char desc[128];
        char delay_str[32];

        switch (actions[i].type) {
            case ACTION_MOVE:
                snprintf(desc, sizeof(desc), "Move: X=%d, Y=%d", actions[i].x, actions[i].y);
                break;
            case ACTION_CLICK: {
                char *btn_name = (actions[i].button == 0x110) ? "Left" :
                                 (actions[i].button == 0x111) ? "Right" : "Middle";
                snprintf(desc, sizeof(desc), "Click: %s button", btn_name);
                break;
            }
            case ACTION_KEY:
                snprintf(desc, sizeof(desc), "Key: %s", actions[i].key);
                break;
            case ACTION_SLEEP:
                snprintf(desc, sizeof(desc), "Delay");
                break;
        }

        if (actions[i].delay_ms > 0) {
            snprintf(delay_str, sizeof(delay_str), "%d ms", actions[i].delay_ms);
        } else {
            snprintf(delay_str, sizeof(delay_str), "None");
        }

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_NUM, i + 1,
                           COL_DESC, desc,
                           COL_DELAY, delay_str,
                           COL_RAW_INDEX, i,
                           -1);
    }
}

static void on_pick_coords_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    sleep(3);

    FILE *fp = popen("hyprctl cursorpos 2>/dev/null || niri msg action cursor-position 2>/dev/null || slurp -p -b 00000000 -f '%x %y' 2>/dev/null", "r");
    if (fp) {
        int x = 0, y = 0;
        if (fscanf(fp, "%d %d", &x, &y) == 2 || fscanf(fp, "%d,%d", &x, &y) == 2) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", x);
            gtk_entry_set_text(GTK_ENTRY(entry_x), buf);
            snprintf(buf, sizeof(buf), "%d", y);
            gtk_entry_set_text(GTK_ENTRY(entry_y), buf);
        }
        pclose(fp);
    }
}

static void add_or_update_action(Action act) {
    const char *delay_txt = gtk_entry_get_text(GTK_ENTRY(entry_delay));
    act.delay_ms = atoi(delay_txt);

    if (editing_index >= 0 && editing_index < action_count) {
        actions[editing_index] = act;
        editing_index = -1;
        gtk_button_set_label(GTK_BUTTON(btn_add_move), "+ Add Move");
    } else {
        if (action_count >= MAX_STEPS) return;
        actions[action_count++] = act;
    }
    update_list_view();
}

static void on_add_move_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    Action act = { .type = ACTION_MOVE };
    act.x = atoi(gtk_entry_get_text(GTK_ENTRY(entry_x)));
    act.y = atoi(gtk_entry_get_text(GTK_ENTRY(entry_y)));
    add_or_update_action(act);
}

static void on_add_click_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    Action act = { .type = ACTION_CLICK };
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_click_type));
    if (idx == 0) act.button = 0x110;
    else if (idx == 1) act.button = 0x111;
    else act.button = 0x112;
    add_or_update_action(act);
}

static void on_add_key_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    Action act = { .type = ACTION_KEY };
    strncpy(act.key, gtk_entry_get_text(GTK_ENTRY(entry_key)), sizeof(act.key) - 1);
    add_or_update_action(act);
}

static void on_add_sleep_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    Action act = { .type = ACTION_SLEEP };
    add_or_update_action(act);
}

static void on_clear_clicked(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    action_count = 0;
    editing_index = -1;
    gtk_button_set_label(GTK_BUTTON(btn_add_move), "+ Add Move");
    update_list_view();
}

static void delete_action(int index) {
    if (index < 0 || index >= action_count) return;
    for (int i = index; i < action_count - 1; i++) {
        actions[i] = actions[i + 1];
    }
    action_count--;
    if (editing_index == index) {
        editing_index = -1;
        gtk_button_set_label(GTK_BUTTON(btn_add_move), "+ Add Move");
    } else if (editing_index > index) {
        editing_index--;
    }
    update_list_view();
}

static void start_editing(int index) {
    if (index < 0 || index >= action_count) return;
    editing_index = index;
    Action act = actions[index];

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", act.delay_ms);
    gtk_entry_set_text(GTK_ENTRY(entry_delay), buf);

    if (act.type == ACTION_MOVE) {
        snprintf(buf, sizeof(buf), "%d", act.x);
        gtk_entry_set_text(GTK_ENTRY(entry_x), buf);
        snprintf(buf, sizeof(buf), "%d", act.y);
        gtk_entry_set_text(GTK_ENTRY(entry_y), buf);
        gtk_button_set_label(GTK_BUTTON(btn_add_move), "💾 Save Move");
    } else if (act.type == ACTION_KEY) {
        gtk_entry_set_text(GTK_ENTRY(entry_key), act.key);
    } else if (act.type == ACTION_CLICK) {
        int idx = (act.button == 0x110) ? 0 : (act.button == 0x111) ? 1 : 2;
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_click_type), idx);
    }
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    (void)tree_view; (void)column; (void)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path)) {
        int raw_idx;
        gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, COL_RAW_INDEX, &raw_idx, -1);
        start_editing(raw_idx);
    }
}

static void menu_edit_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    int idx = GPOINTER_TO_INT(data);
    start_editing(idx);
}

static void menu_delete_cb(GtkWidget *widget, gpointer data) {
    (void)widget;
    int idx = GPOINTER_TO_INT(data);
    delete_action(idx);
}

static gboolean on_tree_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreePath *path;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL)) {
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                int raw_idx;
                gtk_tree_model_get(model, &iter, COL_RAW_INDEX, &raw_idx, -1);

                GtkWidget *menu = gtk_menu_new();
                GtkWidget *item_edit = gtk_menu_item_new_with_label("Edit");
                GtkWidget *item_delete = gtk_menu_item_new_with_label("Delete");

                g_signal_connect(item_edit, "activate", G_CALLBACK(menu_edit_cb), GINT_TO_POINTER(raw_idx));
                g_signal_connect(item_delete, "activate", G_CALLBACK(menu_delete_cb), GINT_TO_POINTER(raw_idx));

                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_edit);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);
                gtk_widget_show_all(menu);

                gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
                gtk_tree_path_free(path);
                return TRUE;
            }
            gtk_tree_path_free(path);
        }
    }
    return FALSE;
}

static void on_generate_clicked(GtkWidget *parent_window, gpointer data) {
    (void)data;
    if (action_count == 0) return;

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Fish Script",
                                                    GTK_WINDOW(parent_window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_current_name(chooser, "wayland_macro.fish");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(chooser);
        FILE *f = fopen(filename, "w");

        if (f) {
            fprintf(f, "#!/usr/bin/env fish\n\n");
            fprintf(f, "set -gx YDOTOOL_SOCKET /tmp/.ydotool_socket\n\n");

            fprintf(f, "sudo pkill ydotoold 2>/dev/null\n");
            fprintf(f, "sudo rm -f /tmp/.ydotool_socket\n\n");

            fprintf(f, "sudo ydotoold --socket-path=/tmp/.ydotool_socket --socket-perm=0666 >/dev/null 2>&1 &\n\n");

            fprintf(f, "set -l timeout 0\n");
            fprintf(f, "while not test -S /tmp/.ydotool_socket\n");
            fprintf(f, "    sleep 0.05\n");
            fprintf(f, "    set timeout (math $timeout + 1)\n");
            fprintf(f, "    if test $timeout -gt 30\n");
            fprintf(f, "        exit 1\n");
            fprintf(f, "    end\n");
            fprintf(f, "end\n");
            fprintf(f, "sleep 0.5\n\n");

            fprintf(f, "begin\n");

            for (int i = 0; i < action_count; i++) {
                switch (actions[i].type) {
                    case ACTION_MOVE:
                        fprintf(f, "    ydotool mousemove -a %d %d\n", actions[i].x, actions[i].y);
                        fprintf(f, "    sleep 0.1\n");
                        break;
                    case ACTION_CLICK:
                        fprintf(f, "    ydotool click 0x%x\n", actions[i].button);
                        fprintf(f, "    sleep 0.1\n");
                        break;
                    case ACTION_KEY:
                        fprintf(f, "    wtype \"%s\"\n", actions[i].key);
                        break;
                    case ACTION_SLEEP:
                        break;
                }

                if (actions[i].delay_ms > 0) {
                    double sec = (double)actions[i].delay_ms / 1000.0;
                    fprintf(f, "    sleep %.2f\n", sec);
                }
            }

            fprintf(f, "    echo \"Done\"\n");
            fprintf(f, "end\n\n");

            fprintf(f, "sleep 0.3\n");
            fprintf(f, "sudo pkill ydotoold 2>/dev/null\n");
            fprintf(f, "sudo rm -f /tmp/.ydotool_socket\n");

            fclose(f);
            chmod(filename, 0755);

            GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(parent_window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_MESSAGE_INFO,
                                                     GTK_BUTTONS_OK,
                                                     "Saved:\n%s", filename);
            gtk_dialog_run(GTK_DIALOG(info));
            gtk_widget_destroy(info);
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Macro Generator");
    gtk_window_set_default_size(GTK_WINDOW(window), 650, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), left_box, FALSE, FALSE, 0);

    GtkWidget *lbl_coords = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_coords), "<b>Mouse Move</b>");
    gtk_box_pack_start(GTK_BOX(left_box), lbl_coords, FALSE, FALSE, 0);

    GtkWidget *grid_coords = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid_coords), 5);
    entry_x = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(entry_x), "500");
    entry_y = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(entry_y), "300");
    gtk_grid_attach(GTK_GRID(grid_coords), gtk_label_new("X:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_coords), entry_x, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_coords), gtk_label_new("Y:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_coords), entry_y, 1, 1, 1, 1);
    gtk_box_pack_start(GTK_BOX(left_box), grid_coords, FALSE, FALSE, 0);

    GtkWidget *btn_pick = gtk_button_new_with_label("Pick Coords (3s)");
    g_signal_connect(btn_pick, "clicked", G_CALLBACK(on_pick_coords_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), btn_pick, FALSE, FALSE, 0);

    btn_add_move = gtk_button_new_with_label("+ Add Move");
    g_signal_connect(btn_add_move, "clicked", G_CALLBACK(on_add_move_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), btn_add_move, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    GtkWidget *lbl_click = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_click), "<b>Mouse Click</b>");
    gtk_box_pack_start(GTK_BOX(left_box), lbl_click, FALSE, FALSE, 0);

    combo_click_type = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_click_type), "Left");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_click_type), "Right");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_click_type), "Middle");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_click_type), 0);
    gtk_box_pack_start(GTK_BOX(left_box), combo_click_type, FALSE, FALSE, 0);

    GtkWidget *btn_add_click = gtk_button_new_with_label("+ Add Click");
    g_signal_connect(btn_add_click, "clicked", G_CALLBACK(on_add_click_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), btn_add_click, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    GtkWidget *lbl_key = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_key), "<b>Key / Text</b>");
    gtk_box_pack_start(GTK_BOX(left_box), lbl_key, FALSE, FALSE, 0);

    entry_key = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_key), "Return");
    gtk_box_pack_start(GTK_BOX(left_box), entry_key, FALSE, FALSE, 0);

    GtkWidget *btn_add_key = gtk_button_new_with_label("+ Add Key");
    g_signal_connect(btn_add_key, "clicked", G_CALLBACK(on_add_key_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), btn_add_key, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    GtkWidget *lbl_delay = gtk_label_new("Delay after action (ms):");
    gtk_box_pack_start(GTK_BOX(left_box), lbl_delay, FALSE, FALSE, 0);
    entry_delay = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_delay), "500");
    gtk_box_pack_start(GTK_BOX(left_box), entry_delay, FALSE, FALSE, 0);

    GtkWidget *btn_add_sleep = gtk_button_new_with_label("+ Add Delay");
    g_signal_connect(btn_add_sleep, "clicked", G_CALLBACK(on_add_sleep_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), btn_add_sleep, FALSE, FALSE, 0);

    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), right_box, TRUE, TRUE, 0);

    list_store = GTK_WIDGET(gtk_list_store_new(NUM_COLS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT));
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "#", renderer, "text", COL_NUM, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Action", renderer, "text", COL_DESC, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Delay", renderer, "text", COL_DELAY, NULL);

    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_tree_view_button_press), NULL);

    GtkWidget *scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_win), tree_view);
    gtk_box_pack_start(GTK_BOX(right_box), scrolled_win, TRUE, TRUE, 0);

    GtkWidget *action_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(right_box), action_bar, FALSE, FALSE, 0);

    GtkWidget *btn_clear = gtk_button_new_with_label("Clear All");
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(action_bar), btn_clear, FALSE, FALSE, 0);

    GtkWidget *btn_generate = gtk_button_new_with_label("Generate Script");
    g_signal_connect(btn_generate, "clicked", G_CALLBACK(on_generate_clicked), window);
    gtk_box_pack_start(GTK_BOX(action_bar), btn_generate, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
