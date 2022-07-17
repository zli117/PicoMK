#ifndef STORAGE_H_
#define STORAGE_H_

#include <string>

#include "utils.h"

Status InitializeStorage();

Status WriteStringToFile(const std::string& content, const std::string& name);
Status ReadFileContent(const std::string& name, std::string* output);
Status GetFileSize(const std::string& name, size_t* output);
Status RemoveFile(const std::string& name);

#endif /* STORAGE_H_ */