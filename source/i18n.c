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
    "Patch ready. Please wait.",
    "Applying patch.",
    "PSUF\n\nProject by RafJaeger.\n\nPSUF V3 for CFW 4.90+ and PS3HEN.\nNot every console is compatible right now, but we are looking for ways around that.\nUse Check dependencies if a patch installs but nothing changes.\nThis app avoids dev_flash, dev_blind, LV1, LV2 and boot plugins."
};

static const char *g_text_pt[TXT_MAX] = {
    "PSUF",
    "Escanear jogos instalados",
    "Escanear jogo montado",
    "Restaurar backup",
    "Sobre",
    "Sair",
    "Compatível com CFW 4.90+ e PS3HEN.",
    "No HEN, ative o HEN antes de abrir este app.",
    "Projeto de RafJaeger.",
    "Analisando jogos. Aguarde.",
    "Escaneamento concluído.",
    "Nenhum jogo encontrado.",
    "Disponível",
    "Padrão encontrado, não testado",
    "Sem padrão conhecido",
    "Teste avançado no PS3",
    "Existe patch para outra versão",
    "Aplicável",
    "Atualização necessária",
    "Desbloquear 60 FPS",
    "Desbloquear FPS / +60",
    "Desbloquear FPS / +60 (não seguro)",
    "Voltar",
    "Aplicar este patch?",
    "Restaurar este backup?",
    "É obrigatório criar um backup antes de alterar arquivos.",
    "Patch enviado para teste.",
    "Não foi possível preparar o patch.",
    "Backup restaurado.",
    "Não foi possível restaurar o backup.",
    "Nenhum backup criado por este app foi encontrado.",
    "Esta opção testa uma rota experimental no PS3.",
    "Patch pronto. Aguarde.",
    "Aplicando patch.",
    "PSUF\n\nProjeto de RafJaeger.\n\nPSUF V3 para CFW 4.90+ e PS3HEN.\nNem todos os consoles são compatíveis no momento, mas estamos procurando formas de contornar isso.\nUse Verificar dependências se o patch for instalado, mas nada mudar.\nEste app evita dev_flash, dev_blind, LV1, LV2 e boot plugins."
};

static const char *g_text_es[TXT_MAX] = {
    "PSUF",
    "Escanear juegos instalados",
    "Escanear juego montado",
    "Restaurar respaldo",
    "Acerca de",
    "Salir",
    "Compatible con CFW 4.90+ y PS3HEN.",
    "En HEN, activa HEN antes de abrir esta app.",
    "Proyecto de RafJaeger.",
    "Analizando juegos. Por favor, espera.",
    "Escaneo completado.",
    "No se encontraron juegos.",
    "Disponible",
    "Patrón encontrado, sin probar",
    "Sin patrón conocido",
    "Prueba avanzada en PS3",
    "Hay un parche para otra versión",
    "Aplicable",
    "Actualización necesaria",
    "Desbloquear 60 FPS",
    "Desbloquear FPS / +60",
    "Desbloquear FPS / +60 (no seguro)",
    "Volver",
    "¿Aplicar este parche?",
    "¿Restaurar este respaldo?",
    "Es obligatorio crear un respaldo antes de modificar archivos.",
    "Parche enviado para pruebas.",
    "No se pudo preparar el parche.",
    "Respaldo restaurado.",
    "No se pudo restaurar el respaldo.",
    "No se encontró ningún respaldo creado por esta app.",
    "Esta opción prueba una ruta experimental en PS3.",
    "Parche preparado. Espera.",
    "Aplicando parche.",
    "PSUF\n\nProyecto de RafJaeger.\n\nPSUF V3 para CFW 4.90+ y PS3HEN.\nNo todas las consolas son compatibles por ahora, pero estamos buscando soluciones.\nUsa Verificar dependencias si el parche se instala, pero nada cambia.\nEsta app evita dev_flash, dev_blind, LV1, LV2 y boot plugins."
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
