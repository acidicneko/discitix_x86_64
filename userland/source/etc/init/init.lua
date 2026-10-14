-- Discitix init. Run by /sbin/init, which provides the `sys` module.
--
-- Services are loaded from SERVICE_DIR, one table per *.lua file:
--
--   name     unique name (required)
--   cmd      argv table, cmd[1] is a path or a name looked up in PATH (required)
--   type     "simple" (default): ready as soon as it has started
--            "oneshot": ready when it exits with status 0
--   after    names of services that must be ready before this one starts
--   tty      terminal for stdin/stdout/stderr, e.g. "/dev/tty1". Without one,
--            stdin is /dev/null and output is appended to LOG_DIR/<name>.log
--   log      overrides the log file of a service without a tty; false = discard
--   respawn  restart a simple service when it exits
--   env      extra environment variables, added to /etc/environment

local SERVICE_DIR = "/etc/init/services"
local LOG_DIR = "/var/log"
local INIT_LOG = LOG_DIR .. "/init.log"
local ENV_FILE = "/etc/environment"

local RESPAWN_DELAY = 1000     -- ms before restarting
local RESPAWN_BURST = 5        -- give up after this many exits...
local RESPAWN_WINDOW = 10000   -- ...within this many ms

local services, order, by_pid = {}, {}, {}

local function log(fmt, ...)
  local line = string.format("%s [%6.2f] %s\n", os.date("%Y-%m-%d %H:%M:%S"),
                             sys.now() / 1000, string.format(fmt, ...))
  local f = io.open(INIT_LOG, "a")
  if f then
    f:write(line)
    f:close()
  else
    io.stderr:write("[init] ", line)
  end
end


local function read_environment()
  local env = {}
  local f = io.open(ENV_FILE, "r")
  if not f then return env end
  for line in f:lines() do
    local key, value = line:match("^%s*([%w_]+)=(.*)$")
    if key then env[key] = value end
  end
  f:close()
  return env
end

local base_env = read_environment()

local function build_env(extra)
  local merged = {}
  for k, v in pairs(base_env) do merged[k] = v end
  for k, v in pairs(extra or {}) do merged[k] = tostring(v) end
  local list = {}
  for k, v in pairs(merged) do list[#list + 1] = k .. "=" .. v end
  table.sort(list)
  return list, merged
end

local function find_program(name, env)
  if name:find("/") then return name end
  for dir in (env.PATH or "/bin"):gmatch("[^:]+") do
    local path = dir .. "/" .. name
    if sys.exists(path) then return path end
  end
  return nil
end

-- Loading and validation ----------------------------------------------------

local function load_services()
  local files, err = sys.listdir(SERVICE_DIR)
  if not files then
    log("cannot read %s: %s", SERVICE_DIR, err)
    return
  end
  table.sort(files)

  for _, file in ipairs(files) do
    if file:match("%.lua$") then
      local path = SERVICE_DIR .. "/" .. file
      local ok, svc = pcall(dofile, path)
      if not ok then
        log("%s: %s", file, svc)
      elseif type(svc) ~= "table" or type(svc.name) ~= "string"
          or type(svc.cmd) ~= "table" or not svc.cmd[1] then
        log("%s: a service needs a name and a cmd table", file)
      elseif services[svc.name] then
        log("%s: duplicate service name '%s'", file, svc.name)
      else
        svc.type = svc.type or "simple"
        svc.after = svc.after or {}
        svc.state = "waiting"
        svc.exits = {}
        if svc.type ~= "simple" and svc.type ~= "oneshot" then
          log("%s: unknown type '%s'", svc.name, tostring(svc.type))
          svc.state = "failed"
        end
        services[svc.name] = svc
        order[#order + 1] = svc
      end
    end
  end
end

local function check_dependencies()
  for _, svc in ipairs(order) do
    for _, dep in ipairs(svc.after) do
      if not services[dep] then
        log("%s: depends on unknown service '%s'", svc.name, dep)
        svc.state = "failed"
      end
    end
  end

  -- Cycles: depth-first search, "visiting" marks the current path
  local mark = {}
  local function visit(svc, path)
    if mark[svc] == "done" then return end
    if mark[svc] == "visiting" then
      local cycle, start = {}, false
      for _, s in ipairs(path) do
        if s == svc then start = true end
        if start then
          cycle[#cycle + 1] = s.name
          s.state = "failed"
        end
      end
      log("dependency cycle: %s -> %s", table.concat(cycle, " -> "), svc.name)
      return
    end
    mark[svc] = "visiting"
    path[#path + 1] = svc
    for _, dep in ipairs(svc.after) do
      if services[dep] then visit(services[dep], path) end
    end
    path[#path] = nil
    mark[svc] = "done"
  end
  for _, svc in ipairs(order) do visit(svc, {}) end
end


local function redirect(fd, path, flags)
  local opened, err = sys.open(path, flags)
  if not opened then return nil, path .. ": " .. err end
  if opened ~= fd then
    sys.dup2(opened, fd)
    sys.close(opened)
  end
  return true
end

-- Runs in the forked child
local function setup_stdio(svc)
  if svc.tty then
    local ok, err = redirect(0, svc.tty, sys.O_RDWR)
    if not ok then return nil, err end
    sys.dup2(0, 1)
    sys.dup2(0, 2)
    return true
  end

  local ok, err = redirect(0, "/dev/null", sys.O_RDONLY)
  if not ok then return nil, err end
  local out = svc.log
  if out == nil then out = LOG_DIR .. "/" .. svc.name .. ".log" end
  if out == false then out = "/dev/null" end
  ok, err = redirect(1, out, sys.O_WRONLY | sys.O_CREAT | sys.O_APPEND)
  if not ok then return nil, err end
  sys.dup2(1, 2)
  return true
end

local function start(svc)
  local envlist, env = build_env(svc.env)
  local path = find_program(svc.cmd[1], env)
  if not path then
    log("%s: %s: not found", svc.name, svc.cmd[1])
    svc.state = "failed"
    return
  end

  local pid, err = sys.fork()
  if not pid then
    log("%s: fork failed: %s", svc.name, err)
    svc.state = "failed"
    return
  end

  if pid == 0 then
    local ok, serr = setup_stdio(svc)
    if not ok then
      io.stderr:write("[init] ", svc.name, ": ", serr, "\n")
      sys.exit(126)
    end
    local _, eerr = sys.exec(path, svc.cmd, envlist)
    io.stderr:write("[init] ", svc.name, ": exec ", path, ": ", tostring(eerr), "\n")
    sys.exit(127)
  end

  svc.pid = pid
  svc.state = "running"
  svc.started = true
  by_pid[pid] = svc
  log("started %s (pid %d)", svc.name, pid)
end

local function is_ready(svc)
  if svc.type == "oneshot" then return svc.state == "done" end
  return svc.started == true
end

local function has_failed(svc)
  return svc.state == "failed" or svc.state == "skipped"
end

-- Start everything whose dependencies are ready; skip what can never start
local function schedule()
  local progress = true
  while progress do
    progress = false
    for _, svc in ipairs(order) do
      if svc.state == "waiting" then
        local blocked, broken = false, nil
        for _, name in ipairs(svc.after) do
          local dep = services[name]
          if has_failed(dep) then
            broken = name
          elseif not is_ready(dep) then
            blocked = true
          end
        end
        if broken then
          log("skipping %s: '%s' failed", svc.name, broken)
          svc.state = "skipped"
          progress = true
        elseif not blocked then
          start(svc)
          progress = true
        end
      end
    end
  end
end

-- Supervision ---------------------------------------------------------------

local function describe(how, code)
  if how == "signaled" then return "killed by signal " .. code end
  return "exited with status " .. code
end

local function on_exit(pid, how, code)
  local svc = by_pid[pid]
  if not svc then return end
  by_pid[pid] = nil
  svc.pid = nil

  if svc.type == "oneshot" then
    if how == "exited" and code == 0 then
      svc.state = "done"
      log("%s finished", svc.name)
    else
      svc.state = "failed"
      log("%s failed: %s", svc.name, describe(how, code))
    end
    return
  end

  log("%s %s", svc.name, describe(how, code))
  if not svc.respawn then
    svc.state = "stopped"
    return
  end

  local now = sys.now()
  local recent = {}
  for _, t in ipairs(svc.exits) do
    if now - t < RESPAWN_WINDOW then recent[#recent + 1] = t end
  end
  recent[#recent + 1] = now
  svc.exits = recent

  if #recent >= RESPAWN_BURST then
    log("%s exited %d times in %d s, not restarting", svc.name, #recent, RESPAWN_WINDOW // 1000)
    svc.state = "failed"
  else
    svc.state = "respawning"
    svc.respawn_at = now + RESPAWN_DELAY
  end
end

local function respawn_due()
  local now, pending = sys.now(), false
  for _, svc in ipairs(order) do
    if svc.state == "respawning" then
      if now >= svc.respawn_at then
        start(svc)
      else
        pending = true
      end
    end
  end
  return pending
end

-- Main ----------------------------------------------------------------------

sys.mkdir("/var")
sys.mkdir(LOG_DIR)
log("---- init started (pid %d) ----", sys.getpid())

load_services()
check_dependencies()
log("%d service(s) loaded", #order)
schedule()

while true do
  local pending = respawn_due()
  -- Block in wait() unless a restart is due soon; then poll
  local pid, how, code = sys.wait(pending)
  if pid and pid > 0 then
    on_exit(pid, how, code)
    schedule()
  elseif pending or pid == 0 then
    sys.sleep(100)
  else
    -- No children left and nothing to restart: stay alive, idle
    sys.sleep(1000)
  end
end
