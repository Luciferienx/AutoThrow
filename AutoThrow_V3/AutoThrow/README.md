# AUTO THROW

Reverb "throw" automatique pour voix. Le plugin écoute la voix : tant que tu rappes, la reverb s'efface ;
dès que tu t'arrêtes, elle s'ouvre en grand et remplit le trou. Plus besoin de dessiner des automations de send.

## Les knobs

**REVERB**
- **ROOM / CHAMBER / PLATE / HALL** : le type de pièce
  - ROOM : petite pièce, rebonds audibles, son proche et sombre (DECAY conseillé : 0,5-2 s)
  - CHAMBER : chambre d'écho de studio, chaude et dense (1,5-4 s)
  - PLATE : plaque, ultra dense et brillante, le classique des voix (2-5 s)
  - HALL : grande salle large et aérée, le son de la V1 (4 s et +)
- **DECAY** : longueur de la queue (0,3 à 30 s)
- **SHIMMER** : octave +12 qui monte dans la queue (effet aérien, angélique)
- **TONE** : couleur de la reverb, sombre à gauche, brillante à droite
- **MIX** : volume du throw

**DETECTION / THROW**
- **SENS** : seuil de détection de la voix. Le trait blanc sur la barre IN le montre. Si le throw part en pleine phrase, baisse SENS
- **GAP** : silence minimum avant que le throw parte. Court (50-100 ms) = throws entre les mots ; long (300 ms et +) = seulement en fin de phrase
- **DUCK** : combien la reverb s'efface pendant que tu rappes. 100 % = voix totalement sèche
- **BLOOM** : vitesse d'ouverture du throw. Court = ça claque, long = ça monte doucement

**Boutons**
- **FREEZE** : gèle la queue à l'infini. À automatiser dans FL sur un mot ou une fin de mesure
- **WET ONLY** : coupe la voix sèche, à activer si le plugin est sur un bus en send

L'anneau à droite indique l'état : **VOICE** (voix détectée, reverb baissée), **THROW** (reverb ouverte), **FROZEN**.

## Où le mettre dans FL Studio

- **En insert** directement sur la piste voix (WET ONLY éteint) : le plus simple
- **Sur un bus en send** (WET ONLY allumé, MIX à fond) : pour mettre un EQ, un comp ou de la saturation après la reverb

Attention : la détection écoute ce qui arrive dans le plugin. Sur un bus en send, si le send est automatisé ou baissé, la détection change aussi.

## Compiler le VST3 (Windows)

### Option A : GitHub, sans rien installer (recommandé)

1. Crée un compte sur github.com puis un nouveau dépôt (bouton **New**), par exemple `AutoThrow`
2. Clique **uploading an existing file** et glisse tout le contenu du dossier (CMakeLists.txt, Source, test, README.md)
3. Le dossier caché `.github` ne passe pas toujours en glisser-déposer. Si besoin : **Add file → Create new file**, tape le nom
   `.github/workflows/build.yml` et colle dedans le contenu du fichier du même nom
4. Onglet **Actions** : le build se lance tout seul (environ 10 min la première fois)
5. Quand c'est vert, clique sur le build puis, en bas, télécharge **AutoThrow-VST3-Windows**
6. Dézippe et copie le dossier `AutoThrow.vst3` dans `C:\Program Files\Common Files\VST3\`
7. Dans FL : **Options → Manage plugins → Find plugins**, puis cherche AutoThrow

### Option B : sur ton PC

1. Installe **Visual Studio 2022 Community** (charge de travail "Développement Desktop en C++") et **CMake**
2. Dans un terminal, dans le dossier du projet :
   ```
   cmake -B build -A x64
   cmake --build build --config Release --target AutoThrow_VST3
   ```
3. Le plugin est dans `build/AutoThrow_artefacts/Release/VST3/AutoThrow.vst3`

## Structure du code

- `Source/AutoThrowEngine.h` : tout le son (détection, ducking, reverb FDN 8 lignes, shimmer, freeze). Pur C++, sans JUCE
- `Source/PluginProcessor.*` : paramètres et lien avec le DAW
- `Source/PluginEditor.*` : l'interface
- `test/render_test.cpp` : test hors DAW qui vérifie le ducking, la détection et la stabilité du freeze

## Licence JUCE

Le plugin utilise JUCE 9. Pour un usage perso, aucun souci. Si un jour tu veux le vendre ou le distribuer,
il faut soit publier le code source (licence AGPLv3), soit prendre une licence JUCE (gratuite sous un certain chiffre d'affaires).
Vérifie les conditions à jour sur juce.com à ce moment-là.
