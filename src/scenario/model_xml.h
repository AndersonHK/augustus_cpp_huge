#pragma once

#define MODEL_DATA_VERSION 2

int scenario_model_export_to_xml(const char *filename);
int scenario_model_xml_parse_file(const char *filename);
