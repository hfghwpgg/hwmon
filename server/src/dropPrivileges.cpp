#include "dropPrivileges.hpp"
#include <cstdlib>
#include <grp.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

// this only works with sudo. idk what to do if we get elevated
// somehow else (or rather, if we dont get SUDO_UID/GID env)
// for now we crash but its probably not a very good idea
void dropPrivileges() {
  SPDLOG_DEBUG("current uid: {} | gid: {}", getuid(), getgid());

  // we arent running as root
  if (getuid() != 0) {
    SPDLOG_DEBUG("we're NOT running as root");
    return;
  }

  const auto uid = getenv("SUDO_UID");
  const auto gid = getenv("SUDO_GID");

  if (!uid || !gid) {
    SPDLOG_CRITICAL("didnt recieve uid/gid from sudo");
    SPDLOG_WARN("dont run this as root user ...");
    throw std::runtime_error("didnt recieve uid/gid from sudo");
  }

  uid_t target_uid = (uid_t)std::stoi(uid);
  gid_t target_gid = (gid_t)std::stoi(gid);
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

  if (getuid() == 0) {
    SPDLOG_CRITICAL("we're still root, aborting");
    throw std::runtime_error("we're still root, aborting");
  }

  SPDLOG_INFO("successfully dropped root privileges");
  SPDLOG_DEBUG("current uid: {} | gid: {}", getuid(), getgid());
}
