#include "assets/image_group_payload_internal.h"

#include "core/crash_context.h"

namespace image_group_payload_internal {

// Input: a group key.
// Output: the merged entry-id namespace across the source chain for that group, or null on failure.
const MergedImageGroup *load_merged_group(const std::string &group_key)
{
    if (const MergedImageGroup *existing = find_merged_group(group_key)) {
        return existing;
    }
    if (g_failed_merged_groups.find(group_key) != g_failed_merged_groups.end()) {
        return nullptr;
    }
    if (g_loading_merged_groups.find(group_key) != g_loading_merged_groups.end()) {
        crash_context_report_error("Detected recursive inherited image group", group_key.c_str());
        g_failed_merged_groups.insert(group_key);
        return nullptr;
    }

    g_loading_merged_groups.insert(group_key);

    const std::vector<GraphicsLayerSource> layers = xml_configured_graphics_sources();
    const std::vector<GraphicsLayerSource> sources = xml_collect_assetlist_sources(layers, group_key.c_str());
    if (sources.empty()) {
        g_loading_merged_groups.erase(group_key);
        g_failed_merged_groups.insert(group_key);
        return nullptr;
    }

    std::unique_ptr<MergedImageGroup> merged = std::make_unique<MergedImageGroup>();
    merged->key = group_key;
    std::vector<const ImageGroupDoc *> source_docs;
    source_docs.reserve(sources.size());

    for (const GraphicsLayerSource &source : sources) {
        const ImageGroupDoc *doc = load_group_doc(group_key, source);
        if (!doc) {
            g_loading_merged_groups.erase(group_key);
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
        source_docs.push_back(doc);
        if (merged->xml_path.empty()) {
            merged->xml_path = doc->xml_path;
        }
    }

    const ImageGroupDoc *inherited_doc = nullptr;
    for (const ImageGroupDoc *doc : source_docs) {
        if (doc->inherits_group()) {
            inherited_doc = doc;
            break;
        }
    }
    if (inherited_doc) {
        if (source_docs.size() != 1) {
            crash_context_report_error("Inherited ImageGroup cannot be merged with another source layer", group_key.c_str());
            g_loading_merged_groups.erase(group_key);
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
        const MergedImageGroup *inherited = load_merged_group(inherited_doc->inherited_group_key);
        if (!inherited || inherited->ordered_entries.empty()) {
            crash_context_report_error("Unable to resolve inherited ImageGroup target", inherited_doc->inherited_group_key.c_str());
            g_loading_merged_groups.erase(group_key);
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
        merged->inherited_group_key = inherited_doc->inherited_group_key;
        merged->logical_units_per_source_pixel = inherited_doc->logical_units_per_source_pixel;
        for (const MergedImageEntrySelector &selector : inherited->ordered_entries) {
            if (selector.source.layer_index <= inherited_doc->source.layer_index) {
                merged->ordered_entries.push_back(selector);
            }
        }
        if (merged->ordered_entries.empty()) {
            crash_context_report_error("Inherited ImageGroup target is not visible from the declaring mod layer", inherited_doc->inherited_group_key.c_str());
            g_loading_merged_groups.erase(group_key);
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
    } else {
        for (const ImageGroupDoc *doc : source_docs) {
            for (const std::string &image_id : doc->ordered_ids) {
                merged->ordered_entries.push_back({ doc->key, doc->source, image_id });
            }
        }
    }

    const MergedImageGroup *merged_ptr = merged.get();
    g_merged_groups.emplace(group_key, std::move(merged));
    g_loading_merged_groups.erase(group_key);
    return merged_ptr;
}

// Input: a merged group, requesting source context, and image id.
// Output: the first exact entry selector visible at or below the requesting source, or null when absent.
const MergedImageEntrySelector *find_merged_entry_selector(const MergedImageGroup &merged, const GraphicsLayerSource &preferred_source, const std::string &image_id)
{
    for (const MergedImageEntrySelector &selector : merged.ordered_entries) {
        if (selector.image_id == image_id && selector.source.layer_index <= preferred_source.layer_index) {
            return &selector;
        }
    }
    return nullptr;
}

// Input: a merged group key, the requesting source context, and one local XML label.
// Output: the resolved entry for the first matching source at or below the requesting source.
const ResolvedImageEntry *materialize_merged_entry(const std::string &group_key, const GraphicsLayerSource &preferred_source, const std::string &image_id)
{
    const MergedImageGroup *merged = load_merged_group(group_key);
    if (!merged) {
        return nullptr;
    }

    const MergedImageEntrySelector *selector = find_merged_entry_selector(*merged, preferred_source, image_id);
    if (!selector) {
        crash_context_report_error("Unable to resolve merged image id", image_id.c_str());
        return nullptr;
    }
    if (merged->inherits_group()) {
        return materialize_scaled_alias_entry(
            group_key,
            *selector,
            merged->logical_units_per_source_pixel);
    }
    return materialize_source_entry(selector->group_key, selector->source, selector->image_id);
}

} // namespace image_group_payload_internal
