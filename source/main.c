#include "app.h"
#include "backup.h"
#include "cache.h"
#include "cover.h"
#include "home_view.h"
#include "i18n.h"
#include "music.h"
#include "pad_input.h"
#include "paypal_qr.h"
#include "patch_db.h"
#include "pix_qr.h"
#include "repo_update.h"
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
static fpsu_cover g_cover;
static fpsu_cover g_banner;
static int g_exit_after_launch;

#define FPSU_MAX_FORCE_CHOICES 12
#define FPSU_MAX_GRAPHICS_CHOICES 32
#define FPSU_SEARCH_MAX 64
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
static int restore_game_backup(fpsu_lang lang, const fpsu_game *game);
static void draw_selected_game_backdrop(void);

static const char *tr(fpsu_lang lang, const char *pt, const char *en)
{
    if (lang == FPSU_LANG_PT) {
        return pt;
    }
    if (lang == FPSU_LANG_ES) {
        /* English keys also cover dynamic Portuguese messages and accent changes. */
        static const struct {
            const char *en;
            const char *es;
        } text[] = {
            {"Unavailable", "No disponible"},
            {"Already runs at 60 FPS", "Ya funciona a 60 FPS"},
            {"60 FPS target", "Objetivo: 60 FPS"},
            {"Unlimited / +60 FPS", "FPS ilimitado / +60"},
            {"Unlimited / +60 FPS unsafe", "FPS ilimitado / +60 no seguro"},
            {"30 FPS test", "30 FPS de prueba"},
            {"FPS patch", "Parche de FPS"},
            {"Safe mode for PS3", "Modo seguro para PS3"},
            {"Close", "Cerrar"},
            {"Project by RafJaeger", "Proyecto de RafJaeger"},
            {"Circle also goes back", "Círculo también vuelve"},
            {"Confirm action", "Confirmar acción"},
            {"Apply", "Aplicar"},
            {"Cancel", "Cancelar"},
            {"X applies this action", "X aplica esta acción"},
            {"Circle cancels", "Círculo cancela"},
            {"Applying patch", "Aplicando parche"},
            {"Please wait, writing small files", "Espera, guardando archivos pequeños"},
            {"Do not power off during this step", "No apagues durante esta etapa"},
            {"Checking dependencies", "Verificando dependencias"},
            {"Folders OK. Waiting a moment before the next step.", "Carpetas listas. Esperando al siguiente paso."},
            {"Could not prepare every folder. Continuing the check.", "No se prepararon todas las carpetas. Continúa la comprobación."},
            {"webMAN answered. Waiting before starting PS3MAPI.", "webMAN respondió. Esperando para iniciar PS3MAPI."},
            {"webMAN did not answer. The missing item will be shown at the end.", "webMAN no respondió. Se indicará al terminar."},
            {"PS3MAPI answered. Finishing the check.", "PS3MAPI respondió. Terminando la comprobación."},
            {"PS3MAPI did not answer. The missing item will be shown at the end.", "PS3MAPI no respondió. Se indicará al terminar."},
            {"not checked because webMAN did not answer", "no comprobado: webMAN no respondió"},
            {"your PS3 does not have webMAN available in the system. install it and check again", "webMAN no está disponible. Instálalo y comprueba de nuevo."},
            {"your PS3 does not have PS3MAPI available in the system. install webMAN MOD with PS3MAPI and check again", "PS3MAPI no está disponible. Instala webMAN MOD con PS3MAPI y comprueba de nuevo."},
            {"PSUF folders", "Carpetas PSUF"},
            {"failed", "falló"},
            {"Everything PSUF uses responded now.", "Todos los servicios de PSUF respondieron."},
            {"If something is missing, install/enable it on the PS3 and come back here to check again.", "Si falta algo, instálalo/actívalo en el PS3 y comprueba de nuevo."},
            {"This screen does not patch a game. It only prepares and checks. The real test still happens when applying a patch.", "Esta pantalla solo prepara y comprueba. La prueba real se hace al aplicar un parche."},
            {"Dependencies ready", "Dependencias listas"},
            {"Missing dependency", "Falta una dependencia"},
            {"webMAN > Overclock", "webMAN > Overclock"},
            {"Search game", "Buscar juego"},
            {"Search", "Buscar"},
            {"(empty)", "(vacío)"},
            {"Type name, Title ID, or version.", "Escribe el nombre, Title ID o versión."},
            {"X key\nSquare delete\nStart confirm", "X tecla\nCuadrado borrar\nStart confirmar"},
            {"Type part of the name, Title ID, or version.", "Escribe parte del nombre, Title ID o versión."},
            {"Arrows choose   X key   Square delete", "Flechas elegir   X tecla   Cuadrado borrar"},
            {"Start/OK confirms   Circle cancels", "Start/OK confirma   Círculo cancela"},
            {"no filter", "sin filtro"},
            {"No game found in this search.", "No hay juegos para esta búsqueda."},
            {"Triangle changes search. Select clears it.", "Triángulo cambia la búsqueda. Select la borra."},
            {"Triangle search   Select clear", "Triángulo buscar   Select borrar"},
            {"Circle back", "Círculo volver"},
            {"Backups > Restore by game", "Respaldos > Restaurar por juego"},
            {"No automatic route", "Sin ruta automática"},
            {"Automatic pattern forcing currently works for 60 FPS and Unlimited / +60 FPS.", "El forzado por patrón funciona con 60 FPS y FPS ilimitado / +60."},
            {"Local EBOOT required", "Se necesita EBOOT local"},
            {"To generate a probable patch, PSUF must read the EBOOT from a folder/HDD install. For ISO/mounted games, use an existing database code.", "PSUF necesita un EBOOT local para generar un parche probable. En ISO/juegos montados, usa un código de la base."},
            {"Force canceled", "Forzado cancelado"},
            {"No files were changed for this game.", "No se cambió ningún archivo de este juego."},
            {"No applicable pattern", "Sin patrón aplicable"},
            {"PSUF tried to generate a probable patch but found no known value that can be turned into NCL.", "No se halló un valor conocido para generar un parche NCL probable."},
            {"No ready database route was found for this game.", "No hay una ruta lista en la base para este juego."},
            {"most likely", "más probable"},
            {"alternative", "alternativa"},
            {"UNSAFE. If it black-screens, restore the backup or remove the patch with PSUF.", "NO SEGURO. Si queda la pantalla negra, restaura el respaldo o quita el parche con PSUF."},
            {"Force this patch", "Forzar este parche"},
            {"Project by RafJaeger.\n\nPSUF V3 for CFW 4.90+ and PS3HEN.\nNot every console is compatible right now, but we are looking for ways around that.\nUse System > Check dependencies if the patch installs but nothing changes.\nDoes not touch dev_flash, dev_blind, LV1, LV2 or boot plugins.", "Proyecto de RafJaeger.\n\nPSUF V3 para CFW 4.90+ y PS3HEN.\nNo todas las consolas son compatibles. Seguimos buscando soluciones.\nUsa Verificar dependencias si el parche se instala pero nada cambia.\nNo toca dev_flash, dev_blind, LV1, LV2 ni boot plugins."},
            {"No PS3 game PARAM.SFO was found.", "No se encontró ningún PARAM.SFO de juego PS3."},
            {"No saved scan", "No hay escaneo guardado"},
            {"Use Library > Scan all games once. Mounted scan then only adds/updates the current game.", "Usa Biblioteca > Escanear todos los juegos. Después, el escaneo montado solo añade/actualiza el juego actual."},
            {"Mounted game not found", "Juego montado no encontrado"},
            {"Mounted game not identified", "Juego montado no identificado"},
            {"Empty library", "Biblioteca vacía"},
            {"Updating library", "Actualizando biblioteca"},
            {"Library updated", "Biblioteca actualizada"},
            {"Updating online", "Actualizando en línea"},
            {"Downloading the GitHub database and validating it first.", "Descargando y validando la base de GitHub."},
            {"Online update failed", "Falló la actualización en línea"},
            {"The online database was not downloaded or validated. The old database was kept.", "No se pudo descargar o validar la base. Se conservó la anterior."},
            {"Reloading the new databases without deleting the saved scan.", "Recargando bases nuevas sin borrar el escaneo guardado."},
            {"Online database updated", "Base en línea actualizada"},
            {"Online database updated. Open Scanned games to review it.", "Base en línea actualizada. Abre la biblioteca para revisarla."},
            {"Online database updated. Now scan games or open Scanned games.", "Base en línea actualizada. Escanea los juegos o abre la biblioteca."},
            {"Preparing NTFS", "Preparando NTFS"},
            {"Refreshing the external drive game list through webMAN.", "Actualizando la lista de juegos del disco externo mediante webMAN."},
            {"30 FPS", "30 FPS"},
            {"FPS Unlocker for PlayStation 3", "FPS Unlocker para PlayStation 3"},
            {"Arrows  Navigate     X  Select", "Flechas  Navegar     X  Elegir"},
            {"Triangle  About     Circle  Exit", "Triángulo  Acerca de     Círculo  Salir"},
            {"Step 1/3: preparing PSUF folders.", "Paso 1/3: preparando carpetas de PSUF."},
            {"Step 2/3: checking if webMAN answers.", "Paso 2/3: comprobando webMAN."},
            {"Step 3/3: enabling PS3MAPI through webMAN.", "Paso 3/3: activando PS3MAPI con webMAN."},
            {"Support the project", "Apoyar el proyecto"},
            {"Pix", "Pix"},
            {"Use whichever QR code works best for you. Thanks for testing, reporting games, and helping PSUF improve.", "Usa el QR que prefieras. Gracias por probar juegos y ayudar a mejorar PSUF."},
            {"X / Circle back", "X / Círculo volver"},
            {"PNGs: pix_qr.png / paypal_qr.png", "PNGs: pix_qr.png / paypal_qr.png"},
            {"Overclock", "Overclock"},
            {"Downclock", "Downclock"},
            {"PS3 default", "Predeterminado de PS3"},
            {"RSX clock through webMAN", "Frecuencia RSX con webMAN"},
            {"Overclock may improve FPS/performance. Watch the temperature. PSUF safe limit: 750/850.", "El overclock puede mejorar los FPS. Vigila la temperatura. Límite seguro de PSUF: 750/850."},
            {"Current webMAN", "Actual en webMAN"},
            {"Could not read current webMAN clock.", "No se pudo leer la frecuencia de webMAN."},
            {"Left/right: 50 MHz", "Izq./der.: 50 MHz"},
            {"PSUF max", "máx. PSUF"},
            {"Mode", "Modo"},
            {"Apply now through webMAN", "Aplicar con webMAN"},
            {"Return to default 500/650", "Restablecer 500/650"},
            {"Normal retail PS3 clock.", "Frecuencia de fábrica de PS3."},
            {"Back", "Volver"},
            {"UP/DOWN item   LEFT/RIGHT changes 50 MHz", "Arriba/abajo elegir   Izq./der. cambiar 50 MHz"},
            {"Triangle refresh   Circle back", "Triángulo actualizar   Círculo volver"},
            {"Invalid clock", "Frecuencia no válida"},
            {"PSUF only accepts values in 50 MHz steps.", "PSUF solo admite pasos de 50 MHz."},
            {"Outside PSUF limit", "Fuera del límite de PSUF"},
            {"PSUF safe limit is GPU 750 / VRAM 850. For more than that, use webMAN directly.", "Límite seguro de PSUF: GPU 750 / VRAM 850. Para valores mayores, usa webMAN."},
            {"Overclock may help FPS, but it adds heat. Monitor temperature; high heat may freeze, show artifacts, or reduce lifespan. PSUF is limited to the safe 750/850 value.", "El overclock aumenta el calor. Vigila la temperatura: puede causar bloqueos, fallos gráficos o acortar la vida útil. Límite seguro de PSUF: 750/850."},
            {"Apply clock through webMAN", "Aplicar frecuencia con webMAN"},
            {"Sending to webMAN", "Enviando a webMAN"},
            {"Applying GPU/VRAM through the local webMAN command.", "Aplicando GPU/VRAM con el comando local de webMAN."},
            {"Clock applied", "Frecuencia aplicada"},
            {"webMAN did not apply", "webMAN no aplicó el cambio"},
            {"Check that webMAN MOD is active and its HTTP server is enabled.", "Comprueba que webMAN MOD y su servidor HTTP estén activos."},
            {"Found", "Encontrados"},
            {"Please wait", "Espera"},
            {"Analyzing games, please wait", "Analizando juegos, espera"},
            {"Identifying mounted game", "Identificando juego montado"},
            {"Analyzing FPS patterns", "Analizando patrones de FPS"},
            {"Game", "Juego"},
            {"of", "de"},
            {"Circle cancels and opens the partial list", "Círculo cancela y abre la lista parcial"},
            {"Nothing is applied automatically", "No se aplica nada automáticamente"},
            {"Checking patch database", "Consultando base de parches"},
            {"Matching games against database", "Comparando juegos con la base"},
            {"PSUF does not modify files automatically", "PSUF no modifica archivos automáticamente"},
            {"Option", "Opción"},
            {"Status", "Estado"},
            {"Confirmed patch for this Title ID/version.", "Parche confirmado para este Title ID/versión."},
            {"Possible pattern. Test carefully and keep a backup.", "Patrón posible. Prueba con cuidado y guarda un respaldo."},
            {"Advanced PS3 test. Use Force probable patch.", "Prueba avanzada en PS3. Usa Forzar parche probable."},
            {"Database has another version. Use Choose/force route if you want to test.", "Hay un parche de otra versión. Para probarlo, usa Elegir/forzar ruta."},
            {"Analysis found compatibility. Experimental apply.", "Análisis compatible. Aplicación experimental."},
            {"Unavailable: no file was found to change.", "No disponible: no hay archivos que modificar."},
            {"60 FPS is already this game's normal target. Do not apply 60 FPS target.", "Este juego ya tiene un objetivo de 60 FPS. No apliques 60 FPS."},
            {"A patch exists for a newer update. Update and scan again.", "Hay un parche para una versión más reciente. Actualiza y escanea de nuevo."},
            {"No patch or pattern has been found for this EBOOT yet.", "Aún no hay parche ni patrón para este EBOOT."},
            {"Restore backup by game", "Restaurar respaldo por juego"},
            {"Title ID", "Title ID"},
            {"Type the game's Title ID.", "Escribe el Title ID del juego."},
            {"Example: BLUS30591", "Ejemplo: BLUS30591"},
            {"Library covers", "Carátulas de la biblioteca"},
            {"Preparing your covers", "Preparando carátulas"},
            {"Reusing local covers and downloading missing ones.", "Reutilizando carátulas locales y descargando las que faltan."},
            {"Circle  Cancel", "Círculo  Cancelar"},
            {"Download interrupted", "Descarga interrumpida"},
            {"Cover download complete", "Descarga de carátulas completa"},
            {"Covers saved/reused", "Carátulas guardadas/reutilizadas"},
            {"Not found", "No encontradas"},
            {"\n\nCircle stopped the rest.", "\n\nCírculo detuvo el resto."},
            {"COVER", "CARÁTULA"},
            {"Game library", "Biblioteca de juegos"},
            {"Graphics patches are available in this game's menu.", "Hay parches gráficos en el menú del juego."},
            {"Arrows  Games    X  Options    Square  Restore    Triangle  Search", "Flechas  Juegos    X  Opciones    Cuadrado  Restaurar    Triángulo  Buscar"},
            {"Select  Covers", "Select  Carátulas"},
            {"Choose a game. X directly restores the selected backup.", "Elige un juego. X restaura su respaldo directamente."},
            {"PSUF restores only this game and removes its generated files.", "PSUF restaura solo este juego y elimina los archivos que generó."},
            {"If no backup exists, it only removes the PSUF patch.", "Si no hay respaldo, solo elimina el parche de PSUF."},
            {"UP/DOWN choose   X restore", "Arriba/abajo elegir   X restaurar"},
            {"No game in library", "Biblioteca sin juegos"},
            {"Scan the games once before restoring a backup by game.", "Escanea los juegos antes de restaurar un respaldo por juego."},
            {"Backups for this game will be restored and FPSU scripts/NCL files will be removed.", "Se restaurarán los respaldos de este juego y se quitarán los scripts/NCL de FPSU."},
            {"Mounted ISO/disc game: only FPSU scripts/NCL files will be removed, without restoring game files.", "ISO/disco montado: solo se quitarán los scripts/NCL de FPSU. No se restaurarán archivos del juego."},
            {"No old backup was found, but FPSU scripts/NCL files for this game can be removed.", "No hay respaldo anterior. Se pueden quitar los scripts/NCL de FPSU de este juego."},
            {"Restoring/removing patch", "Restaurando/eliminando parche"},
            {"Please wait. Cleaning only this game.", "Espera. Limpiando solo este juego."},
            {"Backups restored", "Respaldos restaurados"},
            {"FPSU files removed", "Archivos FPSU eliminados"},
            {"FPSU patch removed", "Parche FPSU eliminado"},
            {"No backup or generated FPSU patch exists for this game.", "No hay respaldo ni parche de FPSU para este juego."},
            {"Unavailable: no game file was found to test or change.", "No disponible: no hay archivos del juego para probar o modificar."},
            {"No need to apply 60 FPS to this game. It already targets 60.", "No hace falta aplicar 60 FPS: ya es el objetivo de este juego."},
            {"The version was not confirmed, but an installable route exists for this Title ID.", "Versión sin confirmar. Hay una ruta instalable para este Title ID."},
            {"This can be applied anyway. UNSAFE: use this when the game is on ISO/external HDD and the version appears wrong or unknown.", "Se permite aplicarlo. NO SEGURO: úsalo en ISO/disco externo si la versión es incorrecta o desconocida."},
            {"Apply without confirmed version", "Aplicar sin confirmar versión"},
            {"Update the game to the indicated version and scan again.", "Actualiza el juego a la versión indicada y escanea de nuevo."},
            {"No code to apply", "Sin código para aplicar"},
            {"There is no installable code for this option. Choose 60 FPS target or Unlimited / +60 FPS when one of them is available.", "No hay código instalable. Elige Objetivo: 60 FPS o FPS ilimitado / +60 si está disponible."},
            {"Ready code required", "Se necesita código listo"},
            {"This option has no ready NCL code in the database. Use the PC app to test/create code or wait for a database update.", "No hay código NCL listo. Usa la app de PC para crear/probar uno o espera una actualización de la base."},
            {"Patch found for another version of the same game.", "Hay un parche para otra versión de este juego."},
            {"This can be applied without EBOOT analysis. UNSAFE: use it for ISO/external HDD when the version appears wrong or unknown.", "Se permite aplicar sin analizar EBOOT. NO SEGURO: úsalo en ISO/disco externo si la versión es incorrecta o desconocida."},
            {"Apply anyway", "Aplicar de todos modos"},
            {"There is information for another version, but no installable code for PSUF to apply.", "Hay datos de otra versión, pero PSUF no tiene código instalable."},
            {"Experimental mode. Keep a backup and test carefully.", "Modo experimental. Guarda un respaldo y prueba con cuidado."},
            {"Applying patch, please wait", "Aplicando parche, espera"},
            {"Creating a small backup and writing NCL.", "Creando un respaldo pequeño y guardando NCL."},
            {"Test build: open the game and wait 2 minutes.", "Versión de prueba: abre el juego y espera 2 minutos."},
            {"If an in-game message appears and FPS does not change, check the process and memory.", "Si aparece un aviso pero los FPS no cambian, revisa el proceso y la memoria."},
            {"If no message appears, send these logs:", "Si no aparece un aviso, envía estos registros:"},
            {"The patch was sent. webMAN will apply it through PS3MAPI when the game starts.", "Parche enviado. webMAN lo aplicará con PS3MAPI al iniciar el juego."},
            {"NCL saved", "NCL guardado"},
            {"The NCL was saved. The automatic webMAN script could not be saved.", "Se guardó el NCL. No se pudo guardar el script automático de webMAN."},
            {"Unsafe patch address", "Dirección de parche no segura"},
            {"webMAN would change an address in this patch. The PSUF blocked the write.", "webMAN cambiaría una dirección de este parche. PSUF bloqueó la escritura."},
            {"If FPS does not change, check memory/address and send this log:", "Si los FPS no cambian, revisa la memoria/dirección y envía este registro:"},
            {"Method not installable", "Método no instalable"},
            {"The PS3 could not turn this option into an installable patch.", "El PS3 no pudo generar un parche instalable para esta opción."},
            {"Choose patch", "Elegir parche"},
            {"Source", "Fuente"},
            {"UP/DOWN choose   X apply with backup", "Arriba/abajo elegir   X aplicar con respaldo"},
            {"Patch selected by the user.", "Parche elegido por el usuario."},
            {"No extra/graphics patch", "Sin parche extra/gráfico"},
            {"No extra/graphics patch was found in the separate database for this Title ID.", "La base separada no tiene parches extra/gráficos para este Title ID."},
            {"Game > Extra/graphics", "Juego > Extras/gráficos"},
            {"Extra/graphics patch", "Parche extra/gráfico"},
            {"UP/DOWN choose   X apply", "Arriba/abajo elegir   X aplicar"},
            {"Imported extra/graphics patch. Not confirmed by PSUF yet.", "Parche extra/gráfico importado. Aún sin confirmar por PSUF."},
            {"This is not an FPS patch. It may change visuals, effects, visual performance, or stability. Test carefully.", "No es un parche de FPS. Puede cambiar gráficos, efectos, rendimiento o estabilidad. Prueba con cuidado."},
            {"Apply extra/graphics patch", "Aplicar parche extra/gráfico"},
            {"Patch", "Parche"},
            {"Game path unavailable", "Ruta del juego no disponible"},
            {"PSUF identified the game, but the path required by webMAN is unavailable.", "Juego identificado, pero falta la ruta necesaria para webMAN."},
            {"PSUF will close and start the selected game.", "PSUF se cerrará e iniciará el juego seleccionado."},
            {"Start game", "Iniciar juego"},
            {"Starting game", "Iniciando juego"},
            {"Preparing the game shortcut.", "Preparando el acceso directo al juego."},
            {"Could not start game", "No se pudo iniciar"},
            {"webMAN did not answer the start shortcut.", "webMAN no respondió al acceso de inicio."},
            {"Graphics patches", "Parches gráficos"},
            {"Force route (unsafe)", "Forzar ruta (no seguro)"},
            {"Restore backup", "Restaurar respaldo"},
            {"Back to library", "Volver a la biblioteca"},
            {"Available visual effects for this game.", "Efectos visuales disponibles para este juego."},
            {"No graphics patch available.", "Sin parches gráficos disponibles."},
            {"Choose a manual database route.", "Elige una ruta manual de la base."},
            {"Restore this game only.", "Restaura solo este juego."},
            {"Starts this game after confirmation.", "Inicia este juego tras confirmar."},
            {"Game options", "Opciones del juego"},
            {"Arrows  Navigate    X  Select", "Flechas  Navegar    X  Elegir"},
            {"Circle  Back", "Círculo  Volver"},
            {"There is no known patch for this option. Use Choose/force route if you want to test with risk.", "No hay parche conocido. Usa Elegir/forzar ruta para probar bajo tu responsabilidad."},
            {"webMAN did not report the last mounted game. The saved library was not changed.", "webMAN no indicó el último juego montado. La biblioteca guardada no cambió."},
            {"webMAN did not report the last mounted game. Mount the game through webMAN and try again.", "webMAN no indicó el último juego montado. Monta uno con webMAN e inténtalo de nuevo."},
            {"The webMAN path could not be identified. The saved library was kept.", "No se identificó la ruta de webMAN. Se conservó la biblioteca guardada."},
            {"The mounted game could not be identified from the webMAN saved path.", "No se identificó el juego montado en la ruta guardada por webMAN."},
            {"Upload the databases with the PC Updater and use Library > Scan all games once.", "Envía las bases con PC Updater y usa Biblioteca > Escanear todos los juegos."},
            {"Reloading PC-uploaded databases without deleting the saved scan.", "Recargando bases enviadas por PC sin borrar el escaneo guardado."},
            {"The PC-uploaded databases were applied to the saved scan. Open Scanned games to review it.", "Bases del PC aplicadas al escaneo guardado. Abre la biblioteca para revisarlo."},
            {"Could not save the theme", "No se pudo guardar el tema"},
            {"Could not write your preference on PS3. Try again.", "No se pudo guardar tu preferencia en PS3. Inténtalo de nuevo."},
            {"webMAN could not confirm the selected game. Check the game path and scan again.", "webMAN no confirmó el juego. Revisa la ruta y escanea de nuevo."},
            {"Delete", "Borrar"},
            {"Clear", "Limpiar"},
            {"Space", "Espacio"},
            {"Canceling...", "Cancelando..."},
            {"The installed games section could not be verified.", "No se pudo verificar la sección de juegos instalados."},
            {"SUPPORT", "APOYAR"},
            {"CLOCK", "FRECUENCIA"},
            {"GPU core: %d MHz", "Núcleo GPU: %d MHz"},
            {"SCAN", "ESCANEAR"},
            {"ANALYSIS", "ANÁLISIS"},
            {"DATABASE", "BASE"},
            {"BACKUP", "RESPALDO"},
            {"SEARCH", "BUSCAR"},
            {"ONLINE", "EN LÍNEA"},
            {"No 60 FPS patch offered: the game already targets 60.", "No ofrece parche de 60 FPS: el juego ya tiene ese objetivo."},
            {"Possible patch, unconfirmed", "Parche posible, sin confirmar"},
            {"Earlier test did not change FPS. Still UNSAFE.", "La prueba anterior no cambió los FPS. Sigue siendo NO SEGURO."},
            {"Available", "Disponible"},
            {"Option saved during the last use.", "Opción guardada en el último uso."},
            {"Applicable local pattern", "Patrón local aplicable"},
            {"Last scan found a local pattern for another version.", "El último escaneo halló un patrón local para otra versión."},
            {"Saved 60 FPS pattern", "Patrón de 60 FPS guardado"},
            {"Saved 30 FPS pattern", "Patrón de 30 FPS guardado"},
            {"Saved FPS pattern", "Patrón de FPS guardado"},
            {"Last scan found an FPS hint, with no payload yet.", "El último escaneo halló una señal de FPS, aún sin payload."},
            {"No EBOOT to analyze", "Sin EBOOT para analizar"},
            {"No game file found to test this version.", "No se halló un archivo del juego para probar esta versión."},
            {"Applied without confirming the version. UNSAFE.", "Aplicado sin confirmar la versión. NO SEGURO."},
            {"Another version patch applied without EBOOT analysis. UNSAFE.", "Parche de otra versión aplicado sin analizar EBOOT. NO SEGURO."},
            {"Patch sent for HEN-safe testing. Temporary files are cleaned later.", "Parche enviado a prueba HEN-safe. Los temporales se borran después."},
            {"PSUF forced pattern", "Patrón forzado por PSUF"},
            {"Forced pattern: 60 FPS", "Patrón forzado: 60 FPS"},
            {"Forced pattern: unlimited / +60 FPS", "Patrón forzado: FPS ilimitado / +60"},
            {"Probable payload generated from the local EBOOT on PS3. UNSAFE.", "Payload probable generado con el EBOOT local en PS3. NO SEGURO."},
            {"Another version patch forced by the user. UNSAFE.", "Parche de otra versión forzado por el usuario. NO SEGURO."},
        };
        size_t i;
        for (i = 0; i < sizeof(text) / sizeof(text[0]); ++i) {
            if (strcmp(en, text[i].en) == 0) return text[i].es;
        }
    }
    return en;
}

static const char *patch_text(fpsu_lang lang, const char *text)
{
    static const struct { const char *saved; const char *en; } messages[] = {
        {"Jogo ja roda a 60 FPS", "Already runs at 60 FPS"},
        {"Nao mostra aplicar 60 FPS porque o jogo ja tem alvo nativo de 60.", "No 60 FPS patch offered: the game already targets 60."},
        {"Patch possivel, nao confirmado", "Possible patch, unconfirmed"},
        {"Teste anterior nao mudou o FPS nesta versao. Mantido como NAO SEGURO.", "Earlier test did not change FPS. Still UNSAFE."},
        {"Disponivel", "Available"},
        {"Opcao salva pelo usuario no ultimo uso.", "Option saved during the last use."},
        {"Aplicavel por padrao local", "Applicable local pattern"},
        {"Ultimo scan achou padrao local para patch de outra versao.", "Last scan found a local pattern for another version."},
        {"Padrao 60 FPS salvo", "Saved 60 FPS pattern"},
        {"Padrao 30 FPS salvo", "Saved 30 FPS pattern"},
        {"Padrao FPS salvo", "Saved FPS pattern"},
        {"Ultimo scan encontrou sinal de FPS, mas ainda nao ha payload concreto.", "Last scan found an FPS hint, with no payload yet."},
        {"Sem EBOOT para analisar", "No EBOOT to analyze"},
        {"O PS3 nao achou arquivo de jogo para testar esta versao.", "No game file found to test this version."},
        {"Aplicado sem confirmar versao do jogo. NAO SEGURO.", "Applied without confirming the version. UNSAFE."},
        {"Patch de outra versao aplicado sem analisar EBOOT. NAO SEGURO.", "Another version patch applied without EBOOT analysis. UNSAFE."},
        {"Patch enviado para teste HEN-safe. Temporarios sao limpos depois.", "Patch sent for HEN-safe testing. Temporary files are cleaned later."},
        {"PSUF force pattern", "PSUF forced pattern"},
        {"Forcado por padrao: 60 FPS", "Forced pattern: 60 FPS"},
        {"Forcado por padrao: FPS ilimitado / +60", "Forced pattern: unlimited / +60 FPS"},
        {"Payload provavel gerado no PS3 a partir do EBOOT local. NAO SEGURO.", "Probable payload generated from the local EBOOT on PS3. UNSAFE."},
        {"Patch de outra versao/update forcado pelo usuario. NAO SEGURO.", "Another version patch forced by the user. UNSAFE."}
    };
    size_t i;
    if (!text) return "";
    for (i = 0; i < sizeof(messages) / sizeof(messages[0]); ++i)
        if (strcmp(text, messages[i].saved) == 0) return tr(lang, text, messages[i].en);
    return text;
}

static u32 poll_input(int pause)
{
    music_update();
    ui_pump();
    if (pause) usleep(8000);
    if (ui_exit_requested()) {
        return FPSU_BUTTON_CIRCLE;
    }
    if (ui_system_menu_open()) {
        if (!pause) usleep(8000);
        return 0;
    }
    if (pause) ui_refresh_frame();
    return pad_input_read_active();
}

static u32 frame_input(void)
{
    return poll_input(1);
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


static void draw_multiline_text(int x, int y, const char *text, u32 color, int scale, int max_chars, int max_lines)
{
    ui_draw_text_box(x, y, text, color, scale, max_chars * (scale == 1 ? 7 : 12),
        scale == 1 ? 22 : 34, max_lines);
}

static void draw_notice(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Modo seguro para PS3", "Safe mode for PS3"), FPSU_VERSION_LABEL);
    ui_draw_panel(96, 152, ui_screen_width() - 192, 318, 20, UI_COLOR_PANEL, accent);
    ui_fill_rect(108, 176, 5, 270, accent);
    ui_draw_text(130, 190, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 250, body, UI_COLOR_MUTED, 2, 54, 4);
    draw_button_hint(130, 402, "X", tr(lang, "Fechar", "Close"), UI_COLOR_ACCENT);
    ui_draw_footer(tr(lang, "Projeto de RafJaeger", "Project by RafJaeger"), tr(lang, "Circle tambem volta", "Circle also goes back"));
}

static void draw_details_notice(fpsu_lang lang, const char *title, const char *body, u32 accent)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Modo seguro para PS3", "Safe mode for PS3"), FPSU_VERSION_LABEL);
    ui_draw_panel(96, 128, ui_screen_width() - 192, 366, 20, UI_COLOR_PANEL, accent);
    ui_fill_rect(108, 152, 5, 318, accent);
    ui_draw_text(130, 162, title, UI_COLOR_TEXT, 3);
    draw_multiline_text(132, 222, body, UI_COLOR_MUTED, 1, 78, 8);
    draw_button_hint(130, 438, "X", tr(lang, "Fechar", "Close"), UI_COLOR_ACCENT);
    ui_draw_footer(tr(lang, "Projeto de RafJaeger", "Project by RafJaeger"), tr(lang, "Circle tambem volta", "Circle also goes back"));
}

static void draw_confirm_notice(fpsu_lang lang, const char *title, const char *body)
{
    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Confirmar acao", "Confirm action"), FPSU_VERSION_LABEL);
    ui_draw_panel(96, 152, ui_screen_width() - 192, 340, 20, UI_COLOR_PANEL, UI_COLOR_ORANGE);
    ui_fill_rect(108, 176, 5, 292, UI_COLOR_ORANGE);
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
    ui_draw_panel(96, 152, ui_screen_width() - 192, 318, 20, UI_COLOR_PANEL, UI_COLOR_ACCENT_2);
    ui_fill_rect(108, 176, 5, 270, UI_COLOR_ACCENT_2);
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

static void draw_home(fpsu_lang lang, int selected, int full_frame)
{
    if (full_frame || !ui_resume_frame()) {
        full_frame = 1;
        ui_begin_frame();
        ui_draw_shell("PSUF", tr(lang, "FPS Unlocker para PlayStation 3", "FPS Unlocker for PlayStation 3"),
            "V3");
        {
            ui_footer_action actions[] = {
                {UI_ICON_ARROWS, lang == FPSU_LANG_PT ? "Navegar" : lang == FPSU_LANG_ES ? "Navegar" : "Navigate"},
                {UI_ICON_CROSS, lang == FPSU_LANG_PT ? "Selecionar" : lang == FPSU_LANG_ES ? "Elegir" : "Select"},
                {UI_ICON_TRIANGLE, lang == FPSU_LANG_PT ? "Sobre" : lang == FPSU_LANG_ES ? "Acerca de" : "About"},
                {UI_ICON_CIRCLE, lang == FPSU_LANG_PT ? "Sair" : lang == FPSU_LANG_ES ? "Salir" : "Exit"}
            };
            ui_draw_footer_actions(48, 654, 1184, actions, 4);
        }
    }
    home_view_draw(lang, selected, full_frame);
    ui_present();
}

static void activate_dependencies(fpsu_lang lang)
{
    char webman_msg[160];
    char ps3mapi_msg[192];
    char body[1280];
    int dirs_ok;
    int webman_ok;
    int ps3mapi_ok;

    draw_busy_notice(lang,
        tr(lang, "Verificando dependencias", "Checking dependencies"),
        tr(lang, "Etapa 1/3: preparando as pastas do PSUF.",
            "Step 1/3: preparing PSUF folders."));

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
        tr(lang, "Etapa 2/3: verificando se o webMAN responde.",
            "Step 2/3: checking if webMAN answers."));
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

    if (webman_ok) {
        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            tr(lang, "Etapa 3/3: ativando PS3MAPI pelo webMAN.",
                "Step 3/3: enabling PS3MAPI through webMAN."));
        ps3mapi_ok = webman_start_ps3mapi(ps3mapi_msg, sizeof(ps3mapi_msg)) == 0;
        draw_busy_notice(lang,
            tr(lang, "Verificando dependencias", "Checking dependencies"),
            ps3mapi_ok ?
                tr(lang, "PS3MAPI respondeu. Finalizando verificacao.",
                    "PS3MAPI answered. Finishing the check.") :
                tr(lang, "PS3MAPI nao respondeu. Vou mostrar o que faltou no final.",
                    "PS3MAPI did not answer. The missing item will be shown at the end."));
        dependency_pause(lang,
            ps3mapi_ok ?
                tr(lang, "PS3MAPI respondeu. Finalizando verificacao.",
                    "PS3MAPI answered. Finishing the check.") :
                tr(lang, "PS3MAPI nao respondeu. Vou mostrar o que faltou no final.",
                    "PS3MAPI did not answer. The missing item will be shown at the end."),
            3);

    } else {
        snprintf(ps3mapi_msg, sizeof(ps3mapi_msg), "%s",
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
    snprintf(body, sizeof(body),
        "%s: %s\nwebMAN: %s\nPS3MAPI: %s\n\n%s\n%s",
        tr(lang, "Pastas PSUF", "PSUF folders"),
        dirs_ok ? "OK" : tr(lang, "falhou", "failed"),
        webman_ok ? "OK" : webman_msg,
        ps3mapi_ok ? "OK" : ps3mapi_msg,
        (dirs_ok && webman_ok && ps3mapi_ok) ?
            tr(lang, "Tudo que o PSUF usa respondeu agora.", "Everything PSUF uses responded now.") :
            tr(lang, "Se faltar algo, instale/ative no PS3 e volte aqui para verificar novamente.",
                "If something is missing, install/enable it on the PS3 and come back here to check again."),
        tr(lang, "Essa tela nao aplica patch em jogo. Ela so prepara e verifica. O teste real continua ao aplicar um patch.",
            "This screen does not patch a game. It only prepares and checks. The real test still happens when applying a patch."));

    app_notice_details(lang,
        (dirs_ok && webman_ok && ps3mapi_ok) ?
            tr(lang, "Dependencias prontas", "Dependencies ready") :
            tr(lang, "Dependencia faltando", "Missing dependency"),
        body,
        (dirs_ok && webman_ok && ps3mapi_ok) ? UI_COLOR_GREEN : UI_COLOR_ACCENT_2);
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
            tr(lang, "Apoiar o projeto", "Support the project"), tr(lang, "APOIAR", "SUPPORT"));
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
        tr(lang, "webMAN > Overclock", "webMAN > Overclock"), tr(lang, "FREQUÊNCIA", "CLOCK"));

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

    snprintf(title, sizeof(title), tr(lang, "Núcleo GPU: %d MHz", "GPU core: %d MHz"), gpu_mhz);
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
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), i18n_text(lang, TXT_SCAN_START), tr(lang, "ESCANEAR", "SCAN"));
    ui_draw_panel(96, 150, ui_screen_width() - 192, 282, 22, UI_COLOR_PANEL, UI_COLOR_ACCENT);
    ui_fill_rect(108, 176, 5, 230, UI_COLOR_ACCENT);
    ui_draw_text(132, 190, title, UI_COLOR_TEXT, 3);
    ui_draw_text(132, 270, short_path, UI_COLOR_MUTED, 1);
    ui_draw_text(132, 322, count, UI_COLOR_ACCENT_2, 2);
    draw_progress_bar(132, 360, ui_screen_width() - 264, 20, (unsigned int)found, (unsigned int)total, UI_COLOR_ACCENT_2);
    ui_draw_footer(tr(lang, "Aguarde", "Please wait"), tr(lang, "Circle cancela", "Circle cancels"));
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
        tr(lang, "Analisando padroes FPS", "Analyzing FPS patterns"), tr(lang, "ANÁLISE", "ANALYSIS"));
    ui_draw_panel(96, 132, ui_screen_width() - 192, 468, 22, UI_COLOR_PANEL, UI_COLOR_ACCENT_2);
    ui_fill_rect(108, 158, 5, 416, UI_COLOR_ACCENT_2);

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
        tr(lang, "Consultando banco de patches", "Checking patch database"), tr(lang, "BASE", "DATABASE"));
    ui_draw_panel(96, 168, ui_screen_width() - 192, 238, 22, UI_COLOR_PANEL, UI_COLOR_ACCENT);
    ui_fill_rect(108, 194, 5, 186, UI_COLOR_ACCENT);
    ui_draw_text(132, 212, tr(lang, "Comparando jogos com a base", "Matching games against database"), UI_COLOR_TEXT, 3);
    snprintf(line, sizeof(line), "%d / %d", index, count);
    ui_draw_text(132, 290, line, UI_COLOR_ACCENT_2, 2);
    draw_progress_bar(132, 330, ui_screen_width() - 264, 20, (unsigned int)index, (unsigned int)count, UI_COLOR_ACCENT_2);
    ui_draw_footer(tr(lang, "Aguarde", "Please wait"),
        tr(lang, "O PSUF não modifica arquivos automaticamente", "PSUF does not modify files automatically"));
    ui_present();
}

typedef struct {
    fpsu_lang lang;
    int found;
    int cancel;
    int last_drawn;
} scan_context;

static int scan_progress_cb(const char *path, int found, void *user)
{
    scan_context *ctx = (scan_context *)user;
    if (!ctx) {
        return 0;
    }
    ctx->found = found;
    if (ctx->last_drawn < 0 || found == 0 || found - ctx->last_drawn >= 2) {
        draw_scan_screen(ctx->lang, path, found);
        ctx->last_drawn = found;
    }
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

static void restore_option_hint(fpsu_patch_option *opt, fpsu_patch_kind kind,
    fpsu_patch_status cached_status, fpsu_patch_method cached_method)
{
    if (!opt) {
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

        if ((i % 12) == 0) {
            draw_database_screen(lang, i, game_count);
            music_update();
            ui_pump();
        }
        g_games[i] = game;
        if (strncmp(game.base_path, "/dev_bdvd", sizeof("/dev_bdvd") - 1) != 0 &&
            strncmp(game.base_path, "/dev_ntfs", sizeof("/dev_ntfs") - 1) != 0 &&
            strncmp(game.base_path, "/net", sizeof("/net") - 1) != 0 &&
            scanner_scan_path(game.base_path, &g_games[i]) == 0 &&
            strcmp(game.title_id, g_games[i].title_id) == 0 &&
            strcmp(g_games[i].version, "*") != 0 &&
            strcmp(g_games[i].version, "00.00") != 0) {
            if (strcmp(game.version, g_games[i].version) != 0) {
                fps60_status = unlock_status = fps30_status = FPSU_STATUS_NOT_FOUND;
                fps60_method = unlock_method = fps30_method = FPSU_METHOD_NONE;
                pattern_candidate = 0;
            }
        } else {
            g_games[i] = game;
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

static void draw_status_line(fpsu_lang lang, int x, int y, int width, const char *label,
    fpsu_patch_status status)
{
    char line[192];
    snprintf(line, sizeof(line), "%s > %s", tr(lang, "Opcao", "Option"), label);
    ui_draw_text(x, y, line, UI_COLOR_TEXT, 1);
    ui_fill_rect(x, y + 24, width, 30, 0xff203244u);
    ui_fill_rect(x, y + 24, 7, 30, status_color(status));
    snprintf(line, sizeof(line), "%s > %s", tr(lang, "Status", "Status"), status_text(lang, status));
    ui_draw_text(x + 18, y + 33, line, status_color(status), 1);
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

static void draw_search_editor(fpsu_lang lang, const char *query, int cursor_row, int cursor_col,
    int title_id_only)
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
        title_id_only ? tr(lang, "Restaurar backup por jogo", "Restore backup by game") :
            tr(lang, "Buscar jogo", "Search game"),
        title_id_only ? tr(lang, "BACKUP", "BACKUP") : tr(lang, "BUSCA", "SEARCH"));
    ui_draw_panel(panel_x, panel_y, panel_w, 420, 18, UI_COLOR_PANEL, UI_COLOR_PANEL_EDGE);
    ui_draw_text(92, 131, title_id_only ? tr(lang, "Title ID", "Title ID") :
        tr(lang, "Busca", "Search"), UI_COLOR_MUTED, 1);
    ui_draw_panel(80, 150, 830, 58, 10, UI_COLOR_BG_TOP, UI_COLOR_ACCENT);
    ui_draw_text(100, 170, query && query[0] ? query : tr(lang, "(vazio)", "(empty)"),
        UI_COLOR_TEXT, 2);
    ui_draw_panel(928, 150, 270, 298, 12, UI_COLOR_PANEL_SOFT, UI_COLOR_PANEL_EDGE);
    ui_draw_text(952, 172, tr(lang, "Pesquisar", "Search"), UI_COLOR_TEXT, 2);
    draw_multiline_text(952, 226,
        title_id_only ? tr(lang, "Digite o Title ID do jogo.", "Type the game's Title ID.") :
            tr(lang, "Digite nome, Title ID ou versao.", "Type name, Title ID, or version."),
        UI_COLOR_MUTED, 1, 28, 4);
    draw_multiline_text(952, 334,
        tr(lang, "X tecla\nSquare apagar\nStart confirmar", "X key\nSquare delete\nStart confirm"),
        UI_COLOR_MUTED, 1, 28, 4);

    for (row = 0; row < search_key_row_count(); ++row) {
        int row_len = 0;
        const search_key *keys = search_key_row(row, &row_len);
        int row_left = keyboard_left;
        int y = top + row * (key_h + row_gap);
        int col;
        int x = row_left;

        for (col = 0; col < row_len; ++col) {
            const char *label = keys[col].label;
            int key_pixels = keys[col].units * key_w + (keys[col].units - 1) * gap;
            int selected = row == cursor_row && col == cursor_col;
            int scale, text_w;
            if (keys[col].action == SEARCH_KEY_BACKSPACE) label = tr(lang, "Apagar", "Delete");
            if (keys[col].action == SEARCH_KEY_CLEAR) label = tr(lang, "Limpar", "Clear");
            if (keys[col].value == ' ') label = tr(lang, "Espaço", "Space");
            scale = strlen(label) > 3 ? 1 : 2;
            text_w = ui_text_width(label, scale);

            ui_draw_panel(x, y, key_pixels, key_h, 6,
                selected ? UI_COLOR_PANEL_INNER : UI_COLOR_PANEL_SOFT,
                selected ? UI_COLOR_ACCENT : UI_COLOR_PANEL_EDGE);
            ui_draw_text(x + (key_pixels - text_w) / 2, y + (scale == 1 ? 20 : 14),
                label, selected ? UI_COLOR_ACCENT : UI_COLOR_TEXT, scale);
            x += key_pixels + gap;
        }
    }

    ui_draw_text(92, 504,
        title_id_only ? tr(lang, "Exemplo: BLUS30591", "Example: BLUS30591") :
            tr(lang, "Digite parte do nome, Title ID ou versao.", "Type part of the name, Title ID, or version."),
        UI_COLOR_MUTED, 2);
    ui_draw_footer(
        tr(lang, "Setas escolher   X tecla   Square apagar", "Arrows choose   X key   Square delete"),
        tr(lang, "Start/OK confirma   Circle cancela", "Start/OK confirms   Circle cancels"));
    ui_present();
}

static int edit_search_query_manual(fpsu_lang lang, char *query, size_t query_size, int title_id_only)
{
    int cursor_row = 0;
    int cursor_col = 0;

    if (!query || query_size == 0) {
        return 0;
    }

    for (;;) {
        u32 btn;
        size_t len;
        draw_search_editor(lang, query, cursor_row, cursor_col, title_id_only);
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
    return edit_search_query_manual(lang, query, query_size, 0);
}

typedef struct {
    fpsu_lang lang; const fpsu_game *game; int index, count, canceled, ticks;
} cover_progress_context;

static int draw_cover_progress(void *user)
{
    cover_progress_context *ctx = user;
    char line[96];
    if (!ctx) return 0;
    if (pad_input_read_pressed() & FPSU_BUTTON_CIRCLE) ctx->canceled = 1;
    if (ui_exit_requested()) ctx->canceled = 1;
    if ((ctx->ticks++ % 2) == 0) {
        ui_begin_frame();
        ui_draw_shell("PSUF", tr(ctx->lang, "Capas da biblioteca", "Library covers"), "V3");
        ui_draw_panel(128, 160, 1024, 390, 24, UI_COLOR_PANEL, UI_COLOR_PANEL_EDGE);
        ui_draw_text(168, 188, tr(ctx->lang, "Preparando suas capas", "Preparing your covers"), UI_COLOR_TEXT, 3);
        snprintf(line, sizeof(line), "%d / %d", ctx->index + 1, ctx->count);
        ui_draw_text(168, 250, line, UI_COLOR_ACCENT, 2);
        ui_draw_text_fit(168, 296, ctx->game->title, UI_COLOR_TEXT, 2, 920);
        ui_draw_text(168, 336, ctx->game->title_id, UI_COLOR_MUTED, 1);
        draw_progress_bar(168, 392, 944, 10, ctx->index, ctx->count, UI_COLOR_ACCENT);
        ui_draw_text(168, 430, tr(ctx->lang, "Reutilizando capas locais e baixando as que faltam.", "Reusing local covers and downloading missing ones."), UI_COLOR_MUTED, 2);
        ui_draw_footer(tr(ctx->lang, "Circle  Cancelar", "Circle  Cancel"),
            ctx->canceled ? tr(ctx->lang, "Cancelando...", "Canceling...") : "");
        ui_present(); music_update();
    }
    return ctx->canceled;
}

static void download_covers_for_all(fpsu_lang lang, fpsu_game_result *results, int count)
{
    int i;
    int downloaded = 0;
    int failed = 0;
    int canceled = 0;
    char summary[320];
    cover_progress_context ctx;

    if (!results || count <= 0) {
        return;
    }

    memset(&ctx, 0, sizeof(ctx)); ctx.lang = lang; ctx.count = count;
    cover_set_progress_callback(draw_cover_progress, &ctx);
    for (i = 0; i < count; ++i) {
        ctx.index = i; ctx.game = &results[i].game; ctx.ticks = 0;
        if (draw_cover_progress(&ctx)) { canceled = 1; break; }
        if (cover_cache_for_game(&results[i].game) == 0) {
            ++downloaded;
        } else {
            ++failed;
        }
        if (ctx.canceled) {
            canceled = 1;
            break;
        }
    }
    cover_set_progress_callback(NULL, NULL);

    snprintf(summary, sizeof(summary), "%s\n%s: %d\n%s: %d%s",
        canceled ? tr(lang, "Download interrompido", "Download interrupted") :
            tr(lang, "Download de capas concluido", "Cover download complete"),
        tr(lang, "Capas salvas/reutilizadas", "Covers saved/reused"), downloaded,
        tr(lang, "Nao encontradas", "Not found"), failed,
        canceled ? tr(lang, "\n\nCircle interrompeu o restante.", "\n\nCircle stopped the rest.") : "");
    app_notice(lang,
        tr(lang, "Capas da biblioteca", "Library covers"), summary,
        failed > 0 ? UI_COLOR_ACCENT_2 : UI_COLOR_GREEN);
}

static void draw_cover_card(fpsu_lang lang, const fpsu_game *game, int x, int y, int w, int h, int show_art)
{
    if (show_art && g_cover.pixels) {
        int art_w, art_h;
        ui_cover_size(g_cover.width, g_cover.height, w - 16, h - 16, &art_w, &art_h);
        x += (w - art_w - 16) / 2;
        y += (h - art_h - 16) / 2;
        w = art_w + 16;
        h = art_h + 16;
    }
    ui_draw_panel(x, y, w, h, 12, 0xff0c202cu, UI_COLOR_ACCENT);
    if (show_art && g_cover.pixels) {
        ui_draw_cover(g_cover.pixels, g_cover.pitch, g_cover.width, g_cover.height,
            x + 8, y + 8, w - 16, h - 16);
    } else {
        ui_draw_text(x + 36, y + 70, "PSUF", UI_COLOR_ACCENT, 3);
        ui_draw_text(x + 28, y + 124, tr(lang, "CAPA", "COVER"), UI_COLOR_MUTED, 2);
        if (game) {
            ui_draw_text(x + 22, y + 164, game->title_id, UI_COLOR_ACCENT_2, 1);
        }
        ui_draw_text(x + 24, y + h - 34, tr(lang, "ONLINE", "ONLINE"), UI_COLOR_DISABLED, 1);
    }
}

static const fpsu_patch_option *result_info_option(const fpsu_game_result *result)
{
    const fpsu_patch_option *options[] = {&result->unlock, &result->fps60, &result->fps30};
    int i;
    for (i = 0; i < 3; ++i) {
        fpsu_patch_status status = options[i]->status;
        if (status == FPSU_STATUS_KNOWN || status == FPSU_STATUS_APPLICABLE ||
            status == FPSU_STATUS_UNTESTED || status == FPSU_STATUS_PC_REQUIRED) return options[i];
    }
    for (i = 0; i < 3; ++i) {
        fpsu_patch_status status = options[i]->status;
        if (status == FPSU_STATUS_UPDATE_REQUIRED || status == FPSU_STATUS_OTHER_VERSION ||
            status == FPSU_STATUS_NATIVE_60) return options[i];
    }
    return &result->unlock;
}

static void draw_results(fpsu_lang lang, fpsu_game_result *results, int total_count,
    const int *visible, int count, int index, const char *query, int show_art)
{
    int i;
    int first = index - 3;
    int left = 64;
    int list_w = 480;
    int detail_x = 585;
    int detail_w = 0;
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

    if (count > 0) {
        r = &results[visible[index]];
    }
    ui_begin_frame();
    if (r && show_art) {
        const fpsu_cover *backdrop = g_banner.pixels ? &g_banner : &g_cover;
        if (backdrop->pixels)
            ui_draw_cover_banner(backdrop->pixels, backdrop->pitch, backdrop->width, backdrop->height);
    }
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE), tr(lang, "Biblioteca de jogos", "Game library"), "V3");
    detail_w = ui_screen_width() - detail_x - 64;

    snprintf(meta, sizeof(meta), "%s: %s", tr(lang, "Busca", "Search"),
        query && query[0] ? query : tr(lang, "sem filtro", "no filter"));
    ui_draw_text(left, 94, meta, query && query[0] ? UI_COLOR_ACCENT_2 : UI_COLOR_MUTED, 1);
    if (count > 0) {
        int shortcut_x = ui_screen_width() - 300;
        ui_draw_panel(shortcut_x, 82, 236, 32, 10, UI_COLOR_PANEL_INNER, UI_COLOR_ACCENT);
        ui_draw_text_fit(shortcut_x + 14, 85, tr(lang, "Select  Capas", "Select  Covers"),
            UI_COLOR_TEXT, 2, 208);
    }

    if (count <= 0) {
        ui_draw_panel(left, 140, list_w, 260, 18, UI_COLOR_PANEL, UI_COLOR_PANEL_EDGE);
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

    info_opt = result_info_option(r);

    for (i = 0; i < 7 && first + i < count; ++i) {
        fpsu_game_result *row = &results[visible[first + i]];
        char body[96];
        snprintf(body, sizeof(body), "%s  %s", row->game.title_id, row->game.version);
        ui_draw_menu_item_slim(left, 120 + i * 72, list_w, row->game.title, body, first + i == index, 1);
    }

    ui_draw_panel(detail_x, 120, detail_w, 102, 18, UI_COLOR_PANEL, UI_COLOR_PANEL_EDGE);
    ui_draw_panel(detail_x, 480, detail_w, 130, 18, UI_COLOR_PANEL, UI_COLOR_PANEL_EDGE);
    ui_draw_text_box(detail_x + 32, 136, r->game.title, UI_COLOR_TEXT, 2, detail_w - 64, 27, 2);
    snprintf(meta, sizeof(meta), "%s  %s", r->game.title_id, r->game.version);
    ui_draw_text(detail_x + 32, 196, meta, UI_COLOR_MUTED, 1);
    draw_cover_card(lang, &r->game, detail_x + 32, 230, 172, 242, show_art);
    draw_status_line(lang, detail_x + 224, 232, 350,
        patch_kind_title(lang, FPSU_PATCH_60, r->fps60.status), r->fps60.status);
    draw_status_line(lang, detail_x + 224, 310, 350,
        patch_kind_title(lang, FPSU_PATCH_UNLOCK, r->unlock.status), r->unlock.status);
    draw_status_line(lang, detail_x + 224, 388, 350,
        patch_kind_title(lang, FPSU_PATCH_30, r->fps30.status), r->fps30.status);
    ui_draw_text_box(detail_x + 32, 487,
        info_opt->status == FPSU_STATUS_NOT_FOUND &&
            patch_db_collect_graphics_options(&r->game, NULL, 0) > 0 ?
            tr(lang, "Há patches gráficos disponíveis no menu deste jogo.",
                "Graphics patches are available in this game's menu.") : status_help(lang, info_opt->status),
        UI_COLOR_MUTED, 1,
        detail_w - 64, 19, 2);
    if (info_opt->label[0] && info_opt->status != FPSU_STATUS_NOT_FOUND) {
        ui_draw_text_fit(detail_x + 32, 530, patch_text(lang, info_opt->label), status_color(info_opt->status), 1, detail_w - 64);
    }
    if (info_opt->note[0] &&
        (info_opt->status == FPSU_STATUS_UPDATE_REQUIRED || info_opt->status == FPSU_STATUS_OTHER_VERSION)) {
        ui_draw_text_box(detail_x + 32, 555, patch_text(lang, info_opt->note), UI_COLOR_MUTED, 1, detail_w - 64, 20, 2);
    }
    if (query && query[0]) {
        snprintf(meta, sizeof(meta), "%d/%d  (%d total)", index + 1, count, total_count);
    } else {
        snprintf(meta, sizeof(meta), "%d/%d", index + 1, total_count);
    }
    ui_draw_text(detail_x + 32, 624, meta, UI_COLOR_ACCENT_2, 1);

    {
        ui_footer_action actions[] = {
            {UI_ICON_ARROWS, lang == FPSU_LANG_ES ? "Juegos" : lang == FPSU_LANG_PT ? "Jogos" : "Games"},
            {UI_ICON_CROSS, lang == FPSU_LANG_ES ? "Opciones" : lang == FPSU_LANG_PT ? "Opções" : "Options"},
            {UI_ICON_SQUARE, lang == FPSU_LANG_ES ? "Restaurar" : lang == FPSU_LANG_PT ? "Restaurar" : "Restore"},
            {UI_ICON_TRIANGLE, lang == FPSU_LANG_ES ? "Buscar" : lang == FPSU_LANG_PT ? "Buscar" : "Search"},
            {UI_ICON_CIRCLE, lang == FPSU_LANG_ES ? "Volver" : lang == FPSU_LANG_PT ? "Voltar" : "Back"}
        };
        ui_draw_footer_actions(48, 654, 1184, actions, 5);
    }
    ui_present();
}

static void draw_selected_game_backdrop(void)
{
    const fpsu_cover *backdrop = g_banner.pixels ? &g_banner : &g_cover;
    if (backdrop->pixels) {
        ui_draw_cover_banner(backdrop->pixels, backdrop->pitch,
            backdrop->width, backdrop->height);
    }
}

static void draw_restore_list(fpsu_lang lang, fpsu_game_result *results, int count, int index)
{
    int first = index - 3;
    int i;
    int list_w = 700;
    fpsu_game *game;

    if (first < 0) {
        first = 0;
    }
    if (first > count - 7) {
        first = count - 7;
    }
    if (first < 0) {
        first = 0;
    }
    game = &results[index].game;

    ui_begin_frame();
    ui_draw_shell(i18n_text(lang, TXT_APP_TITLE),
        tr(lang, "Backups > Restaurar por jogo", "Backups > Restore by game"), tr(lang, "BACKUP", "BACKUP"));
    ui_draw_text(64, 110,
        tr(lang, "Escolha um jogo. X restaura diretamente o backup selecionado.",
            "Choose a game. X directly restores the selected backup."),
        UI_COLOR_MUTED, 1);
    ui_draw_panel(64, 136, 1152, 500, 18, 0xff102834u, UI_COLOR_PANEL_EDGE);

    for (i = 0; i < 7 && first + i < count; ++i) {
        fpsu_game_result *row = &results[first + i];
        char body[96];
        snprintf(body, sizeof(body), "%s  %s", row->game.title_id, row->game.version);
        ui_draw_menu_item_slim(84, 154 + i * 66, list_w, row->game.title, body,
            first + i == index, 1);
    }

    ui_draw_panel(822, 154, 370, 440, 16, 0xff173746u, UI_COLOR_PANEL_EDGE);
    ui_draw_text_fit(852, 184, game->title, UI_COLOR_TEXT, 2, 310);
    ui_draw_text(852, 226, game->title_id, UI_COLOR_ACCENT_2, 2);
    ui_draw_text(852, 264, game->version, UI_COLOR_MUTED, 2);
    draw_multiline_text(852, 322,
        tr(lang, "O PSUF restaura somente este jogo e remove os arquivos gerados por ele.",
            "PSUF restores only this game and removes its generated files."),
        UI_COLOR_MUTED, 1, 40, 3);
    draw_multiline_text(852, 404,
        tr(lang, "Se nao houver backup, ele remove apenas o patch do PSUF.",
            "If no backup exists, it only removes the PSUF patch."),
        UI_COLOR_MUTED, 1, 40, 3);

    {
        ui_footer_action actions[] = {
            {UI_ICON_ARROWS, lang == FPSU_LANG_ES ? "Elegir" : lang == FPSU_LANG_PT ? "Escolher" : "Choose"},
            {UI_ICON_CROSS, lang == FPSU_LANG_ES ? "Restaurar" : lang == FPSU_LANG_PT ? "Restaurar" : "Restore"},
            {UI_ICON_CIRCLE, lang == FPSU_LANG_ES ? "Volver" : lang == FPSU_LANG_PT ? "Voltar" : "Back"}
        };
        ui_draw_footer_actions(48, 654, 1184, actions, 3);
    }
    ui_present();
}

static void show_restore_backup_list(fpsu_lang lang)
{
    int count;
    int index = 0;

    count = cache_read_results(g_results, FPSU_MAX_GAMES);
    count = dedupe_results(g_results, count);
    if (count <= 0) {
        app_notice(lang,
            tr(lang, "Nenhum jogo na biblioteca", "No game in library"),
            tr(lang, "Escaneie os jogos uma vez antes de restaurar um backup por jogo.",
                "Scan the games once before restoring a backup by game."),
            UI_COLOR_ACCENT_2);
        return;
    }

    for (;;) {
        u32 btn;
        draw_restore_list(lang, g_results, count, index);
        btn = frame_input();
        if (btn & FPSU_BUTTON_DOWN) {
            index = (index + 1) % count;
        }
        if (btn & FPSU_BUTTON_UP) {
            index = (index + count - 1) % count;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            restore_game_backup(lang, &g_results[index].game);
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
    }
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
                option->note[0] ? patch_text(lang, option->note) :
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
            option->note[0] ? patch_text(lang, option->note) :
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
                option->note[0] ? patch_text(lang, option->note) :
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
        snprintf(msg, sizeof(msg), "%s\n\n%s", patch_text(lang, option->label),
            tr(lang, "Modo experimental. Crie backup e teste com cuidado.", "Experimental mode. Keep a backup and test carefully."));
        if (!app_confirm(lang, i18n_text(lang, TXT_ACTION_UNLOCK_UNSAFE), msg)) {
            return;
        }
    }

    snprintf(msg, sizeof(msg), "%s\n%s", result->game.title, patch_text(lang, option->label));
    if (!app_confirm(lang, i18n_text(lang, TXT_CONFIRM_APPLY), msg)) {
        return;
    }

    if (fpsu_method_is_ncl_write(option->method)) {
        int patch_result;
        draw_busy_notice(lang,
            tr(lang, "Aplicando patch, aguarde", "Applying patch, please wait"),
            tr(lang, "Criando backup pequeno e gravando NCL.", "Creating a small backup and writing NCL."));
        patch_result = patch_db_write_ncl(&result->game, option, path, sizeof(path));
        if (patch_result == FPSU_PATCH_SCRIPT_READY) {
#if FPSU_HEN_DIAG_BUILD
            snprintf(msg, sizeof(msg), "%s\n%s\n\n%s\n%s\n%s\n%s",
                result->game.title,
                tr(lang, "Build de teste: abra o jogo e espere os 2 minutos.",
                    "Test build: open the game and wait 2 minutes."),
                tr(lang, "Se aparecer aviso no jogo e o FPS nao mudar, verifique o processo e a memoria.",
                    "If an in-game message appears and FPS does not change, check the process and memory."),
                tr(lang, "Se nao aparecer aviso nenhum, mande estes logs:",
                    "If no message appears, send these logs:"),
                FPSU_CACHE_ROOT "/last_apply_setup.log",
                FPSU_CACHE_ROOT "/webman_ingame.log");
#else
            snprintf(msg, sizeof(msg), "%s\n%s\n\n%s\n%s",
                result->game.title,
                tr(lang, "O patch foi enviado. O webMAN executara a aplicacao via PS3MAPI ao iniciar o jogo.",
                    "The patch was sent. webMAN will apply it through PS3MAPI when the game starts."),
                tr(lang, "Se o FPS nao mudar, verifique memoria/endereco e envie este log:",
                    "If FPS does not change, check memory/address and send this log:"),
                FPSU_CACHE_ROOT "/webman_ingame.log");
#endif
            snprintf(option->note, sizeof(option->note), "%s",
                "Patch enviado para teste HEN-safe. Temporarios sao limpos depois.");
            app_notice(lang, i18n_text(lang, TXT_APPLY_OK), msg, UI_COLOR_ACCENT_2);
        } else if (patch_result == FPSU_PATCH_NCL_ONLY) {
            app_notice_details(lang, tr(lang, "NCL salvo", "NCL saved"),
                tr(lang, "O NCL foi salvo. Não foi possível gravar o script automático do webMAN.",
                    "The NCL was saved. The automatic webMAN script could not be saved."),
                UI_COLOR_ACCENT_2);
        } else if (patch_result == FPSU_PATCH_UNSAFE_ADDRESS) {
            app_notice_details(lang, tr(lang, "Endereço de patch inseguro", "Unsafe patch address"),
                tr(lang, "O webMAN mudaria um endereço deste patch. O PSUF bloqueou a escrita.",
                    "webMAN would change an address in this patch. The PSUF blocked the write."),
                UI_COLOR_RED);
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
        draw_selected_game_backdrop();
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
        {
            ui_footer_action actions[] = {
                {UI_ICON_ARROWS, lang == FPSU_LANG_ES ? "Elegir" : lang == FPSU_LANG_PT ? "Escolher" : "Choose"},
                {UI_ICON_CROSS, lang == FPSU_LANG_ES ? "Aplicar + copia" : lang == FPSU_LANG_PT ? "Aplicar + backup" : "Apply + backup"},
                {UI_ICON_CIRCLE, lang == FPSU_LANG_ES ? "Volver" : lang == FPSU_LANG_PT ? "Voltar" : "Back"}
            };
            ui_draw_footer_actions(48, 654, 1184, actions, 3);
        }
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

    snprintf(msg, sizeof(msg), "%s\n\n%s\n\n%s", patch_text(lang, choices[selected].label),
        choices[selected].note[0] ? patch_text(lang, choices[selected].note) :
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
        draw_selected_game_backdrop();
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
                opt->label[0] ? patch_text(lang, opt->label) : tr(lang, "Patch extra/grafico", "Extra/graphics patch"),
                body, selected == first + i, 1);
        }

        snprintf(msg, sizeof(msg), "%d/%d", selected + 1, choice_count);
        ui_draw_text(100, 610, msg, UI_COLOR_ACCENT_2, 2);
        {
            ui_footer_action actions[] = {
                {UI_ICON_ARROWS, lang == FPSU_LANG_ES ? "Elegir" : lang == FPSU_LANG_PT ? "Escolher" : "Choose"},
                {UI_ICON_CROSS, lang == FPSU_LANG_ES ? "Aplicar" : lang == FPSU_LANG_PT ? "Aplicar" : "Apply"},
                {UI_ICON_CIRCLE, lang == FPSU_LANG_ES ? "Volver" : lang == FPSU_LANG_PT ? "Voltar" : "Back"}
            };
            ui_draw_footer_actions(48, 654, 1184, actions, 3);
        }
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
        patch_text(lang, choices[selected].label),
        choices[selected].note[0] ? patch_text(lang, choices[selected].note) :
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
                tr(lang, "Patch", "Patch"), patch_text(lang, option->label),
                tr(lang, "Fonte", "Source"), option->source);
            return;
        }
        snprintf(out, out_size, "%s > %s | %s > %s",
            tr(lang, "Status", "Status"), status_text(lang, option->status),
            tr(lang, "Patch", "Patch"), patch_text(lang, option->label));
    } else {
        snprintf(out, out_size, "%s > %s",
            tr(lang, "Status", "Status"), status_text(lang, option->status));
    }
}

static int launch_game_from_menu(fpsu_lang lang, const fpsu_game *game)
{
    char message[320];
    int launched;

    if (!game || game->base_path[0] == '\0') {
        app_notice(lang,
            tr(lang, "Jogo sem caminho", "Game path unavailable"),
            tr(lang,
                "O PSUF identificou o jogo, mas o caminho necessario para o webMAN nao esta disponivel.",
                "PSUF identified the game, but the path required by webMAN is unavailable."),
            UI_COLOR_RED);
        return 0;
    }

    snprintf(message, sizeof(message), "%s\n\n%s", game->title,
        tr(lang, "O PSUF vai fechar e iniciar o jogo selecionado.",
            "PSUF will close and start the selected game."));
    if (!app_confirm(lang, tr(lang, "Iniciar jogo", "Start game"), message)) return 0;

    draw_busy_notice(lang,
        tr(lang, "Iniciando jogo", "Starting game"),
        tr(lang, "Preparando o atalho do jogo.", "Preparing the game shortcut."));
    launched = webman_play_game(game, message, sizeof(message));
    if (launched != 0) {
        if (launched == FPSU_WEBMAN_PLAY_XMB_UNAVAILABLE)
            snprintf(message, sizeof(message), "%s",
                tr(lang, "Não foi possível verificar a seção de jogos instalados.",
                    "The installed games section could not be verified."));
        app_notice(lang,
            tr(lang, "Falha ao iniciar", "Could not start game"),
            message[0] ? message :
                tr(lang, "O webMAN nao respondeu ao atalho de inicio.",
                    "webMAN did not answer the start shortcut."),
            UI_COLOR_RED);
        return 0;
    }

    g_exit_after_launch = 1;
    return 1;
}

enum {
    ACTION_FPS60, ACTION_UNLOCK, ACTION_FPS30, ACTION_GRAPHICS,
    ACTION_FORCE, ACTION_RESTORE, ACTION_PLAY, ACTION_BACK, ACTION_COUNT
};

static void draw_actions(fpsu_lang lang, const fpsu_game_result *r, int selected, int graphics_count)
{
    char details[3][224];
    const fpsu_patch_option *options[] = {&r->fps60, &r->unlock, &r->fps30};
    const char *titles[] = {
        patch_kind_title(lang, FPSU_PATCH_60, r->fps60.status),
        patch_kind_title(lang, FPSU_PATCH_UNLOCK, r->unlock.status),
        patch_kind_title(lang, FPSU_PATCH_30, r->fps30.status),
        tr(lang, "Patches gráficos", "Graphics patches"),
        tr(lang, "Forçar rota (não seguro)", "Force route (unsafe)"),
        tr(lang, "Restaurar backup", "Restore backup"),
        tr(lang, "Iniciar jogo", "Start game"),
        tr(lang, "Voltar à biblioteca", "Back to library")
    };
    const char *bodies[] = {
        details[0], details[1], details[2],
        graphics_count > 0 ? tr(lang, "Efeitos visuais disponíveis para este jogo.", "Available visual effects for this game.") :
            tr(lang, "Nenhum patch gráfico disponível.", "No graphics patch available."),
        tr(lang, "Escolha uma rota manual do banco.", "Choose a manual database route."),
        tr(lang, "Restaura apenas este jogo.", "Restore this game only."),
        tr(lang, "Inicia este jogo após confirmar.", "Starts this game after confirmation."),
        ""
    };
    const ui_icon icons[] = {UI_ICON_DOT, UI_ICON_UNLOCK, UI_ICON_DOT, UI_ICON_LIBRARY,
        UI_ICON_UNLOCK, UI_ICON_BACKUP, UI_ICON_PLAY, UI_ICON_DOT};
    int i;
    for (i = 0; i < 3; ++i) action_body(details[i], sizeof(details[i]), lang, options[i]);

    ui_begin_frame();
    draw_selected_game_backdrop();
    ui_draw_shell("PSUF", tr(lang, "Opções do jogo", "Game options"), r->game.title_id);
    ui_draw_text_fit(100, 88, r->game.title, UI_COLOR_MUTED, 1, 1050);
    for (i = 0; i < ACTION_COUNT; ++i)
        ui_draw_action_item(100, 120 + i * 63, ui_screen_width() - 200, titles[i], bodies[i],
            selected == i, i >= 3 || options[i]->status != FPSU_STATUS_NOT_FOUND, icons[i]);

    {
        ui_footer_action actions[] = {
            {UI_ICON_ARROWS, lang == FPSU_LANG_ES ? "Navegar" : lang == FPSU_LANG_PT ? "Navegar" : "Navigate"},
            {UI_ICON_CROSS, lang == FPSU_LANG_ES ? "Elegir" : lang == FPSU_LANG_PT ? "Selecionar" : "Select"},
            {UI_ICON_CIRCLE, lang == FPSU_LANG_ES ? "Volver" : lang == FPSU_LANG_PT ? "Voltar" : "Back"}
        };
        ui_draw_footer_actions(48, 654, 1184, actions, 3);
    }
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
            "Nao existe patch conhecido nesta opcao. Use Escolher/forcar rota se quiser testar com risco.",
            "There is no known patch for this option. Use Choose/force route if you want to test with risk."));
    app_notice(lang, i18n_text(lang, TXT_STATUS_NOT_FOUND), msg, UI_COLOR_ACCENT_2);
}

static void show_actions(fpsu_lang lang, fpsu_game_result *r)
{
    int selected = ACTION_FPS60;
    int redraw = 1;
    int graphics_count = patch_db_collect_graphics_options(&r->game, NULL, 0);
    for (;;) {
        u32 btn;
        if (redraw) draw_actions(lang, r, selected, graphics_count);
        btn = frame_input();
        redraw = btn != 0;
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % ACTION_COUNT;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + ACTION_COUNT - 1) % ACTION_COUNT;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            return;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            if (selected == ACTION_PLAY) {
                if (launch_game_from_menu(lang, &r->game)) {
                    return;
                }
            } else if (selected == ACTION_FPS60) {
                if (normal_action_available(&r->fps60)) {
                    apply_option(lang, r, &r->fps60);
                } else {
                    show_normal_action_unavailable(lang, &r->fps60);
                }
            } else if (selected == ACTION_UNLOCK) {
                if (normal_action_available(&r->unlock)) {
                    apply_option(lang, r, &r->unlock);
                } else {
                    show_normal_action_unavailable(lang, &r->unlock);
                }
            } else if (selected == ACTION_FPS30) {
                if (normal_action_available(&r->fps30)) {
                    apply_option(lang, r, &r->fps30);
                } else {
                    show_normal_action_unavailable(lang, &r->fps30);
                }
            } else if (selected == ACTION_GRAPHICS) {
                show_graphics_patches(lang, r);
            } else if (selected == ACTION_FORCE) {
                force_apply_option(lang, r);
            } else if (selected == ACTION_RESTORE) {
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
        "Projeto de RafJaeger.\n\nPSUF V3 para CFW 4.90+ e PS3HEN.\nNem todos os consoles sao compativeis no momento, mas estamos procurando formas de contornar isso.\nUse Sistema > Verificar dependencias se o patch aplica mas nao muda nada.\nNao mexe em dev_flash, dev_blind, LV1, LV2 ou boot plugins.",
        "Project by RafJaeger.\n\nPSUF V3 for CFW 4.90+ and PS3HEN.\nNot every console is compatible right now, but we are looking for ways around that.\nUse System > Check dependencies if the patch installs but nothing changes.\nDoes not touch dev_flash, dev_blind, LV1, LV2 or boot plugins.");
    app_notice(lang, i18n_text(lang, TXT_MENU_ABOUT), body, UI_COLOR_ACCENT);
}

static void show_scan_results(fpsu_lang lang, fpsu_game_result *results, int count)
{
    int index = 0;
    int redraw = 1;
    int settle = 0;
    int art_ready = 0;
    int download_requested = 0;
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
        int previous_index = index;
        int reset_art = 0;

        if (download_requested) {
            /* Serialize explicit downloads with the preview decoder. */
            if (cover_poll_preview(NULL, &g_cover, &g_banner)) {
                download_requested = 0;
                download_covers_for_all(lang, results, count);
                redraw = 1;
            }
        } else if (!art_ready && visible_count > 0 && settle == 0) {
            art_ready = cover_poll_preview(&results[visible[index]].game, &g_cover, &g_banner);
            if (art_ready) redraw = 1;
        }
        if (redraw) draw_results(lang, results, count, visible, visible_count, index, search, art_ready);
        btn = frame_input();
        redraw = btn != 0;
        if (settle) --settle;
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
            index = (index + visible_count - (7 % visible_count)) % visible_count;
        }
        if (visible_count > 0 && (btn & FPSU_BUTTON_CROSS)) {
            show_actions(lang, &results[visible[index]]);
            if (g_exit_after_launch) break;
            reset_art = 1;
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
                reset_art = 1;
            }
        }
        if ((btn & FPSU_BUTTON_SELECT) && visible_count > 0) {
            download_requested = 1;
            reset_art = 1;
        }
        if (btn & FPSU_BUTTON_CIRCLE) break;
        if (reset_art || index != previous_index) {
            settle = 10;
            art_ready = 0;
            cover_release(&g_cover);
            cover_release(&g_banner);
        }
    }
    cover_poll_preview(NULL, &g_cover, &g_banner);
    cover_release(&g_cover);
    cover_release(&g_banner);
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

static void scan_all_and_show(fpsu_lang lang)
{
    scan_context ctx;
    int game_count;

    ctx.lang = lang;
    ctx.found = 0;
    ctx.cancel = 0;
    ctx.last_drawn = -1;
    draw_scan_screen(lang, "", 0);
    draw_busy_notice(lang,
        tr(lang, "Preparando NTFS", "Preparing NTFS"),
        tr(lang,
            "Atualizando a lista de jogos do HD externo pelo webMAN.",
            "Refreshing the external drive game list through webMAN."));
    (void)webman_prepare_ntfs(1);
    draw_scan_screen(lang, "", 0);
    cache_write_progress("scan_start", 0, FPSU_MAX_GAMES, NULL, 0, 0, NULL, 0);
    game_count = scanner_scan_all(g_games, FPSU_MAX_GAMES, scan_progress_cb, &ctx);
    game_count = dedupe_games(g_games, game_count);
    cache_write_progress(ctx.cancel ? "scan_canceled" : "scan_done", game_count, game_count,
        game_count > 0 ? &g_games[game_count - 1] : NULL, 0, 0, NULL, 0);
    match_results(lang, game_count);
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

static void update_patch_database_online(fpsu_lang lang)
{
    char message[192];
    int game_count;
    int ret;

    draw_busy_notice(lang,
        tr(lang, "Atualizando online", "Updating online"),
        tr(lang, "Baixando banco do GitHub e validando antes de trocar.", "Downloading the GitHub database and validating it first."));

    ret = repo_update_databases(message, sizeof(message));
    if (ret != 0) {
        app_notice(lang,
            tr(lang, "Atualizacao online falhou", "Online update failed"),
            message[0] ? message : tr(lang,
                "Nao baixou ou validou o banco online. O banco antigo foi mantido.",
                "The online database was not downloaded or validated. The old database was kept."),
            UI_COLOR_RED);
        return;
    }

    game_count = cache_read_results(g_results, FPSU_MAX_GAMES);
    game_count = dedupe_results(g_results, game_count);
    if (game_count > 0) {
        draw_busy_notice(lang,
            tr(lang, "Atualizando biblioteca", "Updating library"),
            tr(lang, "Recarregando bancos novos sem apagar o scan salvo.", "Reloading the new databases without deleting the saved scan."));
        rebuild_cached_results(lang, game_count);
        cache_write_results(g_results, game_count);
        app_notice(lang,
            tr(lang, "Banco online atualizado", "Online database updated"),
            tr(lang,
                "Banco online atualizado. Abra Jogos scaneados para conferir.",
                "Online database updated. Open Scanned games to review it."),
            UI_COLOR_GREEN);
    } else {
        app_notice(lang,
            tr(lang, "Banco online atualizado", "Online database updated"),
            tr(lang,
                "Banco online atualizado. Agora escaneie jogos ou abra Jogos scaneados.",
                "Online database updated. Now scan games or open Scanned games."),
            UI_COLOR_GREEN);
    }
}

int main(int argc, char **argv)
{
    fpsu_lang lang;
    int selected = 0;
    int running = 1;
    int home_frames = 12;
    int home_full_frame = 1;
    (void)argc;
    (void)argv;

    lang = i18n_detect_language();
    patch_db_cleanup_runtime_artifacts();
    if (ui_init() != 0) {
        return 1;
    }
    pad_input_init();
    music_init();

    while (running && !g_exit_after_launch) {
        u32 btn;
        if (home_frames > 0) {
            draw_home(lang, selected, home_full_frame);
            home_full_frame = 0;
            --home_frames;
        }
        btn = poll_input(home_frames == 0);
        if (btn) home_frames = 8;
        if (btn & FPSU_BUTTON_DOWN) {
            selected = (selected + 1) % FPSU_HOME_ITEMS;
        }
        if (btn & FPSU_BUTTON_UP) {
            selected = (selected + FPSU_HOME_ITEMS - 1) % FPSU_HOME_ITEMS;
        }
        if (btn & FPSU_BUTTON_TRIANGLE) {
            show_about(lang);
            home_full_frame = 1;
        }
        if (btn & FPSU_BUTTON_CIRCLE) {
            running = 0;
        }
        if (btn & FPSU_BUTTON_CROSS) {
            home_full_frame = 1;
            if (selected == 0) {
                scan_all_and_show(lang);
            } else if (selected == 1) {
                show_cached_results(lang);
            } else if (selected == 2) {
                scan_mounted_and_show(lang);
            } else if (selected == 3) {
                show_restore_backup_list(lang);
            } else if (selected == 4) {
                activate_dependencies(lang);
            } else if (selected == 5) {
                show_webman_settings(lang);
            } else if (selected == 6) {
                show_donate(lang);
            } else if (selected == 7) {
                update_patch_database(lang);
            } else if (selected == 8) {
                update_patch_database_online(lang);
            } else if (selected == 9) {
                if (ui_set_dark(!ui_is_dark()) != 0)
                    app_notice(lang, tr(lang, "Não foi possível salvar o tema", "Could not save the theme"),
                        tr(lang, "Falha ao gravar sua preferência no PS3. Tente novamente.",
                            "Could not write your preference on PS3. Try again."),
                        UI_COLOR_RED);
            }
            else {
                running = 0;
            }
        }
    }

    music_shutdown();
    pad_input_shutdown();
    cover_release(&g_cover);
    cover_release(&g_banner);
    ui_shutdown();
    return 0;
}
