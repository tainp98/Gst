#ifndef DIRTOOL_H
#define DIRTOOL_H

#include <string>
#include <iostream>
#include <fstream>
#include <sys/statvfs.h>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <ftw.h>
class DirTool {
public:
    enum class ResultDirTool : unsigned char {
        DIR_SUCCESS = 0,
        ERROR_DIR_NOT_EXIST = 1,
        ERROR_FILE_NOT_EXIST = 2,
        ERROR_UNKNOWN = 3,
        ERROR_CREATE_DIR_FAILED = 4
    };
    static bool isFileExist(const std::string &_file);
    static bool isDirExist(const std::string &_dir);
    static bool makeDir(const std::string &_dir);

    static DirTool::ResultDirTool getNumberOfFilesInDir(const std::string &_dir, int &_numberOfFiles);
    static std::string getCurrentPath();
    static std::string listFileInFolder(std::string const& folderPath);
    static float sizeOfFolder(std::string const& folderPath);
    static std::vector<std::string> splitString(const std::string &_str, const std::string &_delimiter);
    static bool removeFile(std::string const& fileName);
private:
    static inline uint32_t total = 0;
    static int sum(const char *fpath, const struct stat *sb, int typeflag);
};

#endif // DIRTOOL_H

