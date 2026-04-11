#pragma once

#include <string>
#include <vector>

namespace Lucky {

// todo: Split should probably be moved out of FileSystem
void Split(
    std::vector<std::string> &output, const std::string &input, const std::string &delimiter);
std::vector<std::string> Split(const std::string &input, const std::string &delimiter);

std::string GetBasePath();
std::string GetPathName(const std::string &filePath);
std::string GetFileName(const std::string &filePath);
std::string GetFileExtension(const std::string &fileName);
std::string RemoveExtension(const std::string &fileName);

std::string CombinePaths(const std::string &firstPath, const std::string &secondPath);

std::string ReadFile(const std::string &fileName);

} // namespace Lucky
