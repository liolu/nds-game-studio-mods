/* Passer l'intro de Lego Star Wars II (AL7E) : des que le jeu a ouvert son premier ecran, on ouvre les deux ecrans du menu,
 * comme a la fin de l'intro. Mod natif : il ne connait que sdk/nds_mod.h. SPDX-License-Identifier: MIT */
#include "../../sdk/nds_util.h"

/* Lego Star Wars II, version USA */
#define SCREENS 0x02102900u       /* gestionnaire des ecrans : non nul une fois le premier ecran ouvert */
#define FUNC_OPEN_SCREEN 0x0205ecc4u
#define MENU_SCREEN 0x11
#define MENU_ARG 4
#define BACK_SCREEN 0xf

static const nds_api *api;
static int done;

/* dans une interruption, comme le faisait le runtime : le jeu est entre deux ecrans */
static void on_irq(void *user, nds_u32 bits) {
    if (done || !api->read32(SCREENS)) return;
    done = 1;
    nds_fn open = api->function_at(FUNC_OPEN_SCREEN);
    open(SCREENS, MENU_SCREEN, 0, MENU_ARG, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    open(SCREENS, BACK_SCREEN, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

NDS_MOD_EXPORT int nds_mod_load(const nds_api *a, nds_mod *mod) {
    api = a;
    if (!a->function_at(FUNC_OPEN_SCREEN)) {
        a->fail(mod, "fonction du jeu introuvable : ce n'est pas la version USA de Lego Star Wars II");
        return 1;
    }
    a->on_irq(mod, on_irq, 0);
    return 0;
}
