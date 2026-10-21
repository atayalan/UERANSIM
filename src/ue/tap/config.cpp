
#include "config.hpp"

#include <arpa/inet.h>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <ifaddrs.h>
#include <iostream>
#include <linux/if_tun.h>
#include <memory>
#include <mutex>
#include <net/if.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <utils/libc_error.hpp>


namespace nr::ue::tap
{

static constexpr int MAX_INTERFACE_COUNT = 1024;

static std::mutex configMutex;

static std::string NextInterfaceName(const std::string &prefix)
{
    std::set<std::string> names;

    struct ifaddrs *addrs = nullptr, *tmp;

    if (getifaddrs(&addrs) < 0)
        throw LibError("getifaddrs() failed", errno);
    tmp = addrs;

    while (tmp)
    {
        std::string name = tmp->ifa_name;
        if (name.rfind(prefix, 0) == 0)
            names.insert(name);
        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);

    for (int i = 0; i < MAX_INTERFACE_COUNT; i++)
    {
        std::string name = prefix + std::to_string(i);
        if (!names.count(name))
            return name;
    }
    return {};
}


static void TapSetUp(const char *ifName, int mtu)
{
    ifreq ifr{};
    memset(&ifr, 0, sizeof(struct ifreq));

    int sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0)
        throw LibError("socket() failed", errno);

    strcpy(ifr.ifr_name, ifName);

    if (ioctl(sockFd, SIOCGIFFLAGS, &ifr) < 0)
    {
        close(sockFd);
        throw LibError("ioctl(SIOCGIFFLAGS)", errno);
    }

    // ifr_mtu and ifr_flags share a union, so keep the flags aside while setting the MTU
    short flags = ifr.ifr_flags;

    ifr.ifr_mtu = mtu;
    if (ioctl(sockFd, SIOCSIFMTU, &ifr) < 0)
    {
        close(sockFd);
        throw LibError("ioctl(SIOCSIFMTU)", errno);
    }

    ifr.ifr_flags = flags | IFF_UP | IFF_RUNNING;
    if (ioctl(sockFd, SIOCSIFFLAGS, &ifr) < 0)
    {
        close(sockFd);
        throw LibError("ioctl(SIOCSIFFLAGS)", errno);
    }

    close(sockFd);
}

int AllocateTap(const char *ifPrefix, std::string &allocatedName)
{
    // acquire the configuration lock
    const std::lock_guard<std::mutex> lock(configMutex);

    const std::string ifName = NextInterfaceName(ifPrefix);
    if (ifName.empty())
        throw LibError("TAP interface name could not be allocated.", errno);

    ifreq ifr{};
    int fd;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        throw LibError("Open failure /dev/net/tun", errno);

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    strncpy(ifr.ifr_name, ifName.c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0)
    {
        close(fd);
        throw LibError("ioctl(TUNSETIFF)", errno);
    }

    if (ifName != ifr.ifr_name)
    {
        close(fd);
        throw LibError("TAP interface name could not be allocated.");
    }

    allocatedName = ifName;
    return fd;
}

void ConfigureTap(const char *tapName, int mtu)
{
    // acquire the configuration lock
    const std::lock_guard<std::mutex> lock(configMutex);

    TapSetUp(tapName, mtu);
}

}