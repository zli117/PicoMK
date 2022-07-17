#include "configuration.h"

#include "cJSON/cJSON.h"
#include "utils.h"

std::string ConfigObject::ToJSON() const {
  cJSON* root = ToCJSON();
  if (root == NULL) {
    LOG_ERROR("Failed to create json for ConfigObject");
    return "";
  }
  char* string = cJSON_Print(root);
  if (string == NULL) {
    LOG_ERROR("Failed to print json");
  }
  std::string output(string);
  free((void*)string);
  cJSON_Delete(root);
  return output;
}

cJSON* ConfigObject::ToCJSON() const {
  cJSON* root = cJSON_CreateObject();
  if (root == NULL) {
    return NULL;
  }
  for (const auto& [k, v] : members_) {
    cJSON* json = v->ToCJSON();
    if (json == NULL) {
      LOG_WARNING("%s returned NULL json ptr", k);
    }
    cJSON_AddItemToObject(root, k.c_str(), json);
  }
  return root;
}

cJSON* ConfigList::ToCJSON() const {
  cJSON* root = cJSON_CreateArray();
  if (root == NULL) {
    return NULL;
  }
  for (const auto& v : list_) {
    cJSON* json = v->ToCJSON();
    if (json == NULL) {
      LOG_WARNING("NULL json ptr");
    }
    cJSON_AddItemToArray(root, json);
  }
  return root;
}

cJSON* ConfigInt::ToCJSON() const { return cJSON_CreateNumber(value_); }

cJSON* ConfigFloat::ToCJSON() const { return cJSON_CreateNumber(value_); }
