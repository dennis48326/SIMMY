"""PC mod: mods. The buy-mode Hobbies objects (easels, instruments, bookcases, telescopes, exercise
machines, chess and pool tables, the crafting stations, the playground and party games) for
pcobj_convert.py: which ones, what kind, which GameCube base, where the PC mesh goes over the base and
which of the base's own parts (the canvas and its painting, the chess pieces, the rocket) and dress-up props
the converted object keeps.

The GameCube keeps its skill objects on the Skill tab (catalogue function 0x100): the bookshelves (cheap,
expensive: read and study; 2x1), the easel (paint, sell the painting), the electric guitar (2x1: play,
listen), the upright and player pianos (2x2: play, listen), the chess tables (normal, alien: play, join,
cheer), the telescopes (normal, professional: stargaze, spy), the invention bench (2x1: make a rocket,
launch it), the weight bench (2x2: pump iron), the hamster wheel (3x2: run), the punching bag (2x1:
kickbox), the bonsai, the charisma mirror and the chicken checkers; its games (air hockey, foosball, the
driving range, the trampoline, ...) are on the Misc tab (0x40). Each skill object teaches one skill
(creativity: easel, guitar, pianos, bonsai; logic: chess, telescopes, chicken checkers; body: weight
bench, hamster wheel, punching bag; mechanical: invention bench; charisma: mirror; cooking, mechanical
and cleaning: the bookshelves' books).

All of these are animated objects (a character, state animations, effects). A converted one keeps the
base's character, states and effects (mod.ini animated = 1) and shows the PC mesh (anim_model) on the
base's body node, the PC mesh placed over the base's footprint so the base's sims (the base's routing
slots and animations) stand, sit and lie where the PC object has its seat, bench, keys or eyepiece.
The parts of the base its animations move or show that the PC mesh has no counterpart for (props: the
canvas and the painting growing on it, the rocket in its stages on the invention bench, the chess
pieces) are copied from the base's own model onto their nodes (mod.ini prop_nodes: the game draws those
nodes where the base's animation has them). What a sim holds (the guitar, the barbell, the brush and
palette, the air hockey paddle and puck, a book, the rocket parts) are dress-up props the base's trees
put on the sim: they stay; the ones that are the base's own body (the hamster wheel round the runner,
the punching bag) are left off (mod.ini hide_props).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_RATINGS

F_HOBBIES = 0x100          # PC: the Hobbies tab; GameCube: the Skill tab
F_GC_SKILL = 0x100
F_GC_MISC = 0x40
F_GC_DECO = 0x20
# Hobbies sub-sort (OBJD word 0x5E)
HOBBY_SUBSORTS = {1: 'creative', 2: 'knowledge', 4: 'exercise', 8: 'recreation', 0x80: 'misc', 0: 'none'}

# kinds left out, and why (inventory reason)
SKIP = {
    'mic': 'an open-mic stage (stand-up and singing): the GameCube has no singing or comedy object',
    'secretdoor': 'a secret door built into a wall (Apartment Life): a door on the GameCube, not a bookcase',
    'dancesphere': 'a 3x3 dance cage (Nightlife): the GameCube has no dance machine to base it on',
    'bingo': 'a party game played by a crowd (MYSHUNO): the GameCube has no such game',
    'bowling': 'a 2x12 bowling lane: the GameCube has no bowling',
    'logrolling': 'a 7x7 log rolling pool: the GameCube has nothing like it',
    'boardtable': 'a 3x3 game table for four seated sims (mahjong, poker, board game): the GameCube\'s only '
                  'table games are the 1x1 chess tables for two and its Misc games',
    'hoop': 'a 5x5 basketball court: the GameCube has no ball games of that kind',
    'soccer': 'a soccer goal and its field: the GameCube has no ball games of that kind',
    'rink': 'a skating rink (Seasons): the GameCube has no skating',
    'darts': 'a dartboard / axe throwing lane: the GameCube has no throwing games',
    'sauna': 'a sauna (a room with a door, Bon Voyage): the GameCube has nothing like it',
    'playground': 'a playground piece (swings, slide, monkey bars, lookout, merry-go-round): the GameCube\'s '
                  'nearest is its trampoline, whose bouncing does not fit these',
    'performer': 'a street performer\'s mat or podium (tips from passers-by): the GameCube has no street performers',
    'train': 'a 4x3 model train table: the GameCube has no such toy',
    'car': 'a car to restore (FreeTime tinkering): the GameCube has no cars',
    'massage': 'the massage table is converted with Electronics (and left out there: the GameCube\'s is a robot massage machine)',
    'hotspring': 'the hot spring is converted with Plumbing (and left out there: 6x6, the GameCube\'s largest hot tub is 3x3)',
}

HOBBY_BASES = {
    'easel': 'skill_creative_easel', 'piano': 'skill_creative_piano_upright', 'piano1': 'skill_creative_player_piano',
    'guitar': 'skill_creative_electric_guitar', 'bookcase': 'bookshelf_cheap', 'bookcase1': 'bookshelf_expensive',
    'telescope': 'skill_logic_telescope', 'telescope1': 'skill_logic_telescope_professional',
    'chess': 'skill_mental_chess_normal', 'chess1': 'skill_mental_chess_alien',
    'weights': 'skill_physical_weight_bench', 'cardio': 'skill_body_hamsterwheel', 'barre': 'skill_body_punching_bag',
    'workbench': 'skill_mechanical_invention_bench', 'pool': 'game_air_hockey',
    'antfarm': 'art_sculpture_wooden_model',
}

# where it sorts in the catalogue (mod folder prefix "pch<k>_")
HOBBY_SORT = {'easel': '1', 'craft': '1', 'piano': '2', 'piano_grand': '2', 'drums': '2', 'guitar': '2', 'bass': '2',
              'violin': '2', 'synth': '2', 'bookcase': '3', 'telescope': '4', 'chess': '4', 'weights': '5',
              'treadmill': '5', 'bike': '5', 'barre': '5', 'workbench': '6', 'pool': '7', 'antfarm': '8'}

# The bases: their group's tiles (GameCube sub x, y), the GameCube tab, the skill the base teaches, and
# the props: the base's nodes whose strips the converted object keeps from the base's own model (drawn
# where the base's animation puts them: mod.ini prop_nodes)
BASE_INFO = {
    'skill_creative_easel': {'tiles': [(0, 0)], 'tab': F_GC_SKILL, 'skill': 'creativity',
                             'props': list(range(2, 13))},
    'skill_creative_piano_upright': {'tiles': [(0, 0), (1, 0), (0, 1), (1, 1)], 'tab': F_GC_SKILL, 'skill': 'creativity'},
    'skill_creative_player_piano': {'tiles': [(0, 0), (1, 0), (0, 1), (1, 1)], 'tab': F_GC_SKILL, 'skill': 'creativity'},
    'skill_creative_electric_guitar': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_SKILL, 'skill': 'creativity'},
    'bookshelf_cheap': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_SKILL, 'skill': 'cooking / mechanical / cleaning (books)'},
    'bookshelf_expensive': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_SKILL, 'skill': 'cooking / mechanical / cleaning (books)'},
    'skill_logic_telescope': {'tiles': [(0, 0)], 'tab': F_GC_SKILL, 'skill': 'logic'},
    'skill_logic_telescope_professional': {'tiles': [(0, 0)], 'tab': F_GC_SKILL, 'skill': 'logic'},
    'skill_mental_chess_normal': {'tiles': [(0, 0)], 'tab': F_GC_SKILL, 'skill': 'logic', 'props': list(range(1, 12))},
    'skill_mental_chess_alien': {'tiles': [(0, 0)], 'tab': F_GC_SKILL, 'skill': 'logic'},
    'skill_physical_weight_bench': {'tiles': [(0, 0), (0, 1), (1, 0), (1, 1)], 'tab': F_GC_SKILL, 'skill': 'body'},
    'skill_body_hamsterwheel': {'tiles': [(x, y) for x in range(3) for y in range(2)], 'tab': F_GC_SKILL, 'skill': 'body',
                                'body': 4, 'hide_props': [0x8C0E134F]},   # prop_hamsterwheel: the runner wears the wheel
    'skill_body_punching_bag': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_SKILL, 'skill': 'body', 'body': 0,
                                'hide_props': [0x0D2D87FB]},   # prop_skill_physical_punching_bag: the bag the kicker hits
    'skill_mechanical_invention_bench': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_SKILL, 'skill': 'mechanical',
                                         'props': [3, 4, 5, 6, 7]},
    'game_air_hockey': {'tiles': [(x, y) for x in range(3) for y in range(2)], 'tab': F_GC_MISC, 'skill': ''},
    'art_sculpture_wooden_model': {'tiles': [(0, 0)], 'tab': F_GC_DECO, 'skill': '', 'static': True},
}

# The PC mesh over the base: a quarter turn (the PC's long axis onto the base's) and a shift (PC tile
# units, after the PC's tiles are centred in the base's footprint) so the base's sims use it where the PC
# object is used (measured in the game: the sim's spot against the PC seat, bench, keys, eyepiece)
TURN = {'treadmill': 3, 'bike': 3}
SHIFT = {'treadmill': (0.35, 0.0, 0.0), 'chess': (0.0, 0.0, -0.06)}
# the base's dress-up props (models its trees put on the person: TryChangeSuit) a kind leaves off (mod.ini
# hide_props), besides the base's own (BASE_INFO hide_props): a synthesizer's player wears no guitar
HIDE_PROPS = {'synth': [0xB31496D7]}      # prop_electric_guitar


def hobby_guids(cat):
    """Buy-mode Hobbies (catalogue function 0x100). The massage table and the hot spring are listed too
    (as converted elsewhere): the Electronics and Plumbing categories take them first."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_HOBBIES:
            out.append(g)
    return out


def hobby_kind(it):
    sg = (it.semiglobal or '').lower()
    name = it.objname.lower()
    if 'tablemassage' in sg:
        return 'massage'
    if 'hottub' in sg:
        return 'hotspring'
    if 'easel' in sg:
        return 'easel'
    if 'music' in sg:
        if 'piano - grand' in name:
            return 'piano_grand'
        if 'piano' in name:
            return 'piano'
        if 'drum' in name:
            return 'drums'
        if 'bass' in name:
            return 'bass'
        if 'guitar' in name:
            return 'guitar'
        if 'violin' in name:
            return 'violin'
        if 'synth' in name:
            return 'synth'
        if 'mic' in name:
            return 'mic'
        return ''
    if 'bookcase' in sg:
        return 'secretdoor' if 'secret door' in name else 'bookcase'
    if 'telescope' in sg:
        return 'telescope'
    if 'exercisemachine' in sg:
        return 'treadmill' if 'treadmill' in name else 'weights'
    if 'chess' in sg:
        return 'chess'
    if 'pooltable' in sg:
        return 'pool'
    if 'crafting' in sg:
        return 'workbench' if 'robot' in name else 'craft'
    if 'dartboard' in sg:
        return 'darts'
    if 'rink' in sg:
        return 'rink'
    if 'junglegym' in sg or 'swing' in name:
        return 'playground'
    if 'pokertable' in sg or 'mahjong' in name or 'board game' in name:
        return 'boardtable'
    if 'soccer' in sg:
        return 'soccer'
    if 'car' in sg and 'restorable' in name:
        return 'car'
    if 'rugglobals' in sg or 'human statue' in name:
        return 'performer'
    if 'door' in sg and 'sauna' in name:
        return 'sauna'
    for key, kind in (('exercise bike', 'bike'), ('ballet bar', 'barre'), ('electro dance', 'dancesphere'),
                      ('bingo', 'bingo'), ('bowling', 'bowling'), ('log rolling', 'logrolling'),
                      ('basketball', 'hoop'), ('train table', 'train'), ('ant farm', 'antfarm')):
        if key in name:
            return kind
    return ''


def hobby_base(it, kind):
    """The GameCube base by function, and for tiered kinds by the PC price."""
    if kind in ('piano', 'drums'):
        return HOBBY_BASES['piano']
    if kind == 'piano_grand':
        return HOBBY_BASES['piano1']          # the dearer PC piano: the dearer GameCube one
    if kind in ('guitar', 'bass', 'violin', 'synth'):
        return HOBBY_BASES['guitar']
    if kind == 'bookcase':
        return HOBBY_BASES['bookcase1'] if it.price >= 700 else HOBBY_BASES['bookcase']
    if kind == 'telescope':
        return HOBBY_BASES['telescope1'] if it.price >= 1000 else HOBBY_BASES['telescope']
    if kind == 'chess':
        return HOBBY_BASES['chess1'] if it.price >= 1000 else HOBBY_BASES['chess']
    if kind in ('treadmill', 'bike'):
        return HOBBY_BASES['cardio']
    if kind == 'craft':
        return HOBBY_BASES['easel']
    return HOBBY_BASES.get(kind, '')


def describe(kind):
    return {'easel': 'easel', 'craft': 'crafting station (paints as an easel)', 'piano': 'piano', 'piano_grand': 'grand piano',
            'drums': 'drum kit (played as a piano)', 'guitar': 'guitar', 'bass': 'bass guitar', 'violin': 'violin',
            'synth': 'synthesizer', 'bookcase': 'bookcase', 'telescope': 'telescope', 'chess': 'chess table',
            'weights': 'weight machine', 'treadmill': 'treadmill', 'bike': 'exercise bike', 'barre': 'ballet barre',
            'workbench': 'robot workbench', 'pool': 'pool table (air hockey)', 'antfarm': 'ant farm (decoration)'}.get(kind, kind)


def model_order(models):
    """The object's own model first, not a sim's accessory (a guitar or violin the PC sim holds)."""
    good = [m for m in models if not m.lower().startswith('accessory')]
    return good + [m for m in models if m not in good]


def is_dropped(subset, mat):
    """Not drawn: the PC's masks and helper planes, a flashlight's beam."""
    s = f'{subset} {mat}'.lower()
    return s.endswith('_null') or '_null ' in s + ' ' or 'flashlight' in s


def tab_of(base):
    return BASE_INFO.get(base, {}).get('tab', F_GC_SKILL)


def mesh_tiles(meshes):
    """The PC tiles the mesh covers (x, y count from the first tile)."""
    P = np.concatenate([m[2] for m in meshes])
    lo, hi = P.min(0), P.max(0)
    return (max(1, int(np.floor(hi[0] + 0.4)) - int(np.floor(lo[0] + 0.6)) + 1),
            max(1, int(np.floor(hi[1] + 0.4)) - int(np.floor(lo[1] + 0.6)) + 1),
            int(np.floor(lo[0] + 0.6)), int(np.floor(lo[1] + 0.6)))


def turn_quarter(meshes, n):
    """Turned n quarters about z (PC space, about the origin)."""
    out = []
    for m in meshes:
        P, N = m[2].copy(), m[3].copy()
        for _ in range(n % 4):
            P = np.stack([-P[:, 1], P[:, 0], P[:, 2]], 1)
            N = np.stack([-N[:, 1], N[:, 0], N[:, 2]], 1)
        out.append((m[0], m[1], P, N) + tuple(m[4:]))
    return out


def place(meshes, kind, W, H):
    """The PC mesh over the base's W x H footprint (PC space, the first tile's middle at 0): turned onto
    the base's axis, the tiles it covers centred in the footprint, then the kind's shift. Returns
    (meshes, (pw, ph) covered, note)."""
    t = TURN.get(kind, 0)
    if t:
        meshes = turn_quarter(meshes, t)
    pw, ph, x0, y0 = mesh_tiles(meshes)
    dx, dy = (W - pw) / 2.0 - x0, (H - ph) / 2.0 - y0
    sx, sy, sz = SHIFT.get(kind, (0.0, 0.0, 0.0))
    d = np.array([dx + sx, dy + sy, sz])
    out = [(m[0], m[1], m[2] + d) + tuple(m[3:]) for m in meshes]
    note = f'{pw}x{ph} (mesh) on the base\'s {W}x{H}' + (', turned a quarter' if t else '')
    return out, (pw, ph), note


def hide_props(base, kind):
    return list(BASE_INFO.get(base, {}).get('hide_props', ())) + HIDE_PROPS.get(kind, [])


def skill_of(base):
    return BASE_INFO.get(base, {}).get('skill', '')
