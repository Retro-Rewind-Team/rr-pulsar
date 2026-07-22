#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRace2DMap.hpp>
#include <MarioKartWii/UI/Layout/ControlLoader.hpp>

namespace Pulsar {
namespace UI {

namespace {

static const u32 itemTypeCount = 15;
static const u32 itemMapObjectSize = sizeof(CtrlRace2DMapObject);
static const u32 maxItemMapObjects = 64;
static const float itemIconSize = 12.0f;
static const float itemIconZ = 10.0f;
static const u32 itemIconZIdx = 10;
static const u8 itemIconOpacity = 0xa0;
static const u8 itemMapObjectMagic = 0x92;

static const char* const itemIconNames[itemTypeCount] = {
    "kame_green",
    "kame_red",
    "banana",
    "kinoko",
    "star",
    "kame_wing",
    "thunder",
    "dummybox",
    "kinoko_big",
    "bomb_hei",
    "gesso",
    "pow",
    "GoldenKinoko",
    "killer",
    "thunder_c",
};

struct ItemMapObject : public CtrlRace2DMapObject {
    u8 itemType() const { return this->padding[0]; }
    u8 objectIndex() const { return this->padding[1]; }
    bool isItemMapObject() const { return this->padding[2] == itemMapObjectMagic; }
    void setItemInfo(u8 type, u8 index) {
        this->padding[0] = type;
        this->padding[1] = index;
        this->padding[2] = itemMapObjectMagic;
    }
};

static bool IsEnabled() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::RoomType roomType = controller == nullptr ? RKNet::ROOMTYPE_NONE : controller->roomType;
    const System* system = System::sInstance;
    const bool contextEnabled = system != nullptr && system->IsContext(PULSAR_ITEMSONMINIMAP);
    const bool localEnabled = Settings::Mgr::IsCreated() &&
                              Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_ITEMSONMINIMAP) == ITEMSONMINIMAP_ENABLED;

    switch (roomType) {
        case RKNet::ROOMTYPE_NONE:
            return localEnabled;
        case RKNet::ROOMTYPE_FROOM_HOST:
            return contextEnabled || localEnabled;
        case RKNet::ROOMTYPE_FROOM_NONHOST:
            return contextEnabled;
        default:
            return false;
    }
}

static Item::Manager* GetItemManager() {
    return Item::Manager::sInstance;
}

static ItemMapObject* GetFirstItemMapObject(CtrlRace2DMap* map) {
    if (map == nullptr) return nullptr;
    return reinterpret_cast<ItemMapObject*>(reinterpret_cast<u8*>(map) + sizeof(CtrlRace2DMap));
}

static void ConstructItemMapObject(ItemMapObject& object, void** vtable) {
    new (&object) LayoutUIControl;
    *reinterpret_cast<void***>(&object) = vtable;
}

static void LoadItemMapObject(ItemMapObject& object, u8 itemType, u8 objectIndex) {
    ControlLoader loader(&object);
    loader.Load("game_image", "map_start_line", "start_line", nullptr);

    object.LoadPictureLayout("game_image", "item");
    object.SetPicturePane("race_null", itemIconNames[itemType]);
    object.pane = object.layout.GetPaneByName("race_null");
    object.setItemInfo(itemType, objectIndex);
}

static u32 CreateItemMapControls(CtrlRace2DMap* map, u32 childIndex, CtrlRace2DMapObject* templateObject) {
    Item::Manager* manager = GetItemManager();
    if (manager == nullptr || templateObject == nullptr) return childIndex;

    ItemMapObject* object = GetFirstItemMapObject(map);
    if (object == nullptr) return childIndex;
    void** vtable = *reinterpret_cast<void***>(templateObject);
    u32 createdCount = 0;
    for (u32 type = 0; type < itemTypeCount; ++type) {
        const Item::ObjHolder& holder = manager->itemObjHolders[type];
        for (u32 index = 0; index < holder.capacity; ++index) {
            if (createdCount >= maxItemMapObjects) return childIndex;
            ConstructItemMapObject(*object, vtable);
            map->AddControl(++childIndex, object);
            LoadItemMapObject(*object, static_cast<u8>(type), static_cast<u8>(index));
            object = reinterpret_cast<ItemMapObject*>(reinterpret_cast<u8*>(object) + itemMapObjectSize);
            ++createdCount;
        }
    }
    return childIndex;
}

static void UpdateItemMapControl(ItemMapObject* control) {
    if (control == nullptr || !control->isItemMapObject() || control->pictureLayout == nullptr) return;

    nw4r::lyt::Pane* pane = control->pane;
    if (pane != nullptr) {
        pane->alpha = itemIconOpacity;
        pane->size.x = itemIconSize;
        pane->size.z = itemIconSize;
        pane->trans.z = itemIconZ;
    }
    control->zIdx = itemIconZIdx;

    control->isHidden = true;
    if (!IsEnabled()) return;

    Item::Manager* manager = GetItemManager();
    if (manager == nullptr) return;

    const u32 type = control->itemType();
    if (type >= itemTypeCount) return;

    const Item::ObjHolder& holder = manager->itemObjHolders[type];
    const u32 index = control->objectIndex();
    const u32 visibleCount = holder.bodyCount > holder.spawnedCount ? holder.bodyCount : holder.spawnedCount;
    if (index >= visibleCount || holder.itemObj == nullptr) return;

    const Item::Obj* item = holder.itemObj[index];
    if (item == nullptr) return;

    control->isHidden = false;
    control->mapPosition = item->position;
}

}  // namespace

extern "C" u32 GetItemsOnMinimapExpandedMapSize() {
    return sizeof(CtrlRace2DMap) + maxItemMapObjects * itemMapObjectSize;
}
kmCall(0x80858194, GetItemsOnMinimapExpandedMapSize);

extern "C" u32 GetItemsOnMinimapExpandedMapChildCount(u32 baseChildCount) {
    return baseChildCount + maxItemMapObjects;
}

extern "C" u32 CreateItemsOnMinimapControls(CtrlRace2DMap* map, u32 childIndex, CtrlRace2DMapObject* templateObject) {
    return CreateItemMapControls(map, childIndex, templateObject);
}

extern "C" void UpdateItemsOnMinimapControl(ItemMapObject* control) {
    UpdateItemMapControl(control);
}

static asmFunc AddItemMapChildCount() {
    ASM(
        mflr r0;
        stwu r1, -0x10(r1);
        stw r0, 0x14(r1);
        stw r3, 0x8(r1);
        stw r4, 0xc(r1);

        addi r3, r27, 1;
        bl GetItemsOnMinimapExpandedMapChildCount;
        mr r26, r3;

        lwz r4, 0xc(r1);
        lwz r3, 0x8(r1);
        lwz r0, 0x14(r1);
        addi r1, r1, 0x10;
        mtlr r0;
        blr;)
}
kmCall(0x807ea450, AddItemMapChildCount);

static asmFunc CreateItemMapControlsHook() {
    ASM(
        mflr r0;
        stwu r1, -0x10(r1);
        stw r0, 0x14(r1);

        bctrl;

        mr r3, r24;
        mr r4, r28;
        mr r5, r25;
        bl CreateItemsOnMinimapControls;
        mr r28, r3;

        lwz r0, 0x14(r1);
        addi r1, r1, 0x10;
        mtlr r0;
        blr;)
}
kmCall(0x807ea6e0, CreateItemMapControlsHook);

static asmFunc UpdateItemMapControlHook() {
    ASM(
        mflr r0;
        stwu r1, -0x10(r1);
        stw r0, 0x14(r1);

        bctrl;

        mr r3, r31;
        bl UpdateItemsOnMinimapControl;

        lwz r0, 0x14(r1);
        addi r1, r1, 0x10;
        mtlr r0;
        blr;)
}
kmCall(0x807eaca8, UpdateItemMapControlHook);

}  // namespace UI
}  // namespace Pulsar
