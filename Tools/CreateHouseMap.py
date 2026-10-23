"""
Builds the M4 whitebox house map /Game/Head/Maps/Lvl_House from the house block-out floor plan.

Run with the editor closed, after Tools/CreateGameData.py:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<path>/Tools/CreateHouseMap.py"

Everything is placed as normal actors (walls, floors, stairs, lights, objects, the '방' zone, the player start),
so the map can be edited by hand afterwards. Running the script again rebuilds the map from scratch.

Plan coordinates are meters from the north-west corner of the 1층 창고 (x east, y south; north is -Y).
The 1층 is 22m x 12m (창고/차고 column 6m + 본채 16m), the 2층 covers only the 본채, 앞마당 is south of both.
"""

import unreal

MAP_PATH = "/Game/Head/Maps/Lvl_House"
DATA_PATH = "/Game/Head/Data/"
MATERIAL_PATH = "/Game/Head/Materials"

FLOOR_HEIGHT = 300.0
WALL = 20.0            # wall and slab thickness
DOOR_WIDTH = 120.0

FLOOR_Z = {"1": 0.0, "2": FLOOR_HEIGHT}

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
BASE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")


def rect(x0, y0, x1, y1):
    """Plan meters -> cm."""
    return (x0 * 100.0, y0 * 100.0, x1 * 100.0, y1 * 100.0)


# rooms: name -> (floor, rect in cm)
ROOMS = {
    # 1층
    "창고": ("1", rect(0, 0, 6, 4)),
    "차고": ("1", rect(0, 4, 6, 12)),
    "부엌": ("1", rect(6, 0, 12, 5)),
    "식당": ("1", rect(12, 0, 17, 5)),
    "세탁실": ("1", rect(17, 0, 20, 5)),
    "화장실": ("1", rect(20, 0, 22, 5)),
    "중앙 복도": ("1", rect(6, 5, 22, 7)),
    "거실": ("1", rect(6, 7, 13, 12)),
    "현관": ("1", rect(13, 7, 16, 12)),
    "작업실": ("1", rect(16, 7, 20, 12)),
    "1층 계단": ("1", rect(20, 7, 22, 12)),
    "앞마당": ("1", rect(0, 12, 22, 24)),
    # 2층
    "안방": ("2", rect(6, 0, 13, 5)),
    "샤워 화장실": ("2", rect(13, 0, 17, 5)),
    "린넨실": ("2", rect(17, 0, 20, 5)),
    "계단참": ("2", rect(20, 0, 22, 7)),
    "2층 복도": ("2", rect(6, 5, 20, 7)),
    "내 방": ("2", rect(6, 7, 13, 12)),
    "빈방": ("2", rect(13, 7, 20, 12)),
    "2층 계단": ("2", rect(20, 7, 22, 12)),
}

# 1층 rooms with nothing above them get their own roof
SINGLE_STOREY = ("창고", "차고")

# openings between rooms: (room a, room b, center along the shared edge in m or None for the middle, width in cm).
# Shared edges not listed here are solid walls.
DOORS = [
    # 1층
    ("창고", "차고", 3.0, DOOR_WIDTH),
    ("차고", "중앙 복도", None, DOOR_WIDTH),
    ("부엌", "식당", 2.5, DOOR_WIDTH),
    ("부엌", "중앙 복도", 9.0, DOOR_WIDTH),
    ("식당", "중앙 복도", 14.5, DOOR_WIDTH),
    ("세탁실", "중앙 복도", 18.5, DOOR_WIDTH),
    ("화장실", "중앙 복도", 21.0, DOOR_WIDTH),
    ("중앙 복도", "거실", 9.5, 200.0),
    ("중앙 복도", "현관", 14.5, DOOR_WIDTH),
    ("중앙 복도", "작업실", 18.0, DOOR_WIDTH),
    ("중앙 복도", "1층 계단", 20.55, 90.0),    # onto the walkway beside the stairs
    ("거실", "현관", 9.5, DOOR_WIDTH),
    ("현관", "작업실", 9.8, DOOR_WIDTH),
    ("현관", "앞마당", 14.5, DOOR_WIDTH),
    ("차고", "앞마당", 3.0, 280.0),            # garage door
    # 2층
    ("안방", "2층 복도", 9.5, DOOR_WIDTH),
    ("샤워 화장실", "2층 복도", 15.0, DOOR_WIDTH),
    ("린넨실", "2층 복도", 18.5, DOOR_WIDTH),
    ("2층 복도", "계단참", 6.0, DOOR_WIDTH),
    ("내 방", "2층 복도", 9.5, DOOR_WIDTH),
    ("빈방", "2층 복도", 16.5, DOOR_WIDTH),
    ("계단참", "2층 계단", 21.45, 90.0),       # off the top of the stairs
]

# the locked door (창고 열쇠) fills the opening between these two rooms
LOCKED_DOOR = ("차고", "창고")

# stairs: 1층 계단 → 2층 계단. The stairwell is 2m wide: a 90cm walkway runs south along its west half from the
# 복도 door, the stairs climb north along the east half and end at the door to 계단참.
STAIRS = (2100.0, 740.0, 2190.0, 1190.0)
STEPS = 12

HOLES = {
    "2": [STAIRS],    # 2층 floor above the stairs
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


# --- materials --------------------------------------------------------------

def material(name, color):
    path = f"{MATERIAL_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.EditorAssetLibrary.load_asset(path)
    else:
        mi = asset_tools.create_asset(name, MATERIAL_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", BASE_MATERIAL)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1.0))
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


MAT_FLOOR = {"1": material("MI_House_Floor1", (0.45, 0.4, 0.33)),
             "2": material("MI_House_Floor2", (0.5, 0.45, 0.5))}
MAT_GROUND = material("MI_House_Ground", (0.3, 0.3, 0.3))
MAT_WALL = material("MI_House_Wall", (0.82, 0.8, 0.75))
MAT_ROOF = material("MI_House_Roof", (0.35, 0.25, 0.22))
MAT_STAIRS = material("MI_House_Stairs", (0.6, 0.45, 0.3))


# --- helpers ------------------------------------------------------------------

def box(label, folder, center, size, mat):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    component = actor.static_mesh_component
    component.set_static_mesh(CUBE)
    component.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def subtract_hole(r, hole):
    """Splits rect r around hole (both x0, y0, x1, y1)."""
    x0, y0, x1, y1 = r
    hx0, hy0, hx1, hy1 = hole
    if hx1 <= x0 or hx0 >= x1 or hy1 <= y0 or hy0 >= y1:
        return [r]
    hx0, hy0, hx1, hy1 = max(hx0, x0), max(hy0, y0), min(hx1, x1), min(hy1, y1)
    parts = [(x0, y0, x1, hy0), (x0, hy1, x1, y1), (x0, hy0, hx0, hy1), (hx1, hy0, x1, hy1)]
    return [p for p in parts if p[2] - p[0] > 1 and p[3] - p[1] > 1]


def slab(label, folder, r, z, mat, holes=()):
    pieces = [r]
    for hole in holes:
        pieces = [q for p in pieces for q in subtract_hole(p, hole)]
    for index, (x0, y0, x1, y1) in enumerate(pieces):
        box(f"{label}_{index}", folder, ((x0 + x1) / 2, (y0 + y1) / 2, z), (x1 - x0, y1 - y0, WALL), mat)


def subtract_intervals(span, cuts):
    a, b = span
    result = [(a, b)]
    for c0, c1 in sorted(cuts):
        next_result = []
        for s0, s1 in result:
            if c1 <= s0 or c0 >= s1:
                next_result.append((s0, s1))
                continue
            if c0 > s0:
                next_result.append((s0, c0))
            if c1 < s1:
                next_result.append((c1, s1))
        result = next_result
    return [r for r in result if r[1] - r[0] > 1]


def wall(label, floor, axis, line, span):
    """axis 'x': wall along X at y=line, span over x. axis 'y': wall along Y at x=line."""
    z = FLOOR_Z[floor] + FLOOR_HEIGHT / 2
    a, b = span
    length = b - a + WALL
    if axis == "x":
        box(label, f"House/{floor}F/Walls", ((a + b) / 2, line, z), (length, WALL, FLOOR_HEIGHT), MAT_WALL)
    else:
        box(label, f"House/{floor}F/Walls", (line, (a + b) / 2, z), (WALL, length, FLOOR_HEIGHT), MAT_WALL)


def room_edges(r):
    x0, y0, x1, y1 = r
    return [("x", y0, (x0, x1)), ("x", y1, (x0, x1)), ("y", x0, (y0, y1)), ("y", x1, (y0, y1))]


def find_door(a, b):
    for door in DOORS:
        if set(door[:2]) == {a, b}:
            return door
    return None


def door_gap(span, center, width):
    """The opening along a shared edge span, kept clear of the walls at both ends."""
    a, b = span
    lo, hi = a + WALL / 2, b - WALL / 2
    width = min(width, hi - lo)
    center = (a + b) / 2 if center is None else center * 100.0
    center = min(max(center, lo + width / 2), hi - width / 2)
    return (center - width / 2, center + width / 2)


# --- build ----------------------------------------------------------------------

# new_level refuses an existing asset, so an existing map is opened and emptied instead
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    assert level_editor.load_level(MAP_PATH), f"could not open {MAP_PATH}"
    actors.destroy_actors(actors.get_all_level_actors())
else:
    assert level_editor.new_level(MAP_PATH), f"could not create {MAP_PATH}"

# light and sky
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(-40.0, -50.0, 0.0))
sun.set_actor_label("Sun")
sun.light_component.set_intensity(6.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000))
sky.set_actor_label("SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")

# a ground plane around the house, its top flush with the bottom of the 1층 floor
box("Ground", "House", (1100.0, 1200.0, -WALL * 1.5), (6000.0, 6000.0, WALL), MAT_GROUND)

# floors and roof
for name, (floor, r) in ROOMS.items():
    z = FLOOR_Z[floor] - WALL / 2
    slab(f"Floor_{name}", f"House/{floor}F/Floors", r, z, MAT_FLOOR[floor], HOLES.get(floor, ()))

    # the 2nd floor has a ceiling; the 1st floor's ceiling is the 2nd floor where it exists
    if floor == "2":
        slab(f"Roof_{name}", "House/Roof", r, FLOOR_Z["2"] + FLOOR_HEIGHT + WALL / 2, MAT_ROOF)
    elif name in SINGLE_STOREY:
        slab(f"Roof_{name}", "House/Roof", r, FLOOR_Z["2"] + WALL / 2, MAT_ROOF)

# walls: shared edges listed in DOORS get an opening, everything else is solid
openings = {}  # frozenset((a, b)) -> (floor, axis, line, gap)

for floor in ("1", "2"):
    rooms = {n: r for n, (f, r) in ROOMS.items() if f == floor}
    names = list(rooms)
    shared = {}  # (axis, line) -> list of (span, a, b)

    for i, a in enumerate(names):
        for b in names[i + 1:]:
            for axis_a, line_a, span_a in room_edges(rooms[a]):
                for axis_b, line_b, span_b in room_edges(rooms[b]):
                    if axis_a != axis_b or abs(line_a - line_b) > 1:
                        continue
                    overlap = (max(span_a[0], span_b[0]), min(span_a[1], span_b[1]))
                    if overlap[1] - overlap[0] > 1:
                        shared.setdefault((axis_a, round(line_a, 1)), []).append((overlap, a, b))

    # outside parts of every edge
    for name in names:
        for index, (axis, line, span) in enumerate(room_edges(rooms[name])):
            cuts = [s for (sa, sl), items in shared.items() if sa == axis and abs(sl - line) <= 1 for (s, _, _) in items]
            for part_index, part in enumerate(subtract_intervals(span, cuts)):
                wall(f"Wall_{name}_{index}_{part_index}", floor, axis, line, part)

    # shared parts, with an opening where there is a door
    for (axis, line), items in shared.items():
        for overlap, a, b in items:
            door = find_door(a, b)
            gaps = []
            if door:
                gap = door_gap(overlap, door[2], door[3])
                gaps.append(gap)
                openings[frozenset((a, b))] = (floor, axis, line, gap)
            for part_index, part in enumerate(subtract_intervals(overlap, gaps)):
                wall(f"Wall_{a}_{b}_{part_index}", floor, axis, line, part)

for a, b, _, _ in DOORS:
    assert frozenset((a, b)) in openings, f"door between rooms that do not touch: {a} / {b}"

# stairs: 12 solid steps of 25 cm rise standing on the 1st floor, climbing toward -Y (north)
x0, y0, x1, y1 = STAIRS
run = (y1 - y0) / STEPS
rise = FLOOR_HEIGHT / STEPS
for step in range(STEPS):
    height = rise * (step + 1)
    center = ((x0 + x1) / 2, y1 - run * (step + 0.5), FLOOR_Z["1"] + height / 2)
    box(f"Stairs_{step:02d}", "House/Stairs", center, (x1 - x0, run, height), MAT_STAIRS)

# room lights
for name, (floor, (x0, y0, x1, y1)) in ROOMS.items():
    if name == "앞마당":
        continue
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, FLOOR_Z[floor] + FLOOR_HEIGHT - 40))
    light.set_actor_label(f"Light_{name}")
    light.set_folder_path(f"House/{floor}F/Lights")
    light.point_light_component.set_intensity(40.0)
    light.point_light_component.set_attenuation_radius(900.0)

# '방' zone for the AC remote
zone_floor, (zx0, zy0, zx1, zy1) = ROOMS["내 방"]
zone = actors.spawn_actor_from_class(unreal.HeadZone, unreal.Vector((zx0 + zx1) / 2, (zy0 + zy1) / 2, FLOOR_Z[zone_floor] + FLOOR_HEIGHT / 2))
zone.set_actor_label("Zone_Room")
zone.set_editor_property("zone_name", "Room")
zone.root_component.set_editor_property("box_extent", unreal.Vector((zx1 - zx0) / 2, (zy1 - zy0) / 2, FLOOR_HEIGHT / 2))

# body start (거실)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(950.0, 1000.0, 100.0)).set_actor_label("BodyStart")

# objects: (data asset, room, fraction x, fraction y, height above the floor)
OBJECTS = [
    ("DA_Obj_Bed", "내 방", 0.3, 0.75, 0),
    ("DA_Obj_BedroomLightSwitch", "내 방", 0.92, 0.08, 90),
    ("DA_Obj_ACRemote", "내 방", 0.6, 0.45, 0),
    ("DA_Obj_Desk", "내 방", 0.78, 0.85, 0),
    ("DA_Obj_ThermostatUp", "내 방", 0.06, 0.4, 90),
    ("DA_Obj_ThermostatDown", "내 방", 0.06, 0.55, 90),
    ("DA_Obj_DollB", "내 방", 0.5, 0.25, 0),
    ("DA_Obj_Shower", "샤워 화장실", 0.75, 0.8, 0),
    ("DA_Obj_Shampoo", "샤워 화장실", 0.45, 0.85, 0),
    ("DA_Obj_Towel", "샤워 화장실", 0.3, 0.55, 0),
    ("DA_Obj_Toothbrush", "샤워 화장실", 0.3, 0.3, 0),
    ("DA_Obj_BasementKey", "안방", 0.5, 0.5, 0),
    ("DA_Obj_DollC", "안방", 0.75, 0.75, 0),
    ("DA_Obj_DollA", "앞마당", 0.5, 0.5, 0),
    ("DA_Obj_LivingTable", "거실", 0.3, 0.4, 0),
    ("DA_Obj_Basket", "거실", 0.7, 0.25, 0),
    ("DA_Obj_LivingLightSwitch", "거실", 0.05, 0.12, 90),
    ("DA_Obj_Apple", "부엌", 0.4, 0.5, 0),
    ("DA_Obj_SuspiciousEgg", "부엌", 0.6, 0.5, 0),
    ("DA_Obj_TeaSet", "부엌", 0.5, 0.8, 0),
    ("DA_Obj_DollD", "창고", 0.25, 0.3, 0),
]

for data_name, room, fx, fy, height in OBJECTS:
    floor, (x0, y0, x1, y1) = ROOMS[room]
    location = unreal.Vector(x0 + (x1 - x0) * fx, y0 + (y1 - y0) * fy, FLOOR_Z[floor] + height)
    obj = actors.spawn_actor_from_class(unreal.HeadObject, location)
    obj.set_actor_label(data_name.replace("DA_Obj_", "Obj_"))
    obj.set_folder_path(f"Objects/{floor}F")
    obj.set_editor_property("object_data", unreal.load_asset(DATA_PATH + data_name))

# locked 창고 door in its opening; the door mesh is wide along Y, so it turns to fill a wall along X
door_floor, door_axis, door_line, (g0, g1) = openings[frozenset(LOCKED_DOOR)]
door_center = (g0 + g1) / 2
if door_axis == "x":
    door_location, door_rotation = unreal.Vector(door_center, door_line, FLOOR_Z[door_floor]), unreal.Rotator(0.0, 0.0, 90.0)
else:
    door_location, door_rotation = unreal.Vector(door_line, door_center, FLOOR_Z[door_floor]), unreal.Rotator(0.0, 0.0, 0.0)
door = actors.spawn_actor_from_class(unreal.HeadObject, door_location, door_rotation)
door.set_actor_label("Obj_BasementDoor")
door.set_folder_path(f"Objects/{door_floor}F")
door.set_editor_property("object_data", unreal.load_asset(DATA_PATH + "DA_Obj_BasementDoor"))

level_editor.save_current_level()
unreal.log("CreateHouseMap: done")
