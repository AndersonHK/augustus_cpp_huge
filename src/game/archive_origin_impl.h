#include "core/Logger.h"
// Private implementation included after the native archive layout visitor.
#include "game/augustus_save_bridge.h"
#include "game/augustus_record_bridge.h"
#include "game/augustus_model_bridge.h"
#include "game/augustus_accounting_bridge.h"
#include "game/augustus_city_bridge.h"
#include "game/augustus_scenario_bridge.h"
#include "game/augustus_monument_bridge.h"
#include "game/augustus_empire_bridge.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "map/TerrainRegistry.h"
namespace {
struct ArchivePieceDescription { std::string name; int size; bool compressed; };
struct ValidatedArchiveLayout { bool valid = false; int scenario_version = 0; bool mod_metadata = false; };

uint32_t archive_u32(const uint8_t *data)
{
    return uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
}

ValidatedArchiveLayout validate_archive_layout(const uint8_t *bytes, size_t length, int version, const std::vector<ArchivePieceDescription> &pieces, std::map<std::string, std::vector<uint8_t>> *payloads = nullptr)
{
    ValidatedArchiveLayout result;
    size_t offset = 0, decoded_total = 0;
    std::vector<uint8_t> decoded;
    for (size_t index = 0; index < pieces.size(); ++index) {
        const auto &piece = pieces[index];
        size_t size = piece.size;
        if (piece.size == 0) {
            if (length - offset < 4) return result;
            size = archive_u32(bytes + offset); offset += 4;
            if (!size) {
                if (payloads) payloads->emplace(piece.name, std::vector<uint8_t>{});
                continue;
            }
        }
        if (size > MAX_DYNAMIC_PIECE_SIZE || decoded_total + size > 512u * 1024u * 1024u) return result;
        decoded_total += size;
        const uint8_t *payload = nullptr;
        if (piece.compressed) {
            if (length - offset < 4) return result;
            uint32_t compressed_size = archive_u32(bytes + offset); offset += 4;
            if (compressed_size == UNCOMPRESSED) {
                if (size > length - offset) return result;
                payload = bytes + offset; offset += size;
            } else {
                if (!compressed_size || compressed_size > MAX_COMPRESSED_CHUNK_SIZE || compressed_size > length - offset) return result;
                decoded.resize(size);
                int output_size = 0;
                const bool valid = version > SAVE_GAME_LAST_ZIP_COMPRESSION ?
                    zlib_helper_decompress(const_cast<uint8_t *>(bytes + offset), compressed_size, decoded.data(), static_cast<int>(size), &output_size) != 0 :
                    zip_decompress_quiet(bytes + offset, compressed_size, decoded.data(), static_cast<int>(size)) != 0;
                if (!valid) return result;
                payload = decoded.data(); offset += compressed_size;
            }
        } else {
            if (size > length - offset) {
                // Original campaign saves sometimes omit part of the final unused padding.
                if (index + 1 != pieces.size() || version > 0x66 || piece.name != "end_marker") return result;
                size = length - offset;
            }
            payload = bytes + offset; offset += size;
        }
        if (payloads && !payloads->emplace(piece.name, std::vector<uint8_t>(payload, payload + size)).second) return result;
        if (piece.name == "scenario_version") {
            if (size != 4) return result;
            result.scenario_version = static_cast<int>(archive_u32(payload));
            if (result.scenario_version < 1 || result.scenario_version > 64) return result;
        }
        if (piece.name == "mod_metadata") {
            if (size < 9 || archive_u32(payload) != size || archive_u32(payload + 4) != SAVEGAME_MOD_METADATA_VERSION) return result;
            if (!memchr(payload + 8, 0, size - 8)) return result;
            result.mod_metadata = true;
        }
    }
    result.valid = offset == length;
    return result;
}
}

ArchiveOrigin game_file_io_identify_archive(const uint8_t *bytes, size_t length, ArchiveFamily explicit_origin)
{
    ArchiveOrigin result;
    result.diagnostic = "Archive does not match a supported producer layout.";
    if (!bytes || length < 8) return result;
    result.save_version = static_cast<int>(archive_u32(bytes + 4));
    if (result.save_version < 1 || result.save_version > SAVE_GAME_CURRENT_VERSION) return result;
    if (result.save_version > SAVE_GAME_LAST_STATIC_RESOURCES) {
        if (length < 12) return result;
        result.resource_version = static_cast<int>(archive_u32(bytes + 8));
    }
    constexpr int resources[] = {16, 16, 16, 17, 18, 22};
    constexpr int foods[] = {7, 7, 5, 6, 6, 6};
    if (result.resource_version < 0 || result.resource_version >= 6) return result;
    std::vector<ArchivePieceDescription> pieces;
    visit_native_savegame_layout(static_cast<savegame_version_t>(result.save_version), resources[result.resource_version], foods[result.resource_version],
        [&](size_t, const char *name, int size, int compressed) { pieces.push_back({name, size, compressed != 0}); });
    const auto native = validate_archive_layout(bytes, length, result.save_version, pieces);
    const bool native_valid = native.valid && (result.save_version <= SAVE_GAME_LAST_NO_MOD_METADATA || native.mod_metadata);
    bool foreign_valid = false;
    int foreign_scenario = 0;
    auto test_foreign = [&](const char *identity, auto visitor) {
        pieces.clear();
        visitor([&](const char *name, int size, int compressed) { pieces.push_back({name, size, compressed != 0}); });
        const auto checked = validate_archive_layout(bytes, length, result.save_version, pieces);
        if (checked.valid) { foreign_valid = true; foreign_scenario = checked.scenario_version; result.layouts.push_back(identity); }
    };
    const int version = result.save_version, count = resources[result.resource_version], food_count = foods[result.resource_version];
    // Every distinct ordered layout in the pinned SB03 inventory is tried. The
    // result intentionally retains multiple producer candidates when bytes cannot
    // distinguish changes to a record's meaning without a version/stride change.
    if (version <= 0xbd) {
        test_foreign("augustus-layout-0", [&](auto emit) { augustus_archive_layouts::layout_0::visit(version, count, food_count, emit); });
        test_foreign("augustus-layout-1", [&](auto emit) { augustus_archive_layouts::layout_1::visit(version, count, food_count, emit); });
        test_foreign("augustus-layout-2", [&](auto emit) { augustus_archive_layouts::layout_2::visit(version, count, food_count, emit); });
        test_foreign("augustus-layout-3", [&](auto emit) { augustus_archive_layouts::layout_3::visit(version, count, food_count, emit); });
        test_foreign("augustus-layout-4", [&](auto emit) { augustus_archive_layouts::layout_4::visit(version, count, food_count, emit); });
    }
    if (native_valid) result.layouts.push_back("vespasian-native");
    if (!native_valid && !foreign_valid) return result;
    if (native_valid && foreign_valid && version <= SAVE_GAME_LAST_NO_MOD_METADATA) {
        result.family = ArchiveFamily::SharedLegacy;
    } else if (native_valid && foreign_valid) {
        if (explicit_origin != ArchiveFamily::Vespasian && explicit_origin != ArchiveFamily::Augustus) {
            result.status = ArchiveIdentification::Ambiguous;
            result.diagnostic = "Both save families validate. An explicit source family is required; the file extension cannot resolve this ambiguity.";
            return result;
        }
        result.family = explicit_origin;
    } else {
        result.family = native_valid ? ArchiveFamily::Vespasian : ArchiveFamily::Augustus;
        if (explicit_origin != ArchiveFamily::Unknown && explicit_origin != result.family &&
            !(explicit_origin == ArchiveFamily::SharedLegacy && version <= SAVE_GAME_LAST_NO_MOD_METADATA)) {
            result.diagnostic = "Explicit archive origin does not match the validated structure.";
            return result;
        }
    }
    result.scenario_version = result.family == ArchiveFamily::Augustus ? foreign_scenario : native.scenario_version;
    result.status = ArchiveIdentification::Identified;
    result.diagnostic.clear();
    return result;
}

bool game_file_io_decode_augustus_archive(const uint8_t *bytes, size_t length, AugustusArchive &archive, std::string &diagnostic)
{
    AugustusArchive decoded;
    decoded.origin = game_file_io_identify_archive(bytes, length, ArchiveFamily::Augustus);
    if (decoded.origin.status != ArchiveIdentification::Identified || decoded.origin.family != ArchiveFamily::Augustus) {
        diagnostic = decoded.origin.diagnostic.empty() ? "Archive is not a post-fork Augustus save." : decoded.origin.diagnostic;
        return false;
    }
    constexpr int resources[] = {16, 16, 16, 17, 18, 22}, foods[] = {7, 7, 5, 6, 6, 6};
    const int version = decoded.origin.save_version, count = resources[decoded.origin.resource_version], food_count = foods[decoded.origin.resource_version];
    bool found = false;
    for (const auto &layout : decoded.origin.layouts) {
        std::vector<ArchivePieceDescription> descriptions;
        auto emit = [&](const char *name, int size, int compressed) {
            // The producer renamed this member without changing its payload.
            descriptions.push_back({std::string(name) == "building_model_data" ? "model_data" : name, size, compressed != 0});
        };
        if (layout == "augustus-layout-0") augustus_archive_layouts::layout_0::visit(version, count, food_count, emit);
        else if (layout == "augustus-layout-1") augustus_archive_layouts::layout_1::visit(version, count, food_count, emit);
        else if (layout == "augustus-layout-2") augustus_archive_layouts::layout_2::visit(version, count, food_count, emit);
        else if (layout == "augustus-layout-3") augustus_archive_layouts::layout_3::visit(version, count, food_count, emit);
        else if (layout == "augustus-layout-4") augustus_archive_layouts::layout_4::visit(version, count, food_count, emit);
        else continue;
        std::map<std::string, std::vector<uint8_t>> payloads;
        if (!validate_archive_layout(bytes, length, version, descriptions, &payloads).valid) continue;
        if (found && decoded.pieces != payloads) {
            diagnostic = "Producer layouts decode different payloads; an exact producer revision is required.";
            return false;
        }
        decoded.pieces = std::move(payloads);
        found = true;
    }
    if (!found) {
        diagnostic = "No validated Augustus payload layout remains.";
        return false;
    }
    archive = std::move(decoded);
    diagnostic.clear();
    return true;
}

static bool archive_can_enter_native_reader(const buffer *source, ArchiveOrigin &origin, ArchiveFamily explicit_origin)
{
    if (!source) return false;
    origin = game_file_io_identify_archive(source->data, source->size, explicit_origin);
    if (origin.status != ArchiveIdentification::Identified) {
        Logger::error("Save archive identification failed", origin.diagnostic.c_str(), origin.save_version);
        return false;
    }
    if (origin.family == ArchiveFamily::Augustus) {
        AugustusArchive archive;
        std::set<augustus_save::RequiredDefinition> requirements;
        std::vector<augustus_save::ModelException> model_exceptions;
        std::vector<augustus_save::ConstructionException> construction_exceptions;
        augustus_save::AccountingArchive accounting;
        augustus_save::TradeRouteArchive routes;
        augustus_save::CityRecord city;
        augustus_save::ScenarioRecords scenario_records;
        augustus_save::EmpireRecords empire_records;
        std::string diagnostic;
        if (!game_file_io_decode_augustus_archive(source->data, source->size, archive, diagnostic) ||
            !augustus_save::collect_required_definitions(archive, requirements, diagnostic) ||
            !augustus_save::decode_model_exceptions(archive, model_exceptions, diagnostic) ||
            !augustus_save::decode_accounting(archive, accounting, diagnostic) ||
            !augustus_save::decode_trade_routes(archive, routes, diagnostic) ||
            !augustus_save::decode_city(archive, city, diagnostic) ||
            !augustus_save::decode_scenario_records(archive, augustus_save::ActionOrder::Unspecified, scenario_records, diagnostic) ||
            !augustus_save::decode_construction_exceptions(archive, construction_exceptions, diagnostic) ||
            !augustus_save::decode_empire(archive, empire_records, diagnostic)) {
            Logger::error("Augustus import preflight failed", diagnostic.c_str(), origin.save_version);
            return false;
        }
        for (const auto &entry : model_exceptions) requirements.insert({augustus_save::DefinitionKind::Building, entry.target});
        for (const auto &entry : construction_exceptions) requirements.insert({augustus_save::DefinitionKind::PhasedConstruction, entry.building});
        if (!augustus_save::validate_required_definitions(requirements, [](const augustus_save::RequiredDefinition &requirement) {
                if (requirement.kind == augustus_save::DefinitionKind::Terrain) return terrain_registry().find(requirement.name) != nullptr;
                if (requirement.kind == augustus_save::DefinitionKind::Building) return building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr(requirement.name)) != nullptr;
                if (requirement.kind == augustus_save::DefinitionKind::PhasedConstruction) {
                    const auto *definition = building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr(requirement.name));
                    return definition && definition->has_phased_construction();
                }
                const auto *definition = figure_type_registry_impl::definition_for(figure_type_from_xml_name(requirement.name.c_str()));
                return definition && definition->default_profile();
            }, diagnostic)) {
            Logger::error("Augustus import preflight failed", diagnostic.c_str(), origin.save_version);
            return false;
        }
        return true;
    }
    return true;
}
