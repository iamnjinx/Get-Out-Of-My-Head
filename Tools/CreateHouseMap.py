"""
Builds the M4 whitebox house map /Game/Head/Maps/Lvl_House from the design sheet floor plan.

Run with the editor closed, after Tools/CreateGameData.py:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<path>/Tools/CreateHouseMap.py"

Everything is placed as normal actors (walls, floors, stairs, lights, objects, the '방' zone, the player start),
so the map can be edited by hand afterwards. Running the script again rebuilds the map from scratch.

Plan coordinates are the design sheet image pixels, 1 px = 1.9 cm. The 2nd floor is flipped vertically so its
corridor sits above the entrance (현관) and the stairs connect; room adjacency is unchanged.
"""

import unreal

MAP_PATH = "/Game/Head/Maps/Lvl_House"
DATA_PATH = "/Game/Head/Data/"
MATERIAL_PATH = "/Game/Head/Materials"

SCALE = 1.9            # cm per plan pixel
ORIGIN = (141, 697)    # plan pixel of the house's top-left corner
FLOOR_HEIGHT = 300.0
WALL = 20.0            # wall and slab thickness
DOOR_WIDTH = 120.0

FLOOR_Z = {"B": -FLOOR_HEIGHT, "1": 0.0, "2": FLOOR_HEIGHT}

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
BASE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")


def px(x, y):
    return ((x - ORIGIN[0]) * SCALE, (y - ORIGIN[1]) * SCALE)


def rect(x0, y0, x1, y1):
    a, b = px(x0, y0), px(x1, y1)
    return (a[0], a[1], b[0], b[1])


# rooms: name -> (floor, rect in cm)
ROOMS = {
    # 1층
    "마당": ("1", rect(141, 697, 495, 1191)),
    "차고": ("1", rect(495, 697, 778, 873)),
    "현관": ("1", rect(778, 697, 1025, 873)),
    "지하 입구": ("1", rect(1025, 697, 1203, 873)),
    "거실": ("1", rect(495, 873, 884, 1191)),
    "부엌": ("1", rect(884, 873, 1203, 1191)),
    # 2층 (flipped: corridor on the entrance side)
    "복도": ("2", rect(283, 697, 1203, 838)),
    "방": ("2", rect(283, 838, 637, 1191)),
    "화장실": ("2", rect(637, 838, 848, 1191)),
    "안방": ("2", rect(848, 838, 1203, 1191)),
    # 지하
    "창고": ("B", rect(778, 697, 1203, 1049)),
}

# the locked basement door replaces the opening between 현관 and 지하 입구
DOOR_EDGE = ("현관", "지하 입구")

# the only way down is the locked door: 지하 입구 has no other opening
SEALED_ROOM = "지하 입구"


def has_opening(a, b):
    if SEALED_ROOM in (a, b):
        return set((a, b)) == set(DOOR_EDGE)
    return True

# stairs: (floor the stairs start on, x0, y0, x1, y1, direction of travel, rise)
# 현관 → 2층 복도, along +X by the north wall
STAIRS_UP = (1300.0, 12.0, 1600.0, 122.0)
# 지하 입구 → 창고, along +X right behind the basement door
STAIRS_DOWN = (1715.0, 102.0, 1991.0, 232.0)
STEPS = 12

HOLES = {
    "2": [STAIRS_UP],     # 2층 floor above the stairs up
    "1": [STAIRS_DOWN],   # 1층 floor above the stairs down
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
             "2": material("MI_House_Floor2", (0.5, 0.45, 0.5)),
             "B": material("MI_House_FloorB", (0.3, 0.3, 0.3))}
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


def door_gap(span):
    a, b = span
    width = min(DOOR_WIDTH, (b - a) - 40.0)
    center = (a + b) / 2
    return (center - width / 2, center + width / 2)


# --- build ----------------------------------------------------------------------

level_editor.new_level(MAP_PATH)

# light and sky
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(-40.0, -50.0, 0.0))
sun.set_actor_label("Sun")
sun.light_component.set_intensity(6.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000))
sky.set_actor_label("SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")

# a ground plane around the house
box("Ground", "House", (1000.0, 470.0, -FLOOR_HEIGHT - WALL), (6000.0, 6000.0, WALL), MAT_FLOOR["B"])

# floors and roof
for name, (floor, r) in ROOMS.items():
    z = FLOOR_Z[floor] - WALL / 2
    slab(f"Floor_{name}", f"House/{floor}F/Floors", r, z, MAT_FLOOR[floor], HOLES.get(floor, ()))

    # the 2nd floor and the basement have a ceiling; the 1st floor's ceiling is the 2nd floor where it exists
    if floor == "2":
        slab(f"Roof_{name}", "House/Roof", r, FLOOR_Z["2"] + FLOOR_HEIGHT + WALL / 2, MAT_ROOF)

# walls: shared edges get a door-wide opening, outside edges are solid
for floor in ("B", "1", "2"):
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

    # shared parts, with an opening
    for (axis, line), items in shared.items():
        for overlap, a, b in items:
            gaps = [door_gap(overlap)] if has_opening(a, b) else []
            for part_index, part in enumerate(subtract_intervals(overlap, gaps)):
                wall(f"Wall_{a}_{b}_{part_index}", floor, axis, line, part)

# stairs: 12 solid steps of 25 cm rise along +X, standing on the lower floor
def stairs(label, r, lower_z, going_up):
    x0, y0, x1, y1 = r
    run = (x1 - x0) / STEPS
    rise = FLOOR_HEIGHT / STEPS
    for step in range(STEPS):
        # going up: steps climb along +X; going down: they descend along +X
        height = rise * (step + 1) if going_up else FLOOR_HEIGHT - rise * (step + 1)
        if height <= 0:
            continue
        center = (x0 + run * (step + 0.5), (y0 + y1) / 2, lower_z + height / 2)
        box(f"{label}_{step:02d}", "House/Stairs", center, (run, y1 - y0, height), MAT_STAIRS)


stairs("StairsUp", STAIRS_UP, FLOOR_Z["1"], True)
stairs("StairsDown", STAIRS_DOWN, FLOOR_Z["B"], False)

# room lights
for name, (floor, (x0, y0, x1, y1)) in ROOMS.items():
    if name == "마당":
        continue
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, FLOOR_Z[floor] + FLOOR_HEIGHT - 40))
    light.set_actor_label(f"Light_{name}")
    light.set_folder_path(f"House/{floor}F/Lights")
    light.point_light_component.set_intensity(40.0)
    light.point_light_component.set_attenuation_radius(900.0)

# '방' zone for the AC remote
zone_floor, (zx0, zy0, zx1, zy1) = ROOMS["방"]
zone = actors.spawn_actor_from_class(unreal.HeadZone, unreal.Vector((zx0 + zx1) / 2, (zy0 + zy1) / 2, FLOOR_Z[zone_floor] + FLOOR_HEIGHT / 2))
zone.set_actor_label("Zone_Room")
zone.set_editor_property("zone_name", "Room")
zone.root_component.set_editor_property("box_extent", unreal.Vector((zx1 - zx0) / 2, (zy1 - zy0) / 2, FLOOR_HEIGHT / 2))

# body start (거실, from the plan)
start_x, start_y = px(707, 1060)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(start_x, start_y, 100.0)).set_actor_label("BodyStart")

# objects: (data asset, room, fraction x, fraction y, height above the floor)
OBJECTS = [
    ("DA_Obj_Bed", "방", 0.3, 0.75, 0),
    ("DA_Obj_BedroomLightSwitch", "방", 0.92, 0.08, 90),
    ("DA_Obj_ACRemote", "방", 0.6, 0.45, 0),
    ("DA_Obj_Desk", "방", 0.78, 0.85, 0),
    ("DA_Obj_ThermostatUp", "방", 0.06, 0.4, 90),
    ("DA_Obj_ThermostatDown", "방", 0.06, 0.55, 90),
    ("DA_Obj_DollB", "방", 0.5, 0.25, 0),
    ("DA_Obj_Shower", "화장실", 0.75, 0.8, 0),
    ("DA_Obj_Shampoo", "화장실", 0.45, 0.85, 0),
    ("DA_Obj_Towel", "화장실", 0.3, 0.55, 0),
    ("DA_Obj_Toothbrush", "화장실", 0.3, 0.3, 0),
    ("DA_Obj_BasementKey", "안방", 0.5, 0.5, 0),
    ("DA_Obj_DollC", "안방", 0.75, 0.75, 0),
    ("DA_Obj_DollA", "마당", 0.5, 0.5, 0),
    ("DA_Obj_LivingTable", "거실", 0.3, 0.4, 0),
    ("DA_Obj_Basket", "거실", 0.7, 0.25, 0),
    ("DA_Obj_LivingLightSwitch", "거실", 0.05, 0.12, 90),
    ("DA_Obj_Apple", "부엌", 0.4, 0.5, 0),
    ("DA_Obj_SuspiciousEgg", "부엌", 0.6, 0.5, 0),
    ("DA_Obj_TeaSet", "부엌", 0.5, 0.8, 0),
    ("DA_Obj_DollD", "창고", 0.25, 0.75, 0),
]

for data_name, room, fx, fy, height in OBJECTS:
    floor, (x0, y0, x1, y1) = ROOMS[room]
    location = unreal.Vector(x0 + (x1 - x0) * fx, y0 + (y1 - y0) * fy, FLOOR_Z[floor] + height)
    obj = actors.spawn_actor_from_class(unreal.HeadObject, location)
    obj.set_actor_label(data_name.replace("DA_Obj_", "Obj_"))
    obj.set_folder_path(f"Objects/{floor}F")
    obj.set_editor_property("object_data", unreal.load_asset(DATA_PATH + data_name))

# basement door in the opening between 현관 and 지하 입구
_, (ax0, ay0, ax1, ay1) = ROOMS[DOOR_EDGE[0]]
_, (bx0, by0, bx1, by1) = ROOMS[DOOR_EDGE[1]]
door_line = ax1
door_center = sum(door_gap((max(ay0, by0), min(ay1, by1)))) / 2
door = actors.spawn_actor_from_class(unreal.HeadObject, unreal.Vector(door_line, door_center, FLOOR_Z["1"]))
door.set_actor_label("Obj_BasementDoor")
door.set_folder_path("Objects/1F")
door.set_editor_property("object_data", unreal.load_asset(DATA_PATH + "DA_Obj_BasementDoor"))

level_editor.save_current_level()
unreal.log("CreateHouseMap: done")
