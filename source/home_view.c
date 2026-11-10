#include "home_view.h"
#include "ui.h"
#include <stdio.h>

void home_view_draw(fpsu_lang lang, int selected, int full_frame)
{
    static const char *pt[] = {"Escanear todos os jogos", "Abrir biblioteca", "Escanear jogo montado", "Restaurar backup", "Verificar dependências", "Overclock", "Apoiar o projeto", "Atualizar com PC", "Atualizar online", "Modo escuro", "Sair"};
    static const char *en[] = {"Scan all games", "Open library", "Scan mounted game", "Restore backup", "Check dependencies", "Overclock", "Support the project", "Update with PC", "Update online", "Dark mode", "Exit"};
    static const char *es[] = {"Escanear juegos", "Abrir biblioteca", "Escanear montado", "Restaurar respaldo", "Verificar dependencias", "Overclock", "Apoyar el proyecto", "Actualizar con PC", "Actualizar en línea", "Modo oscuro", "Salir"};
    static const char *pt_desc[] = {
        "Encontre jogos instalados e confira os patches disponíveis.", "Abra a biblioteca salva, com capas e opções de FPS.",
        "Identifique o jogo que você montou pelo webMAN.", "Restaure o backup ou remova o patch de um jogo.",
        "Confira a conexão com webMAN e PS3MAPI.", "Ajuste o clock da GPU. Acompanhe a temperatura do console.",
        "Apoie os próximos testes com Pix ou PayPal.", "Recarregue os bancos enviados pelo PSUF PC Updater.",
        "Baixe e valide os bancos mais recentes do projeto.", "Alterne entre azul-petróleo/ciano e preto. A escolha fica salva.",
        "Feche o PSUF e retorne ao menu do PlayStation 3."
    };
    static const char *en_desc[] = {
        "Find installed games and check available patches.", "Open your saved library, with covers and FPS options.",
        "Identify the game mounted through webMAN.", "Restore a game's backup or remove its patch.",
        "Check the connection to webMAN and PS3MAPI.", "Adjust GPU clock. Keep an eye on console temperature.",
        "Support upcoming tests with Pix or PayPal.", "Reload databases sent through PSUF PC Updater.",
        "Download and validate the project's latest databases.", "Switch between teal/cyan and black. Your choice is saved.",
        "Close PSUF and return to the PlayStation 3 menu."
    };
    static const char *es_desc[] = {
        "Encuentra juegos instalados y consulta los parches disponibles.", "Abre tu biblioteca guardada, con carátulas y opciones de FPS.",
        "Identifica el juego montado mediante webMAN.", "Restaura el respaldo de un juego o elimina su parche.",
        "Comprueba la conexión con webMAN y PS3MAPI.", "Ajusta la frecuencia de la GPU. Vigila la temperatura de la consola.",
        "Apoya las próximas pruebas con Pix o PayPal.", "Recarga las bases de datos enviadas con PSUF PC Updater.",
        "Descarga y valida las últimas bases de datos del proyecto.", "Alterna entre azul petróleo/cian y negro. Tu elección se guarda.",
        "Cierra PSUF y vuelve al menú de PlayStation 3."
    };
    static const ui_icon icons[] = {
        UI_ICON_LIBRARY, UI_ICON_LIBRARY, UI_ICON_LIBRARY, UI_ICON_BACKUP,
        UI_ICON_HELP, UI_ICON_CHIP, UI_ICON_DONATE, UI_ICON_UPDATE, UI_ICON_UPDATE,
        UI_ICON_MOON,
        UI_ICON_EXIT
    };
    static float cursor = 0;
    static int previous = -1;
    static int motion_frames;
    static int previous_lift;
    const char **labels = lang == FPSU_LANG_PT ? pt : lang == FPSU_LANG_ES ? es : en;
    const char **desc = lang == FPSU_LANG_PT ? pt_desc : lang == FPSU_LANG_ES ? es_desc : en_desc;
    const int row_step = 41;
    int i, y, lift, old_marker, marker, first_row, last_row, selection_changed;
    char count[24];
    if (selected < 0 || selected >= FPSU_HOME_ITEMS) selected = 0;
    selection_changed = previous != selected;
    if (selection_changed) motion_frames = 8;
    old_marker = (int)(cursor * row_step);
    if (motion_frames > 0) {
        cursor += ((float)selected - cursor) * 0.34f;
        if (--motion_frames == 0) cursor = (float)selected;
    }
    marker = (int)(cursor * row_step);
    lift = motion_frames;

    if (full_frame) {
        ui_draw_panel(48, 112, 322, 524, 24, 0xff102631u, 0xff203a45u);
        ui_draw_text(72, 130, lang == FPSU_LANG_PT ? "Seu PlayStation, sem limites." : lang == FPSU_LANG_ES ? "Tu PlayStation, sin límites." : "Your PlayStation, unlocked.", UI_COLOR_MUTED, 1);
        ui_draw_panel(394, 112, 838, 524, 28, 0xff122c38u, 0xff294551u);
        ui_draw_round_rect(432, 324, 760, 214, 22, 0xff0a1d28u);
        ui_draw_round_rect(448, 340, 728, 182, 20, 0xff0d2330u);
        ui_draw_home_logo(448, 340, 728, 182);
        ui_draw_text(1014, 575, "CFW / HEN 4.90+", UI_COLOR_MUTED, 1);
    }
    first_row = (old_marker < marker ? old_marker : marker) / row_step;
    last_row = ((old_marker > marker ? old_marker : marker) + row_step - 2) / row_step;
    if (selection_changed && previous >= 0) {
        if (previous < first_row) first_row = previous;
        if (previous > last_row) last_row = previous;
        if (selected < first_row) first_row = selected;
        if (selected > last_row) last_row = selected;
    }
    if (full_frame) {
        first_row = 0;
        last_row = FPSU_HOME_ITEMS - 1;
    } else if (selection_changed || old_marker != marker) {
        int top = 165 + first_row * row_step;
        int height = (last_row - first_row + 1) * row_step;
        ui_fill_rect(60, top, 298, height, 0xff102631u);
    } else {
        last_row = -1;
    }
    if (last_row >= 0)
        ui_draw_round_rect(60, 166 + marker, 298, 35, 13, 0xff204653u);
    for (i = first_row; i <= last_row; ++i) {
        y = 169 + i * row_step;
        ui_draw_icon(67, y + 2, icons[i],
            selected == i ? UI_COLOR_ACCENT_2 : 0xff62919eu, selected == i ? 0xff204653u : 0xff102631u);
        ui_draw_text_fit(99, y + 1, labels[i], selected == i ? UI_COLOR_TEXT : UI_COLOR_MUTED, 2, 244);
        if (i == 9) {
            int dark = ui_is_dark();
            ui_draw_round_rect(300, y + 4, 43, 23, 12, dark ? UI_COLOR_ACCENT : 0xffaacdd4u);
            ui_draw_disc(dark ? 332 : 311, y + 15, 8, 0xfff2f7f7u);
        }
    }

    if (full_frame || selection_changed) {
        if (!full_frame) {
            ui_fill_rect(430, 140, 768, 42, 0xff122c38u);
        }
        ui_draw_text(432, 142, selected < 3 ? (lang == FPSU_LANG_EN ? "LIBRARY" : "BIBLIOTECA") :
            selected == 3 ? (lang == FPSU_LANG_ES ? "RESPALDOS" : "BACKUPS") :
            selected < 6 ? (lang == FPSU_LANG_ES ? "CONSOLA" : "CONSOLE") : "PSUF", UI_COLOR_ACCENT, 1);
        snprintf(count, sizeof(count), "%02d / %02d", selected + 1, FPSU_HOME_ITEMS);
        ui_draw_text(1120, 142, count, UI_COLOR_MUTED, 1);
    }
    if (full_frame || selection_changed || lift != previous_lift) {
        if (!full_frame) {
            ui_fill_rect(430, 185, 768, 138, 0xff122c38u);
        }
        ui_draw_text_fit(432, 188 + lift, labels[selected], UI_COLOR_TEXT, 4, 755);
        ui_draw_text_box(434, 248 + lift, desc[selected], UI_COLOR_MUTED, 2, 735, 29, 2);
    }
    previous = selected;
    previous_lift = lift;
}
