#pragma once


typedef enum {
    XML_ASSET_SOURCE_AUTO = 0,
    XML_ASSET_SOURCE_MOD = 1,
    XML_ASSET_SOURCE_AUGUSTUS = 2,
    XML_ASSET_SOURCE_JULIUS = 3
} xml_asset_source;

void xml_init(void);
int xml_process_assetlist_file(const char *xml_file_name);
int xml_process_assetlist_file_from_source(const char *xml_file_name, xml_asset_source source);
void xml_finish(void);
int xml_resolve_assetlist_path(char *full_path, const char *assetlist_key, xml_asset_source source, xml_asset_source *resolved_source);
int xml_resolve_image_path(char *full_path, const char *assetlist_key, const char *image_file_name, xml_asset_source source);
int xml_resolve_group_image_path(char *full_path, const char *group_name, xml_asset_source source);
void xml_get_full_image_path(char *full_path, const char *image_file_name);
void xml_get_full_group_image_path(char *full_path, const char *group_name);


