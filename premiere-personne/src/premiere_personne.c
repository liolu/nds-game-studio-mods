/* Vue a la premiere personne pour Lego Star Wars II (AL7E) : la camera du jeu devient les yeux du joueur, et la souris la dirige.
 * Mod natif : il ne connait que sdk/nds_mod.h. C'est l'essai de l'interface moteur : tout ce que faisait le runtime passe ici par
 * les accroches et les rappels (on_gx, on_polygon, on_present, on_io_read).
 *
 * Le jeu construit sa matrice de vue depuis une position et trois axes : l'accroche de la fonction qui calcule la vue les remplace
 * juste avant le calcul. Le corps du joueur est cache, ses bras restent, leves et poses en bas de l'image ; un viseur est dessine.
 * SPDX-License-Identifier: MIT */
#include "../../sdk/nds_util.h"

/* Lego Star Wars II, version USA */
#define CAMERA 0x02109cd4u
#define PLAYERS 0x02109358u
#define PLAYER_INDEX 0x02102822u
#define CAMERA_YAW 0x02109ccau    /* cap de la camera du jeu, qui oriente les touches de direction */
#define BODY_MATRIX 0x8c          /* matrice du personnage : axes X, Y, Z (son avant), puis position */
#define HEADING_A 0xca
#define HEADING_B 0x6b4
#define RENDER_OBJECT 0xf4
#define ARM_SLOTS 0xff0u          /* emplacements de la pile de matrices qui portent les os des bras */
#define FUNC_CAMERA 0x020ace50u
#define FUNC_CAMERA_YAW 0x020aaeb4u
#define FUNC_MODEL_SHAPE 0x01ffbe94u

#define CAM_FLAGS 0x160
#define CAM_ROTATION 0x14
#define VIEW_DIRTY 0xCu
#define EYE_HEIGHT 5.6f
#define EYE_FORWARD 0.9f
#define RADIANS_PER_COUNT 0.0025f
#define MAX_PITCH 1.3f
#define KEYINPUT 0x04000130u
#define PAD_L (1u << 9)

static const nds_api *api;
static nds_mod *self;
static nds_fn camera_next, yaw_next, shape_next;
static float yaw, pitch;
static int started;
static nds_u32 camera_seen; /* image de la derniere vue calculee par nous : une autre accroche (camera libre) peut passer avant */
static nds_input in;
static volatile int show_crosshair;

#define ARGS nds_u32 r0, nds_u32 r1, nds_u32 r2, nds_u32 r3, nds_u32 a0, nds_u32 a1, nds_u32 a2, nds_u32 a3, nds_u32 a4, nds_u32 a5, nds_u32 a6, nds_u32 a7, nds_u32 a8, nds_u32 a9
#define PASS r0, r1, r2, r3, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9

static nds_u32 frame_now(void) {
    nds_u32 *frame = api->runtime("nds_frame");
    return frame ? *frame : 0;
}

/* la vue est la notre : nous l'avons calculee il y a peu. Le jeu la refait en general toutes les deux images, moins souvent quand
 * il ralentit ; une demi-seconde sans elle veut dire qu'une autre accroche (camera libre) passe avant. */
#define OURS_FRAMES 30
static int ours(void) { return started && frame_now() - camera_seen <= OURS_FRAMES; }

static nds_u32 player(void) { return api->read32(PLAYERS + api->read8(PLAYER_INDEX) * 4); }

/* ---- la camera */
static nds_u64 on_camera(ARGS) {
    static int seen_x, seen_y;
    if (r0 != CAMERA) return camera_next(PASS);
    nds_u32 hero = player();
    if (!hero) {
        started = 0;
        return camera_next(PASS);
    }
    const nds_s32 *body = (const nds_s32 *)api->memory(hero + BODY_MATRIX);
    if (!ours()) {
        /* nouveau personnage, nouveau niveau, ou retour de la camera libre : le regard part dans la direction du personnage */
        started = 1;
        yaw = nds_arc_tangent(nds_real(body[6]), nds_real(body[8]));
        pitch = 0;
        seen_x = in.mouse_dx;
        seen_y = in.mouse_dy;
    }
    camera_seen = frame_now();
    yaw = nds_wrap_angle(yaw - (float)(in.mouse_dx - seen_x) * RADIANS_PER_COUNT);
    pitch -= (float)(in.mouse_dy - seen_y) * RADIANS_PER_COUNT;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;
    if (pitch < -MAX_PITCH) pitch = -MAX_PITCH;
    seen_x = in.mouse_dx;
    seen_y = in.mouse_dy;
    float look_x = nds_sine(yaw), look_z = nds_cosine(yaw), up = nds_sine(pitch), level = nds_cosine(pitch);
    nds_s32 *pos = (nds_s32 *)api->memory(CAMERA);
    pos[0] = body[9] + nds_fx(look_x * EYE_FORWARD);
    pos[1] = body[10] + nds_fx(EYE_HEIGHT);
    pos[2] = body[11] + nds_fx(look_z * EYE_FORWARD);
    nds_s32 *axes = pos + CAM_ROTATION / 4;
    axes[0] = nds_fx(-look_z); axes[1] = 0; axes[2] = nds_fx(look_x); axes[3] = 0;
    axes[4] = nds_fx(-up * look_x); axes[5] = nds_fx(level); axes[6] = nds_fx(-up * look_z); axes[7] = 0;
    axes[8] = nds_fx(-look_x * level); axes[9] = nds_fx(-up); axes[10] = nds_fx(-look_z * level); axes[11] = 0;
    axes[12] = 0; axes[13] = 0; axes[14] = 0; axes[15] = 0x1000;
    api->write32(CAMERA + CAM_FLAGS, api->read32(CAMERA + CAM_FLAGS) | VIEW_DIRTY);
    return camera_next(PASS);
}

/* Le jeu oriente les touches de direction d'apres le cap de sa propre camera : on lui donne le notre. Sur le DS le cap vise a
 * l'oppose du regard. Le personnage regarde aussi ou regarde la camera : l'arme vise au centre de l'image. */
static nds_u64 on_camera_yaw(ARGS) {
    if (ours()) {
        nds_s32 angle = (nds_s32)(yaw / (2 * NDS_PI) * 65536.0f);
        api->write16(CAMERA_YAW, (nds_u16)(angle + 0x8000));
        nds_u32 hero = player();
        if (hero) {
            api->write16(hero + HEADING_A, (nds_u16)angle);
            api->write16(hero + HEADING_B, (nds_u16)angle);
        }
    }
    return yaw_next(PASS);
}

/* ---- le modele du joueur : 0 = rien, 1 = cache, 2 = seuls les bras restent, 3 = petit modele, place comme les bras s'il est dans la main */
#define SHAPE_TORSO 2
#define MODEL_NODE_COUNT 0x17
#define MODEL_MAX_NODES_OF_WEAPON 8
static int mode;

static nds_u64 on_model_shape(ARGS) {
    int before = mode;
    if (ours()) {
        nds_u32 hero = player(), object = api->read32(r0 + 4);
        if (hero && object == hero + RENDER_OBJECT) mode = api->read8(api->read32(r0) + 1) == SHAPE_TORSO ? 2 : 1;
        else if (api->read8(api->read32(object + 4) + MODEL_NODE_COUNT) <= MODEL_MAX_NODES_OF_WEAPON) mode = 3;
    }
    nds_u64 r = shape_next(PASS);
    mode = before;
    return r;
}

/* ---- ce que le moteur 3D fait des matrices : la projection, et l'emplacement de matrice de chaque sommet */
typedef struct { float m[16]; } Mtx; /* au sens du DS (vecteurs en ligne), en unites et non en 4096e */
static Mtx proj, proj_stack[2];
static int mtx_mode, proj_sp, cur_slot;
static int slots[8];
static unsigned vertex_count;

static void m_id(Mtx *m) {
    for (int i = 0; i < 16; i++) m->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

static void m_mul(Mtx *out, const Mtx *a, const Mtx *b) {
    Mtx r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a->m[i * 4 + k] * b->m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    *out = r;
}

static void from_params(Mtx *m, const nds_u32 *p, int rows, int cols) {
    m_id(m);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) m->m[i * 4 + j] = (float)(nds_s32)p[i * cols + j] / 4096.0f;
}

static int on_gx(void *user, nds_u32 cmd, nds_u32 *p, nds_u32 count) {
    Mtx m, t;
    if (cmd >= 0x23 && cmd <= 0x28) {
        slots[vertex_count++ & 7] = cur_slot;
        return 0;
    }
    if (cmd == 0x10) {
        mtx_mode = p[0] & 3;
        return 0;
    }
    if (cmd == 0x14 && mtx_mode != 0 && mtx_mode != 3) {
        cur_slot = p[0] & 31;
        return 0;
    }
    if (mtx_mode != 0) return 0; /* seule la projection compte ici */
    switch (cmd) {
    case 0x11: proj_stack[proj_sp++ & 1] = proj; break;
    case 0x12: proj = proj_stack[--proj_sp & 1]; break;
    case 0x13: proj_stack[0] = proj; break;
    case 0x14: proj = proj_stack[0]; break;
    case 0x15: m_id(&proj); break;
    case 0x16: from_params(&proj, p, 4, 4); break;
    case 0x17: from_params(&proj, p, 4, 3); break;
    case 0x18: from_params(&m, p, 4, 4); m_mul(&t, &m, &proj); proj = t; break;
    case 0x19: from_params(&m, p, 4, 3); m_mul(&t, &m, &proj); proj = t; break;
    case 0x1A: from_params(&m, p, 3, 3); m_mul(&t, &m, &proj); proj = t; break;
    case 0x1B: m_id(&m); m.m[0] = (nds_s32)p[0] / 4096.0f; m.m[5] = (nds_s32)p[1] / 4096.0f; m.m[10] = (nds_s32)p[2] / 4096.0f; m_mul(&t, &m, &proj); proj = t; break;
    case 0x1C: m_id(&m); m.m[12] = (nds_s32)p[0] / 4096.0f; m.m[13] = (nds_s32)p[1] / 4096.0f; m.m[14] = (nds_s32)p[2] / 4096.0f; m_mul(&t, &m, &proj); proj = t; break;
    }
    return 0;
}

/* inverse d'une 4x4 (cofacteurs) ; 0 si elle ne s'inverse pas */
static int m_inverse(Mtx *out, const Mtx *in_m) {
    const float *m = in_m->m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det > -1e-12f && det < 1e-12f) return 0;
    for (int i = 0; i < 16; i++) out->m[i] = inv[i] / det;
    return 1;
}

/* point (ligne) fois matrice */
static void row_mul(float out[4], const float v[4], const Mtx *m) {
    for (int j = 0; j < 4; j++) out[j] = v[0] * m->m[j] + v[1] * m->m[4 + j] + v[2] * m->m[8 + j] + v[3] * m->m[12 + j];
}

/* Sous la camera et pas devant : seul ce que tient le joueur peut etre la */
static int in_hand(const float *view) { return view[0] > -4.5f && view[0] < 4.5f && view[1] > -5.0f && view[1] < -0.5f && view[2] > -1.0f && view[2] < 3.5f; }

/* Les bras du modele pendent le long du corps et sont sous la camera : on annule la pente de la camera (ils la suivent), on les leve
 * vers l'avant autour de l'epaule, puis on les pose en bas de l'image. Unites du jeu, espace de la vue (la camera regarde vers -z). */
#define ARM_SHOULDER_Z 1.0f
#define ARM_RAISE_SIN 0.94f
#define ARM_RAISE_COS 0.34f
#define ARM_SPREAD 0.8f
#define ARM_X 0.0f
#define ARM_Y -1.15f
#define ARM_Z -0.8f

static void hold_arms(float *view) {
    float s = nds_sine(pitch), c = nds_cosine(pitch);
    float y = view[1] * c + view[2] * s, z = view[2] * c - view[1] * s - ARM_SHOULDER_Z;
    view[0] = view[0] * ARM_SPREAD + ARM_X;
    view[1] = y * ARM_RAISE_COS - z * ARM_RAISE_SIN + ARM_Y;
    view[2] = z * ARM_RAISE_COS + y * ARM_RAISE_SIN + ARM_Z;
}

static int on_polygon(void *user, nds_vertex *v, int n, nds_u32 attr, nds_u32 tex, nds_u32 pal) {
    if (mode == 0) return 0;
    if (mode == 1) return 1; /* tete et jambes du joueur : cachees */
    if (mode == 2)
        for (int i = 0; i < n; i++)
            if (!(ARM_SLOTS >> slots[(vertex_count - 1 - i) & 7] & 1)) return 1; /* le torse ne garde que les bras */
    Mtx inverse;
    if (!m_inverse(&inverse, &proj)) return 0;
    for (int i = 0; i < n; i++) {
        float clip[4] = {v[i].x, v[i].y, v[i].z, v[i].w}, view[4];
        row_mul(view, clip, &inverse);
        if (mode == 3 && !in_hand(view)) continue;
        hold_arms(view);
        view[3] = 1.0f;
        row_mul(clip, view, &proj);
        v[i].x = clip[0];
        v[i].y = clip[1];
        v[i].z = clip[2];
        v[i].w = clip[3];
    }
    return 0;
}

/* ---- un viseur, en couleur inverse pour rester visible sur tous les decors, sur l'ecran du moteur 3D */
static void on_present(void *user, nds_u32 *top, nds_u32 *bottom, int w, int h, int stride) {
    if (!show_crosshair) return;
    nds_u32 pow = *(const nds_u16 *)api->memory(0x04000304u); /* lu sans effet de bord : on est dans le fil de dessin */
    nds_u32 *screen = (pow >> 15) ? top : bottom;
    int k = h / 192 > 0 ? h / 192 : 1;
    int cx = w / 2, cy = h / 2, gap = 2 * k, arm = 5 * k, t = k > 1 ? k / 2 : 1;
    for (int i = gap; i < gap + arm; i++)
        for (int j = 0; j < t; j++) {
            int points[4][2] = {{cx + i, cy + j}, {cx - i, cy + j}, {cx + j, cy + i}, {cx + j, cy - i}};
            for (int q = 0; q < 4; q++) {
                int x = points[q][0], y = points[q][1];
                if (x >= 0 && y >= 0 && x < w && y < h) screen[y * stride + x] ^= 0x00FFFFFFu;
            }
        }
}

/* ---- le clic gauche tire (bouton L), curseur retenu : KEYINPUT est actif a 0 */
static void on_io_read(void *user, nds_u32 addr, nds_u32 size, nds_u32 *value) {
    if (addr == KEYINPUT && ours() && (in.window & 1) && (in.mouse_buttons & 1)) *value &= ~PAD_L;
}

static void each_frame(void *user, nds_u32 frame) {
    in.size = sizeof in;
    api->input(&in);
    show_crosshair = ours();
}

NDS_MOD_EXPORT int nds_mod_load(const nds_api *a, nds_mod *mod) {
    api = a;
    self = mod;
    if (!NDS_API_HAS(a, game_function)) {
        a->fail(mod, "runtime trop ancien");
        return 1;
    }
    m_id(&proj);
    if (a->hook(mod, FUNC_CAMERA, (void *)on_camera, (void **)&camera_next) ||
        a->hook(mod, FUNC_CAMERA_YAW, (void *)on_camera_yaw, (void **)&yaw_next) ||
        a->hook(mod, FUNC_MODEL_SHAPE, (void *)on_model_shape, (void **)&shape_next)) {
        a->fail(mod, "fonctions du jeu introuvables : ce n'est pas la version USA de Lego Star Wars II");
        return 1;
    }
    a->on_frame(mod, each_frame, 0);
    a->on_gx(mod, on_gx, 0);
    a->on_polygon(mod, on_polygon, 0);
    a->on_present(mod, on_present, 0);
    a->on_io_read(mod, on_io_read, 0);
    a->capture_mouse(mod, 1);
    a->notice(mod, nds_lang_fr(a) ? "La souris dirige la camera. F7 libere le curseur." : "The mouse moves the camera. F7 frees the cursor.", 3000);
    return 0;
}
