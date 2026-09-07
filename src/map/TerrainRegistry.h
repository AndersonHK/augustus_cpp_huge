#pragma once

// Definition and archive boundary API. Gameplay includes Terrain.h/TerrainMap.h.
#include "map/Terrain.h"
#include "game/mod_definition_loader.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class TerrainRegistry {
public:
    bool load(const std::vector<mod_definition::DefinitionLayer> &layers, std::string &failure);
    void clear();
    bool bind_graphics();
    const Terrain *find(const std::string &name) const;
    const Terrain &require(const std::string &name, const std::string &source) const;
    const Terrain *load_alias(const std::string &name) const;
    TerrainSet bind(const std::string &names, const std::string &source) const;
    const std::map<std::string, std::unique_ptr<Terrain>> &definitions() const { return definitions_; }
    const TerrainTypes &types() const { return types_; }

private:
    std::map<std::string, std::unique_ptr<Terrain>> definitions_;
    std::map<std::string, const Terrain *> load_aliases_;
    TerrainTypes types_;
};

TerrainRegistry &terrain_registry();
int terrain_registry_load();
const char *terrain_registry_failure_reason();
