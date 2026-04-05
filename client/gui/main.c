#include <gtk/gtk.h>
#include "../client_shared.h"

// Global state
int sock = -1;
int is_logged_in = 0;
int current_room = -1;
char my_username[50];

// UI Widgets
GtkWidget *window, *lbl_status, *txt_chat_log, *lbl_hint;
GtkWidget *entry_server_ip, *entry_user, *entry_pass, *entry_room, *entry_msg;
GtkWidget *box_ip, *box_auth, *box_room, *box_chat;

// --- UTILITIES ---

gboolean append_text(const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_chat_log));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, text, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
    return FALSE;
}

gboolean clear_log(gpointer data) {
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_chat_log)), "", -1);
    return FALSE;
}

void show_error(const char *msg) {
    char *full_msg = g_strdup_printf("System: Error - %s", msg);
    g_idle_add((GSourceFunc)append_text, full_msg);
}

// --- NETWORK HANDLERS ---

void *receiver(void *arg) {
    Packet pkt;
    while(sock != -1) {
        memset(&pkt, 0, sizeof(Packet));
        if(read(sock, &pkt, sizeof(Packet)) <= 0) {
            g_idle_add((GSourceFunc)append_text, "System: Disconnected.");
            sock = -1; break;
        }

        if(strcmp(pkt.cmd, CMD_MSG) == 0) {
            crypt_msg(pkt.data, AES_DECRYPT);
            char *sender = strcmp(pkt.arg1, my_username) == 0 ? "me" : pkt.arg1;
            char *log = g_strdup_printf("[%s]: %s", sender, pkt.data);
            g_idle_add((GSourceFunc)append_text, log);
        } 
        else if(strcmp(pkt.cmd, CMD_LOGIN_OK) == 0) {
            is_logged_in = 1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Logged in! Choose a room.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_ip);
            g_idle_add((GSourceFunc)gtk_widget_hide, box_auth);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_room);
        }
        else if(strcmp(pkt.cmd, CMD_REGISTER_OK) == 0) {
            g_idle_add((GSourceFunc)append_text, "System: Registered! You can now login.");
        }
        else if(strcmp(pkt.cmd, CMD_JOIN_OK) == 0 || strcmp(pkt.cmd, CMD_CREATE_OK) == 0) {
            current_room = 1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Entered room.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_room);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_chat);
        }
        else if(strstr(pkt.cmd, "_ERR")) {
            show_error(strlen(pkt.arg1) > 0 ? pkt.arg1 : "Action failed");
        }
        else if(strcmp(pkt.cmd, CMD_LEAVE_OK) == 0) {
            current_room = -1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Left room. Back to Hub.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_chat);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_room);
        }
    }
    return NULL;
}

// --- UI SIGNAL CALLBACKS ---

static void on_btn_check_clicked(GtkButton *btn, gpointer data) {
    if (sock != -1) close(sock);
    sock = connect_to_server(gtk_entry_get_text(GTK_ENTRY(entry_server_ip)));
    
    if (sock != -1) {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Status: Connected");
        pthread_t tid; pthread_create(&tid, NULL, receiver, NULL);
        gtk_widget_set_sensitive(box_auth, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Status: Connect Failed");
    }
}

static void on_btn_login_clicked(GtkButton *btn, gpointer data) {
    strncpy(my_username, gtk_entry_get_text(GTK_ENTRY(entry_user)), 49);
    send_packet(sock, CMD_LOGIN, my_username, gtk_entry_get_text(GTK_ENTRY(entry_pass)), NULL);
}

static void on_btn_reg_clicked(GtkButton *btn, gpointer data) {
    send_packet(sock, CMD_REGISTER, gtk_entry_get_text(GTK_ENTRY(entry_user)), gtk_entry_get_text(GTK_ENTRY(entry_pass)), NULL);
}

static void on_btn_join_clicked(GtkButton *btn, gpointer data) {
    send_packet(sock, CMD_JOIN, gtk_entry_get_text(GTK_ENTRY(entry_room)), NULL, NULL);
}

static void on_btn_create_clicked(GtkButton *btn, gpointer data) {
    send_packet(sock, CMD_CREATE, gtk_entry_get_text(GTK_ENTRY(entry_room)), NULL, NULL);
}

static void on_btn_send_clicked(GtkButton *btn, gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry_msg));
    if (strlen(text) == 0) return;

    if (strcmp(text, "/leave") == 0) {
        send_packet(sock, CMD_LEAVE, NULL, NULL, NULL);
    } else {
        char encrypted[256]; memset(encrypted, 0, 256);
        strncpy(encrypted, text, 255);
        crypt_msg(encrypted, AES_ENCRYPT);
        send_packet(sock, CMD_MSG, my_username, NULL, encrypted);
    }
    gtk_entry_set_text(GTK_ENTRY(entry_msg), "");
}

static void on_btn_leave_clicked(GtkButton *btn, gpointer data) {
    send_packet(sock, CMD_LEAVE, NULL, NULL, NULL);
}

// --- MAIN UI SETUP ---

int main(int argc, char *argv[]) {
    auto_load_driver();
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "AES Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // 1. IP Box
    box_ip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_ip, FALSE, FALSE, 0);
    entry_server_ip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_server_ip), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(box_ip), entry_server_ip, TRUE, TRUE, 0);
    GtkWidget *btn_check = gtk_button_new_with_label("Connect");
    g_signal_connect(btn_check, "clicked", G_CALLBACK(on_btn_check_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_ip), btn_check, FALSE, FALSE, 0);
    lbl_status = gtk_label_new("Status: Disconnected");
    gtk_box_pack_start(GTK_BOX(box_ip), lbl_status, FALSE, FALSE, 5);

    // 2. Auth Box
    box_auth = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_auth, FALSE, FALSE, 0);
    entry_user = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "Username");
    gtk_box_pack_start(GTK_BOX(box_auth), entry_user, FALSE, FALSE, 0);
    entry_pass = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry_pass), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(entry_pass), FALSE);
    g_signal_connect(entry_pass, "activate", G_CALLBACK(on_btn_login_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_auth), entry_pass, FALSE, FALSE, 0);
    
    GtkWidget *auth_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_auth), auth_btns, FALSE, FALSE, 0);
    GtkWidget *b_login = gtk_button_new_with_label("Login");
    g_signal_connect(b_login, "clicked", G_CALLBACK(on_btn_login_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(auth_btns), b_login, TRUE, TRUE, 0);
    GtkWidget *b_reg = gtk_button_new_with_label("Register");
    g_signal_connect(b_reg, "clicked", G_CALLBACK(on_btn_reg_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(auth_btns), b_reg, TRUE, TRUE, 0);
    gtk_widget_set_sensitive(box_auth, FALSE);

    // 3. Room Box
    box_room = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_room, FALSE, FALSE, 0);
    entry_room = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry_room), "Room name");
    g_signal_connect(entry_room, "activate", G_CALLBACK(on_btn_join_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_room), entry_room, FALSE, FALSE, 0);
    
    GtkWidget *room_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_room), room_btns, FALSE, FALSE, 0);
    GtkWidget *b_c = gtk_button_new_with_label("Create");
    g_signal_connect(b_c, "clicked", G_CALLBACK(on_btn_create_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(room_btns), b_c, TRUE, TRUE, 0);
    GtkWidget *b_j = gtk_button_new_with_label("Join");
    g_signal_connect(b_j, "clicked", G_CALLBACK(on_btn_join_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(room_btns), b_j, TRUE, TRUE, 0);
    gtk_widget_hide(box_room);

    // 4. Log / Chat Area
    txt_chat_log = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(txt_chat_log), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_chat_log), GTK_WRAP_WORD_CHAR);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), txt_chat_log);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);

    // 5. Chat Input Box
    box_chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_chat, FALSE, FALSE, 0);
    lbl_hint = gtk_label_new("Type /leave to exit");
    gtk_box_pack_start(GTK_BOX(box_chat), lbl_hint, FALSE, FALSE, 0);
    GtkWidget *m_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_chat), m_box, FALSE, FALSE, 0);
    entry_msg = gtk_entry_new();
    g_signal_connect(entry_msg, "activate", G_CALLBACK(on_btn_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(m_box), entry_msg, TRUE, TRUE, 0);
    GtkWidget *b_s = gtk_button_new_with_label("Send");
    g_signal_connect(b_s, "clicked", G_CALLBACK(on_btn_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(m_box), b_s, FALSE, FALSE, 0);
    GtkWidget *b_l = gtk_button_new_with_label("Leave Room");
    g_signal_connect(b_l, "clicked", G_CALLBACK(on_btn_leave_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_chat), b_l, FALSE, FALSE, 0);
    gtk_widget_hide(box_chat);

    gtk_widget_show_all(window);
    gtk_widget_hide(box_room);
    gtk_widget_hide(box_chat);
    gtk_main();
    return 0;
}
