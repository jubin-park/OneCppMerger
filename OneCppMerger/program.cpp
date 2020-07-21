#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cassert>

#include "eLanguage.h"

namespace merger
{
    const char* PREPROCESSOR_INCLUDE = "#include";
    const char* PREPROCESSOR_PRAGMA_ONCE = "#pragma once";

    const char* MAIN_C_NAME = "main.c";
    const char* MAIN_CPP_NAME = "main.cpp";
    const char* OUTPUT_NAME = "merged";

    const char* EXTENSIONS[] = { ".c", ".cpp", ".h" };
    const int EXTENSION_SIZE = sizeof(EXTENSIONS) / sizeof(const char*);

    const char INTERNAL_HEADER_HEAD = '<';
    const char INTERNAL_HEADER_TAIL = '>';
    const char EXTERNAL_HEADER_HEAD = '"';
    const char EXTERNAL_HEADER_TAIL= '"';

    const size_t INCLUDE_LENGTH = strlen(PREPROCESSOR_INCLUDE);

    std::vector<std::string> innerHeaders;
    std::vector<std::string> outerHeaders;

    eLanguage type = eLanguage::INVALID;

    std::string Trim(const std::string& s)
    {
        std::string::const_iterator it = s.begin();
        while (it != s.end() && (*it) == ' ')
        {
            it++;
        }

        std::string::const_reverse_iterator rit = s.rbegin();
        while (rit.base() != it && (*rit) == ' ')
        {
            rit++;
        }

        return std::string(it, rit.base());
    }

    std::string GetFileNameOnly(std::string filenameAndExtension)
    {
        for (char& c : filenameAndExtension)
        {
            tolower(c);
        }

        for (size_t i = 0; i < EXTENSION_SIZE; ++i) {
            size_t found = filenameAndExtension.find_last_of(EXTENSIONS[i]);
            if (found != std::string::npos)
            {
                return filenameAndExtension.substr(0, found);
            }
        }

        return filenameAndExtension;
    }

    void OpenFileRecursive(const std::string& filename)
    {
        std::string one;
        std::ifstream file(filename);

        if (!file.is_open())
        {
            return;
        }

        char c;
        do
        {
            file.get(c);
        } while (c < 0);

        long long offset = file.tellg();
        if (offset > 0)
        {
            --offset;
            file.seekg(offset);
        }

        // Line
        std::string line;
        while (std::getline(file, line))
        {
            line = Trim(line);

            size_t found = line.find(PREPROCESSOR_INCLUDE);
            if (found == std::string::npos)
            {
                continue;
            }

            std::string path = Trim(line.substr(INCLUDE_LENGTH));

            if ((found = path.find(INTERNAL_HEADER_HEAD)) == 0)
            {
                path = Trim(path.substr(1));

                found = path.find(INTERNAL_HEADER_TAIL);
                if (found == std::string::npos)
                {
                    continue;
                }

                path = Trim(path.substr(0, found));

                if (std::find(innerHeaders.begin(), innerHeaders.end(), path) == innerHeaders.end())
                {
                    innerHeaders.push_back(path);
                }
            }
            else if ((found = path.find(EXTERNAL_HEADER_HEAD)) == 0)
            {
                path = path.substr(1);
                                    
                found = path.find(EXTERNAL_HEADER_TAIL);
                if (found == std::string::npos)
                {
                    continue;
                }
                path = GetFileNameOnly(path.substr(0, found));
                    
                if (std::find(outerHeaders.begin(), outerHeaders.end(), path) == outerHeaders.end())
                {
                    OpenFileRecursive(path + EXTENSIONS[2]);
                    outerHeaders.push_back(path);
                }
            }
        }
        file.close();
    }

    bool IsFileExisted(const std::string& filename)
    {
        std::ifstream file(filename);

        bool bExisted = file.is_open() && !file.fail();
        file.close();

        return bExisted;
    }

    void WriteCodeExceptPreprocessors(const std::string& filename, std::ofstream& ofstream)
    {
        std::ifstream file(filename);

        if (!file.is_open())
        {
            return;
        }

        char c;
        size_t offset = 0;
        do
        {
            file.get(c);
            ++offset;
        } while (c < 0);

        if (offset > 0)
        {
            --offset;
        }
        file.seekg(offset);

        std::string line;

        while (std::getline(file, line))
        {
            std::string trimmedLine(Trim(line));

            size_t found = trimmedLine.find(PREPROCESSOR_INCLUDE);
            if (found == 0)
            {
                continue;
            }

            found = trimmedLine.find(PREPROCESSOR_PRAGMA_ONCE);
            if (found == 0)
            {
                continue;
            }

            ofstream << line << '\n';
        }

        long long pos = ofstream.tellp();
        if (pos >= 2)
        {
            ofstream.seekp(pos - 2);
            ofstream << "\0\0";
        }
    }

    void CreateMergedFile()
    {
        assert (type != eLanguage::INVALID);

        std::string filename(OUTPUT_NAME);
        std::ofstream output;

        const int extensionIndex = (type == eLanguage::CPP ? 1 : 0);

        filename.append(EXTENSIONS[extensionIndex]);
        output.open(filename.c_str());

        if (!output.is_open())
        {
            std::cout << "load error\n";
            return;
        }

        // inner headers
        for (const std::string& header : innerHeaders)
        {
            output << PREPROCESSOR_INCLUDE << ' ' << '<' << header << '>' << '\n';
        }
        long long pos = output.tellp();
        if (pos >= 2)
        {
            output.seekp(pos - 2);
            output << "\0\0";
        }

        // outer headers first
        for (const std::string& header : outerHeaders)
        {
            if (IsFileExisted(header + EXTENSIONS[2]))
            {
                WriteCodeExceptPreprocessors(header + EXTENSIONS[2], output);
            }
        }

        // outer sources second
        for (const std::string& header : outerHeaders)
        {
            if (IsFileExisted(header + EXTENSIONS[extensionIndex]))
            {
                WriteCodeExceptPreprocessors(header + EXTENSIONS[extensionIndex], output);
            }
        }

        WriteCodeExceptPreprocessors(type == eLanguage::CPP ? MAIN_CPP_NAME : MAIN_C_NAME, output);

        output.close();
    }

    void Run()
    {
        innerHeaders.reserve(32);
        outerHeaders.reserve(32);

        if (IsFileExisted(MAIN_C_NAME))
        {
            type = eLanguage::C;
            OpenFileRecursive(MAIN_C_NAME);
            CreateMergedFile();
        }
        else if (IsFileExisted(MAIN_CPP_NAME))
        {
            type = eLanguage::CPP;
            OpenFileRecursive(MAIN_CPP_NAME);
            CreateMergedFile();
        }
        else
        {
            std::cout << "main file is not existed.\n";
        }
    }
}

int main()
{
    merger::Run();
    return 0;
}