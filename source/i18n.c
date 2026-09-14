#include "i18n.h"

#include <sysutil/sysutil.h>

/* Some prebuilt PSL1GHT toolchains omit the language ID defines. */
#ifndef SYSUTIL_LANG_ENGLISH_US
#define SYSUTIL_LANG_ENGLISH_US 1
#endif

#ifndef SYSUTIL_LANG_PORTUGUESE_PT
#define SYSUTIL_LANG_PORTUGUESE_PT 7
#endif

#ifndef SYSUTIL_LANG_PORTUGUESE_BR
#define SYSUTIL_LANG_PORTUGUESE_BR 17
#endif

#ifndef SYSUTIL_LANG_SPANISH
#define SYSUTIL_LANG_SPANISH 3
#endif

static const char *g_text_en[TXT_MAX] = {
    "PSUF",
    "Scan installed games",
    "Scan mounted game",
    "Restore backup",
    "About",
    "Exit",
    "Compatible with CFW 4.90+ and PS3HEN.",
    "On HEN, enable HEN before opening this app.",
    "Project by RafJaeger.",
    "Analyzing games. Please wait.",
    "Scan finished.",
    "No games found.",
    "Available",
    "Pattern found, not tested",
    "No known pattern",
    "Advanced PS3 test",
    "Patch exists for another version",
    "Applicable",
    "Update required",
    "Unlock 60 FPS",
    "Unlock FPS / +60",
    "Unlock FPS / +60 (unsafe)",
    "Back",
    "Apply this patch?",
    "Restore this backup?",
    "A backup is required before changing files.",
    "Patch sent for testing.",
    "Could not prepare the patch.",
    "Backup restored.",
    "Could not restore backup.",
    "No backup created by this app was found.",
    "This option will test an experimental route on PS3.",
    "PSUF\n\nProject by RafJaeger.\n\nPSUF V2.5.28 for CFW 4.90+ and PS3HEN.\nNot every console is compatible right now, but we are looking for ways around that.\nUse System > Check dependencies if a patch installs but nothing changes.\nThis app avoids dev_flash, LV1, LV2 and boot plugins."
};

static const char *g_text_pt[TXT_MAX] = {
    "PSUF",
    "Scanear jogos instalados",
    "Scanear jogo montado",
    "Restaurar backup",
    "Sobre",
    "Sair",
    "Compativel com CFW 4.90+ e PS3HEN.",
    "No HEN, ative o HEN antes de abrir este app.",
    "Projeto de RafJaeger.",
    "Analisando jogos. Aguarde.",
    "Scan finalizado.",
    "Nenhum jogo encontrado.",
    "Disponivel",
    "Padrao encontrado, nao testado",
    "Sem padrao conhecido",
    "Teste avancado PS3",
    "Patch existe para outra versao",
    "Aplicavel",
    "Precisa atualizar",
    "Desbloquear 60 FPS",
    "Desbloquear FPS / +60",
    "Desbloquear FPS / +60 (nao seguro)",
    "Voltar",
    "Aplicar este patch?",
    "Restaurar este backup?",
    "Backup obrigatorio antes de alterar arquivos.",
    "Patch enviado para teste.",
    "Nao consegui preparar o patch.",
    "Backup restaurado.",
    "Nao foi possivel restaurar backup.",
    "Nenhum backup criado por este app foi encontrado.",
    "Esta opcao testa uma rota experimental no PS3.",
    "PSUF\n\nProjeto de RafJaeger.\n\nPSUF V2.5.28 para CFW 4.90+ e PS3HEN.\nNem todos os consoles sao compativeis no momento, mas estamos procurando formas de contornar isso.\nUse App > Sair ou Circle na tela inicial. Evite PS/Home > Sair do jogo se o console travar.\nEste app evita dev_flash, LV1, LV2 e boot plugins."
};

static const char *g_text_es[TXT_MAX] = {
    "PSUF",
    "Escanear juegos instalados",
    "Escanear juego montado",
    "Restaurar backup",
    "Acerca de",
    "Salir",
    "Compatible con CFW 4.90+ y PS3HEN.",
    "En HEN, activa HEN antes de abrir esta app.",
    "Proyecto de RafJaeger.",
    "Analizando juegos. Espera.",
    "Escaneo terminado.",
    "No se encontraron juegos.",
    "Disponible",
    "Patron encontrado, no probado",
    "Sin patron conocido",
    "Prueba avanzada PS3",
    "Hay patch para otra version",
    "Aplicable",
    "Necesita actualizar",
    "Desbloquear 60 FPS",
    "Desbloquear FPS / +60",
    "Desbloquear FPS / +60 (no seguro)",
    "Volver",
    "Aplicar este patch?",
    "Restaurar este backup?",
    "Backup obligatorio antes de cambiar archivos.",
    "Patch enviado para prueba.",
    "No se pudo preparar el patch.",
    "Backup restaurado.",
    "No se pudo restaurar el backup.",
    "No se encontro backup creado por esta app.",
    "Esta opcion prueba una ruta experimental en PS3.",
    "PSUF\n\nProyecto de RafJaeger.\n\nPSUF V2.5.28 para CFW 4.90+ y PS3HEN.\nNo todas las consolas son compatibles por ahora, pero estamos buscando formas de arreglarlo.\nUsa App > Salir o Circle en la pantalla inicial. Evita PS/Home > Salir del juego si la consola se congela.\nEsta app evita dev_flash, LV1, LV2 y boot plugins."
};

fpsu_lang i18n_detect_language(void)
{
    s32 lang = SYSUTIL_LANG_ENGLISH_US;
    if (sysUtilGetSystemParamInt(SYSUTIL_SYSTEMPARAM_ID_LANG, &lang) == 0) {
        if (lang == SYSUTIL_LANG_PORTUGUESE_BR || lang == SYSUTIL_LANG_PORTUGUESE_PT) {
            return FPSU_LANG_PT;
        }
        if (lang == SYSUTIL_LANG_SPANISH) {
            return FPSU_LANG_ES;
        }
    }
    return FPSU_LANG_EN;
}

const char *i18n_text(fpsu_lang lang, fpsu_text_id id)
{
    if (id < 0 || id >= TXT_MAX) {
        return "";
    }
    if (lang == FPSU_LANG_PT) {
        return g_text_pt[id];
    }
    if (lang == FPSU_LANG_ES) {
        return g_text_es[id];
    }
    return g_text_en[id];
}
