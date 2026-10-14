return {
  name = "motd",
  type = "oneshot",
  cmd = { "/etc/motd.sh" },
  tty = "/dev/tty0",
}
