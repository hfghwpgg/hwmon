#include "dropPrivileges.hpp"
#include <cstdlib>
#include <grp.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unistd.h>

void dropPrivileges() {
  // we arent running as root
  if (getuid() != 0) {
    spdlog::debug("we're NOT running as root");
    return;
  }

  const auto uid = getenv("SUDO_UID");
  const auto gid = getenv("SUDO_GID");

  if (!uid || !gid) {
    spdlog::critical("didnt recieve uid/gid from sudo");
    throw std::runtime_error("didnt recieve uid/gid from sudo");
  }

  uid_t target_uid = (uid_t)atoi(uid);
  gid_t target_gid = (gid_t)atoi(gid);
  // clear root groups
  if (setgroups(0, NULL) != 0) {
    perror("setgroups");
    throw std::runtime_error("failed to clear root groups");
  }

  // change gid
  if (setgid(target_gid) != 0) {
    perror("setgid");
    throw std::runtime_error("failed to clear root gid");
  }

  // change uid
  if (setuid(target_uid) != 0) {
    perror("setuid");
    throw std::runtime_error("failed to clear root uid");
  }

  // sanity check
  if (setuid(0) == 0) {
    spdlog::critical("we're still root, aborting");
    throw std::runtime_error("we're still root, aborting");
  }

  spdlog::debug("successfully dropped root privileges");
}
