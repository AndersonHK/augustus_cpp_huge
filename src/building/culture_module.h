#pragma once

#include <string>
namespace building_type_registry_impl {

enum class CultureModuleType {
    None,
    Theater,
    Amphitheater,
    Arena,
    Colosseum,
    ColosseumPresence,
    Hippodrome,
    Tavern,
    School,
    Library,
    Academy,
    Hospital,
    Oracle,
    Temple
};

enum class CultureModuleCountMode {
    Active,
    Total,
    Working
};

class CultureModule {
public:
    explicit CultureModule(std::string path);

    const char *path() const;
    void set_type(CultureModuleType type);
    CultureModuleType type() const;
private:
    std::string path_;
    CultureModuleType type_ = CultureModuleType::None;
};

} // namespace building_type_registry_impl
