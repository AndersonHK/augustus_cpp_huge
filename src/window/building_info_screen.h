#pragma once

class Building;

enum class BuildingInfoScreenId {
    Other,
    Storage,
    StorageOrders,
    StorageRoadblockOrders,
    FoundationRoadblock,
    FoundationRoadblockOrders,
};

// Resolves one authoritative building-info screen before draw/input dispatch.
// The ids are presentation-oriented so they can become XML document/template
// ids without leaking building-type conditionals into future DOM code.
class BuildingInfoScreenSelection {
public:
    static BuildingInfoScreenSelection resolve(const Building &building, int special_orders);

    BuildingInfoScreenId id() const;
    bool isStorageFamily() const;
    bool isFoundationRoadblockFamily() const;

private:
    explicit BuildingInfoScreenSelection(BuildingInfoScreenId id);

    BuildingInfoScreenId id_ = BuildingInfoScreenId::Other;
};
