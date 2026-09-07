#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <vector>

class Terrain;

// A value collection of bound definitions. Ordering compares addresses only to
// canonicalize sets; neither position nor address is a serialized identity.
class TerrainSet {
public:
    TerrainSet() = default;
    TerrainSet(const Terrain &terrain) : entries_{&terrain} {}
    TerrainSet(std::initializer_list<const Terrain *> entries) : entries_(entries) { normalize(); }

    const std::vector<const Terrain *> &entries() const { return entries_; }
    bool empty() const { return entries_.empty(); }
    explicit operator bool() const { return !empty(); }
    bool contains(const Terrain &terrain) const { return std::binary_search(entries_.begin(), entries_.end(), &terrain, std::less<const Terrain *>()); }
    bool contains_all(const TerrainSet &other) const { return std::includes(entries_.begin(), entries_.end(), other.entries_.begin(), other.entries_.end(), std::less<const Terrain *>()); }
    bool intersects(const TerrainSet &other) const
    {
        for (const auto *entry : other.entries_) if (contains(*entry)) return true;
        return false;
    }
    void add(const Terrain &terrain)
    {
        const auto at = std::lower_bound(entries_.begin(), entries_.end(), &terrain, std::less<const Terrain *>());
        if (at == entries_.end() || *at != &terrain) entries_.insert(at, &terrain);
    }
    TerrainSet &operator|=(const TerrainSet &other) { for (const auto *entry : other.entries_) add(*entry); return *this; }
    TerrainSet &operator-=(const TerrainSet &other)
    {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Terrain *entry) { return other.contains(*entry); }), entries_.end());
        return *this;
    }
    TerrainSet operator|(const TerrainSet &other) const { TerrainSet result(*this); result |= other; return result; }
    TerrainSet operator-(const TerrainSet &other) const { TerrainSet result(*this); result -= other; return result; }
    TerrainSet intersection(const TerrainSet &other) const
    {
        TerrainSet result;
        for (const auto *entry : entries_) if (other.contains(*entry)) result.add(*entry);
        return result;
    }
    TerrainSet operator&(const TerrainSet &other) const { return intersection(other); }
    TerrainSet operator^(const TerrainSet &other) const { return (*this - other) | (other - *this); }
    TerrainSet &operator&=(const TerrainSet &other) { *this = intersection(other); return *this; }
    bool operator==(const TerrainSet &other) const { return entries_ == other.entries_; }
    bool operator!=(const TerrainSet &other) const { return !(*this == other); }
    bool operator<(const TerrainSet &other) const { return std::lexicographical_compare(entries_.begin(), entries_.end(), other.entries_.begin(), other.entries_.end(), std::less<const Terrain *>()); }

private:
    void normalize()
    {
        entries_.erase(std::remove(entries_.begin(), entries_.end(), nullptr), entries_.end());
        std::sort(entries_.begin(), entries_.end(), std::less<const Terrain *>());
        entries_.erase(std::unique(entries_.begin(), entries_.end()), entries_.end());
    }
    std::vector<const Terrain *> entries_;
};
