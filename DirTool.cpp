#include "DirTool.h"

bool DirTool::isFileExist(const std::string &_file){
    struct stat buffer;
    return (stat (_file.c_str(), &buffer) == 0);
}

bool DirTool::isDirExist(const std::string &_dir){
    struct stat info;
    if (stat(_dir.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

bool DirTool::makeDir(const std::string &_dir){
    mode_t mode = 0755;
    int ret = mkdir(_dir.c_str(), mode);
    if (ret == 0)
        return true;
    switch (errno)
    {
    case ENOENT: {
        int pos = _dir.find_last_of('/');
        if (pos == std::string::npos)
            return false;
        if (!DirTool::makeDir( _dir.substr(0, pos) ))
            return false;
        return 0 == mkdir(_dir.c_str(), mode);
    }
    case EEXIST:
        // done!
        return DirTool::isDirExist(_dir);

    default:
        return false;
    }
}

DirTool::ResultDirTool DirTool::getNumberOfFilesInDir(const std::string &_dir, int &_numberOfFiles){
    struct dirent *de;
    DIR *dir = opendir(_dir.c_str());
    if(!dir)
    {
        return DirTool::ResultDirTool::ERROR_UNKNOWN;
    }

    unsigned long count=0;
    while(de = readdir(dir))
    {
        ++count;
    }
    _numberOfFiles = count;
    return DirTool::ResultDirTool::DIR_SUCCESS;
}

std::string DirTool::getCurrentPath()
{
    char tmp[256];
    memset(tmp, 0, sizeof (tmp));
    getcwd(tmp, sizeof (tmp));
    std::string dir(tmp, strlen(tmp));
    return dir;
}

std::string DirTool::listFileInFolder(const std::string &folderPath)
{
    size_t count = 0;
    struct dirent *res;
    struct stat sb;
    std::string listFile = "";
    //    const char *path = "SaveVideo";

    if (stat(folderPath.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)){
        DIR *folder = opendir ( folderPath.c_str() );

        if (access ( folderPath.c_str(), F_OK ) != -1 ){
            if ( folder ){
                while ( ( res = readdir ( folder ) ) ){
                    if ( strcmp( res->d_name, "." ) && strcmp( res->d_name, ".." ) ){
                        //                        printf("%zu) - %s\n", count + 1, res->d_name);
                        listFile += res->d_name;
                        listFile += ",";
                        count++;
                    }
                }

                closedir ( folder );
            }else{
                perror ( "Could not open the directory" );
                exit( EXIT_FAILURE);
            }
        }

    }else{
        printf("The %s it cannot be opened or is not a directory\n", folderPath.c_str());
        exit( EXIT_FAILURE);
    }
    return listFile;
}

float DirTool::sizeOfFolder(const std::string &folderPath)
{
    total = 0;
    if (access(folderPath.c_str(), R_OK)) {
        return 1;
    }
    if (ftw(folderPath.c_str(), &sum, 1)) {
        perror("ftw");
        return 2;
    }
    return static_cast<float>(total/1000000.0); // Mb
}

std::vector<std::string> DirTool::splitString(const std::string &_str, const std::string &_delimiter)
{
    std::vector<std::string> res;
        std::string strTmp = _str;
        size_t pos = 0;
        std::string token;
        while((pos = strTmp.find(_delimiter)) != std::string::npos){
            token = strTmp.substr(0, pos);
            res.push_back(token);
            strTmp.erase(0, pos + _delimiter.length());
        }
        if(strTmp.compare("") != 0){
            res.push_back(strTmp);
        }
        return res;
}

bool DirTool::removeFile(const std::string &fileName)
{
    int stat = remove(fileName.c_str());
    if(stat == 0)
    {
        printf("Remove File %s Success\n", fileName.c_str());
        return true;
    }
    else
    {
        printf("Can not Remove File %s\n", fileName.c_str());
        return false;
    }
}

int DirTool::sum(const char *fpath, const struct stat *sb, int typeflag)
{
    total += sb->st_size;
    return 0;
}



