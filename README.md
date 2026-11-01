# Mods de Nds Game Studio

Les mods de base de l'app. Le launcher lit ce dépôt quand on ouvre l'onglet **Moding** d'un jeu (Options > Launcher > Les mods de l'app) et montre ceux qui vont avec ce jeu ; on les installe en un clic, et on les coupe ou on les retire de la même façon.

## Rangement : un dossier par mod

```
nds-mods.json          la liste des dossiers
<mod>/
  nds-mod.json         nom, version, jeux compatibles
  <CODE DU JEU>/       ce qui ne vaut que pour ce jeu
    native.json        fonctions demandées au runtime
    fichiers/          fichiers du jeu au format de l'export (PNG, OBJ, WAV...)
    remplacements/     fichiers au format du jeu
```

Un mod a un seul endroit, une seule version et une seule description, même s'il va avec plusieurs jeux : ajouter un jeu, c'est ajouter son dossier et son code dans `games`.

`nds-mod.json` :

```json
{ "format": 1, "id": "mon-mod", "name": "Mon mod", "version": "1.0.0", "author": "...", "description": "...", "games": ["AL7E"] }
```

`native.json` : `{ "features": ["interpolate"] }`. Un mod ne contient que des données : il demande une fonction que le runtime de l'app possède déjà, et n'apporte aucun code.

| Fonction | Effet |
| --- | --- |
| `interpolate` | Images intermédiaires entre celles du jeu |
| `first_person` | Caméra dans les yeux du personnage (jeux décrits dans `native/games/patches.json` du studio) |
| `network` | Liaison sans fil du jeu passée par le réseau du PC |

## Les mods

| Mod | Jeux |
| --- | --- |
| [60 images par seconde](60-images-par-seconde) | Lego Star Wars II (AL7E) |
| [Vue à la première personne](premiere-personne) | Lego Star Wars II (AL7E) |
| [Multijoueur en réseau](multijoueur) | Lego Star Wars II (AL7E) |
