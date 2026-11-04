/* Camera libre pour Lego Star Wars II (AL7E) : une touche detache la camera du jeu, en jeu comme dans une cinematique ;
 * une autre gele la scene. Mod natif : il ne connait que sdk/nds_mod.h.
 *
 * La camera passe par l'accroche de la fonction qui calcule la vue. Pour geler une scene, rien de nouveau dans le jeu : la mise a
 * jour du niveau ne tourne plus, ses taches sont coupees et ses objets recoivent le drapeau de pause du moteur ; ils ne bougent plus
 * mais restent dessines. SPDX-License-Identifier: MIT */
#include "../../sdk/nds_util.h"

/* Lego Star Wars II, version USA */
#define CAMERA 0x02109cd4u
#define FUNC_CAMERA 0x020ace50u       /* calcule la vue depuis la camera */
#define FUNC_LEVEL_UPDATE 0x0208cdb0u /* mise a jour du niveau, sautee pendant le gel */
#define FUNC_LEVEL_EXIT 0x0208cf08u
#define FUNC_GET_TICK 0x020ce6c8u
#define GAME_FLAGS 0x027e02a8u
#define OBJECTS 0x021019f4u
#define TASKS 0x021042d4u
#define TIMERS 0x0210930cu

#define CAM_ROTATION 0x14
#define CAM_FLAGS 0x160
#define VIEW_DIRTY 0xCu /* la vue et le volume de vue sont a refaire */
#define CAM_WORDS 21    /* position, puis matrice 4x4 de CAM_ROTATION : ce qu'on remet en quittant */
#define RADIANS_PER_COUNT 0.0025f /* souris */
#define MAX_PITCH 1.5f
#define SPEED 0.25f /* unites du jeu par image du DS ; le personnage mesure environ 6 unites */
#define SPEED_FAST 4.0f
#define SPEED_SLOW 0.2f
#define MAX_STEP 8 /* images rattrapees au plus, apres un arret du jeu */

/* bits de KEYINPUT */
#define PAD_A 1u
#define PAD_B 2u
#define PAD_RIGHT 16u
#define PAD_LEFT 32u
#define PAD_UP 64u
#define PAD_DOWN 128u
#define PAD_R 256u
#define PAD_L 512u

static const nds_api *api;
static nds_mod *self;
static nds_fn camera_next, level_update_next, level_exit_next;
static int key_free = NDS_KEY_C, key_freeze = NDS_KEY_V;
static int state; /* 0 = camera du jeu, 1 = camera libre, 3 = camera libre et scene gelee */
static nds_input in;

#define ARGS nds_u32 r0, nds_u32 r1, nds_u32 r2, nds_u32 r3, nds_u32 a0, nds_u32 a1, nds_u32 a2, nds_u32 a3, nds_u32 a4, nds_u32 a5, nds_u32 a6, nds_u32 a7, nds_u32 a8, nds_u32 a9
#define PASS r0, r1, r2, r3, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9

static nds_s32 *ds32(nds_u32 a) { return (nds_s32 *)api->memory(a); }

static void say(const char *en, const char *fr) { api->notice(self, nds_lang_fr(api) ? fr : en, 3000); }

/* ---- les touches : chaque image, sur le clavier entier envoye par le launcher */
static void keys(void *user, nds_u32 frame) {
    static int was_free, was_freeze;
    in.size = sizeof in;
    api->input(&in);
    int now_free = NDS_KEY_DOWN(&in, key_free), now_freeze = NDS_KEY_DOWN(&in, key_freeze);
    int before = state;
    if (now_free && !was_free) state = state ? 0 : 1;
    else if (now_freeze && !was_freeze) state = state == 3 ? 1 : 3;
    was_free = now_free;
    was_freeze = now_freeze;
    if (state == before) return;
    api->take_input(self, state != 0); /* le jeu ne recoit plus de touches : le personnage reste sur place */
    api->capture_mouse(self, state != 0);
    if (!state) say("Free camera off", "Camera libre coupee");
    else if (state == 1) say("Free camera: mouse, arrows, L and R. Freeze key: freezes the scene.", "Camera libre : souris, fleches, L et R. Touche de gel : gele la scene.");
    else say("Scene frozen. Freeze key resumes, free camera key leaves.", "Scene gelee. La touche de gel reprend, celle de la camera quitte.");
}

/* ---- la camera */
static nds_u64 on_camera(ARGS) {
    static int on, seen_x, seen_y;
    static nds_u32 last_frame, frames;
    static float x, y, z, yaw, pitch;
    static nds_s32 saved[CAM_WORDS];
    frames++;
    nds_s32 *cam = ds32(CAMERA);
    if (r0 != CAMERA) return camera_next(PASS);
    if (!state) {
        if (on) {
            /* le jeu reprend sa camera ou il l'avait laissee */
            on = 0;
            for (int i = 0; i < CAM_WORDS; i++) cam[i] = saved[i];
            api->write32(CAMERA + CAM_FLAGS, api->read32(CAMERA + CAM_FLAGS) | VIEW_DIRTY);
        }
        return camera_next(PASS);
    }
    nds_u32 *frame = api->runtime("nds_frame");
    nds_u32 now = frame ? *frame : frames;
    if (!on) {
        /* on part de la ou regarde le jeu : la troisieme ligne de ses axes pointe vers l'arriere */
        on = 1;
        for (int i = 0; i < CAM_WORDS; i++) saved[i] = cam[i];
        const nds_s32 *axes = cam + CAM_ROTATION / 4;
        float back_x = nds_real(axes[8]), back_y = nds_real(axes[9]), back_z = nds_real(axes[10]);
        x = nds_real(cam[0]);
        y = nds_real(cam[1]);
        z = nds_real(cam[2]);
        yaw = nds_arc_tangent(-back_x, -back_z);
        pitch = nds_arc_tangent(-back_y, -back_x * nds_sine(yaw) - back_z * nds_cosine(yaw));
        seen_x = in.mouse_dx;
        seen_y = in.mouse_dy;
        last_frame = now;
    }
    yaw = nds_wrap_angle(yaw - (float)(in.mouse_dx - seen_x) * RADIANS_PER_COUNT);
    pitch -= (float)(in.mouse_dy - seen_y) * RADIANS_PER_COUNT;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;
    if (pitch < -MAX_PITCH) pitch = -MAX_PITCH;
    seen_x = in.mouse_dx;
    seen_y = in.mouse_dy;
    float look_x = nds_sine(yaw), look_z = nds_cosine(yaw), up = nds_sine(pitch), level = nds_cosine(pitch);
    /* la vue se calcule une fois par image du jeu, 30 par seconde en general : la vitesse se compte en images du DS */
    nds_u32 pad = in.ds_keys;
    nds_u32 step = now - last_frame;
    last_frame = now;
    if (step > MAX_STEP) step = MAX_STEP;
    float distance = SPEED * (float)step;
    if (pad & PAD_A) distance *= SPEED_FAST;
    if (pad & PAD_B) distance *= SPEED_SLOW;
    float forward = (pad & PAD_UP ? distance : 0) - (pad & PAD_DOWN ? distance : 0);
    float right = (pad & PAD_RIGHT ? distance : 0) - (pad & PAD_LEFT ? distance : 0);
    float rise = (pad & PAD_R ? distance : 0) - (pad & PAD_L ? distance : 0);
    x += look_x * level * forward - look_z * right;
    y += up * forward + rise;
    z += look_z * level * forward + look_x * right;
    cam[0] = nds_fx(x);
    cam[1] = nds_fx(y);
    cam[2] = nds_fx(z);
    /* lignes de la camera : droite, haut, arriere */
    nds_s32 *axes = cam + CAM_ROTATION / 4;
    axes[0] = nds_fx(-look_z); axes[1] = 0; axes[2] = nds_fx(look_x); axes[3] = 0;
    axes[4] = nds_fx(-up * look_x); axes[5] = nds_fx(level); axes[6] = nds_fx(-up * look_z); axes[7] = 0;
    axes[8] = nds_fx(-look_x * level); axes[9] = nds_fx(-up); axes[10] = nds_fx(-look_z * level); axes[11] = 0;
    axes[12] = 0; axes[13] = 0; axes[14] = 0; axes[15] = 0x1000;
    api->write32(CAMERA + CAM_FLAGS, api->read32(CAMERA + CAM_FLAGS) | VIEW_DIRTY);
    /* la camera libre passe avant tout autre mod de camera (vue a la premiere personne) : le jeu calcule la vue tel quel */
    return api->game_function(FUNC_CAMERA)(PASS);
}

/* ---- geler la scene */
#define GAME_PAUSED 3u    /* le jeu est deja en pause : il saute lui-meme la mise a jour du niveau */
#define GAME_MULTI 0x40u  /* a plusieurs : les autres machines attendent la notre */
#define LISTS 2           /* listes d'objets et de taches, toutes deux de 0x38 octets : un en-tete, puis la tete de liste a +4 */
#define LIST_SIZE 0x38
#define OBJECT_FLAGS 8
#define OBJECT_PAUSED 0x40000u
#define TASK_ACTIVE 4
#define MAX_TASKS 512
#define TIMER_COUNT 5     /* compteurs du jeu en temps reel, de 16 octets chacun ; octet +0xD = en marche */
#define TIMER_BASE 0x428
#define TIMER_MANAGER 0x68

static int frozen;
static nds_u32 tasks[MAX_TASKS];
static int task_count;

static void objects_paused(int on) {
    for (int list = 0; list < LISTS; list++)
        for (nds_u32 node = api->read32(OBJECTS + list * LIST_SIZE + 4); node; node = api->read32(node + 4)) {
            nds_u32 object = api->read32(node), flags = api->read32(object + OBJECT_FLAGS);
            api->write32(object + OBJECT_FLAGS, on ? flags | OBJECT_PAUSED : flags & ~OBJECT_PAUSED);
        }
}

/* Les taches coupees ici sont les seules remises en marche : le jeu peut en avoir arrete d'autres */
static void tasks_stop(void) {
    for (int list = 0; list < LISTS; list++)
        for (nds_u32 node = api->read32(TASKS + list * LIST_SIZE + 4); node; node = api->read32(node + 4)) {
            nds_u32 task = api->read32(node);
            if (api->read8(task + TASK_ACTIVE) && task_count < MAX_TASKS) {
                api->write8(task + TASK_ACTIVE, 0);
                tasks[task_count++] = task;
            }
        }
}

static void tasks_resume(void) {
    for (int list = 0; list < LISTS; list++)
        for (nds_u32 node = api->read32(TASKS + list * LIST_SIZE + 4); node; node = api->read32(node + 4)) {
            nds_u32 task = api->read32(node);
            for (int i = 0; i < task_count; i++)
                if (tasks[i] == task) {
                    api->write8(task + TASK_ACTIVE, 1);
                    break;
                }
        }
    task_count = 0;
}

/* Ces compteurs lisent l'horloge : sans ca, le temps du gel compterait d'un coup a la reprise */
static void timers_hold(void) {
    nds_u32 manager = api->read32(TIMERS + TIMER_MANAGER);
    if (!manager) return;
    nds_u32 now = (nds_u32)api->function_at(FUNC_GET_TICK)(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (int i = 0; i < TIMER_COUNT; i++) {
        nds_u32 timer = manager + TIMER_BASE + i * 16;
        if (api->read8(timer + 0xD)) api->write32(timer, now);
    }
}

static void thaw(void) {
    frozen = 0;
    tasks_resume();
    objects_paused(0);
    timers_hold();
}

static nds_u64 on_level_update(ARGS) {
    nds_u32 flags = api->read32(GAME_FLAGS);
    int want = (state & 2) && !(flags & (GAME_PAUSED | GAME_MULTI));
    if (!want) {
        if (frozen) thaw();
        return level_update_next(PASS);
    }
    frozen = 1;
    /* a chaque image : le niveau a pu creer des objets juste avant le gel */
    tasks_stop();
    objects_paused(1);
    timers_hold();
    return ((nds_u64)r1 << 32) | r0; /* la mise a jour du niveau ne tourne pas */
}

/* On quitte le niveau : ses objets et ses taches ne doivent pas rester arretes */
static nds_u64 on_level_exit(ARGS) {
    if (frozen) thaw();
    return level_exit_next(PASS);
}

NDS_MOD_EXPORT int nds_mod_load(const nds_api *a, nds_mod *mod) {
    api = a;
    self = mod;
    if (!NDS_API_HAS(a, game_function)) {
        a->fail(mod, "runtime trop ancien : game_function manque");
        return 1;
    }
    key_free = nds_setting_int(a, mod, "free", NDS_KEY_C);
    key_freeze = nds_setting_int(a, mod, "freeze", NDS_KEY_V);
    if (a->hook(mod, FUNC_CAMERA, (void *)on_camera, (void **)&camera_next) ||
        a->hook(mod, FUNC_LEVEL_UPDATE, (void *)on_level_update, (void **)&level_update_next) ||
        a->hook(mod, FUNC_LEVEL_EXIT, (void *)on_level_exit, (void **)&level_exit_next)) {
        a->fail(mod, "fonctions du jeu introuvables : ce n'est pas la version USA de Lego Star Wars II");
        return 1;
    }
    a->on_frame(mod, keys, 0);
    say("Free camera: press the free camera key (C by default). The freeze key (V) freezes the scene.", "Camera libre : appuyez sur la touche de la camera (C par defaut). La touche de gel (V) gele la scene.");
    return 0;
}
