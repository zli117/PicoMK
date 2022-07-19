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

Status ConfigObject::FromCJSON(const cJSON* json) {
  if (json == NULL || !cJSON_IsObject(json)) {
    return ERROR;
  }
  for (auto& [k, v] : members_) {
    const cJSON* child = cJSON_GetObjectItemCaseSensitive(json, k.c_str());
    if (v->FromCJSON(child) != OK) {
      return ERROR;
    }
  }
  return OK;
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

Status ConfigList::FromCJSON(const cJSON* json) {
  if (json == NULL || !cJSON_IsArray(json) ||
      cJSON_GetArraySize(json) != list_.size()) {
    return ERROR;
  }
  for (size_t i = 0; i < list_.size(); ++i) {
    const cJSON* child = cJSON_GetArrayItem(json, i);
    if (list_[i]->FromCJSON(child) != OK) {
      return ERROR;
    }
  }
  return OK;
}

cJSON* ConfigInt::ToCJSON() const { return cJSON_CreateNumber(value_); }

Status ConfigInt::FromCJSON(const cJSON* json) {
  if (json == NULL || !cJSON_IsNumber(json)) {
    return ERROR;
  }
  const int value = json->valueint;
  if (value < min_ || value > max_) {
    return ERROR;
  }
  value_ = value;
  return OK;
}

cJSON* ConfigFloat::ToCJSON() const { return cJSON_CreateNumber(value_); }

Status ConfigFloat::FromCJSON(const cJSON* json) {
  if (json == NULL || !cJSON_IsNumber(json)) {
    return ERROR;
  }
  const float value = json->valuedouble;
  if (value < min_ || value > max_) {
    return ERROR;
  }
  value_ = value;
  return OK;
}

Status ParseConfig(const std::string& json, Config* default_config) {
  cJSON* c_json = cJSON_ParseWithLength(json.c_str(), json.size());
  if (c_json == NULL) {
    return ERROR;
  }
  const Status status = default_config->FromCJSON(c_json);
  cJSON_Delete(c_json);
  return status;
}
