# Nds Game Studio mods

The app's base mods. The launcher reads this repository when you open a game's **Modding** tab (Settings > Launcher > The app's mods) and lists the mods for that game. Install, turn off or remove them with one click.

*En français : les mods de base de l'app, listés dans l'onglet Moding de chaque jeu. Chaque mod a aussi son nom et sa description en français (`name_fr`, `description_fr`).*

## Layout: one folder per mod

```
nds-mods.json          list of the folders
<mod>/
  nds-mod.json         name, version, compatible games
  <GAME CODE>/         what only applies to this game
    native.json        features asked of the runtime
    fichiers/          game files in the export format (PNG, OBJ, WAV...)
    remplacements/     files in the game's format
```

A mod has one place, one version and one description, even for several games: to add a game, add its folder and its code in `games`.

`nds-mod.json`:

```json
{ "format": 1, "id": "my-mod", "name": "My mod", "name_fr": "Mon mod", "version": "1.0.0", "author": "...", "description": "...", "description_fr": "...", "games": ["AL7E"] }
```

`native.json`: `{ "features": ["interpolate"] }`. A mod only holds data: it asks for a feature the app's runtime already has and brings no code.

| Feature | Effect |
| --- | --- |
| `interpolate` | Extra frames between the game's frames |
| `network` | The game's wireless link over the PC's network |

## Native mods (format 2)

A mod can also bring code: a library per computer that only knows [`sdk/nds_mod.h`](sdk/nds_mod.h) (MIT), loaded by the game's runtime. The launcher says « Runs code on this computer » before installing it. First-person view, Free camera and Skip intro are such mods: their source is in `<mod>/src/`, their library in `<mod>/lib/<computer>/`.

```json
{ "format": 2, "id": "camera-libre", "version": "2.0.0", "type": "native", "api": "1.0", "games": { "AL7E": ["<rom SHA-1>"] },
  "plugin": { "windows-x86": "lib/windows-x86/camera_libre.dll" },
  "settings": [ { "key": "free", "name": "Free camera key", "type": "keybind", "default": "KeyC" } ] }
```

`./build.sh` compiles every `src/*.c` for Windows (32-bit `gcc -m32` and `ld`, no C library, like the runtime). `sdk/nds_util.h` has the small helpers the base mods share (sine, arc tangent, settings).

*En français : un mod peut aussi apporter du code, une bibliotheque par machine qui ne connait que `sdk/nds_mod.h`. Vue a la premiere personne, Camera libre et Passer l'intro en sont : sources dans `src/`, bibliotheque dans `lib/`. `./build.sh` les compile.*

## Mods

| Mod | Games |
| --- | --- |
| [60 frames per second](60-images-par-seconde) | Lego Star Wars II (AL7E) |
| [First-person view](premiere-personne) | Lego Star Wars II (AL7E) |
| [Network multiplayer](multijoueur) | Lego Star Wars II (AL7E) |
| [Skip intro](skip-intro) | Lego Star Wars II (AL7E) |
| [Free camera](camera-libre) | Lego Star Wars II (AL7E) |
