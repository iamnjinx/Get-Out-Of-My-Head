"""
Creates the game data assets from the design sheet (Get Out Of My Head — Design Sheet):
statuses, the room temperature, the 4 missions and the 22 house objects.

Run with the editor closed:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<path>/Tools/CreateGameData.py"

Existing assets are overwritten, so this is the source of truth until the data is edited in the editor.
"""

import unreal

DATA_PATH = "/Game/Head/Data"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")

# placeholder until M7 sounds
TEST_SOUND = unreal.load_asset("/Engine/EngineSounds/1kSineTonePing")

# zone name of the 2층 방 (AHeadZone in the house map)
ROOM_ZONE = "Room"


def text(value):
    return unreal.Text(value)


def make_asset(name, cls):
    path = f"{DATA_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return asset_tools.create_asset(name, DATA_PATH, cls, factory)


def set_props(obj, **props):
    for key, value in props.items():
        try:
            obj.set_editor_property(key, value)
        except Exception:
            # bool properties keep their "b" prefix on some engine versions
            obj.set_editor_property("b_" + key, value)
    return obj


# --- statuses -------------------------------------------------------------

def status(name, display, duration=0.0, stackable=False, tiers=()):
    asset = make_asset(name, unreal.HeadStatusData)
    set_props(asset, display_name=text(display), duration=duration, stackable=stackable,
              stack_expiry=unreal.HeadStackExpiry.ONE_BY_ONE, tiers=list(tiers))
    return asset


def tier(min_stacks=1, speed=1.0, immobile=False, fall=0.0):
    return set_props(unreal.HeadStatusTier(), min_stacks=min_stacks, speed_multiplier=speed, immobile=immobile, fall_chance=fall)


TEETH = status("DA_Status_TeethBrushed", "양치")
CLEAN = status("DA_Status_Clean", "청결")
SHAMPOO = status("DA_Status_Shampoo", "샴푸")
WET = status("DA_Status_Wet", "젖음", tiers=[tier(fall=0.1)])
FULLNESS = status("DA_Status_Fullness", "포만감", duration=10.0, stackable=True,
                  tiers=[tier(min_stacks=10, speed=0.5), tier(min_stacks=20, immobile=True)])
ASLEEP = status("DA_Status_Asleep", "잠")

# --- world variables ------------------------------------------------------

ROOM_TEMP = set_props(make_asset("DA_Var_RoomTemperature", unreal.HeadWorldVariableData),
                      display_name=text("방 온도"), initial_value=24, unit=text("도"))

# --- missions: identity first, objects refer to them ------------------------

def mission(name, display, priority, description):
    return set_props(make_asset(name, unreal.HeadMissionData),
                     display_name=text(display), priority=priority, description=text(description))


FALL_ASLEEP = mission("DA_Mission_FallAsleep", "잠에 들기", 1,
                      "샴푸 → 샤워기 → 수건 → 칫솔 → 침실 불 끄기 → 인형을 지닌 채 침대")
POWER_OUTAGE = mission("DA_Mission_PowerOutage", "정전", 2,
                       "침실 불, 거실 불, 에어컨 리모컨을 모두 켜기. 리모컨은 '방' 안에서만 켜진다")
PLAYING_HOUSE = mission("DA_Mission_PlayingHouse", "소꿉놀이 하기", 7,
                        "거실 테이블에 인형 4개(A~D)와 다도 세트를 올려 두기. 인형D는 1층 창고에 있다")
HATCH_EGG = mission("DA_Mission_HatchEgg", "수상한 알 부화시키기", 8,
                    "바구니에 알·수건을 넣어 2층 책상에 올리고, 방 온도 36도 15초 → 38도 15초 유지")

# --- conditions / effects helpers ------------------------------------------

C = unreal.HeadConditionType
OTHER = unreal.HeadConditionTarget.OTHER
ITEM = unreal.HeadConditionTarget.ITEM


def condition(type_, fail="", **props):
    return set_props(unreal.HeadCondition(), type=type_, fail_text=text(fail), **props)


def add_status(status_asset, stacks=1):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.ADD_STATUS, status=status_asset, stacks=stacks)


def remove_status(status_asset):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.REMOVE_STATUS, status=status_asset)


def play_sound(description, sound=None):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.SOUND, sound_text=text(description), sound=sound)


def add_variable(variable, delta):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.ADD_VARIABLE, variable=variable, delta=delta)


def attach_slot(item, conditions=(), mode=unreal.HeadInteractMode.INSTANT):
    return set_props(unreal.HeadAttachSlot(), item=item, mode=mode, conditions=list(conditions))


INSTANT = unreal.HeadInteractMode.INSTANT
HOLD_KEEP = unreal.HeadInteractMode.HOLD_KEEP
HOLD_RESET = unreal.HeadInteractMode.HOLD_RESET


def obj(name, display, mesh, scale, color, **props):
    asset = make_asset(name, unreal.HeadObjectData)
    defaults = dict(mode=INSTANT, hold_seconds=0.0, conditions=[], effects=[], carryable=False, consume_on_use=False,
                    respawn=False, break_on_throw=False, no_interact=False, toggle=False, initially_on=False,
                    on_effects=[], off_effects=[], large_furniture=False, door=False, label_variable=None, attach_slots=[])
    defaults.update(props)
    set_props(asset, display_name=text(display), mesh=mesh, mesh_scale=unreal.Vector(*scale),
              color=unreal.LinearColor(*color, 1.0), **defaults)
    return asset


# --- the 22 objects (design sheet object list) -------------------------------

# 2층 방
BEDROOM_LIGHT = obj("DA_Obj_BedroomLightSwitch", "침실 불 스위치", CUBE, (0.1, 0.1, 0.2), (1.0, 0.9, 0.4),
                    toggle=True, initially_on=False)

DOLL_A = obj("DA_Obj_DollA", "인형A", CYLINDER, (0.15, 0.15, 0.3), (0.95, 0.6, 0.7),
             carryable=True, effects=[play_sound("Darling I Love You", TEST_SOUND)])
DOLL_B = obj("DA_Obj_DollB", "인형B", CYLINDER, (0.15, 0.15, 0.3), (0.6, 0.75, 0.95), carryable=True)
DOLL_C = obj("DA_Obj_DollC", "인형C", CYLINDER, (0.15, 0.15, 0.3), (0.7, 0.95, 0.6), carryable=True)
DOLL_D = obj("DA_Obj_DollD", "인형D", CYLINDER, (0.15, 0.15, 0.3), (0.95, 0.85, 0.5), carryable=True)

BED = obj("DA_Obj_Bed", "침대", CUBE, (2.0, 1.4, 0.5), (0.85, 0.85, 0.95),
          conditions=[
              condition(C.MISSION_HOLDER_PRESENT, "아직 자고 싶지 않아.", mission=FALL_ASLEEP),
              condition(C.LACKS_STATUS, "몸이 젖어있어", status=WET),
              condition(C.HAS_STATUS, "씻고 자야지", status=CLEAN),
              condition(C.HAS_STATUS, "양치는 하고 자야지", status=TEETH),
              condition(C.HOLDING, "인형이 없으면 무서워서 못 자", object=DOLL_A),
              condition(C.TOGGLE_OFF, "불은 끄고 자야지", target=OTHER, target_object=BEDROOM_LIGHT),
          ],
          effects=[add_status(ASLEEP)])

AC_REMOTE = obj("DA_Obj_ACRemote", "에어컨 리모컨", CUBE, (0.06, 0.15, 0.04), (0.9, 0.9, 0.9),
                carryable=True, toggle=True, initially_on=False,
                conditions=[condition(C.IN_ZONE, zone=ROOM_ZONE)],
                on_effects=[play_sound("삐빅", TEST_SOUND)], off_effects=[play_sound("삐빅", TEST_SOUND)])

THERMO_UP = obj("DA_Obj_ThermostatUp", "온도 조절기(+)", CUBE, (0.2, 0.1, 0.3), (0.9, 0.4, 0.2),
                label_variable=ROOM_TEMP, effects=[add_variable(ROOM_TEMP, 1)])
THERMO_DOWN = obj("DA_Obj_ThermostatDown", "온도 조절기(−)", CUBE, (0.2, 0.1, 0.3), (0.2, 0.5, 0.9),
                  label_variable=ROOM_TEMP, effects=[add_variable(ROOM_TEMP, -1)])

# 2층 화장실
SHAMPOO_OBJ = obj("DA_Obj_Shampoo", "샴푸", CYLINDER, (0.15, 0.15, 0.3), (0.9, 0.5, 0.8),
                  effects=[add_status(SHAMPOO)])
SHOWER = obj("DA_Obj_Shower", "샤워기", CUBE, (0.4, 0.4, 2.0), (0.6, 0.8, 0.9),
             mode=HOLD_RESET, hold_seconds=2.0,
             conditions=[condition(C.HAS_STATUS, status=SHAMPOO)],
             effects=[add_status(WET), remove_status(SHAMPOO), add_status(CLEAN)])
TOWEL = obj("DA_Obj_Towel", "수건", CUBE, (0.4, 0.25, 0.08), (0.95, 0.95, 0.95),
            mode=HOLD_RESET, hold_seconds=5.0, carryable=True,
            conditions=[condition(C.HAS_STATUS, status=WET)],
            effects=[remove_status(WET)])
TOOTHBRUSH = obj("DA_Obj_Toothbrush", "칫솔", CYLINDER, (0.05, 0.05, 0.25), (0.3, 0.8, 1.0),
                 mode=HOLD_RESET, hold_seconds=5.0, carryable=True, effects=[add_status(TEETH)])

# 2층 안방
BASEMENT_KEY = obj("DA_Obj_BasementKey", "창고 열쇠", CUBE, (0.1, 0.05, 0.05), (0.85, 0.75, 0.2),
                   carryable=True, no_interact=True)

# 1층 부엌
APPLE = obj("DA_Obj_Apple", "사과", SPHERE, (0.2, 0.2, 0.2), (0.85, 0.1, 0.1),
            mode=HOLD_KEEP, hold_seconds=3.0, carryable=True, consume_on_use=True,
            effects=[remove_status(TEETH), add_status(FULLNESS)])
EGG = obj("DA_Obj_SuspiciousEgg", "수상한 알", SPHERE, (0.18, 0.18, 0.24), (0.9, 0.85, 0.6),
          carryable=True, consume_on_use=True, break_on_throw=True, respawn=True,
          effects=[add_status(FULLNESS)])
TEA_SET = obj("DA_Obj_TeaSet", "다도 세트", CUBE, (0.3, 0.2, 0.12), (0.75, 0.9, 0.85),
              carryable=True, no_interact=True)

# 1층 거실
LIVING_LIGHT = obj("DA_Obj_LivingLightSwitch", "거실 불 스위치", CUBE, (0.1, 0.1, 0.2), (1.0, 0.85, 0.5),
                   toggle=True, initially_on=True)

HOUSE_ONLY = "소꿉놀이를 하려는 사람만 올려 둘 수 있다"
LIVING_TABLE = obj("DA_Obj_LivingTable", "거실 테이블", CUBE, (1.2, 0.8, 0.45), (0.6, 0.45, 0.3),
                   large_furniture=True, no_interact=True,
                   attach_slots=[attach_slot(item, [condition(C.MISSION_HOLDER_PRESENT, HOUSE_ONLY, mission=PLAYING_HOUSE)])
                                 for item in (DOLL_A, DOLL_B, DOLL_C, DOLL_D, TEA_SET)])

EGG_ONLY = "알을 부화시키려는 사람만 넣을 수 있다"
BASKET = obj("DA_Obj_Basket", "바구니", CYLINDER, (0.4, 0.4, 0.25), (0.65, 0.45, 0.2),
             carryable=True, no_interact=True,
             attach_slots=[attach_slot(item, [condition(C.MISSION_HOLDER_PRESENT, EGG_ONLY, mission=HATCH_EGG)])
                           for item in (EGG, TOWEL)])

DESK = obj("DA_Obj_Desk", "책상", CUBE, (0.8, 1.4, 0.75), (0.55, 0.4, 0.3),
           large_furniture=True, no_interact=True,
           attach_slots=[attach_slot(BASKET, [
               condition(C.MISSION_HOLDER_PRESENT, "알을 부화시키려는 사람만 올려 둘 수 있다", mission=HATCH_EGG),
               condition(C.ATTACHED, "바구니에 수상한 알이 없다", target=ITEM, object=EGG),
               condition(C.ATTACHED, "바구니에 수건이 없다", target=ITEM, object=TOWEL)])])

# 1층 차고 ↔ 창고
BASEMENT_DOOR = obj("DA_Obj_BasementDoor", "창고 문", CUBE, (0.15, 1.25, 2.2), (0.45, 0.3, 0.2),
                    door=True, toggle=True, large_furniture=True, initially_on=False,
                    conditions=[condition(C.HOLDING, "창고 열쇠가 필요하다", object=BASEMENT_KEY)])

# --- mission rules --------------------------------------------------------

set_props(FALL_ASLEEP, staged=False, conditions=[condition(C.HAS_STATUS, status=ASLEEP)])

set_props(POWER_OUTAGE, staged=False, conditions=[
    condition(C.TOGGLE_ON, target=OTHER, target_object=BEDROOM_LIGHT),
    condition(C.TOGGLE_ON, target=OTHER, target_object=LIVING_LIGHT),
    condition(C.TOGGLE_ON, target=OTHER, target_object=AC_REMOTE),
])

set_props(PLAYING_HOUSE, staged=False, conditions=[
    condition(C.ATTACHED, target=OTHER, target_object=LIVING_TABLE, object=item)
    for item in (DOLL_A, DOLL_B, DOLL_C, DOLL_D, TEA_SET)
])


def stage(conditions, seconds):
    return set_props(unreal.HeadMissionStage(), conditions=list(conditions), hold_seconds=seconds)


set_props(HATCH_EGG, staged=True,
          prerequisites=[
              condition(C.ATTACHED, target=OTHER, target_object=BASKET, object=EGG),
              condition(C.ATTACHED, target=OTHER, target_object=BASKET, object=TOWEL),
              condition(C.ATTACHED, target=OTHER, target_object=DESK, object=BASKET),
          ],
          stages=[
              stage([condition(C.VARIABLE_EQUALS, variable=ROOM_TEMP, value=36)], 15.0),
              stage([condition(C.VARIABLE_EQUALS, variable=ROOM_TEMP, value=38)], 15.0),
          ])

# --- rule set ---------------------------------------------------------------
# The house map (Tools/CreateHouseMap.py) places its own objects. These placements are only used
# by maps without placed objects (e.g. Lvl_ThirdPerson as a quick test room).

ALL_OBJECTS = [BED, BEDROOM_LIGHT, AC_REMOTE, THERMO_UP, THERMO_DOWN, DESK, SHOWER, SHAMPOO_OBJ, TOWEL, TOOTHBRUSH,
               BASEMENT_KEY, DOLL_A, DOLL_B, DOLL_C, DOLL_D, LIVING_TABLE, BASKET, APPLE, EGG, TEA_SET,
               BASEMENT_DOOR, LIVING_LIGHT]
assert len(ALL_OBJECTS) == 22

placements = []
for index, asset in enumerate(ALL_OBJECTS):
    row, column = divmod(index, 11)
    offset = unreal.Vector(250.0 + row * 300.0, -660.0 + column * 130.0, 0.0)
    placements.append(set_props(unreal.HeadObjectPlacement(), object=asset, offset=offset))

RULE_SET = set_props(make_asset("DA_RuleSet", unreal.HeadRuleSet),
                     world_variables=[ROOM_TEMP],
                     missions=[FALL_ASLEEP, POWER_OUTAGE, PLAYING_HOUSE, HATCH_EGG],
                     placements=placements)

unreal.EditorAssetLibrary.save_directory(DATA_PATH, only_if_is_dirty=False, recursive=True)
unreal.log("CreateGameData: done")
