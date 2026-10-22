"""
Creates the M2 data assets from the design sheet values (Get Out Of My Head — Design Sheet).

Run with the editor closed:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<path>/Tools/CreateM2Data.py"

Existing assets are overwritten, so this is the source of truth until the data is edited in the editor.
"""

import unreal

DATA_PATH = "/Game/Head/Data"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")


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

# --- missions (M2 only needs their identity) -------------------------------

def mission(name, display, priority, description):
    return set_props(make_asset(name, unreal.HeadMissionData),
                     display_name=text(display), priority=priority, description=text(description))


FALL_ASLEEP = mission("DA_Mission_FallAsleep", "잠에 들기", 1,
                      "샴푸 → 샤워기 → 수건 → 칫솔 → 침실 불 끄기 → 인형을 지닌 채 침대")
POWER_OUTAGE = mission("DA_Mission_PowerOutage", "정전", 2,
                       "침실 불, 거실 불, 에어컨 리모컨을 모두 켜기")
PLAYING_HOUSE = mission("DA_Mission_PlayingHouse", "소꿉놀이 하기", 7,
                        "거실 테이블에 인형 4개(A~D)와 다도 세트를 올려 두기")
HATCH_EGG = mission("DA_Mission_HatchEgg", "수상한 알 부화시키기", 8,
                    "바구니에 알·수건을 넣어 2층 책상에 올리고, 방 온도 36도 15초 → 38도 15초 유지")

# --- conditions / effects helpers ------------------------------------------

def condition(type_, fail="", **props):
    return set_props(unreal.HeadCondition(), type=type_, fail_text=text(fail), **props)


def add_status(status_asset, stacks=1):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.ADD_STATUS, status=status_asset, stacks=stacks)


def remove_status(status_asset):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.REMOVE_STATUS, status=status_asset)


def add_variable(variable, delta):
    return set_props(unreal.HeadEffect(), type=unreal.HeadEffectType.ADD_VARIABLE, variable=variable, delta=delta)


INSTANT = unreal.HeadInteractMode.INSTANT
HOLD_KEEP = unreal.HeadInteractMode.HOLD_KEEP
HOLD_RESET = unreal.HeadInteractMode.HOLD_RESET


def obj(name, display, mesh, scale, color, **props):
    asset = make_asset(name, unreal.HeadObjectData)
    set_props(asset, display_name=text(display), mesh=mesh, mesh_scale=unreal.Vector(*scale),
              color=unreal.LinearColor(*color, 1.0), **props)
    return asset


# --- objects (values from the design sheet object list) --------------------

SHAMPOO_OBJ = obj("DA_Obj_Shampoo", "샴푸", CYLINDER, (0.15, 0.15, 0.3), (0.9, 0.5, 0.8),
                  mode=INSTANT, effects=[add_status(SHAMPOO)])

SHOWER = obj("DA_Obj_Shower", "샤워기", CUBE, (0.4, 0.4, 2.0), (0.6, 0.8, 0.9),
             mode=HOLD_RESET, hold_seconds=2.0,
             conditions=[condition(unreal.HeadConditionType.HAS_STATUS, status=SHAMPOO)],
             effects=[add_status(WET), remove_status(SHAMPOO), add_status(CLEAN)])

TOWEL = obj("DA_Obj_Towel", "수건", CUBE, (0.4, 0.25, 0.08), (0.95, 0.95, 0.95),
            mode=HOLD_RESET, hold_seconds=5.0, carryable=True,
            conditions=[condition(unreal.HeadConditionType.HAS_STATUS, status=WET)],
            effects=[remove_status(WET)])

TOOTHBRUSH = obj("DA_Obj_Toothbrush", "칫솔", CYLINDER, (0.05, 0.05, 0.25), (0.3, 0.8, 1.0),
                 mode=HOLD_RESET, hold_seconds=5.0, carryable=True,
                 effects=[add_status(TEETH)])

APPLE = obj("DA_Obj_Apple", "사과", SPHERE, (0.2, 0.2, 0.2), (0.85, 0.1, 0.1),
            mode=HOLD_KEEP, hold_seconds=3.0, carryable=True, consume_on_use=True,
            effects=[remove_status(TEETH), add_status(FULLNESS)])

EGG = obj("DA_Obj_SuspiciousEgg", "수상한 알", SPHERE, (0.18, 0.18, 0.24), (0.9, 0.85, 0.6),
          mode=INSTANT, carryable=True, consume_on_use=True, break_on_throw=True, respawn=True,
          effects=[add_status(FULLNESS)])

BASKET = obj("DA_Obj_Basket", "바구니", CYLINDER, (0.4, 0.4, 0.25), (0.65, 0.45, 0.2),
             carryable=True, no_interact=True,
             attach_slots=[
                 set_props(unreal.HeadAttachSlot(), item=EGG, mode=INSTANT, conditions=[
                     condition(unreal.HeadConditionType.MISSION_HOLDER_PRESENT, "알을 부화시키려는 사람만 넣을 수 있다", mission=HATCH_EGG)]),
                 set_props(unreal.HeadAttachSlot(), item=TOWEL, mode=INSTANT, conditions=[
                     condition(unreal.HeadConditionType.MISSION_HOLDER_PRESENT, "알을 부화시키려는 사람만 넣을 수 있다", mission=HATCH_EGG)]),
             ])

THERMO_UP = obj("DA_Obj_ThermostatUp", "온도 조절기(+)", CUBE, (0.2, 0.1, 0.3), (0.9, 0.4, 0.2),
                mode=INSTANT, label_variable=ROOM_TEMP, effects=[add_variable(ROOM_TEMP, 1)])

THERMO_DOWN = obj("DA_Obj_ThermostatDown", "온도 조절기(−)", CUBE, (0.2, 0.1, 0.3), (0.2, 0.5, 0.9),
                  mode=INSTANT, label_variable=ROOM_TEMP, effects=[add_variable(ROOM_TEMP, -1)])

BEDROOM_LIGHT = obj("DA_Obj_BedroomLightSwitch", "침실 불 스위치", CUBE, (0.1, 0.1, 0.2), (1.0, 0.9, 0.4),
                    mode=INSTANT, toggle=True, initially_on=False)

BASEMENT_KEY = obj("DA_Obj_BasementKey", "지하실 열쇠", CUBE, (0.1, 0.05, 0.05), (0.85, 0.75, 0.2),
                   carryable=True, no_interact=True)

BASEMENT_DOOR = obj("DA_Obj_BasementDoor", "지하실 문", CUBE, (0.15, 1.0, 2.2), (0.45, 0.3, 0.2),
                    mode=INSTANT, door=True, toggle=True, large_furniture=True, initially_on=False,
                    conditions=[condition(unreal.HeadConditionType.HOLDING, "지하실 열쇠가 필요하다", object=BASEMENT_KEY)])

DESK = obj("DA_Obj_Desk", "책상", CUBE, (0.8, 1.4, 0.75), (0.55, 0.4, 0.3),
           large_furniture=True, no_interact=True,
           attach_slots=[
               set_props(unreal.HeadAttachSlot(), item=BASKET, mode=INSTANT, conditions=[
                   condition(unreal.HeadConditionType.MISSION_HOLDER_PRESENT, "알을 부화시키려는 사람만 올려 둘 수 있다", mission=HATCH_EGG),
                   condition(unreal.HeadConditionType.ATTACHED, "바구니에 수상한 알이 없다", target=unreal.HeadConditionTarget.ITEM, object=EGG),
                   condition(unreal.HeadConditionType.ATTACHED, "바구니에 수건이 없다", target=unreal.HeadConditionTarget.ITEM, object=TOWEL)]),
           ])

# --- rule set ---------------------------------------------------------------

TEST_LAYOUT = [SHAMPOO_OBJ, SHOWER, TOWEL, TOOTHBRUSH, APPLE, EGG, BASKET, THERMO_UP, THERMO_DOWN, BEDROOM_LIGHT, BASEMENT_KEY]

placements = []
for index, asset in enumerate(TEST_LAYOUT):
    offset = unreal.Vector(250.0, -600.0 + index * 120.0, 0.0)
    placements.append(set_props(unreal.HeadObjectPlacement(), object=asset, offset=offset))

# large pieces stand in a second row so they do not block the small objects
placements.append(set_props(unreal.HeadObjectPlacement(), object=DESK, offset=unreal.Vector(500.0, -250.0, 0.0)))
placements.append(set_props(unreal.HeadObjectPlacement(), object=BASEMENT_DOOR, offset=unreal.Vector(500.0, 250.0, 0.0)))

RULE_SET = set_props(make_asset("DA_RuleSet", unreal.HeadRuleSet),
                     world_variables=[ROOM_TEMP],
                     missions=[FALL_ASLEEP, POWER_OUTAGE, PLAYING_HOUSE, HATCH_EGG],
                     placements=placements)

unreal.EditorAssetLibrary.save_directory(DATA_PATH, only_if_is_dirty=False, recursive=True)
unreal.log("CreateM2Data: done")
