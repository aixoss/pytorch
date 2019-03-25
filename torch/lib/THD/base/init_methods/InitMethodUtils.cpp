#include "InitMethodUtils.hpp"

//#include <ifaddrs.h>
#ifndef GENERIC_AIX_IFADDRS_H
#define GENERIC_AIX_IFADDRS_H

#include <sys/socket.h>
#include <net/if.h>

#undef  ifa_dstaddr
#undef  ifa_broadaddr
#define ifa_broadaddr ifa_dstaddr

struct ifaddrs {
  struct ifaddrs  *ifa_next;
  char            *ifa_name;
  unsigned int     ifa_flags;
  struct sockaddr *ifa_addr;
  struct sockaddr *ifa_netmask;
  struct sockaddr *ifa_dstaddr;
};

extern int getifaddrs(struct ifaddrs **);
extern void freeifaddrs(struct ifaddrs *);

#endif

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <tuple>

#include <string.h>
#include <sys/ioctl.h>



/********************************************************************
 *** NOTE: this generic version written specifically for AIX 5.3  ***
 ********************************************************************/

#define MAX(x,y) ((x)>(y)?(x):(y))
#define SIZE(p) MAX((p).sa_len,sizeof(p))


static struct sockaddr *
sa_dup (struct sockaddr *sa1)
{
  struct sockaddr *sa2;
  size_t sz = sa1->sa_len;
  sa2 = (struct sockaddr *) calloc(1,sz);
  memcpy(sa2,sa1,sz);
  return(sa2);
}


void freeifaddrs (struct ifaddrs *ifp)
{
  if (NULL == ifp) return;
  free(ifp->ifa_name);
  free(ifp->ifa_addr);
  free(ifp->ifa_netmask);
  free(ifp->ifa_dstaddr);
  freeifaddrs(ifp->ifa_next);
  free(ifp);
}


int getifaddrs (struct ifaddrs **ifap)
{
  int  sd, ifsize;
  char *ccp, *ecp;
  struct ifconf ifc;
  struct ifreq *ifr;
  struct ifaddrs *cifa = NULL; /* current */
  struct ifaddrs *pifa = NULL; /* previous */
  const size_t IFREQSZ = sizeof(struct ifreq);

  sd = socket(AF_INET, SOCK_DGRAM, 0);

  *ifap = NULL;

  /* find how much memory to allocate for the SIOCGIFCONF call */
  if (ioctl(sd, SIOCGSIZIFCONF, (caddr_t)&ifsize) < 0) return(-1);

  ifc.ifc_req = (struct ifreq *) calloc(1,ifsize);
  ifc.ifc_len = ifsize;

  if (ioctl(sd, SIOCGIFCONF, &ifc) < 0) return(-1);

  ccp = (char *)ifc.ifc_req;
  ecp = ccp + ifsize;

  while (ccp < ecp) {

    ifr = (struct ifreq *) ccp;
    ifsize = sizeof(ifr->ifr_name) + SIZE(ifr->ifr_addr);
    cifa = (struct ifaddrs *) calloc(1, sizeof(struct ifaddrs));
    cifa->ifa_next = NULL;
    cifa->ifa_name = strdup(ifr->ifr_name);

    if (pifa == NULL) *ifap = cifa; /* first one */
    else     pifa->ifa_next = cifa;

    if (ioctl(sd, SIOCGIFADDR, ifr, IFREQSZ) < 0) return(-1);
    cifa->ifa_addr = sa_dup(&ifr->ifr_addr);

    if (ioctl(sd, SIOCGIFNETMASK, ifr, IFREQSZ) < 0) return(-1);
    cifa->ifa_netmask = sa_dup(&ifr->ifr_addr);

    cifa->ifa_flags = 0;
    cifa->ifa_dstaddr = NULL;

    if (0 == ioctl(sd, SIOCGIFFLAGS, ifr)) /* optional */
      cifa->ifa_flags = ifr->ifr_flags;

    if (ioctl(sd, SIOCGIFDSTADDR, ifr, IFREQSZ) < 0) {
      if (0 == ioctl(sd, SIOCGIFBRDADDR, ifr, IFREQSZ))
         cifa->ifa_dstaddr = sa_dup(&ifr->ifr_addr);
    }
    else cifa->ifa_dstaddr = sa_dup(&ifr->ifr_addr);

    pifa = cifa;
    ccp += ifsize;
  }
  return 0;
}


namespace thd {

namespace {

void sendPeerName(int socket) {
  struct sockaddr_storage master_addr;
  socklen_t master_addr_len = sizeof(master_addr);
  SYSCHECK(getpeername(
      socket,
      reinterpret_cast<struct sockaddr*>(&master_addr),
      &master_addr_len));

  std::string addr_str =
      sockaddrToString(reinterpret_cast<struct sockaddr*>(&master_addr));
  send_string(socket, addr_str);
}

} // namespace

std::vector<std::string> getInterfaceAddresses() {
  struct ifaddrs* ifa;
  SYSCHECK(getifaddrs(&ifa));
  ResourceGuard ifaddrs_guard([ifa]() { ::freeifaddrs(ifa); });

  std::vector<std::string> addresses;

  while (ifa != nullptr) {
    struct sockaddr* addr = ifa->ifa_addr;
    if (addr) {
      bool is_loopback = ifa->ifa_flags & IFF_LOOPBACK;
      bool is_ip = addr->sa_family == AF_INET || addr->sa_family == AF_INET6;
      if (is_ip && !is_loopback) {
        addresses.push_back(sockaddrToString(addr));
      }
    }
    ifa = ifa->ifa_next;
  }

  return addresses;
}

std::string discoverWorkers(int listen_socket, rank_type world_size) {
  // accept connections from workers so they can know our address
  std::vector<int> sockets(world_size - 1);
  for (rank_type i = 0; i < world_size - 1; ++i) {
    std::tie(sockets[i], std::ignore) = accept(listen_socket);
  }

  std::string public_addr;
  for (auto socket : sockets) {
    sendPeerName(socket);
    public_addr = recv_string(socket);
    ::close(socket);
  }
  return public_addr;
}

std::pair<std::string, std::string> discoverMaster(
    std::vector<std::string> addresses,
    port_type port) {
  // try to connect to address via any of the addresses
  std::string master_address = "";
  int socket;
  for (const auto& address : addresses) {
    try {
      socket = connect(address, port, true, 2000);
      master_address = address;
      break;
    } catch (...) {
    } // when connection fails just try different address
  }

  if (master_address == "") {
    throw std::runtime_error(
        "could not establish connection with other processes");
  }
  ResourceGuard socket_guard([socket]() { ::close(socket); });
  sendPeerName(socket);
  std::string my_address = recv_string(socket);

  return std::make_pair(master_address, my_address);
}

rank_type getRank(
    const std::vector<int>& ranks,
    int assigned_rank,
    size_t order) {
  if (assigned_rank >= 0) {
    return assigned_rank;
  } else {
    std::vector<bool> taken_ranks(ranks.size());
    for (auto rank : ranks) {
      if (rank >= 0)
        taken_ranks[rank] = true;
    }

    auto unassigned = std::count(ranks.begin(), ranks.begin() + order, -1) + 1;
    rank_type rank = 0;
    while (true) {
      if (!taken_ranks[rank])
        unassigned--;
      if (unassigned == 0)
        break;
      rank++;
    }

    return rank;
  }
}
} // namespace thd
