#include "app.h"
#include "backup.h"
#include "cache.h"
#include "i18n.h"
#include "music.h"
#include "pad_input.h"
#include "paypal_qr.h"
#include "patch_db.h"
#include "pix_qr.h"
#include "scanner.h"
#include "ui.h"
#include "webman_control.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/memory.h>
#include <sysutil/sysutil.h>

static fpsu_game g_games[FPSU_MAX_GAMES];
static fpsu_game_result g_results[FPSU_MAX_GAMES];

#define FPSU_MAX_FORCE_CHOICES 12
#define FPSU_MAX_GRAPHICS_CHOICES 32
#define FPSU_SEARCH_MAX 64
#define FPSU_HOME_ITEMS 9
#define FPSU_CLOCK_MIN_MHZ 300
#define FPSU_CLOCK_STEP_MHZ 50
#define FPSU_CLOCK_DEFAULT_GPU_MHZ 500
#define FPSU_CLOCK_DEFAULT_VRAM_MHZ 650
#define FPSU_CLOCK_MAX_GPU_MHZ 750
#define FPSU_CLOCK_MAX_VRAM_MHZ 850
#define FPSU_CLOCK_MENU_ITEMS 5

static void mark_unavailable_without_files(fpsu_game_result *r);
static void draw_progress_bar(int x, int y, int w, int h, unsigned int value, unsigned int total, u32 color);
static void apply_result_overrides(fpsu_game_result *r);

static const char *tr(fpsu_lang lang, const char *pt, const char *en)
{
    if (lang == FPSU_LANG_PT) {
        return pt;
    }
    if (lang == FPSU_LANG_ES) {
        if (strcmp(pt, "Indisponivel") == 0) return "No disponible";
        if (strcmp(pt, "Ja roda a 60 FPS") == 0) return "Ya corre a 60 FPS";
        if (strcmp(pt, "60 FPS alvo") == 0) return "60 FPS fijo";
        if (strcmp(pt, "FPS ilimitado / +60") == 0) return "FPS ilimitado / +60";
        if (strcmp(pt, "FPS ilimitado / +60 nao seguro") == 0) return "FPS ilimitado / +60 no seguro";
        if (strcmp(pt, "30 FPS teste") == 0) return "30 FPS prueba";
        if (strcmp(pt, "Patch FPS") == 0) return "Patch FPS";
        if (strcmp(pt, "Modo seguro para PS3") == 0) return "Modo seguro para PS3";
        if (strcmp(pt, "Fechar") == 0) return "Cerrar";
        if (strcmp(pt, "Circle tambem volta") == 0) return "Circle tambien vuelve";
        if (strcmp(pt, "Confirmar acao") == 0) return "Confirmar accion";
        if (strcmp(pt, "Aplicar") == 0) return "Aplicar";
        if (strcmp(pt, "Cancelar") == 0) return "Cancelar";
        if (strcmp(pt, "X aplica esta acao") == 0) return "X aplica esta accion";
        if (strcmp(pt, "Circle cancela") == 0) return "Circle cancela";
        if (strcmp(pt, "Aplicando patch") == 0) return "Aplicando patch";
        if (strcmp(pt, "Aguarde, gravando arquivos pequenos") == 0) return "Espera, guardando archivos pequenos";
        if (strcmp(pt, "Nao desligue durante esta etapa") == 0) return "No apagues durante esta etapa";
        if (strcmp(pt, "Biblioteca > Scanear todos os jogos") == 0) return "Biblioteca > Escanear todos los juegos";
        if (strcmp(pt, "Biblioteca > Jogos scaneados") == 0) return "Biblioteca > Juegos escaneados";
        if (strcmp(pt, "Biblioteca > Scanear jogo montado") == 0) return "Biblioteca > Escanear juego montado";
        if (strcmp(pt, "Backups > Restaurar por jogo") == 0) return "Backups > Restaurar por juego";
        if (strcmp(pt, "Sistema > Verificar dependencias") == 0) return "Sistema > Verificar dependencias";
        if (strcmp(pt, "webMAN > Overclock") == 0) return "webMAN > Overclock";
        if (strcmp(pt, "App > Apoiar projeto") == 0) return "App > Apoyar proyecto";
        if (strcmp(pt, "App > Atualizar com PC") == 0) return "App > Actualizar con PC";
        if (strcmp(pt, "App > Sair") == 0) return "App > Salir";
        if (strcmp(pt, "Lista jogos > compara patches > analisa EBOOT quando precisa.") == 0) return "Lista juegos > compara patches > analiza EBOOT cuando hace falta.";
        if (strcmp(pt, "Abre o ultimo scan salvo sem varrer o HD de novo.") == 0) return "Abre el ultimo escaneo guardado sin leer el HD de nuevo.";
        if (strcmp(pt, "Usa o ultimo jogo montado pelo webMAN, sem abrir /dev_bdvd.") == 0) return "Usa el ultimo juego montado por webMAN, sin leer todo /dev_bdvd.";
        if (strcmp(pt, "Escolha um jogo > restaure so o backup dele.") == 0) return "Elige un juego > restaura solo su backup.";
        if (strcmp(pt, "Prepara pastas e verifica webMAN, PS3MAPI e Artemis.") == 0) return "Prepara carpetas y verifica webMAN, PS3MAPI y Artemis.";
        if (strcmp(pt, "Overclock para tentar ganhar FPS; fique de olho na temperatura.") == 0) return "Overclock para intentar ganar FPS; mira la temperatura.";
        if (strcmp(pt, "Pix e PayPal para ajudar os proximos testes.") == 0) return "Pix y PayPal para ayudar las proximas pruebas.";
        if (strcmp(pt, "Baixa o banco do GitHub do projeto e valida antes de trocar.") == 0) return "Baja la base del GitHub del proyecto y la valida antes de cambiarla.";
        if (strcmp(pt, "Use o PSUF PC Updater para enviar bancos de FPS e graficos.") == 0) return "Usa PSUF PC Updater para enviar bases de FPS y graficos.";
        if (strcmp(pt, "Fecha do jeito seguro. Evite PS/Home > Sair do jogo.") == 0) return "Cierra de forma segura. Evita PS/Home > Salir del juego.";
        if (strcmp(pt, "UP/DOWN navegar   X selecionar") == 0) return "UP/DOWN navegar   X seleccionar";
        if (strcmp(pt, "Triangle Sobre   Circle Sair") == 0) return "Triangle Acerca de   Circle Salir";
        if (strcmp(pt, "Buscar jogo") == 0) return "Buscar juego";
        if (strcmp(pt, "Pesquisar") == 0) return "Buscar";
        if (strcmp(pt, "Busca") == 0) return "Busqueda";
        if (strcmp(pt, "(vazio)") == 0) return "(vacio)";
        if (strcmp(pt, "Digite parte do nome, Title ID ou versao.") == 0) return "Escribe parte del nombre, Title ID o version.";
        if (strcmp(pt, "Digite nome, Title ID ou versao.") == 0) return "Escribe nombre, Title ID o version.";
        if (strcmp(pt, "X tecla\nSquare apagar\nStart confirmar") == 0) return "X tecla\nSquare borrar\nStart confirmar";
        if (strcmp(pt, "Setas escolher   X letra   Square apagar") == 0) return "Flechas elegir   X letra   Square borrar";
        if (strcmp(pt, "Setas escolher   X tecla   Square apagar") == 0) return "Flechas elegir   X tecla   Square borrar";
        if (strcmp(pt, "Start confirmar   Select limpar   Circle cancelar") == 0) return "Start confirmar   Select limpiar   Circle cancelar";
        if (strcmp(pt, "Start/OK confirma   Circle cancela") == 0) return "Start/OK confirma   Circle cancela";
        if (strcmp(pt, "Biblioteca > Jogos encontrados") == 0) return "Biblioteca > Juegos encontrados";
        if (strcmp(pt, "sem filtro") == 0) return "sin filtro";
        if (strcmp(pt, "Nenhum jogo encontrado nessa busca.") == 0) return "No se encontro ningun juego en esta busqueda.";
        if (strcmp(pt, "Triangle muda a busca. Select limpa.") == 0) return "Triangle cambia la busqueda. Select limpia.";
        if (strcmp(pt, "Triangle buscar   Select limpar") == 0) return "Triangle buscar   Select limpiar";
        if (strcmp(pt, "Circle voltar") == 0) return "Circle volver";
        if (strcmp(pt, "Jogo > Acoes") == 0) return "Juego > Acciones";
        if (strcmp(pt, "Jogo > Acoes > 60 FPS alvo") == 0) return "Juego > Acciones > 60 FPS fijo";
        if (strcmp(pt, "Jogo > Acoes > FPS ilimitado / +60") == 0) return "Juego > Acciones > FPS ilimitado / +60";
        if (strcmp(pt, "Jogo > Acoes > FPS ilimitado / +60 (nao seguro)") == 0) return "Juego > Acciones > FPS ilimitado / +60 (no seguro)";
        if (strcmp(pt, "Jogo > Acoes > 30 FPS") == 0) return "Juego > Acciones > 30 FPS";
        if (strcmp(pt, "Jogo > Acoes > 30 FPS (teste)") == 0) return "Juego > Acciones > 30 FPS (prueba)";
        if (strcmp(pt, "Jogo > Acoes > Patches extras/graficos (teste)") == 0) return "Juego > Acciones > Patches extra/graficos (prueba)";
        if (strcmp(pt, "Lista efeitos visuais e outros codigos do banco separado.") == 0) return "Lista efectos visuales y otros codigos de la base separada.";
        if (strcmp(pt, "Nenhum patch extra/grafico para este Title ID.") == 0) return "No hay patch extra/grafico para este Title ID.";
        if (strcmp(pt, "Jogo > Teste > Escolher/forcar rota (NAO SEGURO)") == 0) return "Juego > Prueba > Elegir/forzar ruta (NO SEGURO)";
        if (strcmp(pt, "Mostra rotas prontas do banco. Nao analisa EBOOT.") == 0) return "Muestra rutas listas de la base. No analiza EBOOT.";
        if (strcmp(pt, "Jogo > Analise > Analisar este jogo") == 0) return "Juego > Analisis > Analizar este juego";
        if (strcmp(pt, "Le o EBOOT deste jogo e tenta transformar em Aplicavel.") == 0) return "Lee el EBOOT de este juego e intenta volverlo aplicable.";
        if (strcmp(pt, "Jogo > Backup > Restaurar este jogo") == 0) return "Juego > Backup > Restaurar este juego";
        if (strcmp(pt, "Restaura somente o backup deste Title ID.") == 0) return "Restaura solo el backup de este Title ID.";
        if (strcmp(pt, "Voltar > Biblioteca") == 0) return "Volver > Biblioteca";
        if (strcmp(pt, "UP/DOWN navegar   X executar opcao") == 0) return "UP/DOWN navegar   X ejecutar opcion";
        if (strcmp(pt, "Nenhum PARAM.SFO de jogo PS3 foi localizado.") == 0) return "No se encontro ningun PARAM.SFO de juego PS3.";
        if (strcmp(pt, "Nenhum scan salvo") == 0) return "No hay escaneo guardado";
        if (strcmp(pt, "Use Biblioteca > Scanear todos os jogos uma vez. Depois o scan montado so adiciona/atualiza o jogo atual.") == 0) return "Usa Biblioteca > Escanear todos los juegos una vez. Despues el escaneo montado solo agrega/actualiza el juego actual.";
        if (strcmp(pt, "Jogo montado nao encontrado") == 0) return "Juego montado no encontrado";
        if (strcmp(pt, "Jogo montado nao identificado") == 0) return "Juego montado no identificado";
        if (strcmp(pt, "Biblioteca vazia") == 0) return "Biblioteca vacia";
        if (strcmp(pt, "Atualizando biblioteca") == 0) return "Actualizando biblioteca";
        if (strcmp(pt, "Biblioteca atualizada") == 0) return "Biblioteca actualizada";
        if (strcmp(pt, "Projeto de RafJaeger") == 0) return "Proyecto de RafJaeger";
        if (strcmp(pt, "Verificando dependencias") == 0) return "Verificando dependencias";
        if (strcmp(pt, "Verifica webMAN, PS3MAPI e Artemis um por vez, com pausa para nao travar.") == 0) return "Verifica webMAN, PS3MAPI y Artemis uno por uno, con pausa para no congelar.";
        if (strcmp(pt, "Etapa 1/4: preparando as pastas do PSUF.") == 0) return "Paso 1/4: preparando las carpetas de PSUF.";
        if (strcmp(pt, "Pastas OK. Aguardando um pouco antes da proxima etapa.") == 0) return "Carpetas OK. Esperando un poco antes del siguiente paso.";
        if (strcmp(pt, "Nao consegui preparar todas as pastas. Vou continuar a verificacao.") == 0) return "No pude preparar todas las carpetas. Voy a continuar la verificacion.";
        if (strcmp(pt, "Etapa 2/4: verificando se o webMAN responde.") == 0) return "Paso 2/4: verificando si webMAN responde.";
        if (strcmp(pt, "webMAN respondeu. Aguardando antes de iniciar o PS3MAPI.") == 0) return "webMAN respondio. Esperando antes de iniciar PS3MAPI.";
        if (strcmp(pt, "webMAN nao respondeu. Vou mostrar o que faltou no final.") == 0) return "webMAN no respondio. Voy a mostrar lo que falta al final.";
        if (strcmp(pt, "Etapa 3/4: ativando PS3MAPI pelo webMAN.") == 0) return "Paso 3/4: activando PS3MAPI por webMAN.";
        if (strcmp(pt, "PS3MAPI respondeu. Aguardando antes de verificar Artemis.") == 0) return "PS3MAPI respondio. Esperando antes de verificar Artemis.";
        if (strcmp(pt, "PS3MAPI nao respondeu. Vou mostrar o que faltou no final.") == 0) return "PS3MAPI no respondio. Voy a mostrar lo que falta al final.";
        if (strcmp(pt, "Etapa 4/4: verificando Artemis pelo webMAN.") == 0) return "Paso 4/4: verificando Artemis por webMAN.";
        if (strcmp(pt, "Artemis respondeu. Finalizando verificacao.") == 0) return "Artemis respondio. Terminando la verificacion.";
        if (strcmp(pt, "Artemis nao respondeu. Vou mostrar o que faltou no final.") == 0) return "Artemis no respondio. Voy a mostrar lo que falta al final.";
        if (strcmp(pt, "nao verificado porque o webMAN nao respondeu") == 0) return "no verificado porque webMAN no respondio";
        if (strcmp(pt, "Preparando PSUF e verificando webMAN/PS3MAPI/Artemis.") == 0) return "Preparando PSUF y verificando webMAN/PS3MAPI/Artemis.";
        if (strcmp(pt, "seu PS3 nao tem webMAN disponivel no sistema. instale para verificar novamente") == 0) return "tu PS3 no tiene webMAN disponible en el sistema. instala para verificar de nuevo";
        if (strcmp(pt, "seu PS3 nao tem PS3MAPI disponivel no sistema. instale webMAN MOD com PS3MAPI e verifique novamente") == 0) return "tu PS3 no tiene PS3MAPI disponible en el sistema. instala webMAN MOD con PS3MAPI y verifica de nuevo";
        if (strcmp(pt, "seu PS3 nao tem Artemis disponivel no sistema. instale para verificar novamente") == 0) return "tu PS3 no tiene Artemis disponible en el sistema. instala para verificar de nuevo";
        if (strcmp(pt, "Pastas PSUF") == 0) return "Carpetas PSUF";
        if (strcmp(pt, "falhou") == 0) return "fallo";
        if (strcmp(pt, "Tudo que o PSUF usa respondeu agora.") == 0) return "Todo lo que PSUF usa respondio ahora.";
        if (strcmp(pt, "Se faltar algo, instale/ative no PS3 e volte aqui para verificar novamente.") == 0) return "Si falta algo, instalalo/activalo en el PS3 y vuelve aqui para verificar de nuevo.";
        if (strcmp(pt, "Dependencias prontas") == 0) return "Dependencias listas";
        if (strcmp(pt, "Dependencia faltando") == 0) return "Falta una dependencia";
        if (strcmp(pt, "Essa tela nao aplica patch em jogo. Ela so prepara e verifica. O teste real continua ao aplicar um patch.") == 0) return "Esta pantalla no aplica patch a un juego. Solo prepara y verifica. La prueba real sigue al aplicar un patch.";
        if (strcmp(pt, "Codigo existe no banco") == 0) return "El codigo existe en la base";
        if (strcmp(pt, "A analise nao achou padrao novo neste EBOOT, mas existe codigo salvo para este jogo, outra versao ou outra regiao. Use Escolher/forcar rota se quiser testar.") == 0) return "El analisis no encontro un patron nuevo en este EBOOT, pero hay codigo guardado para este juego, otra version u otra region. Usa Elegir/forzar ruta si quieres probar.";
        if (strcmp(pt, "Analise desativada no jogo montado") == 0) return "Analisis desactivado en juego montado";
        if (strcmp(pt, "Para evitar travamento, o PSUF nao le EBOOT/ISO inteiro no jogo montado. Ele so usa metadados e patches conhecidos.") == 0) return "Para evitar congelamientos, PSUF no lee el EBOOT/ISO entero del juego montado. Solo usa metadatos y patches conocidos.";
        if (strcmp(pt, "Prepara pastas, tenta iniciar os servicos e faz um teste no proprio PSUF.") == 0) return "Prepara carpetas, intenta iniciar los servicios y hace una prueba en PSUF.";
        if (strcmp(pt, "Preparando PSUF, verificando servicos e testando escrita em memoria.") == 0) return "Preparando PSUF, verificando servicios y probando escritura en memoria.";
        if (strcmp(pt, "Teste PSUF") == 0) return "Prueba PSUF";
        if (strcmp(pt, "nao rodou porque o PS3MAPI nao respondeu") == 0) return "no se ejecuto porque PS3MAPI no respondio";
        if (strcmp(pt, "Teste PSUF OK: escrita de memoria funcionou") == 0) return "Prueba PSUF OK: la escritura de memoria funciono";
        if (strcmp(pt, "Teste PSUF falhou: PS3MAPI nao aceitou escrita") == 0) return "Prueba PSUF fallo: PS3MAPI no acepto la escritura";
        if (strcmp(pt, "Teste PSUF falhou: PS3MAPI respondeu, mas a memoria nao mudou") == 0) return "Prueba PSUF fallo: PS3MAPI respondio, pero la memoria no cambio";
        if (strcmp(pt, "O teste nao fica salvo. Ele so muda uma variavel temporaria do PSUF e volta ao normal.") == 0) return "La prueba no queda guardada. Solo cambia una variable temporal de PSUF y vuelve a la normalidad.";
        if (strcmp(pt, "Abrindo teclado") == 0) return "Abriendo teclado";
        if (strcmp(pt, "Use o teclado do PS3 para pesquisar por nome, Title ID ou versao.") == 0) return "Usa el teclado del PS3 para buscar por nombre, Title ID o version.";
        if (strcmp(pt, "Atualizando online") == 0) return "Actualizando online";
        if (strcmp(pt, "Baixando banco do GitHub e validando antes de trocar.") == 0) return "Bajando la base de GitHub y validando antes de cambiarla.";
        if (strcmp(pt, "Banco online atualizado") == 0) return "Base online actualizada";
        if (strcmp(pt, "Banco online atualizado. Abra Jogos scaneados para conferir.") == 0) return "Base online actualizada. Abre Juegos escaneados para revisar.";
        if (strcmp(pt, "Banco online atualizado. Agora escaneie jogos ou abra Jogos scaneados.") == 0) return "Base online actualizada. Ahora escanea juegos o abre Juegos escaneados.";
        if (strcmp(pt, "Atualizacao online falhou") == 0) return "Fallo la actualizacion online";
        if (strcmp(pt, "update_url.txt nao esta configurado. Publique o banco no GitHub e gere o PKG de novo.") == 0) return "update_url.txt no esta configurado. Publica la base en GitHub y genera el PKG de nuevo.";
        if (strcmp(pt, "Nao baixou ou validou o banco online. O banco antigo foi mantido.") == 0) return "No se pudo bajar o validar la base online. La base anterior se mantuvo.";
        if (strcmp(pt, "Detalhe") == 0) return "Detalle";
        if (strcmp(pt, "Sem rota automatica") == 0) return "Sin ruta automatica";
        if (strcmp(pt, "O forcar por padrao automatico hoje funciona para 60 FPS e FPS ilimitado / +60.") == 0) return "El forzado automatico por patron hoy funciona para 60 FPS y FPS ilimitado / +60.";
        if (strcmp(pt, "Precisa de EBOOT local") == 0) return "Necesita EBOOT local";
        if (strcmp(pt, "Para gerar um patch provavel, o PSUF precisa ler o EBOOT de uma instalacao em pasta/HDD. Em ISO/jogo montado, use um codigo ja existente no banco.") == 0) return "Para generar un patch probable, PSUF necesita leer el EBOOT de una instalacion en carpeta/HDD. En ISO/juego montado, usa un codigo que ya exista en la base.";
        if (strcmp(pt, "Forcar cancelado") == 0) return "Forzado cancelado";
        if (strcmp(pt, "Nada foi alterado neste jogo.") == 0) return "No se cambio nada en este juego.";
        if (strcmp(pt, "Sem padrao aplicavel") == 0) return "Sin patron aplicable";
        if (strcmp(pt, "O PSUF tentou gerar um patch provavel, mas nao achou valor conhecido para transformar em NCL.") == 0) return "PSUF intento generar un patch probable, pero no encontro un valor conocido para convertir en NCL.";
        if (strcmp(pt, "Nao achei rota pronta no banco para este jogo.") == 0) return "No encontre una ruta lista en la base para este juego.";
        if (strcmp(pt, "mais provavel") == 0) return "mas probable";
        if (strcmp(pt, "alternativa") == 0) return "alternativa";
        if (strcmp(pt, "Forcar este patch") == 0) return "Forzar este patch";
        if (strcmp(pt, "NAO SEGURO. Se der tela preta, restaure o backup ou remova o patch pelo PSUF.") == 0) return "NO SEGURO. Si da pantalla negra, restaura el backup o quita el patch con PSUF.";
        if (strcmp(pt, "Projeto de RafJaeger.\n\nPSUF V2.5.28 para CFW 4.90+ e PS3HEN.\nNem todos os consoles sao compativeis no momento, mas estamos procurando formas de contornar isso.\nUse Sistema > Verificar dependencias se o patch aplica mas nao muda nada.\nNao mexe em dev_flash, dev_blind, LV1, LV2 ou boot plugins.") == 0) return "Proyecto de RafJaeger.\n\nPSUF V2.5.28 para CFW 4.90+ y PS3HEN.\nNo todas las consolas son compatibles por ahora, pero estamos buscando formas de arreglarlo.\nUsa Sistema > Verificar dependencias si el patch se instala pero no cambia nada.\nNo toca dev_flash, dev_blind, LV1, LV2 ni boot plugins.";
    }
    return en;
}

static u32 frame_input(void)
{
    music_update();
    ui_pump();
    usleep(16000);
    if (ui_exit_requested()) {
        return FPSU_BUTTON_CIRCLE;
    }
    return pad_input_read_active();
}

static int ensure_dependency_dir(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0') {
        return -1;
    }
    if (stat(path, &st) == 0) {
        return 0;
    }
    return mkdir(path, 0777);
}

static int prepare_dependency_dirs(void)
{
    int ok = 1;

    ok = ensure_dependency_dir(FPSU_DATA_ROOT) == 0 && ok;
    ok = ensure_dependency_dir(FPSU_CACHE_ROOT) == 0 && ok;
    ok = ensure_dependency_dir(FPSU_BACKUP_ROOT) == 0 && ok;
    ok = ensure_dependency_dir("/dev_hdd0/tmp/artemis") == 0 && ok;
    ok = ensure_dependency_dir("/dev_hdd0/tmp/wm_ingame") == 0 && ok;
    ok = ensure_dependency_dir(FPSU_DATA_ROOT "/patches") == 0 && ok;
    return ok ? 0 : -1;
}

static const char *status_text(fpsu_lang lang, fpsu_patch_status status)
{
    switch (status) {
    case FPSU_STATUS_KNOWN:
        return i18n_text(lang, TXT_STATUS_AVAILABLE);
    case FPSU_STATUS_UNTESTED:
        return i18n_text(lang, TXT_STATUS_UNTESTED);
    case FPSU_STATUS_PC_REQUIRED:
        return i18n_text(lang, TXT_STATUS_PC_REQUIRED);
    case FPSU_STATUS_OTHER_VERSION:
        return i18n_text(lang, TXT_STATUS_OTHER_VERSION);
    case FPSU_STATUS_APPLICABLE:
        return i18n_text(lang, TXT_STATUS_APPLICABLE);
    case FPSU_STATUS_UNAVAILABLE:
        return tr(lang, "Indisponivel", "Unavailable");
    case FPSU_STATUS_NATIVE_60:
        return tr(lang, "Ja roda a 60 FPS", "Already runs at 60 FPS");
    case FPSU_STATUS_UPDATE_REQUIRED:
        return i18n_text(lang, TXT_STATUS_UPDATE_REQUIRED);
    default:
        return i18n_text(lang, TXT_STATUS_NOT_FOUND);
    }
}

static u32 status_color(fpsu_patch_status status)
{
    switch (status) {
    case FPSU_STATUS_KNOWN:
        return UI_COLOR_GREEN;
    case FPSU_STATUS_UNTESTED:
        return UI_COLOR_ACCENT_2;
    case FPSU_STATUS_PC_REQUIRED:
        return UI_COLOR_ACCENT_2;
    case FPSU_STATUS_OTHER_VERSION:
        return UI_COLOR_ACCENT_2;
    case FPSU_STATUS_APPLICABLE:
        return UI_COLOR_GREEN;
    case FPSU_STATUS_UNAVAILABLE:
        return UI_COLOR_RED;
    case FPSU_STATUS_NATIVE_60:
        return UI_COLOR_GREEN;
    case FPSU_STATUS_UPDATE_REQUIRED:
        return UI_COLOR_ACCENT_2;
    default:
        return UI_COLOR_ACCENT_2;
    }
}

static const char *patch_kind_title(fpsu_lang lang, fpsu_patch_kind kind, fpsu_patch_status status)
{
    switch (kind) {
    case FPSU_PATCH_60:
        return tr(lang, "60 FPS alvo", "60 FPS target");
    case FPSU_PATCH_UNLOCK:
        return status == FPSU_STATUS_KNOWN ?
            tr(lang, "FPS ilimitado / +60", "Unlimited / +60 FPS") :
            tr(lang, "FPS ilimitado / +60 nao seguro", "Unlimited / +60 FPS unsafe");
    case FPSU_PATCH_30:
        return status == FPSU_STATUS_KNOWN ?
            tr(lang, "30 FPS", "30 FPS") :
            tr(lang, "30 FPS teste", "30 FPS test");
    default:
        return tr(lang, "Patch FPS", "FPS patch");
    }
}

static void draw_button_hint(int x, int y, const char *button, const char *label, u32 color)
{
    ui_fill_rect(x, y, 38, 28, 0xff203244u);
    ui_draw_text(x + 12, y + 7, button, color, 2);
    ui_draw_text(x + 48, y + 8, label, UI_COLOR_MUTED, 2);
}

static void draw_compact_menu_item(int x, int y, int w, const char *title, const char *body,
    int selected, int enabled)
{
    const int h = 54;
    u32 panel = selected ? 0xff24465au : UI_COLOR_PANEL;
    u32 stripe = selected ? UI_COLOR_ACCENT_2 : UI_COLOR_ACCENT;
    u32 title_color = enabled ? UI_COLOR_TEXT : UI_COLOR_DISABLED;
    u32 body_color = enabled ? UI_COLOR_MUTED : UI_COLOR_DISABLED;

    ui_fill_rect(x, y, w, h, panel);
    ui_fill_rect(x, y, 7, h, stripe);
    if (selected) {
        ui_fill_rect(x + 7, y, w - 7, 2, UI_COLOR_ACCENT_2);
        ui_fill_rect(x + 7, y + h - 2, w - 7, 2, UI_COLOR_ACCENT_2);
    }
    ui_draw_text(x + 24, y + 8, title, title_color, 2);
    if (body && body[0]) {
        ui_draw_text(x + 24, y + 32, body, body_color, 1);
    }
}

static void draw_multiline_text(int x, int y, const char *text, u32 color, int scale, int max_chars, int max_lines)
{
    char line[128];
    int line_len = 0;
    int lines = 0;
    const char *p;

    if (!text || max_chars <= 0 || max_lines <= 0) {
        return;
    }

    for (p = text; *p && lines < max_lines; ++p) {
        if (*p == '\n' || line_len >= max_chars) {
            line[line_len] = '\0';
            ui_draw_text(x, y + lines * (scale >= 2 ? 34 : 22), line, color, scale);
            ++lines;
            line_len = 0;
            if (*p == '\n') {
                continue;
            }
        }
        if (line_len + 1 < (int)sizeof(line)) {
            line[line_len++] = *p;
        }
    }
    if (line_len > 0 && lines < max_lines) {
        line[line_len] = '\0';
        ui_draw_text(x, y + lines * (scale >= 2 ? 34 : 22), line, color, scale);
    }
}

static void draw_notice(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Modo seguro para PS3", "Safe mode for PS3"), FPSU_VERSION_LABEL);
    ui_fill_rect(96, 152, ui_screen_width() - 192, 318, UI_COLOR_PANEL);
    ui_fill_rect(96, 152, 9, 318, accent);
    ui_draw_text(130, 190, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 250, body, UI_COLOR_MUTED, 2, 54, 4);
    draw_button_hint(130, 402, "X", tr(lang, "Fechar", "Close"), UI_COLOR_ACCENT);
    ui_draw_footer(tr(lang, "Projeto de RafJaeger", "Project by RafJaeger"), tr(lang, "Circle tambem volta", "Circle also goes back"));
}

static void draw_details_notice(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Modo seguro para PS3", "Safe mode for PS3"), FPSU_VERSION_LABEL);
    ui_fill_rect(96, 128, ui_screen_width() - 192, 366, UI_COLOR_PANEL);
    ui_fill_rect(96, 128, 9, 366, accent);
    ui_draw_text(130, 162, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 222, body, UI_COLOR_MUTED, 1, 78, 8);
    draw_button_hint(130, 438, "X", tr(lang, "Fechar", "Close"), UI_COLOR_ACCENT);
    ui_draw_footer(tr(lang, "Projeto de RafJaeger", "Project by RafJaeger"), tr(lang, "Circle tambem volta", "Circle also goes back"));
}

static void draw_confirm_notice(fpsu_lang lang, const char *title, const char *body)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Confirmar acao", "Confirm action"), FPSU_VERSION_LABEL);
    ui_fill_rect(96, 152, ui_screen_width() - 192, 340, UI_COLOR_PANEL);
    ui_fill_rect(96, 152, 9, 340, UI_COLOR_ORANGE);
    ui_draw_text(130, 190, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 250, body, UI_COLOR_MUTED, 2, 54, 4);
    draw_button_hint(130, 414, "X", tr(lang, "Aplicar", "Apply"), UI_COLOR_GREEN);
    draw_button_hint(320, 414, "O", tr(lang, "Cancelar", "Cancel"), UI_COLOR_RED);
    ui_draw_footer(tr(lang, "X aplica esta acao", "X applies this action"),
        tr(lang, "Circle cancela", "Circle cancels"));
}

static void draw_busy_notice(fpsu_lang lang, const char *title, const char *body)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Aplicando patch", "Applying patch"), FPSU_VERSION_LABEL);
    ui_fill_rect(96, 152, ui_screen_width() - 192, 318, UI_COLOR_PANEL);
    ui_fill_rect(96, 152, 9, 318, UI_COLOR_ACCENT_2);
    ui_draw_text(130, 190, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 250, body, UI_COLOR_MUTED, 2, 54, 3);
    draw_progress_bar(132, 354, ui_screen_width() - 264, 20, 1, 2, UI_COLOR_ACCENT_2);
    ui_draw_footer(tr(lang, "Aguarde, gravando arquivos pequenos", "Please wait, writing small files"),
        tr(lang, "Nao desligue durante esta etapa", "Do not power off during this step"));
    ui_present();
    ui_pump();
    music_update();
}

static void dependency_pause(fpsu_lang lang, const char *body, int seconds)
{
    int i;
    int frames;

    if (seconds < 1) {
        seconds = 1;
    }
    frames = seconds * 4;
    for (i = 0; i < frames; ++i) {
        draw_busy_notice(lang, tr(lang, "Verificando dependencias", "Checking dependencies"), body);
        usleep(250000);
        ui_pump();
        music_update();
    }
}

static void app_notice(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    for (;;) {
        u32 btn;
        draw_notice(lang, title, body, accent);
        ui_present();
        btn = frame_input();
        if (btn & (FPSU_BUTTON_CROSS | FPSU_BUTTON_CIRCLE | FPSU_BUTTON_TRIANGLE)) {
            return;
        }
    }
}

static void app_notice_details(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    for (;;) {
        u32 btn;
        draw_details_notice(lang, title, body, accent);
        ui_present();
        btn = frame_input();
        if (btn & (FPSU_BUTTON_CROSS | FPSU_BUTTON_CIRCLE | FPSU_BUTTON_TRIANGLE)) {
            return;
        }
    }
}

static int app_confirm(fpsu_lang lang, const char *title, const char *body)
{
    for (;;) {
        u32 btn;
        draw_confirm_notice(lang, title, body);
        ui_present();
        btn = frame_input();
        if (btn & FPSU_BUTTON_CROSS) {
            return 1;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return 0;
        }
    }
}

static void draw_home(fpsu_lang lang, int selected)
{
    const char *titles[FPSU_HOME_ITEMS];
    const char *bodies[FPSU_HOME_ITEMS];
    int i;
    int left = 82;
    int top = 98;
    int width = ui_screen_width() - 164;

    titles[0] = tr(lang, "Biblioteca > Scanear todos os jogos", "Library > Scan all games");
    titles[1] = tr(lang, "Biblioteca > Jogos scaneados", "Library > Scanned games");
    titles[2] = tr(lang, "Biblioteca > Scanear jogo montado", "Library > Scan mounted game");
    titles[3] = tr(lang, "Backups > Restaurar por jogo", "Backups > Restore by game");
    titles[4] = tr(lang, "Sistema > Verificar dependencias", "System > Check dependencies");
    titles[5] = tr(lang, "webMAN > Overclock", "webMAN > Overclock");
    titles[6] = tr(lang, "App > Apoiar projeto", "App > Donate");
    titles[7] = tr(lang, "App > Atualizar com PC", "App > Update with PC");
    titles[8] = tr(lang, "App > Sair", "App > Exit");

    bodies[0] = tr(lang, "Lista jogos > compara patches > analisa EBOOT quando precisa.", "Lists games > matches patches > analyzes EBOOT when needed.");
    bodies[1] = tr(lang, "Abre o ultimo scan salvo sem varrer o HD de novo.", "Opens the last saved scan without scanning HDD again.");
    bodies[2] = tr(lang, "Usa o ultimo jogo montado pelo webMAN, sem abrir /dev_bdvd.", "Uses the last webMAN-mounted game without opening /dev_bdvd.");
    bodies[3] = tr(lang, "Escolha um jogo > restaure so o backup dele.", "Choose one game > restore only its backup.");
    bodies[4] = tr(lang, "Verifica webMAN, PS3MAPI e Artemis um por vez, com pausa para nao travar.",
        "Checks webMAN, PS3MAPI, and Artemis one by one, with a pause to avoid freezing.");
    bodies[5] = tr(lang, "Overclock para tentar ganhar FPS; fique de olho na temperatura.", "Overclock to try gaining FPS; keep an eye on temperature.");
    bodies[6] = tr(lang, "Pix e PayPal para ajudar os proximos testes.", "Pix and PayPal to support the next tests.");
    bodies[7] = tr(lang, "Use o PSUF PC Updater para enviar bancos de FPS e graficos.", "Use PSUF PC Updater to send FPS and graphics databases.");
    bodies[8] = tr(lang, "Fecha do jeito seguro. Evite PS/Home > Sair do jogo.", "Safe exit. Avoid PS/Home > Quit Game.");

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), i18n_text(lang, TXT_COMPAT_LINE), tr(lang, "Projeto de RafJaeger", "Project by RafJaeger"));

    ui_fill_rect(82, 94, width, 1, 0xff2f4657u);
    for (i = 0; i < FPSU_HOME_ITEMS; ++i) {
        draw_compact_menu_item(left, top + i * 56, width, titles[i], bodies[i], selected == i, 1);
    }

    ui_draw_footer(tr(lang, "UP/DOWN navegar   X selecionar", "UP/DOWN navigate   X select"),
        tr(lang, "Triangle Sobre   Circle Sair", "Triangle About   Circle Exit"));
    ui_present();
}

static void activate_dependencies(fpsu_lang lang)
{
    char webman_msg[160];
    char ps3mapi_msg[192];
    char artemis_msg[160];
    char body[1280];
    int dirs_ok;
    int webman_ok;
    int ps3mapi_ok;
    int artemis_ok;

    draw_busy_notice(lang,
        tr(lang, "Verificando dependencias", "Checking dependencies"),
        tr(lang, "Etapa 1/4: preparando as pastas do PSUF.",
            "Step 1/4: preparing PSUF folders."));

    dirs_ok = prepare_dependency_dirs() == 0;
    draw_busy_notice(lang,
        tr(lang, "Verificando dependencias", "Checking dependencies"),
        dirs_ok ?
            tr(lang, "Pastas OK. Aguardando um pouco antes da proxima etapa.",
                "Folders OK. Waiting a moment before the next step.") :
            tr(lang, "Nao consegui preparar todas as pastas. Vou continuar a verificacao.",
                "Could not prepare every folder. Continuing the check."));
    dependency_pause(lang,
        dirs_ok ?
            tr(lang, "Pastas OK. Aguardando um pouco antes da proxima etapa.",
                "Folders OK. Waiting a moment before the next step.") :
            tr(lang, "Nao consegui preparar todas as pastas. Vou continuar a verificacao.",
                "Could not prepare every folder. Continuing the check."),
        2);

    draw_busy_notice(lang,
        tr(lang, "Verificando dependencias", "Checking dependencies"),
        tr(lang, "Etapa 2/4: verificando se o webMAN responde.",
            "Step 2/4: checking if webMAN answers."));
    webman_ok = webman_check_available(webman_msg, sizeof(webman_msg)) == 0;
    draw_busy_notice(lang,
        tr(lang, "Verificando dependencias", "Checking dependencies"),
        webman_ok ?
            tr(lang, "webMAN respondeu. Aguardando antes de iniciar o PS3MAPI.",
                "webMAN answered. Waiting before starting PS3MAPI.") :
            tr(lang, "webMAN nao respondeu. Vou mostrar o que faltou no final.",
                "webMAN did not answer. The missing item will be shown at the end."));
    dependency_pause(lang,
        webman_ok ?
            tr(lang, "webMAN respondeu. Aguardando antes de iniciar o PS3MAPI.",
                "webMAN answered. Waiting before starting PS3MAPI.") :
            tr(lang, "webMAN nao respondeu. Vou mostrar o que faltou no final.",
                "webMAN did not answer. The missing item will be shown at the end."),
        2);

    ps3mapi_ok = 0;
    artemis_ok = 0;

    if (webman_ok) {
        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            tr(lang, "Etapa 3/4: ativando PS3MAPI pelo webMAN.",
                "Step 3/4: enabling PS3MAPI through webMAN."));
        ps3mapi_ok = webman_start_ps3mapi(ps3mapi_msg, sizeof(ps3mapi_msg)) == 0;
        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            ps3mapi_ok ?
                tr(lang, "PS3MAPI respondeu. Aguardando antes de verificar Artemis.",
                    "PS3MAPI answered. Waiting before checking Artemis.") :
                tr(lang, "PS3MAPI nao respondeu. Vou mostrar o que faltou no final.",
                    "PS3MAPI did not answer. The missing item will be shown at the end."));
        dependency_pause(lang,
            ps3mapi_ok ?
                tr(lang, "PS3MAPI respondeu. Aguardando antes de verificar Artemis.",
                    "PS3MAPI answered. Waiting before checking Artemis.") :
                tr(lang, "PS3MAPI nao respondeu. Vou mostrar o que faltou no final.",
                    "PS3MAPI did not answer. The missing item will be shown at the end."),
            3);

        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            tr(lang, "Etapa 4/4: verificando Artemis pelo webMAN.",
                "Step 4/4: checking Artemis through webMAN."));
        artemis_ok = webman_check_artemis(artemis_msg, sizeof(artemis_msg)) == 0;
        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            artemis_ok ?
                tr(lang, "Artemis respondeu. Finalizando verificacao.",
                    "Artemis answered. Finishing check.") :
                tr(lang, "Artemis nao respondeu. Vou mostrar o que faltou no final.",
                    "Artemis did not answer. The missing item will be shown at the end."));
        dependency_pause(lang,
            artemis_ok ?
                tr(lang, "Artemis respondeu. Finalizando verificacao.",
                    "Artemis answered. Finishing check.") :
                tr(lang, "Artemis nao respondeu. Vou mostrar o que faltou no final.",
                    "Artemis did not answer. The missing item will be shown at the end."),
            2);
    } else {
        snprintf(ps3mapi_msg, sizeof(ps3mapi_msg), "%s",
            tr(lang, "nao verificado porque o webMAN nao respondeu",
                "not checked because webMAN did not answer"));
        snprintf(artemis_msg, sizeof(artemis_msg), "%s",
            tr(lang, "nao verificado porque o webMAN nao respondeu",
                "not checked because webMAN did not answer"));
    }

    if (!webman_ok) {
        snprintf(webman_msg, sizeof(webman_msg), "%s",
            tr(lang, "seu PS3 nao tem webMAN disponivel no sistema. instale para verificar novamente",
                "your PS3 does not have webMAN available in the system. install it and check again"));
    }
    if (!ps3mapi_ok) {
        snprintf(ps3mapi_msg, sizeof(ps3mapi_msg), "%s",
            tr(lang, "seu PS3 nao tem PS3MAPI disponivel no sistema. instale webMAN MOD com PS3MAPI e verifique novamente",
                "your PS3 does not have PS3MAPI available in the system. install webMAN MOD with PS3MAPI and check again"));
    }
    if (!artemis_ok) {
        snprintf(artemis_msg, sizeof(artemis_msg), "%s",
            tr(lang, "seu PS3 nao tem Artemis disponivel no sistema. instale para verificar novamente",
                "your PS3 does not have Artemis available in the system. install it and check again"));
    }

    snprintf(body, sizeof(body),
        "%s: %s\nwebMAN: %s\nPS3MAPI: %s\nArtemis: %s\n\n%s\n%s",
        tr(lang, "Pastas PSUF", "PSUF folders"),
        dirs_ok ? "OK" : tr(lang, "falhou", "failed"),
        webman_ok ? "OK" : webman_msg,
        ps3mapi_ok ? "OK" : ps3mapi_msg,
        artemis_ok ? "OK" : artemis_msg,
        (dirs_ok && webman_ok && ps3mapi_ok && artemis_ok) ?
            tr(lang, "Tudo que o PSUF usa respondeu agora.", "Everything PSUF uses responded now.") :
            tr(lang, "Se faltar algo, instale/ative no PS3 e volte aqui para verificar novamente.",
                "If something is missing, install/enable it on the PS3 and come back here to check again."),
        tr(lang, "Essa tela nao aplica patch em jogo. Ela so prepara e verifica. O teste real continua ao aplicar um patch.",
            "This screen does not patch a game. It only prepares and checks. The real test still happens when applying a patch."));

    app_notice_details(lang,
        (dirs_ok && webman_ok && ps3mapi_ok && artemis_ok) ?
            tr(lang, "Dependencias prontas", "Dependencies ready") :
            tr(lang, "Dependencia faltando", "Missing dependency"),
        body,
        (dirs_ok && webman_ok && ps3mapi_ok && artemis_ok) ? UI_COLOR_GREEN : UI_COLOR_ACCENT_2);
}

static void draw_qr_matrix(int x, int y, int module, const char *const *matrix, int size)
{
    int quiet = 4;
    int total = (size + quiet * 2) * module;
    int row, col;

    ui_fill_rect(x, y, total, total, 0xffffffffu);
    for (row = 0; row < size; ++row) {
        for (col = 0; col < size; ++col) {
            if (matrix[row][col] == '1') {
                ui_fill_rect(x + (col + quiet) * module,
                    y + (row + quiet) * module, module, module, 0xff000000u);
            }
        }
    }
}

static void draw_pix_qr(int x, int y, int module)
{
    draw_qr_matrix(x, y, module, FPSU_PIX_QR, FPSU_PIX_QR_SIZE);
}

static void draw_paypal_qr(int x, int y, int module)
{
    draw_qr_matrix(x, y, module, FPSU_PAYPAL_QR, FPSU_PAYPAL_QR_SIZE);
}

static void show_donate(fpsu_lang lang)
{
    for (;;) {
        u32 btn;
        ui_begin_frame();
        ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
            tr(lang, "Apoiar o projeto", "Support the project"), "DONATE");
        ui_draw_text(204, 116, tr(lang, "Pix", "Pix"), UI_COLOR_TEXT, 3);
        ui_draw_text(766, 116, "PayPal", UI_COLOR_TEXT, 3);
        draw_pix_qr(120, 150, 5);
        draw_paypal_qr(700, 150, 5);
        draw_multiline_text(180, 460,
            tr(lang,
                "Se quiser apoiar, use o QR que preferir. Obrigado por testar, reportar jogos e ajudar o PSUF a melhorar.",
                "Use whichever QR code works best for you. Thanks for testing, reporting games, and helping PSUF improve."),
            UI_COLOR_MUTED, 2, 80, 3);
        ui_draw_footer(tr(lang, "X / Circle voltar", "X / Circle back"),
            tr(lang, "PNGs: pix_qr.png / paypal_qr.png", "PNGs: pix_qr.png / paypal_qr.png"));
        ui_present();

        btn = frame_input();
        if (btn & (FPSU_BUTTON_CROSS | FPSU_BUTTON_CIRCLE | FPSU_BUTTON_TRIANGLE)) {
            return;
        }
    }
}

static int clock_clamp_step(int value, int max_value)
{
    int stepped;

    if (value < FPSU_CLOCK_MIN_MHZ) {
        return FPSU_CLOCK_MIN_MHZ;
    }
    if (value > max_value) {
        return max_value;
    }
    stepped = ((value + FPSU_CLOCK_STEP_MHZ / 2) / FPSU_CLOCK_STEP_MHZ) * FPSU_CLOCK_STEP_MHZ;
    if (stepped < FPSU_CLOCK_MIN_MHZ) {
        stepped = FPSU_CLOCK_MIN_MHZ;
    }
    if (stepped > max_value) {
        stepped = max_value;
    }
    return stepped;
}

static void clock_adjust_step(int *value, int max_value, int direction)
{
    if (!value || direction == 0) {
        return;
    }
    *value += direction > 0 ? FPSU_CLOCK_STEP_MHZ : -FPSU_CLOCK_STEP_MHZ;
    *value = clock_clamp_step(*value, max_value);
}

static const char *clock_mode_label(fpsu_lang lang, int gpu_mhz, int vram_mhz)
{
    if (gpu_mhz > FPSU_CLOCK_DEFAULT_GPU_MHZ || vram_mhz > FPSU_CLOCK_DEFAULT_VRAM_MHZ) {
        return tr(lang, "Overclock", "Overclock");
    }
    if (gpu_mhz < FPSU_CLOCK_DEFAULT_GPU_MHZ || vram_mhz < FPSU_CLOCK_DEFAULT_VRAM_MHZ) {
        return tr(lang, "Downclock", "Downclock");
    }
    return tr(lang, "Padrao PS3", "PS3 default");
}

static void draw_webman_clock_settings(fpsu_lang lang, int selected, int gpu_mhz, int vram_mhz,
    int current_gpu_mhz, int current_vram_mhz, int have_current)
{
    char title[96];
    char body[224];
    char line[192];
    int width = ui_screen_width() - 164;

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
        tr(lang, "webMAN > Overclock", "webMAN > Overclock"), "CLOCK");

    ui_fill_rect(82, 104, width, 132, UI_COLOR_PANEL);
    ui_fill_rect(82, 104, 9, 132, UI_COLOR_ORANGE);
    ui_draw_text(112, 126, tr(lang, "Clock RSX via webMAN", "RSX clock through webMAN"), UI_COLOR_TEXT, 2);
    draw_multiline_text(112, 158,
        tr(lang,
            "Overclock pode aumentar FPS/desempenho. Fique de olho na temperatura. Limite seguro do PSUF: 750/850.",
            "Overclock may improve FPS/performance. Watch the temperature. PSUF safe limit: 750/850."),
        UI_COLOR_MUTED, 1, 92, 3);
    if (have_current) {
        snprintf(line, sizeof(line), "%s: GPU %d MHz | VRAM %d MHz",
            tr(lang, "Atual webMAN", "Current webMAN"), current_gpu_mhz, current_vram_mhz);
    } else {
        snprintf(line, sizeof(line), "%s",
            tr(lang, "Nao consegui ler o clock atual do webMAN.", "Could not read current webMAN clock."));
    }
    ui_draw_text(112, 216, line, have_current ? UI_COLOR_ACCENT_2 : UI_COLOR_ORANGE, 1);

    snprintf(title, sizeof(title), "GPU Core: %d MHz", gpu_mhz);
    snprintf(body, sizeof(body), "%s | %s 750 MHz",
        tr(lang, "Esquerda/direita: 50 MHz", "Left/right: 50 MHz"),
        tr(lang, "max PSUF", "PSUF max"));
    ui_draw_menu_item(100, 252, width - 36, title, body, selected == 0, 1);

    snprintf(title, sizeof(title), "VRAM: %d MHz", vram_mhz);
    snprintf(body, sizeof(body), "%s | %s 850 MHz",
        tr(lang, "Esquerda/direita: 50 MHz", "Left/right: 50 MHz"),
        tr(lang, "max PSUF", "PSUF max"));
    ui_draw_menu_item(100, 328, width - 36, title, body, selected == 1, 1);

    snprintf(body, sizeof(body), "%s: %s",
        tr(lang, "Modo", "Mode"), clock_mode_label(lang, gpu_mhz, vram_mhz));
    ui_draw_menu_item(100, 404, width - 36,
        tr(lang, "Aplicar agora via webMAN", "Apply now through webMAN"),
        body, selected == 2, 1);

    ui_draw_menu_item(100, 480, width - 36,
        tr(lang, "Voltar ao padrao 500/650", "Return to default 500/650"),
        tr(lang, "Clock retail normal do PS3.", "Normal retail PS3 clock."),
        selected == 3, 1);

    ui_draw_menu_item(100, 556, width - 36,
        tr(lang, "Voltar", "Back"), "", selected == 4, 1);

    ui_draw_footer(tr(lang, "UP/DOWN item   LEFT/RIGHT muda 50 MHz", "UP/DOWN item   LEFT/RIGHT changes 50 MHz"),
        tr(lang, "Triangle reler   Circle voltar", "Triangle refresh   Circle back"));
    ui_present();
}

static int apply_webman_clock(fpsu_lang lang, int gpu_mhz, int vram_mhz)
{
    char msg[1024];
    char result[256];
    const char *mode;

    if (gpu_mhz % FPSU_CLOCK_STEP_MHZ != 0 || vram_mhz % FPSU_CLOCK_STEP_MHZ != 0) {
        app_notice(lang,
            tr(lang, "Clock invalido", "Invalid clock"),
            tr(lang, "O PSUF so aceita valores de 50 em 50 MHz.", "PSUF only accepts values in 50 MHz steps."),
            UI_COLOR_RED);
        return 0;
    }
    if (gpu_mhz < FPSU_CLOCK_MIN_MHZ || gpu_mhz > FPSU_CLOCK_MAX_GPU_MHZ ||
        vram_mhz < FPSU_CLOCK_MIN_MHZ || vram_mhz > FPSU_CLOCK_MAX_VRAM_MHZ) {
        app_notice(lang,
            tr(lang, "Fora do limite do PSUF", "Outside PSUF limit"),
            tr(lang,
                "O limite seguro pelo PSUF e GPU 750 / VRAM 850. Para mais que isso, use o webMAN direto.",
                "PSUF safe limit is GPU 750 / VRAM 850. For more than that, use webMAN directly."),
            UI_COLOR_RED);
        return 0;
    }

    mode = clock_mode_label(lang, gpu_mhz, vram_mhz);
    snprintf(msg, sizeof(msg),
        "GPU %d MHz\nVRAM %d MHz\n%s: %s\n\n%s",
        gpu_mhz, vram_mhz, tr(lang, "Modo", "Mode"), mode,
        tr(lang,
            "Overclock pode ajudar FPS, mas esquenta mais. Monitore a temperatura; calor alto pode travar, dar artefatos ou reduzir a vida util. O PSUF limita ao valor seguro 750/850.",
            "Overclock may help FPS, but it adds heat. Monitor temperature; high heat may freeze, show artifacts, or reduce lifespan. PSUF is limited to the safe 750/850 value."));
    if (!app_confirm(lang,
            tr(lang, "Aplicar clock pelo webMAN", "Apply clock through webMAN"), msg)) {
        return 0;
    }

    draw_busy_notice(lang,
        tr(lang, "Enviando para o webMAN", "Sending to webMAN"),
        tr(lang, "Aplicando GPU/VRAM pelo comando local do webMAN.", "Applying GPU/VRAM through the local webMAN command."));
    if (webman_set_gpu_clock(gpu_mhz, vram_mhz, result, sizeof(result)) == 0) {
        app_notice(lang,
            tr(lang, "Clock aplicado", "Clock applied"),
            result, UI_COLOR_GREEN);
        return 1;
    }

    app_notice(lang,
        tr(lang, "webMAN nao aplicou", "webMAN did not apply"),
        result[0] ? result :
            tr(lang, "Confira se o webMAN MOD esta ativo e com servidor HTTP ligado.", "Check that webMAN MOD is active and its HTTP server is enabled."),
        UI_COLOR_RED);
    return 0;
}

static void show_webman_settings(fpsu_lang lang)
{
    int selected = 0;
    int gpu_mhz = FPSU_CLOCK_DEFAULT_GPU_MHZ;
    int vram_mhz = FPSU_CLOCK_DEFAULT_VRAM_MHZ;
    int current_gpu_mhz = 0;
    int current_vram_mhz = 0;
    int have_current;

    have_current = webman_get_gpu_clock(&current_gpu_mhz, &current_vram_mhz) == 0;
    if (have_current) {
        gpu_mhz = clock_clamp_step(current_gpu_mhz, FPSU_CLOCK_MAX_GPU_MHZ);
        vram_mhz = clock_clamp_step(current_vram_mhz, FPSU_CLOCK_MAX_VRAM_MHZ);
    }

    for (;;) {
        u32 btn;
        draw_webman_clock_settings(lang, selected, gpu_mhz, vram_mhz,
            current_gpu_mhz, current_vram_mhz, have_current);
        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % FPSU_CLOCK_MENU_ITEMS;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + FPSU_CLOCK_MENU_ITEMS - 1) % FPSU_CLOCK_MENU_ITEMS;
        }
        if (btn & FPSU_BUTTON_LEFT) {
            if (selected == 0) {
                clock_adjust_step(&gpu_mhz, FPSU_CLOCK_MAX_GPU_MHZ, -1);
            } else if (selected == 1) {
                clock_adjust_step(&vram_mhz, FPSU_CLOCK_MAX_VRAM_MHZ, -1);
            }
        }
        if (btn & FPSU_BUTTON_RIGHT) {
            if (selected == 0) {
                clock_adjust_step(&gpu_mhz, FPSU_CLOCK_MAX_GPU_MHZ, 1);
            } else if (selected == 1) {
                clock_adjust_step(&vram_mhz, FPSU_CLOCK_MAX_VRAM_MHZ, 1);
            }
        }
        if (btn & FPSU_BUTTON_TRIANGLE) {
            have_current = webman_get_gpu_clock(&current_gpu_mhz, &current_vram_mhz) == 0;
            if (have_current) {
                gpu_mhz = clock_clamp_step(current_gpu_mhz, FPSU_CLOCK_MAX_GPU_MHZ);
                vram_mhz = clock_clamp_step(current_vram_mhz, FPSU_CLOCK_MAX_VRAM_MHZ);
            }
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            if (selected == 2) {
                if (apply_webman_clock(lang, gpu_mhz, vram_mhz)) {
                    have_current = webman_get_gpu_clock(&current_gpu_mhz, &current_vram_mhz) == 0;
                }
            } else if (selected == 3) {
                gpu_mhz = FPSU_CLOCK_DEFAULT_GPU_MHZ;
                vram_mhz = FPSU_CLOCK_DEFAULT_VRAM_MHZ;
                if (apply_webman_clock(lang, gpu_mhz, vram_mhz)) {
                    have_current = webman_get_gpu_clock(&current_gpu_mhz, &current_vram_mhz) == 0;
                }
            } else if (selected == 4) {
                return;
            }
        }
    }
}

static void draw_progress_bar(int x, int y, int w, int h, unsigned int value, unsigned int total, u32 color)
{
    int fill = 0;
    if (total > 0) {
        fill = (int)(((unsigned long long)value * (unsigned int)w) / total);
        if (fill > w) {
            fill = w;
        }
    }
    ui_fill_rect(x, y, w, h, 0xff203244u);
    ui_fill_rect(x, y, fill, h, color);
    ui_fill_rect(x, y, w, 2, 0xff3d5668u);
    ui_fill_rect(x, y + h - 2, w, 2, 0xff0f1c28u);
}

static void tail_text(char *out, size_t out_size, const char *src, size_t max_chars)
{
    size_t len;
    if (!out || out_size == 0) {
        return;
    }
    if (!src) {
        out[0] = '\0';
        return;
    }
    len = strlen(src);
    if (len <= max_chars || max_chars + 4 >= out_size) {
        snprintf(out, out_size, "%s", src);
        return;
    }
    snprintf(out, out_size, "...%s", src + len - max_chars);
}

static void draw_scan_screen_total(fpsu_lang lang, const char *path, int found, int total,
    const char *title)
{
    char count[96];
    char short_path[176];
    if (total <= 0) {
        total = FPSU_MAX_GAMES;
    }
    tail_text(short_path, sizeof(short_path), path, 150);
    snprintf(count, sizeof(count), "%s: %d / %d", tr(lang, "Encontrados", "Found"), found, total);
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), i18n_text(lang, TXT_SCAN_START), "SCAN");
    ui_fill_rect(96, 150, ui_screen_width() - 192, 282, UI_COLOR_PANEL);
    ui_fill_rect(96, 150, 9, 282, UI_COLOR_ACCENT);
    ui_draw_text(132, 190, title, UI_COLOR_TEXT, 3);
    ui_draw_text(132, 270, short_path, UI_COLOR_MUTED, 1);
    ui_draw_text(132, 322, count, UI_COLOR_ACCENT_2, 2);
    draw_progress_bar(132, 360, ui_screen_width() - 264, 20, (unsigned int)found, (unsigned int)total, UI_COLOR_ACCENT_2);
    ui_draw_footer(tr(lang, "Aguarde, sem alterar arquivos", "Please wait, no files changed"), tr(lang, "Circle cancela", "Circle cancels"));
    ui_present();
}

static void draw_scan_screen(fpsu_lang lang, const char *path, int found)
{
    draw_scan_screen_total(lang, path, found, FPSU_MAX_GAMES,
        tr(lang, "Analisando jogos, aguarde", "Analyzing games, please wait"));
}

static void draw_mounted_scan_screen(fpsu_lang lang, const char *path, int found)
{
    draw_scan_screen_total(lang, path, found, 1,
        tr(lang, "Identificando jogo montado", "Identifying mounted game"));
}

static void draw_analysis_screen(fpsu_lang lang, const fpsu_game *game, int index, int count,
    unsigned int scanned, unsigned int limit)
{
    char line[160];
    char bytes[160];
    char short_path[176];
    unsigned int game_part = 0;
    unsigned int overall;
    unsigned int overall_total;

    if (limit == 0) {
        limit = 1;
    }
    if (scanned > limit) {
        scanned = limit;
    }
    if (count <= 0) {
        count = 1;
    }

    game_part = (unsigned int)(((unsigned long long)scanned * 1000u) / limit);
    overall = (unsigned int)(index * 1000) + game_part;
    overall_total = (unsigned int)(count * 1000);

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
        tr(lang, "Analisando padroes FPS", "Analyzing FPS patterns"), "ANALISE");
    ui_fill_rect(96, 132, ui_screen_width() - 192, 468, UI_COLOR_PANEL);
    ui_fill_rect(96, 132, 9, 468, UI_COLOR_ACCENT_2);

    ui_draw_text(132, 172, tr(lang, "Analisando jogos, aguarde", "Analyzing games, please wait"), UI_COLOR_TEXT, 3);
    snprintf(line, sizeof(line), "%s %d %s %d", tr(lang, "Jogo", "Game"), index + 1, tr(lang, "de", "of"), count);
    ui_draw_text(132, 236, line, UI_COLOR_ACCENT_2, 2);
    draw_progress_bar(132, 274, ui_screen_width() - 264, 22, overall, overall_total, UI_COLOR_ACCENT_2);

    ui_draw_text(132, 330, game ? game->title : "", UI_COLOR_TEXT, 2);
    if (game) {
        snprintf(line, sizeof(line), "%s  %s", game->title_id, game->version);
        ui_draw_text(132, 366, line, UI_COLOR_MUTED, 2);
        tail_text(short_path, sizeof(short_path), game->eboot_path, 150);
        ui_draw_text(132, 414, short_path, UI_COLOR_MUTED, 1);
    }

    snprintf(bytes, sizeof(bytes), "EBOOT: %u MB / %u MB", scanned / (1024u * 1024u),
        limit / (1024u * 1024u));
    ui_draw_text(132, 466, bytes, UI_COLOR_MUTED, 2);
    draw_progress_bar(132, 506, ui_screen_width() - 264, 18, scanned, limit, UI_COLOR_ACCENT);

    ui_draw_footer(tr(lang, "Circle cancela e abre lista parcial", "Circle cancels and opens the partial list"),
        tr(lang, "Nada e aplicado automaticamente", "Nothing is applied automatically"));
    ui_present();
}

static void draw_database_screen(fpsu_lang lang, int index, int count)
{
    char line[128];
    if (count <= 0) {
        count = 1;
    }
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
        tr(lang, "Consultando banco de patches", "Checking patch database"), "DB");
    ui_fill_rect(96, 168, ui_screen_width() - 192, 238, UI_COLOR_PANEL);
    ui_fill_rect(96, 168, 9, 238, UI_COLOR_ACCENT);
    ui_draw_text(132, 212, tr(lang, "Comparando jogos com a base", "Matching games against database"), UI_COLOR_TEXT, 3);
    snprintf(line, sizeof(line), "%d / %d", index, count);
    ui_draw_text(132, 290, line, UI_COLOR_ACCENT_2, 2);
    draw_progress_bar(132, 330, ui_screen_width() - 264, 20, (unsigned int)index, (unsigned int)count, UI_COLOR_ACCENT_2);
    ui_draw_footer(tr(lang, "Aguarde, sem alterar arquivos", "Please wait, no files changed"),
        tr(lang, "Depois voce escolhe o jogo", "You choose the game after this"));
    ui_present();
}

typedef struct {
    fpsu_lang lang;
    int found;
    int cancel;
} scan_context;

static int scan_progress_cb(const char *path, int found, void *user)
{
    scan_context *ctx = (scan_context *)user;
    if (!ctx) {
        return 0;
    }
    ctx->found = found;
    draw_scan_screen(ctx->lang, path, found);
    music_update();
    if (pad_input_read_pressed() & FPSU_BUTTON_CIRCLE) {
        ctx->cancel = 1;
        return 1;
    }
    return 0;
}

static int is_game_data(const fpsu_game *game)
{
    return game && strcmp(game->category, "GD") == 0;
}

static int is_update_only_entry(const fpsu_game *game)
{
    return is_game_data(game) && game->category[0] && strstr(game->category, "DG") == NULL;
}

static int is_playable_entry(const fpsu_game *game)
{
    return game && !is_game_data(game);
}

static int same_title_id(const fpsu_game *a, const fpsu_game *b)
{
    return a && b && a->title_id[0] != '\0' && b->title_id[0] != '\0' &&
        strcmp(a->title_id, b->title_id) == 0;
}

static int version_is_unknown_ui(const char *version)
{
    return !version || version[0] == '\0' || strcmp(version, "*") == 0 ||
        strcmp(version, "00.00") == 0;
}

static int normalized_title_equal(const char *a, const char *b)
{
    size_t ia = 0;
    size_t ib = 0;

    if (!a || !b || a[0] == '\0' || b[0] == '\0') {
        return 0;
    }

    for (;;) {
        while (a[ia] && !isalnum((unsigned char)a[ia])) {
            ++ia;
        }
        while (b[ib] && !isalnum((unsigned char)b[ib])) {
            ++ib;
        }
        if (!a[ia] || !b[ib]) {
            break;
        }
        if (toupper((unsigned char)a[ia]) != toupper((unsigned char)b[ib])) {
            return 0;
        }
        ++ia;
        ++ib;
    }

    while (a[ia] && !isalnum((unsigned char)a[ia])) {
        ++ia;
    }
    while (b[ib] && !isalnum((unsigned char)b[ib])) {
        ++ib;
    }
    return a[ia] == '\0' && b[ib] == '\0';
}

static int same_display_game_for_data_merge(const fpsu_game *a, const fpsu_game *b)
{
    if (!a || !b) {
        return 0;
    }
    if (!normalized_title_equal(a->title, b->title)) {
        return 0;
    }
    return is_game_data(a) != is_game_data(b);
}

static int path_is_update_eboot(const char *path)
{
    return path && strncmp(path, "/dev_hdd0/game/", 15) == 0;
}

static int game_uses_dev_bdvd(const fpsu_game *game)
{
    return game && (strcmp(game->base_path, "/dev_bdvd") == 0 ||
        strncmp(game->sfo_path, "/dev_bdvd/", 10) == 0 ||
        strncmp(game->eboot_path, "/dev_bdvd/", 10) == 0);
}

static int path_is_external_media(const char *path)
{
    return path &&
        (strncmp(path, "/dev_usb", 8) == 0 ||
            strncmp(path, "/dev_ntfs", 9) == 0 ||
            strncmp(path, "/net", 4) == 0);
}

static int is_mounted_media(const fpsu_game *game)
{
    return game && (game->is_iso || game_uses_dev_bdvd(game) ||
        path_is_external_media(game->base_path) ||
        path_is_external_media(game->sfo_path) ||
        path_is_external_media(game->eboot_path));
}

static void copy_game_exec_from(fpsu_game *dst, const fpsu_game *src)
{
    if (!dst || !src || src->eboot_path[0] == '\0') {
        return;
    }
    if (!same_title_id(dst, src)) {
        return;
    }
    snprintf(dst->version, sizeof(dst->version), "%s", src->version);
    snprintf(dst->eboot_path, sizeof(dst->eboot_path), "%s", src->eboot_path);
    if (is_game_data(src) && is_playable_entry(dst)) {
        snprintf(dst->category, sizeof(dst->category), "%s", "DG+GD");
    }
}

static void merge_cached_game_info(fpsu_game *game, const fpsu_game_result *cached, int count)
{
    int i;
    if (!game || !cached || count <= 0 || game->title_id[0] == '\0') {
        return;
    }
    for (i = 0; i < count; ++i) {
        if (!same_title_id(game, &cached[i].game)) {
            continue;
        }
        if (version_is_unknown_ui(game->version) &&
            !version_is_unknown_ui(cached[i].game.version)) {
            snprintf(game->version, sizeof(game->version), "%s", cached[i].game.version);
        }
        if (game->eboot_path[0] == '\0' && cached[i].game.eboot_path[0] != '\0' &&
            !game_uses_dev_bdvd(&cached[i].game)) {
            snprintf(game->eboot_path, sizeof(game->eboot_path), "%s", cached[i].game.eboot_path);
        }
        return;
    }
}

static int should_replace_display_game(const fpsu_game *current, const fpsu_game *candidate)
{
    if (!current || !candidate) {
        return 0;
    }
    if (is_game_data(current) && is_playable_entry(candidate)) {
        return 1;
    }
    if (!same_title_id(current, candidate)) {
        return 0;
    }
    if (game_uses_dev_bdvd(current) && !game_uses_dev_bdvd(candidate)) {
        return 1;
    }
    if (current->eboot_path[0] == '\0' && candidate->eboot_path[0] != '\0') {
        return 1;
    }
    if (is_playable_entry(current) == is_playable_entry(candidate) &&
        path_is_update_eboot(current->eboot_path) && !path_is_update_eboot(candidate->eboot_path)) {
        return 1;
    }
    return 0;
}

static int dedupe_games(fpsu_game *games, int count)
{
    int out_count = 0;
    int i;

    if (!games || count <= 0) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        int existing = -1;
        int j;
        for (j = 0; j < out_count; ++j) {
            if (same_title_id(&games[j], &games[i]) ||
                same_display_game_for_data_merge(&games[j], &games[i])) {
                existing = j;
                break;
            }
        }

        if (existing < 0) {
            games[out_count++] = games[i];
            continue;
        }

        if (should_replace_display_game(&games[existing], &games[i])) {
            fpsu_game old = games[existing];
            games[existing] = games[i];
            if (old.eboot_path[0] != '\0' && path_is_update_eboot(old.eboot_path)) {
                copy_game_exec_from(&games[existing], &old);
            }
        } else if (games[existing].eboot_path[0] == '\0' ||
            path_is_update_eboot(games[i].eboot_path) ||
            is_game_data(&games[i])) {
            copy_game_exec_from(&games[existing], &games[i]);
        }
    }

    for (i = 0; i < out_count;) {
        if (is_update_only_entry(&games[i])) {
            int j;
            for (j = i; j + 1 < out_count; ++j) {
                games[j] = games[j + 1];
            }
            --out_count;
            continue;
        }
        ++i;
    }

    return out_count;
}

static int dedupe_results(fpsu_game_result *results, int count)
{
    int out_count = 0;
    int i;

    if (!results || count <= 0) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        int existing = -1;
        int j;
        for (j = 0; j < out_count; ++j) {
            if (same_title_id(&results[j].game, &results[i].game) ||
                same_display_game_for_data_merge(&results[j].game, &results[i].game)) {
                existing = j;
                break;
            }
        }

        if (existing < 0) {
            results[out_count++] = results[i];
            continue;
        }

        if (should_replace_display_game(&results[existing].game, &results[i].game)) {
            fpsu_game old = results[existing].game;
            fpsu_game_result merged = results[i];
            copy_game_exec_from(&merged.game, &old);
            if (results[existing].pattern_candidate) {
                merged.pattern_candidate = 1;
            }
            results[existing] = merged;
        } else {
            copy_game_exec_from(&results[existing].game, &results[i].game);
            if (results[i].pattern_candidate) {
                results[existing].pattern_candidate = 1;
            }
        }
    }

    for (i = 0; i < out_count;) {
        if (is_update_only_entry(&results[i].game)) {
            int j;
            for (j = i; j + 1 < out_count; ++j) {
                results[j] = results[j + 1];
            }
            --out_count;
            continue;
        }
        ++i;
    }

    return out_count;
}

static int title_is_one_of(const fpsu_game *game, const char *const *ids, int count)
{
    int i;
    if (!game) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (strcmp(game->title_id, ids[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int game_is_known_native60(const fpsu_game *game)
{
    static const char *const ids[] = {
        "BLUS30591", "BLES01031",
        "BLES01717",
        "BLUS30580",
        "BLES00176",
        "BLUS30066"
    };
    return patch_db_is_native60(game) ||
        title_is_one_of(game, ids, (int)(sizeof(ids) / sizeof(ids[0])));
}

static int hardware_lab_failed_unlock(const fpsu_game *game)
{
    static const char *const ids[] = {
        "NPUA80959",
        "BLES01717",
        "NPUB30984",
        "BLUS30580",
        "BLES00229",
        "BLES00176",
        "BCES01141",
        "BLUS30762"
    };
    if (game && strcmp(game->title_id, "BLUS30451") == 0 &&
        (strcmp(game->version, "01.00") == 0 || strcmp(game->version, "01.61") == 0)) {
        return 1;
    }
    return title_is_one_of(game, ids, (int)(sizeof(ids) / sizeof(ids[0])));
}

static void mark_native_60_option(fpsu_patch_option *opt)
{
    if (!opt) {
        return;
    }
    opt->kind = FPSU_PATCH_60;
    opt->status = FPSU_STATUS_NATIVE_60;
    opt->method = FPSU_METHOD_NONE;
    opt->payload[0] = '\0';
    snprintf(opt->label, sizeof(opt->label), "%s", "Jogo ja roda a 60 FPS");
    snprintf(opt->note, sizeof(opt->note), "%s",
        "Nao mostra aplicar 60 FPS porque o jogo ja tem alvo nativo de 60.");
}

static void downgrade_failed_lab_option(fpsu_patch_option *opt)
{
    if (!opt || opt->status == FPSU_STATUS_NOT_FOUND || opt->status == FPSU_STATUS_UNAVAILABLE ||
        opt->status == FPSU_STATUS_NATIVE_60) {
        return;
    }
    if (opt->status == FPSU_STATUS_KNOWN || opt->status == FPSU_STATUS_APPLICABLE) {
        opt->status = FPSU_STATUS_UNTESTED;
    }
    if (!opt->label[0]) {
        snprintf(opt->label, sizeof(opt->label), "%s", "Patch possivel, nao confirmado");
    }
    snprintf(opt->note, sizeof(opt->note), "%s",
        "Teste anterior nao mudou o FPS nesta versao. Mantido como NAO SEGURO.");
}

static void apply_result_overrides(fpsu_game_result *r)
{
    if (!r) {
        return;
    }
    if (game_is_known_native60(&r->game)) {
        mark_native_60_option(&r->fps60);
    }
    if (hardware_lab_failed_unlock(&r->game)) {
        downgrade_failed_lab_option(&r->fps60);
        downgrade_failed_lab_option(&r->unlock);
    }
}

static void match_results(fpsu_lang lang, int game_count)
{
    int i;
    for (i = 0; i < game_count; ++i) {
        if ((i % 8) == 0) {
            draw_database_screen(lang, i, game_count);
            cache_write_progress("database", i, game_count, &g_games[i], 0, 0, g_results, game_count);
            music_update();
            ui_pump();
        }
        patch_db_match_game(&g_games[i], &g_results[i]);
        mark_unavailable_without_files(&g_results[i]);
        apply_result_overrides(&g_results[i]);
    }
    draw_database_screen(lang, game_count, game_count);
    cache_write_progress("database_done", game_count, game_count,
        game_count > 0 ? &g_games[game_count - 1] : NULL, 0, 0, g_results, game_count);
}

typedef struct {
    fpsu_lang lang;
    int index;
    int count;
    int cancel;
    unsigned int last_progress_write;
} pattern_context;

static int pattern_progress_cb(const fpsu_game *game, unsigned int scanned, unsigned int limit, void *user)
{
    pattern_context *ctx = (pattern_context *)user;
    if (!ctx) {
        return 0;
    }
    draw_analysis_screen(ctx->lang, game, ctx->index, ctx->count, scanned, limit);
    music_update();
    ui_pump();
    if (scanned == 0 || scanned >= limit || scanned - ctx->last_progress_write >= (4u * 1024u * 1024u)) {
        cache_write_progress("analysis", ctx->index + 1, ctx->count, game, scanned, limit,
            g_results, ctx->count);
        ctx->last_progress_write = scanned;
    }
    if (pad_input_read_pressed() & FPSU_BUTTON_CIRCLE) {
        ctx->cancel = 1;
        return 1;
    }
    return 0;
}

static int force_pattern_progress_cb(const fpsu_game *game, unsigned int scanned, unsigned int limit, void *user)
{
    pattern_context *ctx = (pattern_context *)user;
    if (!ctx) {
        return 0;
    }
    draw_analysis_screen(ctx->lang, game, 0, 1, scanned, limit);
    music_update();
    ui_pump();
    if (pad_input_read_pressed() & FPSU_BUTTON_CIRCLE) {
        ctx->cancel = 1;
        return 1;
    }
    return 0;
}

static void mark_pattern_candidate(fpsu_game_result *r)
{
    fpsu_patch_option *opts[2];
    int i;
    if (!r) {
        return;
    }

    r->pattern_candidate = 1;
    opts[0] = &r->fps60;
    opts[1] = &r->unlock;
    for (i = 0; i < 2; ++i) {
        fpsu_patch_option *opt = opts[i];
        if (opt->status == FPSU_STATUS_KNOWN || opt->status == FPSU_STATUS_APPLICABLE) {
            continue;
        }
        if (opt->status == FPSU_STATUS_OTHER_VERSION && opt->payload[0] != '\0') {
            opt->status = FPSU_STATUS_APPLICABLE;
            snprintf(opt->label, sizeof(opt->label), "%s", "Applicable after analysis");
            snprintf(opt->note, sizeof(opt->note), "%s", "Other-version patch matched local scan. Experimental apply is available.");
        } else if (opt->status == FPSU_STATUS_NOT_FOUND) {
            opt->kind = i == 0 ? FPSU_PATCH_60 : FPSU_PATCH_UNLOCK;
            opt->status = FPSU_STATUS_UNTESTED;
            opt->method = FPSU_METHOD_PATTERN_ONLY;
            snprintf(opt->label, sizeof(opt->label), "%s", i == 0 ? "Detected 60 FPS pattern" : "Detected FPS pattern");
            snprintf(opt->note, sizeof(opt->note), "%s", "Pattern found by local scan. PS3 probable patch test available.");
        }
    }
}

static void restore_option_hint(fpsu_patch_option *opt, fpsu_patch_kind kind,
    fpsu_patch_status cached_status, fpsu_patch_method cached_method)
{
    if (!opt) {
        return;
    }
    if (cached_status == FPSU_STATUS_KNOWN && opt->payload[0] != '\0') {
        opt->kind = kind;
        opt->status = FPSU_STATUS_KNOWN;
        opt->method = fpsu_method_is_ncl_write(cached_method) ? cached_method : FPSU_METHOD_NCL;
        if (!opt->label[0]) {
            snprintf(opt->label, sizeof(opt->label), "%s", "Disponivel");
        }
        snprintf(opt->note, sizeof(opt->note), "%s",
            "Opcao salva pelo usuario no ultimo uso.");
        return;
    }
    if (cached_status == FPSU_STATUS_APPLICABLE &&
        opt->status == FPSU_STATUS_OTHER_VERSION &&
        opt->payload[0] != '\0') {
        opt->kind = kind;
        opt->status = FPSU_STATUS_APPLICABLE;
        snprintf(opt->label, sizeof(opt->label), "%s", "Aplicavel por padrao local");
        snprintf(opt->note, sizeof(opt->note), "%s",
            "Ultimo scan achou padrao local para patch de outra versao.");
        return;
    }
    if ((cached_status == FPSU_STATUS_UNTESTED || cached_method == FPSU_METHOD_PATTERN_ONLY) &&
        opt->status == FPSU_STATUS_NOT_FOUND) {
        opt->kind = kind;
        opt->status = FPSU_STATUS_UNTESTED;
        opt->method = FPSU_METHOD_PATTERN_ONLY;
        snprintf(opt->label, sizeof(opt->label), "%s",
            kind == FPSU_PATCH_60 ? "Padrao 60 FPS salvo" :
                (kind == FPSU_PATCH_30 ? "Padrao 30 FPS salvo" : "Padrao FPS salvo"));
        snprintf(opt->note, sizeof(opt->note), "%s",
            "Ultimo scan encontrou sinal de FPS, mas ainda nao ha payload concreto.");
    }
}

static void mark_unavailable_without_files(fpsu_game_result *r)
{
    fpsu_patch_option *opts[3];
    int i;
    if (!r || r->game.eboot_path[0] != '\0') {
        return;
    }
    opts[0] = &r->fps60;
    opts[1] = &r->unlock;
    opts[2] = &r->fps30;
    for (i = 0; i < 3; ++i) {
        if (opts[i]->status == FPSU_STATUS_NOT_FOUND && opts[i]->method == FPSU_METHOD_NONE) {
            opts[i]->status = FPSU_STATUS_UNAVAILABLE;
            snprintf(opts[i]->label, sizeof(opts[i]->label), "%s", "Sem EBOOT para analisar");
            snprintf(opts[i]->note, sizeof(opts[i]->note), "%s",
                "O PS3 nao achou arquivo de jogo para testar esta versao.");
        }
    }
}

static void rebuild_cached_results(fpsu_lang lang, int game_count)
{
    int i;
    for (i = 0; i < game_count; ++i) {
        fpsu_game game = g_results[i].game;
        fpsu_patch_status fps60_status = g_results[i].fps60.status;
        fpsu_patch_method fps60_method = g_results[i].fps60.method;
        fpsu_patch_status unlock_status = g_results[i].unlock.status;
        fpsu_patch_method unlock_method = g_results[i].unlock.method;
        fpsu_patch_status fps30_status = g_results[i].fps30.status;
        fpsu_patch_method fps30_method = g_results[i].fps30.method;
        int pattern_candidate = g_results[i].pattern_candidate;

        g_games[i] = game;
        if ((i % 12) == 0) {
            draw_database_screen(lang, i, game_count);
            music_update();
            ui_pump();
        }
        patch_db_match_game(&g_games[i], &g_results[i]);
        restore_option_hint(&g_results[i].fps60, FPSU_PATCH_60, fps60_status, fps60_method);
        restore_option_hint(&g_results[i].unlock, FPSU_PATCH_UNLOCK, unlock_status, unlock_method);
        restore_option_hint(&g_results[i].fps30, FPSU_PATCH_30, fps30_status, fps30_method);
        mark_unavailable_without_files(&g_results[i]);
        apply_result_overrides(&g_results[i]);
        if (pattern_candidate) {
            g_results[i].pattern_candidate = 1;
        }
    }
    draw_database_screen(lang, game_count, game_count);
}

static int result_needs_pattern_analysis(const fpsu_game_result *r)
{
    if (!r || r->game.eboot_path[0] == '\0') {
        return 0;
    }
    if (is_mounted_media(&r->game)) {
        return 0;
    }
    return r->fps60.status == FPSU_STATUS_NOT_FOUND ||
        r->fps60.status == FPSU_STATUS_OTHER_VERSION ||
        r->unlock.status == FPSU_STATUS_NOT_FOUND ||
        r->unlock.status == FPSU_STATUS_OTHER_VERSION;
}

static int analyze_result_patterns(fpsu_lang lang, fpsu_game_result *r, int index, int count, int full_scan)
{
    pattern_context ctx;
    int ret;

    if (!r || r->game.eboot_path[0] == '\0') {
        return 0;
    }

    ctx.lang = lang;
    ctx.index = index;
    ctx.count = count;
    ctx.cancel = 0;
    ctx.last_progress_write = 0;
    if (full_scan) {
        ret = scanner_find_patterns_full_ex(&r->game, pattern_progress_cb, &ctx);
    } else {
        ret = scanner_find_patterns_ex(&r->game, pattern_progress_cb, &ctx);
    }
    if (ret > 0) {
        mark_pattern_candidate(r);
    }
    if (ctx.cancel) {
        return -1;
    }
    return ret;
}

static void analyze_all_results(fpsu_lang lang, int game_count)
{
    int i;
    int canceled = 0;

    for (i = 0; i < game_count; ++i) {
        int ret;
        if (g_results[i].game.eboot_path[0] == '\0') {
            draw_analysis_screen(lang, &g_results[i].game, i, game_count, 1, 1);
            cache_write_progress("skip_no_eboot", i + 1, game_count, &g_results[i].game, 0, 0,
                g_results, game_count);
            continue;
        }
        if (!result_needs_pattern_analysis(&g_results[i])) {
            draw_analysis_screen(lang, &g_results[i].game, i, game_count, 1, 1);
            cache_write_progress("skip_db_match", i + 1, game_count, &g_results[i].game, 0, 0,
                g_results, game_count);
            continue;
        }
        ret = analyze_result_patterns(lang, &g_results[i], i, game_count, 0);
        cache_write_results(g_results, game_count);
        if (ret < 0) {
            canceled = 1;
            break;
        }
    }

    if (!canceled && game_count > 0) {
        draw_analysis_screen(lang, &g_results[game_count - 1].game, game_count - 1, game_count, 1, 1);
    }
    if (canceled) {
        app_notice(lang,
            tr(lang, "Analise cancelada", "Analysis canceled"),
            tr(lang, "Abrindo a lista parcial. Nada foi alterado nos jogos.", "Opening the partial list. No game files were changed."),
            UI_COLOR_ORANGE);
    }
}

static void draw_status_line(fpsu_lang lang, int x, int y, const char *label, fpsu_patch_status status)
{
    char line[192];
    snprintf(line, sizeof(line), "%s > %s", tr(lang, "Opcao", "Option"), label);
    ui_draw_text(x, y, line, UI_COLOR_TEXT, 2);
    ui_fill_rect(x, y + 30, 552, 34, 0xff203244u);
    ui_fill_rect(x, y + 30, 7, 34, status_color(status));
    snprintf(line, sizeof(line), "%s > %s", tr(lang, "Status", "Status"), status_text(lang, status));
    ui_draw_text(x + 18, y + 39, line, status_color(status), 1);
}

static void draw_status_legend(fpsu_lang lang, int x, int y)
{
    ui_fill_rect(x, y, 18, 18, UI_COLOR_GREEN);
    ui_draw_text(x + 28, y + 3, tr(lang, "Verde aplicavel", "Green applicable"), UI_COLOR_MUTED, 1);
    ui_fill_rect(x + 180, y, 18, 18, UI_COLOR_ACCENT_2);
    ui_draw_text(x + 208, y + 3, tr(lang, "Amarelo possivel", "Yellow possible"), UI_COLOR_MUTED, 1);
    ui_fill_rect(x + 372, y, 18, 18, UI_COLOR_RED);
    ui_draw_text(x + 400, y + 3, tr(lang, "Vermelho impossivel", "Red impossible"), UI_COLOR_MUTED, 1);
}

static const char *status_help(fpsu_lang lang, fpsu_patch_status status)
{
    switch (status) {
    case FPSU_STATUS_KNOWN:
        return tr(lang, "Patch confirmado para este Title ID/versao.", "Confirmed patch for this Title ID/version.");
    case FPSU_STATUS_UNTESTED:
        return tr(lang, "Padrao possivel. Teste com cuidado e mantenha backup.", "Possible pattern. Test carefully and keep a backup.");
    case FPSU_STATUS_PC_REQUIRED:
        return tr(lang, "Teste avancado pelo PS3. Use Forcar patch provavel.", "Advanced PS3 test. Use Force probable patch.");
    case FPSU_STATUS_OTHER_VERSION:
        return tr(lang, "A base tem patch de outra versao. Use Escolher/forcar rota se quiser testar.", "Database has another version. Use Choose/force route if you want to test.");
    case FPSU_STATUS_APPLICABLE:
        return tr(lang, "Analise achou compatibilidade. Aplicacao experimental.", "Analysis found compatibility. Experimental apply.");
    case FPSU_STATUS_UNAVAILABLE:
        return tr(lang, "Indisponivel: o app nao achou arquivo para mexer.", "Unavailable: no file was found to change.");
    case FPSU_STATUS_NATIVE_60:
        return tr(lang, "60 FPS ja e o alvo normal deste jogo. Nao aplique 60 alvo.", "60 FPS is already this game's normal target. Do not apply 60 FPS target.");
    case FPSU_STATUS_UPDATE_REQUIRED:
        return tr(lang, "Existe patch para update mais novo. Atualize e escaneie de novo.", "A patch exists for a newer update. Update and scan again.");
    default:
        return tr(lang, "Ainda nao achamos patch ou padrao para este EBOOT.", "No patch or pattern has been found for this EBOOT yet.");
    }
}

static int contains_text_ci(const char *text, const char *query)
{
    size_t text_len;
    size_t query_len;
    size_t i;
    size_t j;

    if (!query || query[0] == '\0') {
        return 1;
    }
    if (!text) {
        return 0;
    }
    text_len = strlen(text);
    query_len = strlen(query);
    if (query_len == 0) {
        return 1;
    }
    if (text_len < query_len) {
        return 0;
    }
    for (i = 0; i <= text_len - query_len; ++i) {
        for (j = 0; j < query_len; ++j) {
            if (toupper((unsigned char)text[i + j]) != toupper((unsigned char)query[j])) {
                break;
            }
        }
        if (j == query_len) {
            return 1;
        }
    }
    return 0;
}

static int result_matches_search(const fpsu_game_result *result, const char *query)
{
    if (!result || !query || query[0] == '\0') {
        return 1;
    }
    return contains_text_ci(result->game.title, query) ||
        contains_text_ci(result->game.title_id, query) ||
        contains_text_ci(result->game.version, query);
}

static int build_search_index(const fpsu_game_result *results, int count,
    const char *query, int *visible, int max_visible)
{
    int i;
    int visible_count = 0;

    if (!results || !visible || max_visible <= 0) {
        return 0;
    }
    for (i = 0; i < count && visible_count < max_visible; ++i) {
        if (result_matches_search(&results[i], query)) {
            visible[visible_count++] = i;
        }
    }
    return visible_count;
}

typedef struct {
    const char *label;
    char value;
    int units;
    int action;
} search_key;

#define SEARCH_KEY_CHAR 0
#define SEARCH_KEY_BACKSPACE 1
#define SEARCH_KEY_CLEAR 2
#define SEARCH_KEY_OK 3

static const search_key *search_key_row(int row, int *count)
{
    static const search_key row0[] = {
        {"1", '1', 1, SEARCH_KEY_CHAR}, {"2", '2', 1, SEARCH_KEY_CHAR},
        {"3", '3', 1, SEARCH_KEY_CHAR}, {"4", '4', 1, SEARCH_KEY_CHAR},
        {"5", '5', 1, SEARCH_KEY_CHAR}, {"6", '6', 1, SEARCH_KEY_CHAR},
        {"7", '7', 1, SEARCH_KEY_CHAR}, {"8", '8', 1, SEARCH_KEY_CHAR},
        {"9", '9', 1, SEARCH_KEY_CHAR}, {"0", '0', 1, SEARCH_KEY_CHAR}
    };
    static const search_key row1[] = {
        {"q", 'Q', 1, SEARCH_KEY_CHAR}, {"w", 'W', 1, SEARCH_KEY_CHAR},
        {"e", 'E', 1, SEARCH_KEY_CHAR}, {"r", 'R', 1, SEARCH_KEY_CHAR},
        {"t", 'T', 1, SEARCH_KEY_CHAR}, {"y", 'Y', 1, SEARCH_KEY_CHAR},
        {"u", 'U', 1, SEARCH_KEY_CHAR}, {"i", 'I', 1, SEARCH_KEY_CHAR},
        {"o", 'O', 1, SEARCH_KEY_CHAR}, {"p", 'P', 1, SEARCH_KEY_CHAR}
    };
    static const search_key row2[] = {
        {"a", 'A', 1, SEARCH_KEY_CHAR}, {"s", 'S', 1, SEARCH_KEY_CHAR},
        {"d", 'D', 1, SEARCH_KEY_CHAR}, {"f", 'F', 1, SEARCH_KEY_CHAR},
        {"g", 'G', 1, SEARCH_KEY_CHAR}, {"h", 'H', 1, SEARCH_KEY_CHAR},
        {"j", 'J', 1, SEARCH_KEY_CHAR}, {"k", 'K', 1, SEARCH_KEY_CHAR},
        {"l", 'L', 1, SEARCH_KEY_CHAR}, {".", '.', 1, SEARCH_KEY_CHAR}
    };
    static const search_key row3[] = {
        {"z", 'Z', 1, SEARCH_KEY_CHAR}, {"x", 'X', 1, SEARCH_KEY_CHAR},
        {"c", 'C', 1, SEARCH_KEY_CHAR}, {"v", 'V', 1, SEARCH_KEY_CHAR},
        {"b", 'B', 1, SEARCH_KEY_CHAR}, {"n", 'N', 1, SEARCH_KEY_CHAR},
        {"m", 'M', 1, SEARCH_KEY_CHAR}, {"-", '-', 1, SEARCH_KEY_CHAR},
        {"_", '_', 1, SEARCH_KEY_CHAR}, {"DEL", 0, 1, SEARCH_KEY_BACKSPACE}
    };
    static const search_key row4[] = {
        {"CLR", 0, 2, SEARCH_KEY_CLEAR},
        {"SPACE", ' ', 6, SEARCH_KEY_CHAR},
        {"OK", 0, 2, SEARCH_KEY_OK}
    };

    switch (row) {
    case 0:
        if (count) *count = (int)(sizeof(row0) / sizeof(row0[0]));
        return row0;
    case 1:
        if (count) *count = (int)(sizeof(row1) / sizeof(row1[0]));
        return row1;
    case 2:
        if (count) *count = (int)(sizeof(row2) / sizeof(row2[0]));
        return row2;
    case 3:
        if (count) *count = (int)(sizeof(row3) / sizeof(row3[0]));
        return row3;
    case 4:
        if (count) *count = (int)(sizeof(row4) / sizeof(row4[0]));
        return row4;
    default:
        if (count) *count = 0;
        return NULL;
    }
}

static int search_key_row_count(void)
{
    return 5;
}

static int search_key_row_len(int row)
{
    int count = 0;
    (void)search_key_row(row, &count);
    return count;
}

static const search_key *search_key_at(int row, int col)
{
    int count = 0;
    const search_key *keys = search_key_row(row, &count);

    if (!keys || count <= 0) {
        return NULL;
    }
    if (col < 0) {
        col = 0;
    }
    if (col >= count) {
        col = count - 1;
    }
    return &keys[col];
}

static void draw_search_editor(fpsu_lang lang, const char *query, int cursor_row, int cursor_col)
{
    int row;
    int top = 218;
    int gap = 4;
    int key_w = 76;
    int key_h = 42;
    int row_gap = 4;
    int keyboard_left = 80;
    int panel_x = 64;
    int panel_y = 110;
    int panel_w = ui_screen_width() - 128;

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
        tr(lang, "Buscar jogo", "Search game"), "SEARCH");
    ui_fill_rect(panel_x, panel_y, panel_w, 420, 0xff9fa7adu);
    ui_fill_rect(panel_x + 2, panel_y + 2, panel_w - 4, 416, 0xffc3c7cbu);
    ui_fill_rect(80, 124, 290, 22, 0xffd8d8d8u);
    ui_draw_text(92, 131, tr(lang, "Busca", "Search"), 0xff343a40u, 1);
    ui_fill_rect(80, 150, 830, 58, 0xff22272du);
    ui_fill_rect(84, 154, 822, 50, 0xff10151bu);
    ui_draw_text(100, 170, query && query[0] ? query : tr(lang, "(vazio)", "(empty)"),
        UI_COLOR_TEXT, 2);
    ui_fill_rect(928, 150, 270, 298, 0xff7e858bu);
    ui_draw_text(952, 172, tr(lang, "Pesquisar", "Search"), 0xfff2f7f7u, 2);
    draw_multiline_text(952, 226,
        tr(lang, "Digite nome, Title ID ou versao.", "Type name, Title ID, or version."),
        0xffe5e9ecu, 1, 28, 4);
    draw_multiline_text(952, 334,
        tr(lang, "X tecla\nSquare apagar\nStart confirmar", "X key\nSquare delete\nStart confirm"),
        0xffe5e9ecu, 1, 28, 4);

    for (row = 0; row < search_key_row_count(); ++row) {
        int row_len = 0;
        const search_key *keys = search_key_row(row, &row_len);
        int row_left = keyboard_left;
        int y = top + row * (key_h + row_gap);
        int col;
        int x = row_left;

        for (col = 0; col < row_len; ++col) {
            int key_pixels = keys[col].units * key_w + (keys[col].units - 1) * gap;
            int selected = row == cursor_row && col == cursor_col;
            int scale = strlen(keys[col].label) > 3 ? 1 : 2;
            int text_w = (int)strlen(keys[col].label) * 6 * scale;

            ui_fill_rect(x, y, key_pixels, key_h, selected ? 0xffd2eaffu : 0xffeef0f2u);
            ui_fill_rect(x, y, key_pixels, 1, 0xffffffffu);
            ui_fill_rect(x, y + key_h - 1, key_pixels, 1, 0xff899098u);
            if (selected) {
                ui_fill_rect(x, y, key_pixels, 3, UI_COLOR_ACCENT);
                ui_fill_rect(x, y + key_h - 3, key_pixels, 3, UI_COLOR_ACCENT);
            }
            ui_draw_text(x + (key_pixels - text_w) / 2, y + (scale == 1 ? 20 : 14),
                keys[col].label, 0xff303840u, scale);
            x += key_pixels + gap;
        }
    }

    ui_draw_text(92, 504,
        tr(lang, "Digite parte do nome, Title ID ou versao.", "Type part of the name, Title ID, or version."),
        0xff303840u, 2);
    ui_draw_footer(
        tr(lang, "Setas escolher   X tecla   Square apagar", "Arrows choose   X key   Square delete"),
        tr(lang, "Start/OK confirma   Circle cancela", "Start/OK confirms   Circle cancels"));
    ui_present();
}

static int edit_search_query_manual(fpsu_lang lang, char *query, size_t query_size)
{
    int cursor_row = 0;
    int cursor_col = 0;

    if (!query || query_size == 0) {
        return 0;
    }

    for (;;) {
        u32 btn;
        size_t len;
        draw_search_editor(lang, query, cursor_row, cursor_col);
        btn = frame_input();
        if (btn & FPSU_BUTTON_RIGHT) {
            int row_len = search_key_row_len(cursor_row);
            cursor_col = row_len > 0 ? (cursor_col + 1) % row_len : 0;
        }
        if (btn & FPSU_BUTTON_LEFT) {
            int row_len = search_key_row_len(cursor_row);
            cursor_col = row_len > 0 ? (cursor_col + row_len - 1) % row_len : 0;
        }
        if (btn & FPSU_BUTTON_DOWN) {
            int rows = search_key_row_count();
            int row_len;
            cursor_row = (cursor_row + 1) % rows;
            row_len = search_key_row_len(cursor_row);
            if (cursor_col >= row_len) {
                cursor_col = row_len > 0 ? row_len - 1 : 0;
            }
        }
        if (btn & FPSU_BUTTON_UP) {
            int rows = search_key_row_count();
            int row_len;
            cursor_row = (cursor_row + rows - 1) % rows;
            row_len = search_key_row_len(cursor_row);
            if (cursor_col >= row_len) {
                cursor_col = row_len > 0 ? row_len - 1 : 0;
            }
        }
        if (btn & FPSU_BUTTON_CROSS) {
            const search_key *key = search_key_at(cursor_row, cursor_col);
            len = strlen(query);
            if (!key) {
                continue;
            }
            if (key->action == SEARCH_KEY_OK) {
                return 1;
            }
            if (key->action == SEARCH_KEY_CLEAR) {
                query[0] = '\0';
                continue;
            }
            if (key->action == SEARCH_KEY_BACKSPACE) {
                if (len > 0) {
                    query[len - 1] = '\0';
                }
                continue;
            }
            if (len + 1 < query_size) {
                query[len] = key->value;
                query[len + 1] = '\0';
            }
        }
        if (btn & FPSU_BUTTON_SQUARE) {
            len = strlen(query);
            if (len > 0) {
                query[len - 1] = '\0';
            }
        }
        if (btn & FPSU_BUTTON_SELECT) {
            query[0] = '\0';
        }
        if (btn & FPSU_BUTTON_START) {
            return 1;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return 0;
        }
    }
}

static int edit_search_query(fpsu_lang lang, char *query, size_t query_size)
{
    return edit_search_query_manual(lang, query, query_size);
}

static void draw_results(fpsu_lang lang, fpsu_game_result *results, int total_count,
    const int *visible, int count, int index, const char *query)
{
    int i;
    int first = index - 3;
    int left = 64;
    int list_w = 480;
    int detail_x = 585;
    char meta[128];
    const fpsu_game_result *r = NULL;
    const fpsu_patch_option *info_opt;

    if (first < 0) {
        first = 0;
    }
    if (first > count - 7) {
        first = count - 7;
    }
    if (first < 0) {
        first = 0;
    }

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Biblioteca > Jogos encontrados", "Library > Games found"), "LIBRARY");

    snprintf(meta, sizeof(meta), "%s: %s", tr(lang, "Busca", "Search"),
        query && query[0] ? query : tr(lang, "sem filtro", "no filter"));
    ui_draw_text(left, 94, meta, query && query[0] ? UI_COLOR_ACCENT_2 : UI_COLOR_MUTED, 1);

    if (count <= 0) {
        ui_fill_rect(left, 140, list_w, 260, UI_COLOR_PANEL);
        ui_draw_text(left + 24, 168,
            tr(lang, "Nenhum jogo encontrado nessa busca.", "No game found in this search."),
            UI_COLOR_TEXT, 2);
        ui_draw_text(left + 24, 214,
            tr(lang, "Triangle muda a busca. Select limpa.", "Triangle changes search. Select clears it."),
            UI_COLOR_MUTED, 2);
        ui_draw_footer(
            tr(lang, "Triangle buscar   Select limpar", "Triangle search   Select clear"),
            tr(lang, "Circle voltar", "Circle back"));
        ui_present();
        return;
    }

    r = &results[visible[index]];
    info_opt = &r->unlock;

    for (i = 0; i < 7 && first + i < count; ++i) {
        fpsu_game_result *row = &results[visible[first + i]];
        char body[96];
        snprintf(body, sizeof(body), "%s  %s", row->game.title_id, row->game.version);
        ui_draw_menu_item(left, 120 + i * 72, list_w, row->game.title, body, first + i == index, 1);
    }

    ui_fill_rect(detail_x, 120, ui_screen_width() - detail_x - 64, 500, UI_COLOR_PANEL);
    ui_fill_rect(detail_x, 120, 9, 500, UI_COLOR_ACCENT);
    ui_draw_text(detail_x + 32, 152, r->game.title, UI_COLOR_TEXT, 2);
    snprintf(meta, sizeof(meta), "%s  %s", r->game.title_id, r->game.version);
    ui_draw_text(detail_x + 32, 190, meta, UI_COLOR_MUTED, 2);
    draw_status_line(lang, detail_x + 32, 230, patch_kind_title(lang, FPSU_PATCH_60, r->fps60.status), r->fps60.status);
    draw_status_line(lang, detail_x + 32, 302, patch_kind_title(lang, FPSU_PATCH_UNLOCK, r->unlock.status), r->unlock.status);
    draw_status_line(lang, detail_x + 32, 374, patch_kind_title(lang, FPSU_PATCH_30, r->fps30.status), r->fps30.status);
    if (r->fps30.status != FPSU_STATUS_NOT_FOUND && r->fps30.status != FPSU_STATUS_UNAVAILABLE) {
        info_opt = &r->fps30;
    }
    if (info_opt->status != FPSU_STATUS_UPDATE_REQUIRED && info_opt->status != FPSU_STATUS_OTHER_VERSION &&
        (r->fps60.status == FPSU_STATUS_UPDATE_REQUIRED || r->fps60.status == FPSU_STATUS_OTHER_VERSION)) {
        info_opt = &r->fps60;
    }
    if (info_opt->status != FPSU_STATUS_UPDATE_REQUIRED && info_opt->status != FPSU_STATUS_OTHER_VERSION &&
        (r->fps30.status == FPSU_STATUS_UPDATE_REQUIRED || r->fps30.status == FPSU_STATUS_OTHER_VERSION)) {
        info_opt = &r->fps30;
    }
    ui_draw_text(detail_x + 32, 452, status_help(lang, info_opt->status), UI_COLOR_MUTED, 1);
    if (info_opt->label[0] && info_opt->status != FPSU_STATUS_NOT_FOUND) {
        ui_draw_text(detail_x + 32, 478, info_opt->label, status_color(info_opt->status), 1);
    }
    if (info_opt->note[0] &&
        (info_opt->status == FPSU_STATUS_UPDATE_REQUIRED || info_opt->status == FPSU_STATUS_OTHER_VERSION)) {
        draw_multiline_text(detail_x + 32, 502, info_opt->note, UI_COLOR_MUTED, 1, 62, 2);
    }
    ui_draw_text(detail_x + 32, 542, tr(lang, "X abre: Jogo > Acoes > aplicar/analisar/restaurar.", "X opens: Game > Actions > apply/analyze/restore."), UI_COLOR_MUTED, 1);
    ui_draw_text(detail_x + 32, 568, tr(lang, "Square: Jogo > Backup > restaurar somente este jogo.", "Square: Game > Backup > restore only this game."), UI_COLOR_MUTED, 1);
    draw_status_legend(lang, detail_x + 32, 594);
    if (query && query[0]) {
        snprintf(meta, sizeof(meta), "%d/%d  (%d total)", index + 1, count, total_count);
    } else {
        snprintf(meta, sizeof(meta), "%d/%d", index + 1, total_count);
    }
    ui_draw_text(detail_x + 32, 622, meta, UI_COLOR_ACCENT_2, 2);

    ui_draw_footer(tr(lang, "UP/DOWN jogo   X opcoes   Triangle busca", "UP/DOWN game   X options   Triangle search"),
        tr(lang, "Select limpa   Circle voltar", "Select clears   Circle back"));
    ui_present();
}

static int restore_game_backup(fpsu_lang lang, const fpsu_game *game)
{
    char target[FPSU_MAX_PATH];
    char backup[FPSU_MAX_PATH];
    char msg[1024];
    int restored = 0;
    int removed = 0;
    int has_backup;

    has_backup = !is_mounted_media(game) &&
        backup_find_latest_for_title(game->title_id, target, sizeof(target), backup, sizeof(backup)) == 0;

    if (has_backup) {
        snprintf(msg, sizeof(msg), "%s\n%s\n\n%s\n\n%s", game->title, game->title_id, target,
            tr(lang,
                "Vou restaurar backups deste jogo e remover scripts/NCL criados pelo FPSU.",
                "Backups for this game will be restored and FPSU scripts/NCL files will be removed."));
    } else {
        snprintf(msg, sizeof(msg), "%s\n%s\n\n%s", game->title, game->title_id,
            is_mounted_media(game) ?
                tr(lang,
                    "Jogo montado por ISO/disco: vou remover apenas scripts/NCL criados pelo FPSU, sem restaurar arquivo de jogo.",
                    "Mounted ISO/disc game: only FPSU scripts/NCL files will be removed, without restoring game files.") :
                tr(lang,
                    "Nao achei backup antigo, mas posso remover os scripts/NCL criados pelo FPSU para este jogo.",
                    "No old backup was found, but FPSU scripts/NCL files for this game can be removed."));
    }
    if (!app_confirm(lang, i18n_text(lang, TXT_CONFIRM_RESTORE), msg)) {
        return 0;
    }

    draw_busy_notice(lang,
        tr(lang, "Restaurando/removendo patch", "Restoring/removing patch"),
        tr(lang, "Aguarde. Limpando apenas este jogo.", "Please wait. Cleaning only this game."));

    removed = patch_db_remove_generated(game);
    if (has_backup && backup_restore_all_for_title(game->title_id, &restored) == 0) {
        snprintf(msg, sizeof(msg), "%s\n%s: %d\n%s: %d", game->title,
            tr(lang, "Backups restaurados", "Backups restored"), restored,
            tr(lang, "Arquivos FPSU removidos", "FPSU files removed"), removed);
        app_notice(lang, i18n_text(lang, TXT_RESTORE_OK), msg, UI_COLOR_GREEN);
        return 1;
    }

    if (removed > 0) {
        snprintf(msg, sizeof(msg), "%s\n%s: %d", game->title,
            tr(lang, "Arquivos FPSU removidos", "FPSU files removed"), removed);
        app_notice(lang,
            tr(lang, "Patch FPSU removido", "FPSU patch removed"),
            msg, UI_COLOR_GREEN);
        return 1;
    }

    if (!has_backup) {
        app_notice(lang, i18n_text(lang, TXT_NO_BACKUP),
            tr(lang, "Nao existe backup nem patch FPSU gerado para este jogo.", "No backup or generated FPSU patch exists for this game."),
            UI_COLOR_RED);
        return 0;
    }

    app_notice(lang, i18n_text(lang, TXT_RESTORE_FAIL), game->title, UI_COLOR_RED);
    return 0;
}

static void apply_option(fpsu_lang lang, fpsu_game_result *result, fpsu_patch_option *option)
{
    char msg[1024];
    char path[FPSU_MAX_PATH];

    if (!result || !option) {
        return;
    }

    if (option->status == FPSU_STATUS_UNAVAILABLE) {
        app_notice(lang, status_text(lang, option->status),
            tr(lang, "Indisponivel: o app nao achou arquivo do jogo para testar ou alterar.", "Unavailable: no game file was found to test or change."),
            UI_COLOR_RED);
        return;
    }

    if (option->status == FPSU_STATUS_NATIVE_60) {
        app_notice(lang, status_text(lang, option->status),
            tr(lang, "Nao precisa aplicar 60 FPS neste jogo. Ele ja roda com alvo de 60.", "No need to apply 60 FPS to this game. It already targets 60."),
            UI_COLOR_GREEN);
        return;
    }

    if (option->status == FPSU_STATUS_UPDATE_REQUIRED) {
        if (fpsu_method_is_ncl_write(option->method) && option->payload[0] != '\0') {
            snprintf(msg, sizeof(msg), "%s\n\n%s\n\n%s", result->game.title,
                option->note[0] ? option->note :
                    tr(lang, "A versao nao foi confirmada, mas existe uma rota instalavel para este Title ID.", "The version was not confirmed, but an installable route exists for this Title ID."),
                tr(lang,
                    "Vou deixar aplicar mesmo assim. NAO SEGURO: use quando o jogo esta em ISO/HD externo e a versao aparece errada ou desconhecida.",
                    "This can be applied anyway. UNSAFE: use this when the game is on ISO/external HDD and the version appears wrong or unknown."));
            if (!app_confirm(lang,
                    tr(lang, "Aplicar sem confirmar versao", "Apply without confirmed version"), msg)) {
                return;
            }
            option->status = FPSU_STATUS_APPLICABLE;
            snprintf(option->note, sizeof(option->note), "%s",
                "Aplicado sem confirmar versao do jogo. NAO SEGURO.");
        } else {
        app_notice(lang, status_text(lang, option->status),
            option->note[0] ? option->note :
                tr(lang, "Atualize o jogo para a versao indicada e escaneie novamente.", "Update the game to the indicated version and scan again."),
            UI_COLOR_ACCENT_2);
        return;
        }
    }

    if (option->status == FPSU_STATUS_NOT_FOUND || option->method == FPSU_METHOD_NONE) {
        app_notice(lang,
            tr(lang, "Sem codigo para aplicar", "No code to apply"),
            tr(lang,
                "Nao existe codigo instalavel nesta opcao. Escolha 60 FPS alvo ou FPS ilimitado / +60 quando um deles aparecer disponivel.",
                "There is no installable code for this option. Choose 60 FPS target or Unlimited / +60 FPS when one of them is available."),
            UI_COLOR_ACCENT_2);
        return;
    }

    if (option->status == FPSU_STATUS_PC_REQUIRED ||
        option->method == FPSU_METHOD_EBOOT_PC ||
        option->method == FPSU_METHOD_PATTERN_ONLY) {
        app_notice(lang,
            tr(lang, "Precisa de codigo pronto", "Ready code required"),
            tr(lang,
                "Esta opcao nao tem codigo NCL pronto no banco. Use o app de PC para testar/criar codigo ou aguarde atualizacao do banco.",
                "This option has no ready NCL code in the database. Use the PC app to test/create code or wait for a database update."),
            UI_COLOR_ACCENT_2);
        return;
    }

    if (option->status == FPSU_STATUS_OTHER_VERSION) {
        if (option->payload[0] != '\0' && fpsu_method_is_ncl_write(option->method)) {
            snprintf(msg, sizeof(msg), "%s\n\n%s\n\n%s", result->game.title,
                option->note[0] ? option->note :
                    tr(lang, "Patch encontrado para outra versao do mesmo jogo.", "Patch found for another version of the same game."),
                tr(lang,
                    "Vou deixar aplicar sem analisar EBOOT. NAO SEGURO: use para ISO/HD externo quando a versao aparece errada ou desconhecida.",
                    "This can be applied without EBOOT analysis. UNSAFE: use it for ISO/external HDD when the version appears wrong or unknown."));
            if (!app_confirm(lang,
                    tr(lang, "Aplicar mesmo assim", "Apply anyway"), msg)) {
                return;
            }
            option->status = FPSU_STATUS_APPLICABLE;
            snprintf(option->note, sizeof(option->note), "%s",
                "Patch de outra versao aplicado sem analisar EBOOT. NAO SEGURO.");
        } else {
            app_notice(lang,
                tr(lang, "Sem codigo para aplicar", "No code to apply"),
                tr(lang,
                    "Existe informacao de outra versao, mas nao ha codigo instalavel para aplicar pelo PSUF.",
                    "There is information for another version, but no installable code for PSUF to apply."),
                UI_COLOR_ACCENT_2);
            return;
        }
    }

    if (option->status == FPSU_STATUS_UNTESTED || option->status == FPSU_STATUS_APPLICABLE) {
        snprintf(msg, sizeof(msg), "%s\n\n%s", option->label,
            tr(lang, "Modo experimental. Crie backup e teste com cuidado.", "Experimental mode. Keep a backup and test carefully."));
        if (!app_confirm(lang, i18n_text(lang, TXT_ACTION_UNLOCK_UNSAFE), msg)) {
            return;
        }
    }

    snprintf(msg, sizeof(msg), "%s\n%s", result->game.title, option->label);
    if (!app_confirm(lang, i18n_text(lang, TXT_CONFIRM_APPLY), msg)) {
        return;
    }

    if (fpsu_method_is_ncl_write(option->method)) {
        draw_busy_notice(lang,
            tr(lang, "Aplicando patch, aguarde", "Applying patch, please wait"),
            tr(lang, "Criando backup pequeno e gravando NCL.", "Creating a small backup and writing NCL."));
        if (patch_db_write_ncl(&result->game, option, path, sizeof(path)) == 0) {
#if FPSU_HEN_DIAG_BUILD
            snprintf(msg, sizeof(msg), "%s\n%s\n\n%s\n%s\n%s\n%s",
                result->game.title,
                tr(lang, "Build de teste: abra o jogo e espere os 2 minutos.",
                    "Test build: open the game and wait 2 minutes."),
                tr(lang, "Se aparecer aviso no jogo e o FPS nao mudar, aperte START para forcar pelo Artemis.",
                    "If an in-game message appears and FPS does not change, press START to force it through Artemis."),
                tr(lang, "Se nao aparecer aviso nenhum, mande estes logs:",
                    "If no message appears, send these logs:"),
                FPSU_CACHE_ROOT "/last_apply_setup.log",
                FPSU_CACHE_ROOT "/webman_ingame.log");
#else
            snprintf(msg, sizeof(msg), "%s\n%s\n\n%s\n%s",
                result->game.title,
                tr(lang, "O patch foi enviado. Agora quem aplica no jogo e o webMAN/Artemis.",
                    "The patch was sent. webMAN/Artemis applies it in game."),
                tr(lang, "Se o FPS nao mudar, provavelmente falhou no Artemis ou PS3MAPI. Mande este log:",
                    "If FPS does not change, Artemis or PS3MAPI probably failed. Send this log:"),
                FPSU_CACHE_ROOT "/webman_ingame.log");
#endif
            option->status = FPSU_STATUS_KNOWN;
            snprintf(option->note, sizeof(option->note), "%s",
                "Patch enviado para teste HEN-safe. Temporarios sao limpos depois.");
            app_notice(lang, i18n_text(lang, TXT_APPLY_OK), msg, UI_COLOR_ACCENT_2);
        } else {
            app_notice(lang, i18n_text(lang, TXT_APPLY_FAIL), result->game.title, UI_COLOR_RED);
        }
        return;
    }

    app_notice(lang,
        tr(lang, "Metodo nao instalavel", "Method not installable"),
        tr(lang, "O PS3 nao conseguiu transformar esta opcao em patch instalavel.", "The PS3 could not turn this option into an installable patch."),
        UI_COLOR_ACCENT_2);
}

static void analyze_selected_game(fpsu_lang lang, fpsu_game_result *r)
{
    static fpsu_patch_option force_choices[FPSU_MAX_FORCE_CHOICES];
    int ret;

    if (r->game.eboot_path[0] == '\0') {
        app_notice(lang, i18n_text(lang, TXT_STATUS_NOT_FOUND),
            tr(lang, "EBOOT.BIN nao encontrado para analisar.", "EBOOT.BIN was not found for analysis."),
            UI_COLOR_RED);
        return;
    }
    if (is_mounted_media(&r->game)) {
        app_notice(lang,
            tr(lang, "Analise desativada no jogo montado", "Mounted game analysis disabled"),
            tr(lang,
                "Para evitar travamento, o PSUF nao le EBOOT/ISO inteiro no jogo montado. Ele so usa metadados e patches conhecidos.",
                "To avoid freezes, PSUF does not read EBOOT/full ISO for mounted games. It only uses metadata and known patches."),
            UI_COLOR_ACCENT_2);
        return;
    }

    ret = analyze_result_patterns(lang, r, 0, 1, 1);
    if (ret > 0) {
        app_notice(lang,
            r->unlock.status == FPSU_STATUS_APPLICABLE || r->fps60.status == FPSU_STATUS_APPLICABLE ?
                i18n_text(lang, TXT_STATUS_APPLICABLE) : i18n_text(lang, TXT_STATUS_UNTESTED),
            tr(lang, "Padrao encontrado. Se aparecer Aplicavel, teste com backup e cuidado.", "Pattern found. If Applicable appears, test with backup and care."),
            r->unlock.status == FPSU_STATUS_APPLICABLE || r->fps60.status == FPSU_STATUS_APPLICABLE ? UI_COLOR_GREEN : UI_COLOR_ORANGE);
    } else if (ret < 0) {
        app_notice(lang,
            tr(lang, "Analise cancelada", "Analysis canceled"),
            tr(lang, "Nada foi alterado neste jogo.", "No files were changed for this game."),
            UI_COLOR_ORANGE);
    } else {
        int routes = patch_db_collect_game_options(&r->game, force_choices, FPSU_MAX_FORCE_CHOICES);
        if (routes > 0 ||
            r->fps60.status == FPSU_STATUS_OTHER_VERSION ||
            r->unlock.status == FPSU_STATUS_OTHER_VERSION ||
            r->fps30.status == FPSU_STATUS_OTHER_VERSION ||
            r->fps60.status == FPSU_STATUS_UPDATE_REQUIRED ||
            r->unlock.status == FPSU_STATUS_UPDATE_REQUIRED ||
            r->fps30.status == FPSU_STATUS_UPDATE_REQUIRED) {
            app_notice(lang,
                tr(lang, "Codigo existe no banco", "Code exists in database"),
                tr(lang,
                    "A analise nao achou padrao novo neste EBOOT, mas existe codigo salvo para este jogo, outra versao ou outra regiao. Use Escolher/forcar rota se quiser testar.",
                    "Analysis found no new pattern in this EBOOT, but saved code exists for this game, another version, or another region. Use Choose/force route if you want to test."),
                UI_COLOR_ACCENT_2);
        } else {
            app_notice(lang, i18n_text(lang, TXT_STATUS_NOT_FOUND),
                tr(lang, "Nenhum padrao FPS conhecido foi encontrado neste EBOOT.", "No known FPS pattern was found in this EBOOT."),
                UI_COLOR_RED);
        }
    }
}

static int can_force_choice(const fpsu_patch_option *option)
{
    if (!option ||
        option->kind == FPSU_PATCH_NONE ||
        option->status == FPSU_STATUS_NOT_FOUND ||
        option->status == FPSU_STATUS_UNAVAILABLE ||
        option->status == FPSU_STATUS_NATIVE_60) {
        return 0;
    }
    if (option->payload[0] != '\0' && fpsu_method_is_ncl_write(option->method)) {
        return 1;
    }
    return 0;
}

static int force_choice_already_seen(const fpsu_patch_option *choices, int count,
    const fpsu_patch_option *option)
{
    int i;
    if (!choices || !option) {
        return 1;
    }
    for (i = 0; i < count; ++i) {
        if (choices[i].kind == option->kind &&
            strcmp(choices[i].payload, option->payload) == 0) {
            return 1;
        }
    }
    return 0;
}

static int compact_force_choices(fpsu_patch_option *choices, int count)
{
    static fpsu_patch_option compact[FPSU_MAX_FORCE_CHOICES];
    int out = 0;
    int i;

    if (!choices || count <= 0) {
        return 0;
    }
    memset(compact, 0, sizeof(compact));
    for (i = 0; i < count && i < FPSU_MAX_FORCE_CHOICES; ++i) {
        if (!can_force_choice(&choices[i])) {
            continue;
        }
        if (force_choice_already_seen(compact, out, &choices[i])) {
            continue;
        }
        compact[out++] = choices[i];
    }
    memcpy(choices, compact, sizeof(compact));
    return out;
}

static int prepare_force_choice(fpsu_lang lang, fpsu_game_result *r, fpsu_patch_option *option)
{
    pattern_context ctx;
    int ret;

    if (!r || !option) {
        return 0;
    }
    if (option->payload[0] != '\0' && fpsu_method_is_ncl_write(option->method)) {
        return 1;
    }
    if (option->kind != FPSU_PATCH_60 && option->kind != FPSU_PATCH_UNLOCK) {
        app_notice(lang,
            tr(lang, "Sem rota automatica", "No automatic route"),
            tr(lang,
                "O forcar por padrao automatico hoje funciona para 60 FPS e FPS ilimitado / +60.",
                "Automatic pattern forcing currently works for 60 FPS and Unlimited / +60 FPS."),
            UI_COLOR_ACCENT_2);
        return 0;
    }
    if (r->game.eboot_path[0] == '\0' || is_mounted_media(&r->game)) {
        app_notice(lang,
            tr(lang, "Precisa de EBOOT local", "Local EBOOT required"),
            tr(lang,
                "Para gerar um patch provavel, o PSUF precisa ler o EBOOT de uma instalacao em pasta/HDD. Em ISO/jogo montado, use um codigo ja existente no banco.",
                "To generate a probable patch, PSUF must read the EBOOT from a folder/HDD install. For ISO/mounted games, use an existing database code."),
            UI_COLOR_ACCENT_2);
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.lang = lang;
    ctx.index = 0;
    ctx.count = 1;
    ret = scanner_build_force_payload_ex(&r->game, option->kind,
        force_pattern_progress_cb, &ctx, option->payload, sizeof(option->payload));
    if (ctx.cancel || ret < 0) {
        app_notice(lang,
            tr(lang, "Forcar cancelado", "Force canceled"),
            tr(lang, "Nada foi alterado neste jogo.", "No files were changed for this game."),
            UI_COLOR_ORANGE);
        return 0;
    }
    if (ret <= 0 || option->payload[0] == '\0') {
        app_notice(lang,
            tr(lang, "Sem padrao aplicavel", "No applicable pattern"),
            tr(lang,
                "O PSUF tentou gerar um patch provavel, mas nao achou valor conhecido para transformar em NCL.",
                "PSUF tried to generate a probable patch but found no known value that can be turned into NCL."),
            UI_COLOR_ACCENT_2);
        return 0;
    }

    option->method = FPSU_METHOD_NCL;
    option->status = FPSU_STATUS_APPLICABLE;
    snprintf(option->source, sizeof(option->source), "%s", "PSUF force pattern");
    snprintf(option->label, sizeof(option->label), "%s",
        option->kind == FPSU_PATCH_60 ? "Forcado por padrao: 60 FPS" :
            "Forcado por padrao: FPS ilimitado / +60");
    snprintf(option->note, sizeof(option->note), "%s",
        "Payload provavel gerado no PS3 a partir do EBOOT local. NAO SEGURO.");
    option->delay_seconds = FPSU_DEFAULT_PATCH_DELAY_SECONDS;
    return 1;
}

static void force_apply_option(fpsu_lang lang, fpsu_game_result *r)
{
    static fpsu_patch_option choices[FPSU_MAX_FORCE_CHOICES];
    int choice_count = 0;
    int selected = 0;
    char msg[1024];

    if (!r) {
        return;
    }

    memset(choices, 0, sizeof(choices));
    choice_count = patch_db_collect_game_options(&r->game, choices, FPSU_MAX_FORCE_CHOICES);
    if (choice_count < FPSU_MAX_FORCE_CHOICES) {
        choices[choice_count++] = r->fps60;
    }
    if (choice_count < FPSU_MAX_FORCE_CHOICES) {
        choices[choice_count++] = r->unlock;
    }
    if (choice_count < FPSU_MAX_FORCE_CHOICES) {
        choices[choice_count++] = r->fps30;
    }
    choice_count = compact_force_choices(choices, choice_count);

    if (choice_count == 0) {
        app_notice(lang, status_text(lang, FPSU_STATUS_UNAVAILABLE),
            tr(lang, "Nao achei rota pronta no banco para este jogo.", "No ready database route was found for this game."),
            UI_COLOR_RED);
        return;
    }

    for (;;) {
        int i;
        int shown;
        int start;
        u32 btn;
        ui_begin_frame();
        ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
            tr(lang, "Escolha o patch", "Choose patch"), r->game.title_id);
        ui_draw_text(100, 94, r->game.title, UI_COLOR_MUTED, 1);
        start = selected - 5;
        if (start < 0) {
            start = 0;
        }
        if (choice_count > 6 && start > choice_count - 6) {
            start = choice_count - 6;
        }
        shown = choice_count - start;
        if (shown > 6) {
            shown = 6;
        }
        for (i = 0; i < shown; ++i) {
            int item = start + i;
            char body[224];
            snprintf(body, sizeof(body), "%d/%d | %s > %s | %s > %s | %s",
                item + 1, choice_count,
                tr(lang, "Status", "Status"), status_text(lang, choices[item].status),
                tr(lang, "Fonte", "Source"), choices[item].source[0] ? choices[item].source : "PSUF",
                item == 0 ? tr(lang, "mais provavel", "most likely") : tr(lang, "alternativa", "alternative"));
            ui_draw_menu_item(100, 118 + i * 84, ui_screen_width() - 200,
                choices[item].label[0] ? choices[item].label :
                    patch_kind_title(lang, choices[item].kind, choices[item].status),
                body, selected == item, 1);
        }
        ui_draw_footer(tr(lang, "UP/DOWN escolher   X aplicar com backup", "UP/DOWN choose   X apply with backup"),
            tr(lang, "Circle voltar", "Circle back"));
        ui_present();

        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % choice_count;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + choice_count - 1) % choice_count;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            break;
        }
    }

    if (!prepare_force_choice(lang, r, &choices[selected])) {
        return;
    }

    snprintf(msg, sizeof(msg), "%s\n\n%s\n\n%s", choices[selected].label,
        choices[selected].note[0] ? choices[selected].note :
            tr(lang, "Patch escolhido pelo usuario.", "Patch selected by the user."),
        tr(lang,
            "NAO SEGURO. Se der tela preta, restaure o backup ou remova o patch pelo PSUF.",
            "UNSAFE. If it black-screens, restore the backup or remove the patch with PSUF."));
    if (!app_confirm(lang, tr(lang, "Forcar este patch", "Force this patch"), msg)) {
        return;
    }
    if (choices[selected].status == FPSU_STATUS_OTHER_VERSION ||
        choices[selected].status == FPSU_STATUS_UPDATE_REQUIRED) {
        choices[selected].status = FPSU_STATUS_APPLICABLE;
        snprintf(choices[selected].note, sizeof(choices[selected].note), "%s",
            "Patch de outra versao/update forcado pelo usuario. NAO SEGURO.");
    }
    apply_option(lang, r, &choices[selected]);
}

static void show_graphics_patches(fpsu_lang lang, fpsu_game_result *r)
{
    static fpsu_patch_option choices[FPSU_MAX_GRAPHICS_CHOICES];
    int choice_count;
    int selected = 0;
    char msg[1024];

    if (!r) {
        return;
    }

    memset(choices, 0, sizeof(choices));
    choice_count = patch_db_collect_graphics_options(&r->game, choices, FPSU_MAX_GRAPHICS_CHOICES);
    if (choice_count <= 0) {
        app_notice(lang,
            tr(lang, "Sem patch extra/grafico", "No extra/graphics patch"),
            tr(lang,
                "Nao achei patch extra/grafico no banco separado para este Title ID.",
                "No extra/graphics patch was found in the separate database for this Title ID."),
            UI_COLOR_ACCENT_2);
        return;
    }

    for (;;) {
        int i;
        int first;
        int visible_rows = 6;
        u32 btn;

        if (selected < 0) {
            selected = 0;
        }
        if (selected >= choice_count) {
            selected = choice_count - 1;
        }
        first = selected - visible_rows + 1;
        if (first < 0) {
            first = 0;
        }

        ui_begin_frame();
        ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
            tr(lang, "Jogo > Extras/graficos", "Game > Extra/graphics"), r->game.title_id);
        ui_draw_text(100, 90, r->game.title, UI_COLOR_MUTED, 1);

        for (i = 0; i < visible_rows && first + i < choice_count; ++i) {
            char body[224];
            fpsu_patch_option *opt = &choices[first + i];
            snprintf(body, sizeof(body), "%s > %s | %s > %s",
                tr(lang, "Status", "Status"), status_text(lang, opt->status),
                tr(lang, "Fonte", "Source"), opt->source[0] ? opt->source : "PSUF");
            ui_draw_menu_item(100, 120 + i * 78, ui_screen_width() - 200,
                opt->label[0] ? opt->label : tr(lang, "Patch extra/grafico", "Extra/graphics patch"),
                body, selected == first + i, 1);
        }

        snprintf(msg, sizeof(msg), "%d/%d", selected + 1, choice_count);
        ui_draw_text(100, 610, msg, UI_COLOR_ACCENT_2, 2);
        ui_draw_footer(tr(lang, "UP/DOWN escolher   X aplicar", "UP/DOWN choose   X apply"),
            tr(lang, "Circle voltar", "Circle back"));
        ui_present();

        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % choice_count;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + choice_count - 1) % choice_count;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            break;
        }
    }

    snprintf(msg, sizeof(msg), "%s\n\n%s\n\n%s",
        choices[selected].label,
        choices[selected].note[0] ? choices[selected].note :
            tr(lang, "Patch extra/grafico importado. Ainda nao confirmado pelo PSUF.", "Imported extra/graphics patch. Not confirmed by PSUF yet."),
        tr(lang,
            "Nao e patch de FPS. Pode alterar imagem, efeitos, desempenho visual ou estabilidade. Teste com cuidado.",
            "This is not an FPS patch. It may change visuals, effects, visual performance, or stability. Test carefully."));
    if (!app_confirm(lang, tr(lang, "Aplicar patch extra/grafico", "Apply extra/graphics patch"), msg)) {
        return;
    }
    apply_option(lang, r, &choices[selected]);
}

static void action_body(char *out, size_t out_size, fpsu_lang lang, const fpsu_patch_option *option)
{
    if (!out || out_size == 0 || !option) {
        return;
    }
    if (option->label[0] && option->status != FPSU_STATUS_NOT_FOUND) {
        if (option->source[0]) {
            snprintf(out, out_size, "%s > %s | %s > %s | %s > %s",
                tr(lang, "Status", "Status"), status_text(lang, option->status),
                tr(lang, "Patch", "Patch"), option->label,
                tr(lang, "Fonte", "Source"), option->source);
            return;
        }
        snprintf(out, out_size, "%s > %s | %s > %s",
            tr(lang, "Status", "Status"), status_text(lang, option->status),
            tr(lang, "Patch", "Patch"), option->label);
    } else {
        snprintf(out, out_size, "%s > %s",
            tr(lang, "Status", "Status"), status_text(lang, option->status));
    }
}

static void draw_actions(fpsu_lang lang, const fpsu_game_result *r, int selected, int graphics_count)
{
    char body[224];

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Jogo > Acoes", "Game > Actions"), r->game.title_id);
    ui_draw_text(100, 82, r->game.title, UI_COLOR_MUTED, 1);

    action_body(body, sizeof(body), lang, &r->fps60);
    ui_draw_menu_item(100, 98, ui_screen_width() - 200,
        tr(lang, "Jogo > Acoes > 60 FPS alvo", "Game > Actions > 60 FPS target"),
        body, selected == 0, r->fps60.status != FPSU_STATUS_NOT_FOUND);

    action_body(body, sizeof(body), lang, &r->unlock);
    ui_draw_menu_item(100, 162, ui_screen_width() - 200,
        r->unlock.status == FPSU_STATUS_KNOWN ?
            tr(lang, "Jogo > Acoes > FPS ilimitado / +60", "Game > Actions > Unlimited / +60 FPS") :
            tr(lang, "Jogo > Acoes > FPS ilimitado / +60 (nao seguro)", "Game > Actions > Unlimited / +60 FPS (unsafe)"),
        body, selected == 1, r->unlock.status != FPSU_STATUS_NOT_FOUND);

    action_body(body, sizeof(body), lang, &r->fps30);
    ui_draw_menu_item(100, 226, ui_screen_width() - 200,
        r->fps30.status == FPSU_STATUS_KNOWN ?
            tr(lang, "Jogo > Acoes > 30 FPS", "Game > Actions > 30 FPS") :
            tr(lang, "Jogo > Acoes > 30 FPS (teste)", "Game > Actions > 30 FPS (test)"),
        body, selected == 2, r->fps30.status != FPSU_STATUS_NOT_FOUND);

    ui_draw_menu_item(100, 290, ui_screen_width() - 200,
        tr(lang, "Jogo > Acoes > Patches extras/graficos (teste)", "Game > Actions > Extra/graphics patches (test)"),
        graphics_count > 0 ?
            tr(lang, "Lista efeitos visuais e outros codigos do banco separado.", "Lists visual effects and other codes from the separate database.") :
            tr(lang, "Nenhum patch extra/grafico para este Title ID.", "No extra/graphics patch for this Title ID."),
        selected == 3, 1);

    ui_draw_menu_item(100, 354, ui_screen_width() - 200,
        tr(lang, "Jogo > Teste > Escolher/forcar rota (NAO SEGURO)", "Game > Test > Choose/force route (UNSAFE)"),
        tr(lang, "Mostra rotas prontas do banco. Nao analisa EBOOT.", "Shows ready database routes. Does not analyze EBOOT."),
        selected == 4, 1);

    ui_draw_menu_item(100, 418, ui_screen_width() - 200,
        tr(lang, "Jogo > Analise > Analisar este jogo", "Game > Analysis > Analyze this game"),
        tr(lang, "Le o EBOOT deste jogo e tenta transformar em Aplicavel.", "Reads this EBOOT and tries to make it Applicable."),
        selected == 5, 1);

    ui_draw_menu_item(100, 482, ui_screen_width() - 200,
        tr(lang, "Jogo > Backup > Restaurar este jogo", "Game > Backup > Restore this game"),
        tr(lang, "Restaura somente o backup deste Title ID.", "Restores only this Title ID backup."),
        selected == 6, 1);

    ui_draw_menu_item(100, 546, ui_screen_width() - 200,
        tr(lang, "Voltar > Biblioteca", "Back > Library"), "", selected == 7, 1);

    ui_draw_footer(tr(lang, "UP/DOWN navegar   X executar opcao", "UP/DOWN navigate   X run option"),
        tr(lang, "Circle voltar", "Circle back"));
    ui_present();
}

static int normal_action_available(const fpsu_patch_option *option)
{
    return option && option->status != FPSU_STATUS_NOT_FOUND;
}

static void show_normal_action_unavailable(fpsu_lang lang, const fpsu_patch_option *option)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "%s\n\n%s",
        option ? patch_kind_title(lang, option->kind, option->status) :
            tr(lang, "Patch FPS", "FPS patch"),
        tr(lang,
            "Nao existe patch conhecido nesta opcao. Use Analisar este jogo ou Escolher/forcar rota se quiser testar com risco.",
            "There is no known patch for this option. Use Analyze this game or Choose/force route if you want to test with risk."));
    app_notice(lang, i18n_text(lang, TXT_STATUS_NOT_FOUND), msg, UI_COLOR_ACCENT_2);
}

static void show_actions(fpsu_lang lang, fpsu_game_result *r)
{
    int selected = 0;
    int graphics_count = patch_db_collect_graphics_options(&r->game, NULL, 0);
    for (;;) {
        u32 btn;
        draw_actions(lang, r, selected, graphics_count);
        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % 8;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + 7) % 8;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            if (selected == 0) {
                if (normal_action_available(&r->fps60)) {
                    apply_option(lang, r, &r->fps60);
                } else {
                    show_normal_action_unavailable(lang, &r->fps60);
                }
            } else if (selected == 1) {
                if (normal_action_available(&r->unlock)) {
                    apply_option(lang, r, &r->unlock);
                } else {
                    show_normal_action_unavailable(lang, &r->unlock);
                }
            } else if (selected == 2) {
                if (normal_action_available(&r->fps30)) {
                    apply_option(lang, r, &r->fps30);
                } else {
                    show_normal_action_unavailable(lang, &r->fps30);
                }
            } else if (selected == 3) {
                show_graphics_patches(lang, r);
            } else if (selected == 4) {
                force_apply_option(lang, r);
            } else if (selected == 5) {
                analyze_selected_game(lang, r);
            } else if (selected == 6) {
                restore_game_backup(lang, &r->game);
            } else {
                return;
            }
        }
    }
}

static void show_about(fpsu_lang lang)
{
    const char *body = tr(lang,
        "Projeto de RafJaeger.\n\nPSUF V2.5.28 para CFW 4.90+ e PS3HEN.\nNem todos os consoles sao compativeis no momento, mas estamos procurando formas de contornar isso.\nUse Sistema > Verificar dependencias se o patch aplica mas nao muda nada.\nNao mexe em dev_flash, dev_blind, LV1, LV2 ou boot plugins.",
        "Project by RafJaeger.\n\nPSUF V2.5.28 for CFW 4.90+ and PS3HEN.\nNot every console is compatible right now, but we are looking for ways around that.\nUse System > Check dependencies if the patch installs but nothing changes.\nDoes not touch dev_flash, dev_blind, LV1, LV2 or boot plugins.");
    app_notice(lang, i18n_text(lang, TXT_MENU_ABOUT), body, UI_COLOR_ACCENT);
}

static void show_scan_results(fpsu_lang lang, fpsu_game_result *results, int count)
{
    int index = 0;
    int visible[FPSU_MAX_GAMES];
    int visible_count;
    char search[FPSU_SEARCH_MAX];

    if (count <= 0) {
        app_notice(lang, i18n_text(lang, TXT_NO_GAMES),
            tr(lang, "Nenhum PARAM.SFO de jogo PS3 foi localizado.", "No PS3 game PARAM.SFO was found."),
            UI_COLOR_RED);
        return;
    }

    search[0] = '\0';
    visible_count = build_search_index(results, count, search, visible, FPSU_MAX_GAMES);

    for (;;) {
        u32 btn;
        draw_results(lang, results, count, visible, visible_count, index, search);
        btn = frame_input();
        if (visible_count > 0 && (btn & FPSU_BUTTON_DOWN)) {
            index = (index + 1) % visible_count;
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_UP)) {
            index = (index + visible_count - 1) % visible_count;
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_RIGHT)) {
            index = (index + 7) % visible_count;
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_LEFT)) {
            index = (index + visible_count - 7) % visible_count;
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_CROSS)) {
            show_actions(lang, &results[visible[index]]);
            cache_write_results(results, count);
            visible_count = build_search_index(results, count, search, visible, FPSU_MAX_GAMES);
            if (index >= visible_count) {
                index = visible_count > 0 ? visible_count - 1 : 0;
            }
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_SQUARE)) {
            restore_game_backup(lang, &results[visible[index]].game);
        }
        if (btn & FPSU_BUTTON_TRIANGLE) {
            if (edit_search_query(lang, search, sizeof(search))) {
                visible_count = build_search_index(results, count, search, visible, FPSU_MAX_GAMES);
                index = 0;
            }
        }
        if (btn & FPSU_BUTTON_SELECT) {
            search[0] = '\0';
            visible_count = build_search_index(results, count, search, visible, FPSU_MAX_GAMES);
            index = 0;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
    }
}

static int show_cached_results(fpsu_lang lang)
{
    int game_count;

    game_count = cache_read_results(g_results, FPSU_MAX_GAMES);
    game_count = dedupe_results(g_results, game_count);
    if (game_count <= 0) {
        app_notice(lang,
            tr(lang, "Nenhum scan salvo", "No saved scan"),
            tr(lang, "Use Biblioteca > Scanear todos os jogos uma vez. Depois o scan montado so adiciona/atualiza o jogo atual.", "Use Library > Scan all games once. Mounted scan then only adds/updates the current game."),
            UI_COLOR_ACCENT_2);
        return 0;
    }

    cache_write_progress("cache_load", game_count, game_count,
        &g_results[game_count - 1].game, 0, 0, g_results, game_count);
    rebuild_cached_results(lang, game_count);
    cache_write_results(g_results, game_count);
    show_scan_results(lang, g_results, game_count);
    return 1;
}

static void scan_all_and_show(fpsu_lang lang, int deep_analysis)
{
    scan_context ctx;
    int game_count;

    ctx.lang = lang;
    ctx.found = 0;
    ctx.cancel = 0;
    draw_scan_screen(lang, "", 0);
    cache_write_progress("scan_start", 0, FPSU_MAX_GAMES, NULL, 0, 0, NULL, 0);
    game_count = scanner_scan_all(g_games, FPSU_MAX_GAMES, scan_progress_cb, &ctx);
    game_count = dedupe_games(g_games, game_count);
    cache_write_progress(ctx.cancel ? "scan_canceled" : "scan_done", game_count, game_count,
        game_count > 0 ? &g_games[game_count - 1] : NULL, 0, 0, NULL, 0);
    match_results(lang, game_count);
    cache_write_results(g_results, game_count);
    if (!ctx.cancel && deep_analysis) {
        analyze_all_results(lang, game_count);
    }
    cache_write_results(g_results, game_count);
    show_scan_results(lang, g_results, game_count);
}

static void trim_mounted_path(char *path)
{
    char *start;
    size_t len;

    if (!path) {
        return;
    }

    start = strstr(path, "/dev_");
    if (!start) {
        start = strstr(path, "/net");
    }
    if (start && start != path) {
        memmove(path, start, strlen(start) + 1);
    }

    while (*path == ' ' || *path == '\t') {
        memmove(path, path + 1, strlen(path));
    }

    len = strlen(path);
    while (len > 0 && (path[len - 1] == '\n' || path[len - 1] == '\r' ||
            path[len - 1] == ' ' || path[len - 1] == '\t')) {
        path[--len] = '\0';
    }
}

static int same_text_ci_local(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    while (*a || *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        if (*a) {
            ++a;
        }
        if (*b) {
            ++b;
        }
    }
    return 1;
}

static int mounted_iso_tail_is_supported(const char *tail)
{
    const char *p;

    if (!tail) {
        return 0;
    }
    if (tail[0] == '\0') {
        return 1;
    }
    if (tail[0] != '.') {
        return 0;
    }

    p = tail + 1;
    if (*p != '\0' && isdigit((unsigned char)*p)) {
        while (*p) {
            if (!isdigit((unsigned char)*p)) {
                return 0;
            }
            ++p;
        }
        return 1;
    }

    return same_text_ci_local(tail + 1, "ntfs") || same_text_ci_local(tail + 1, "enc");
}

static int path_looks_like_iso(const char *path)
{
    const char *p;
    if (!path) {
        return 0;
    }
    for (p = path; *p; ++p) {
        if (p[0] == '.' && p[1] && p[2] && p[3] &&
            tolower((unsigned char)p[1]) == 'i' &&
            tolower((unsigned char)p[2]) == 's' &&
            tolower((unsigned char)p[3]) == 'o') {
            if (mounted_iso_tail_is_supported(p + 4)) {
                return 1;
            }
        }
    }
    return 0;
}

static int read_webman_last_game_path(char *out, size_t out_size)
{
    static const char *paths[] = {
        "/dev_hdd0/tmp/wmtmp/last_game.txt",
        "/dev_hdd0/tmp/last_game.txt",
        "/dev_hdd0/tmp/wm_res/last_game.txt"
    };
    unsigned int i;

    if (!out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FILE *in = fopen(paths[i], "rb");
        if (!in) {
            continue;
        }
        if (fgets(out, (int)out_size, in)) {
            fclose(in);
            trim_mounted_path(out);
            if (out[0] != '\0' && strncmp(out, "/dev_bdvd", 9) != 0) {
                return 0;
            }
        } else {
            fclose(in);
        }
    }

    out[0] = '\0';
    return -1;
}

static void scan_mounted_and_show(fpsu_lang lang)
{
    fpsu_game_result mounted_result;
    char mounted_path[FPSU_MAX_PATH];
    int game_count;
    int existing_index = -1;
    int i;
    int scan_ret;

    draw_mounted_scan_screen(lang, "webMAN last_game.txt", 0);
    cache_write_progress("scan_mounted", 0, 1, NULL, 0, 0, NULL, 0);
    game_count = cache_read_results(g_results, FPSU_MAX_GAMES);
    game_count = dedupe_results(g_results, game_count);

    if (read_webman_last_game_path(mounted_path, sizeof(mounted_path)) != 0) {
        if (game_count > 0) {
            app_notice(lang,
                tr(lang, "Jogo montado nao encontrado", "Mounted game not found"),
                tr(lang,
                    "O webMAN nao informou o ultimo jogo montado. Nao alterei a biblioteca salva.",
                    "webMAN did not report the last mounted game. The saved library was not changed."),
                UI_COLOR_ACCENT_2);
            show_scan_results(lang, g_results, game_count);
            return;
        }
        app_notice(lang, i18n_text(lang, TXT_NO_GAMES),
            tr(lang,
                "O webMAN nao informou o ultimo jogo montado. Monte o jogo pelo webMAN e tente de novo.",
                "webMAN did not report the last mounted game. Mount the game through webMAN and try again."),
            UI_COLOR_RED);
        return;
    }

    draw_mounted_scan_screen(lang, mounted_path, 0);
    cache_write_progress("scan_mounted_path", 0, 1, NULL, 0, 0, NULL, 0);
    scan_ret = path_looks_like_iso(mounted_path) ?
        scanner_scan_mounted_iso_metadata(mounted_path, &g_games[0]) :
        scanner_scan_path(mounted_path, &g_games[0]);
    if (scan_ret != 0) {
        scan_ret = scanner_scan_mounted_iso_metadata(mounted_path, &g_games[0]);
    }
    if (scan_ret != 0) {
        if (game_count > 0) {
            app_notice(lang,
                tr(lang, "Jogo montado nao identificado", "Mounted game not identified"),
                tr(lang,
                    "Nao consegui ler os dados desse caminho do webMAN. A biblioteca salva foi mantida.",
                    "The webMAN path could not be identified. The saved library was kept."),
                UI_COLOR_ACCENT_2);
            show_scan_results(lang, g_results, game_count);
            return;
        }
        app_notice(lang, i18n_text(lang, TXT_NO_GAMES),
            tr(lang,
                "Nao consegui identificar o jogo montado pelo caminho salvo do webMAN.",
                "The mounted game could not be identified from the webMAN saved path."),
            UI_COLOR_RED);
        return;
    }

    merge_cached_game_info(&g_games[0], g_results, game_count);
    draw_mounted_scan_screen(lang, mounted_path, 1);
    cache_write_progress("scan_mounted_done", 1, 1, &g_games[0], 0, 0, NULL, 0);
    cache_write_progress("scan_mounted_db_start", 1, 1, &g_games[0], 0, 0, NULL, 0);
    patch_db_match_game(&g_games[0], &mounted_result);
    mark_unavailable_without_files(&mounted_result);
    apply_result_overrides(&mounted_result);
    cache_write_progress("scan_mounted_db_done", 1, 1, &g_games[0], 0, 0, &mounted_result, 1);

    cache_write_progress("scan_mounted_cache_start", 1, 1, &g_games[0], 0, 0, &mounted_result, 1);
    for (i = 0; i < game_count; ++i) {
        if (same_title_id(&g_results[i].game, &mounted_result.game) ||
            same_display_game_for_data_merge(&g_results[i].game, &mounted_result.game)) {
            existing_index = i;
            break;
        }
    }
    if (existing_index >= 0) {
        fpsu_game old_game = g_results[existing_index].game;
        int old_pattern = g_results[existing_index].pattern_candidate;
        copy_game_exec_from(&mounted_result.game, &old_game);
        if (old_pattern) {
            mounted_result.pattern_candidate = 1;
        }
        g_results[existing_index] = mounted_result;
    } else if (game_count < FPSU_MAX_GAMES) {
        g_results[game_count++] = mounted_result;
    } else {
        g_results[game_count - 1] = mounted_result;
    }
    game_count = dedupe_results(g_results, game_count);
    cache_write_results(g_results, game_count);
    cache_write_progress("scan_mounted_cache_done", game_count, game_count, &g_games[0], 0, 0,
        g_results, game_count);
    show_scan_results(lang, g_results, game_count);
}

static void backup_list_and_show(fpsu_lang lang)
{
    if (!show_cached_results(lang)) {
        scan_all_and_show(lang, 0);
    }
}

static void update_patch_database(fpsu_lang lang)
{
    int game_count;

    game_count = cache_read_results(g_results, FPSU_MAX_GAMES);
    game_count = dedupe_results(g_results, game_count);
    if (game_count <= 0) {
        app_notice(lang,
            tr(lang, "Biblioteca vazia", "Empty library"),
            tr(lang,
                "Envie os bancos pelo PC Updater e use Biblioteca > Scanear todos os jogos uma vez.",
                "Upload the databases with the PC Updater and use Library > Scan all games once."),
            UI_COLOR_ACCENT_2);
        return;
    }

    draw_busy_notice(lang,
        tr(lang, "Atualizando biblioteca", "Updating library"),
        tr(lang, "Recarregando bancos enviados pelo PC sem apagar o scan salvo.", "Reloading PC-uploaded databases without deleting the saved scan."));
    rebuild_cached_results(lang, game_count);
    cache_write_results(g_results, game_count);
    app_notice(lang,
        tr(lang, "Biblioteca atualizada", "Library updated"),
        tr(lang,
            "Os bancos enviados pelo PC foram aplicados ao scan salvo. Abra Jogos scaneados para conferir.",
            "The PC-uploaded databases were applied to the saved scan. Open Scanned games to review it."),
        UI_COLOR_GREEN);
}

int main(int argc, char **argv)
{
    fpsu_lang lang;
    int selected = 0;
    int running = 1;
    (void)argc;
    (void)argv;

    lang = i18n_detect_language();
    patch_db_cleanup_runtime_artifacts();
    if (ui_init() != 0) {
        return 1;
    }
    pad_input_init();
    music_init();

    while (running) {
        u32 btn;
        draw_home(lang, selected);
        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % FPSU_HOME_ITEMS;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + FPSU_HOME_ITEMS - 1) % FPSU_HOME_ITEMS;
        }
        if (btn & FPSU_BUTTON_TRIANGLE) {
            show_about(lang);
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            running = 0;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            if (selected == 0) {
                scan_all_and_show(lang, 1);
            } else if (selected == 1) {
                show_cached_results(lang);
            } else if (selected == 2) {
                scan_mounted_and_show(lang);
            } else if (selected == 3) {
                backup_list_and_show(lang);
            } else if (selected == 4) {
                activate_dependencies(lang);
            } else if (selected == 5) {
                show_webman_settings(lang);
            } else if (selected == 6) {
                show_donate(lang);
            } else if (selected == 7) {
                update_patch_database(lang);
            } else {
                running = 0;
            }
        }
    }

    music_shutdown();
    pad_input_shutdown();
    ui_shutdown();
    return 0;
}
