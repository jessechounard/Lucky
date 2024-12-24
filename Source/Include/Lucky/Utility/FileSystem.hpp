#pragma once

#include <string>
#include <vector>

namespace Lucky
{
    void Split(std::vector<std::string> &output, const std::string &input, const std::string &delimiter);
    std::vector<std::string> Split(const std::string &input, const std::string &delimiter);

    std::string GetPathName(const std::string &filePath);
    std::string GetFileExtension(const std::string &fileName);

    std::string CombinePaths(const std::string &firstPath, const std::string &secondPath);

    std::string ReadFile(const std::string &fileName);
} // namespace Lucky
