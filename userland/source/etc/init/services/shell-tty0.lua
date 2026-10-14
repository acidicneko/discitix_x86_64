return {
  name = "shell-tty0",
  cmd = { "/bin/dash" },
  tty = "/dev/tty0",
  after = { "motd" },
  respawn = true,
}
