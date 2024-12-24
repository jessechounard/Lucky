#include <fstream>
#include <sstream>

#include <Lucky/Utility/FileSystem.hpp>

namespace Lucky
{
    void Split(std::vector<std::string> &output, const std::string &input, const std::string &delimiter)
    {
        size_t start = 0, end = 0;

        while (end != std::string::npos)
        {
            end = input.find(delimiter, start);

            // if at end, use length = maxLength, else use length = end - start.
            std::string s = input.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
            if (s != "")
                output.push_back(s);

            // if at end, use start = maxSize, else use start = end + delimiter.
            start = ((end > (std::string::npos - delimiter.size())) ? std::string::npos : end + delimiter.size());
        }
    }

    std::vector<std::string> Split(const std::string &input, const std::string &delimiter)
    {
        std::vector<std::string> output;
        Split(output, input, delimiter);
        return output;
    }

    std::string GetPathName(const std::string &filePath)
    {
        std::string directoryName = "";
        std::vector<std::string> splitPath = Split(filePath, "/");

        int splitCount = static_cast<int>(splitPath.size());
        for (int i = 0; i < splitCount - 1; i++)
        {
            directoryName += splitPath[i];
            directoryName += "/";
        }

        return directoryName;
    }

    std::string GetFileExtension(const std::string &fileName)
    {
        auto index = fileName.rfind('.');

        if (index != std::string::npos)
            return fileName.substr(index + 1);

        return "";
    }

    // attempts to remove ".." directories from the beginning of the second path
    // if there's a non ".." path at the end of the firstPath to also remove
    // then combine
    std::string CombinePaths(const std::string &firstPath, const std::string &secondPath)
    {
        std::vector<std::string> splitFirst = Split(firstPath, "/");
        std::vector<std::string> splitSecond = Split(secondPath, "/");

        while (!splitSecond.empty() && splitSecond[0] == "..")
        {
            if (!splitFirst.empty() && splitFirst[splitFirst.size() - 1] != "..")
            {
                splitSecond.erase(splitSecond.begin());
                splitFirst.erase(splitFirst.begin() + splitFirst.size() - 1);
            }
            else
            {
                break;
            }
        }

        splitFirst.insert(splitFirst.end(), splitSecond.begin(), splitSecond.end());

        std::string combinedPaths;

        int splitCount = static_cast<int>(splitFirst.size());
        for (int i = 0; i < splitCount - 1; i++)
        {
            combinedPaths += splitFirst[i];
            combinedPaths += "/";
        }
        combinedPaths += splitFirst[splitFirst.size() - 1];

        return combinedPaths;
    }

    std::string ReadFile(const std::string &fileName)
    {
        std::ifstream t(fileName);
        std::stringstream buffer;
        buffer << t.rdbuf();
        return buffer.str();
    }
} // namespace Lucky
