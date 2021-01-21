#include "FileSystemIo.h"

#ifdef ANDROID

android::SystemControlClient *mSystemControl;

size_t FileSystem_create() {
    if (mSystemControl == NULL) {
        mSystemControl = android::SystemControlClient::getInstance();
        if (mSystemControl == NULL)
            return -1;
    }
    return 0;
}

size_t FileSystem_readFile(const char *name, char *value) {
    std::string strvalue;
    if (mSystemControl == NULL || name == NULL || value == NULL)
        return -1;

    mSystemControl->readSysfs(name, strvalue);
    strncpy(value, strvalue.c_str(),
            std::min(sizeof(value), sizeof(strvalue.c_str())));
    return 0;
}


size_t FileSystem_writeFile(const char *name, const char *value) {
    if (mSystemControl == NULL || name == NULL || value == NULL)
        return -1;

    mSystemControl->writeSysfs(name, value, strlen(value));
    return 0;
}

size_t FileSystem_setAudioParam(int param1, int param2, int param3) {
    if (mSystemControl == NULL)
        return -1;

    mSystemControl->setAudioParam(param1, param2, param3);
    return 0;
}

size_t FileSystem_release() {
    if (mSystemControl == NULL)
        return -1;

    mSystemControl = NULL;
    return 0;
}

#else

size_t FileSystem_create()  {
    return 0;
}

size_t FileSystem_readFile(const char *name, char *cmd) {
    int fd, len, ret;
    ALOGV("open file \"%s\"\n", name);

    fd = open(name, O_WRONLY);
    if (fd == -1)
    {
        ALOGV("cannot open file \"%s\"", name);
        return -1;
    }

    len = strlen(cmd);

    ret = read(fd, cmd, len);
    if (ret != len)
    {
        ALOGV("read failed file:\"%s\" cmd:\"%s\" error:\"%s\"\n", name, cmd, strerror(errno));
        close(fd);
        return AM_Dmx_ERROR;
    }
    else
        ALOGV("read file:\"%s\" cmd:\"%s\" ok \n", name, cmd);

    close(fd);
    return AM_Dmx_SUCCESS;
}


size_t FileSystem_writeFile(const char *name, const char *cmd) {
    int fd, len, ret;
    ALOGV("open file \"%s\"\n", name);

    fd = open(name, O_WRONLY);
    if (fd == -1)
    {
        ALOGV(fmt,...)("cannot open file \"%s\"", name);
        return AM_Dmx_ERROR;
    }

    len = strlen(cmd);

    ret = write(fd, cmd, len);
    if (ret != len)
    {
        ALOGV("write failed file:\"%s\" cmd:\"%s\" error:\"%s\"\n", name, cmd, strerror(errno));
        close(fd);
        return AM_Dmx_ERROR;
    }
    else
        ALOGV("write file:\"%s\" cmd:\"%s\" ok \n", name, cmd);
    close(fd);
    return AM_Dmx_SUCCESS;
}


int FileSystem_setAudioParam(int param1, int param2, int param3);
{
    return 0;
}

int FileSystem_release();
{
    return 0;
}


#endif
