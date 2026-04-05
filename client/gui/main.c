#include <gtk/gtk.h>
#include "../client_shared.h"

// Global state
int sock = -1;
int is_logged_in = 0;
int current_room = -1;
char my_username[50];

// UI Elements
GtkWidget *window;
GtkWidget *entry_server_ip;
GtkWidget *btn_check;
GtkWidget *lbl_status;
GtkWidget *txt_chat_log;
GtkWidget *entry_msg;
GtkWidget *btn_send;
GtkWidget *entry_user;
GtkWidget *entry_pass;
GtkWidget *entry_room;
GtkWidget *box_ip;
GtkWidget *box_auth;
GtkWidget *box_room;
GtkWidget *box_chat;
GtkWidget *lbl_hint;

// Thread safety
void append_text(const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_chat_log));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, text, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

gboolean clear_log(gpointer data) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_chat_log));
    gtk_text_buffer_set_text(buffer, "", -1);
    return FALSE; // Don't call again
}

// Network Receiver
void *receiver(void *arg) {
    Packet pkt;
    while(sock != -1) {
        memset(&pkt, 0, sizeof(Packet));
        int bytes_read = read(sock, &pkt, sizeof(Packet));
        if(bytes_read <= 0) {
            g_idle_add((GSourceFunc)append_text, "System: Disconnected from server.");
            sock = -1;
            break;
        }

        if(strcmp(pkt.cmd, "MSG") == 0) {
            crypt_msg(pkt.data, AES_DECRYPT);
            char *sender = strcmp(pkt.arg1, my_username) == 0 ? "me" : pkt.arg1;
            char *msg = g_strdup_printf("[%s]: %s", sender, pkt.data);
            g_idle_add((GSourceFunc)append_text, msg);
        } else if(strcmp(pkt.cmd, "REGISTER_OK") == 0) {
            g_idle_add((GSourceFunc)append_text, "System: Registration successful! You can now login.");
        } else if(strcmp(pkt.cmd, "REGISTER_ERR") == 0) {
            char *msg = g_strdup_printf("System: Registration failed: %s", pkt.arg1);
            g_idle_add((GSourceFunc)append_text, msg);
        } else if(strcmp(pkt.cmd, "LOGIN_OK") == 0) {
            is_logged_in = 1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Login successful! Welcome to the Room Hub.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_ip);
            g_idle_add((GSourceFunc)gtk_widget_hide, box_auth);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_room);
        } else if(strcmp(pkt.cmd, "CREATE_OK") == 0 || strcmp(pkt.cmd, "JOIN_OK") == 0) {
            current_room = 1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Entered room successfully.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_room);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_chat);
        } else if(strcmp(pkt.cmd, "CREATE_ERR") == 0 || strcmp(pkt.cmd, "JOIN_ERR") == 0) {
            const char *error = strlen(pkt.arg1) > 0 ? pkt.arg1 : "Room not found / Action failed";
            char *msg = g_strdup_printf("System: Error - %s", error);
            g_idle_add((GSourceFunc)append_text, msg);
        } else if(strcmp(pkt.cmd, "LEAVE_OK") == 0) {
            current_room = -1;
            g_idle_add((GSourceFunc)clear_log, NULL);
            g_idle_add((GSourceFunc)append_text, "System: Left the room. Back to Room Hub.");
            g_idle_add((GSourceFunc)gtk_widget_hide, box_chat);
            g_idle_add((GSourceFunc)gtk_widget_show_all, box_room);
        }
    }
    return NULL;
}

// UI Handlers
static void on_btn_check_clicked(GtkButton *btn, gpointer data) {
    const char *ip = gtk_entry_get_text(GTK_ENTRY(entry_server_ip));
    if (sock != -1) close(sock);

    sock = connect_to_server(ip);
    if (sock != -1) {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Connected!");
        pthread_t tid;
        pthread_create(&tid, NULL, receiver, NULL);
        gtk_widget_set_sensitive(box_auth, TRUE);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl_status), "Failed to connect.");
    }
}

static void on_btn_login_clicked(GtkButton *btn, gpointer data) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    strcpy(pkt.cmd, "LOGIN");
    strcpy(pkt.arg1, gtk_entry_get_text(GTK_ENTRY(entry_user)));
    strcpy(pkt.arg2, gtk_entry_get_text(GTK_ENTRY(entry_pass)));
    strcpy(my_username, pkt.arg1);
    send(sock, &pkt, sizeof(Packet), 0);
}

static void on_btn_reg_clicked(GtkButton *btn, gpointer data) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    strcpy(pkt.cmd, "REGISTER");
    strcpy(pkt.arg1, gtk_entry_get_text(GTK_ENTRY(entry_user)));
    strcpy(pkt.arg2, gtk_entry_get_text(GTK_ENTRY(entry_pass)));
    send(sock, &pkt, sizeof(Packet), 0);
}

static void on_btn_create_room_clicked(GtkButton *btn, gpointer data) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    strcpy(pkt.cmd, "CREATE");
    strcpy(pkt.arg1, gtk_entry_get_text(GTK_ENTRY(entry_room)));
    send(sock, &pkt, sizeof(Packet), 0);
}

static void on_btn_join_room_clicked(GtkButton *btn, gpointer data) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    strcpy(pkt.cmd, "JOIN");
    strcpy(pkt.arg1, gtk_entry_get_text(GTK_ENTRY(entry_room)));
    send(sock, &pkt, sizeof(Packet), 0);
}

static void on_btn_send_clicked(GtkButton *btn, gpointer data) {
    const char *input = gtk_entry_get_text(GTK_ENTRY(entry_msg));
    if (strlen(input) == 0) return;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    
    if (strcmp(input, "/leave") == 0) {
        strcpy(pkt.cmd, "LEAVE");
    } else {
        strcpy(pkt.cmd, "MSG");
        strcpy(pkt.arg1, my_username);
        strncpy(pkt.data, input, 255);
        crypt_msg(pkt.data, AES_ENCRYPT);
    }
    
    send(sock, &pkt, sizeof(Packet), 0);
    gtk_entry_set_text(GTK_ENTRY(entry_msg), "");
}

static void on_btn_leave_clicked(GtkButton *btn, gpointer data) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    strcpy(pkt.cmd, "LEAVE");
    send(sock, &pkt, sizeof(Packet), 0);
}

int main(int argc, char *argv[]) {
    auto_load_driver();
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Simple AES Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // 1. IP Box
    box_ip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_ip, FALSE, FALSE, 5);
    entry_server_ip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_server_ip), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(box_ip), entry_server_ip, TRUE, TRUE, 5);
    GtkWidget *btn_check = gtk_button_new_with_label("Check Server");
    g_signal_connect(btn_check, "clicked", G_CALLBACK(on_btn_check_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_ip), btn_check, FALSE, FALSE, 5);
    lbl_status = gtk_label_new("Disconnected");
    gtk_box_pack_start(GTK_BOX(box_ip), lbl_status, FALSE, FALSE, 5);

    // 2. Auth Box
    box_auth = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_auth, FALSE, FALSE, 5);
    entry_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "Username");
    gtk_box_pack_start(GTK_BOX(box_auth), entry_user, FALSE, FALSE, 5);
    entry_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_pass), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_pass), "Password");
    g_signal_connect(entry_pass, "activate", G_CALLBACK(on_btn_login_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_auth), entry_pass, FALSE, FALSE, 5);
    
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_auth), btn_box, FALSE, FALSE, 5);
    GtkWidget *btn_login = gtk_button_new_with_label("Login");
    g_signal_connect(btn_login, "clicked", G_CALLBACK(on_btn_login_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_login, TRUE, TRUE, 5);
    GtkWidget *btn_reg = gtk_button_new_with_label("Register");
    g_signal_connect(btn_reg, "clicked", G_CALLBACK(on_btn_reg_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_reg, TRUE, TRUE, 5);
    gtk_widget_set_sensitive(box_auth, FALSE);

    // 3. Room Box
    box_room = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_room, FALSE, FALSE, 5);
    entry_room = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_room), "Room name");
    g_signal_connect(entry_room, "activate", G_CALLBACK(on_btn_join_room_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_room), entry_room, FALSE, FALSE, 5);
    
    GtkWidget *room_btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_room), room_btn_box, FALSE, FALSE, 5);
    GtkWidget *btn_create = gtk_button_new_with_label("Create Room");
    g_signal_connect(btn_create, "clicked", G_CALLBACK(on_btn_create_room_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(room_btn_box), btn_create, TRUE, TRUE, 5);
    GtkWidget *btn_join = gtk_button_new_with_label("Join Room");
    g_signal_connect(btn_join, "clicked", G_CALLBACK(on_btn_join_room_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(room_btn_box), btn_join, TRUE, TRUE, 5);

    // 4. Chat Log (Always visible in middle)
    txt_chat_log = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(txt_chat_log), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_chat_log), GTK_WRAP_WORD_CHAR);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), txt_chat_log);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 5);

    // 5. Chat Input Box (Message entry + Send + Leave)
    box_chat = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), box_chat, FALSE, FALSE, 5);
    
    lbl_hint = gtk_label_new("Type /leave or click button to exit");
    gtk_box_pack_start(GTK_BOX(box_chat), lbl_hint, FALSE, FALSE, 2);

    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(box_chat), msg_box, FALSE, FALSE, 5);
    entry_msg = gtk_entry_new();
    g_signal_connect(entry_msg, "activate", G_CALLBACK(on_btn_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(msg_box), entry_msg, TRUE, TRUE, 5);
    GtkWidget *btn_send = gtk_button_new_with_label("Send");
    g_signal_connect(btn_send, "clicked", G_CALLBACK(on_btn_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(msg_box), btn_send, FALSE, FALSE, 5);
    
    GtkWidget *btn_leave = gtk_button_new_with_label("Leave Room");
    g_signal_connect(btn_leave, "clicked", G_CALLBACK(on_btn_leave_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box_chat), btn_leave, FALSE, FALSE, 5);

    // Initial Visibility
    gtk_widget_show_all(window);
    gtk_widget_hide(box_room);
    gtk_widget_hide(box_chat);

    gtk_main();
    return 0;
}
