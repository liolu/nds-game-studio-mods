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
| `first_person` | Mouse-driven camera in the character's eyes (games described in the studio's `native/games/patches.json`) |
| `network` | The game's wireless link over the PC's network |
| `skip_intro` | Starts on the game's menu |

## Mods

| Mod | Games |
| --- | --- |
| [60 frames per second](60-images-par-seconde) | Lego Star Wars II (AL7E) |
| [First-person view](premiere-personne) | Lego Star Wars II (AL7E) |
| [Network multiplayer](multijoueur) | Lego Star Wars II (AL7E) |
| [Skip intro](skip-intro) | Lego Star Wars II (AL7E) |
